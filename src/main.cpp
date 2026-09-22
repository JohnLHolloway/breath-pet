#include <Arduino.h>
#include <TFT_eSPI.h>
#include <esp_system.h>
#include <Wire.h>
#include <ModulesCSTSelf.tpp>
#include <ModulesCSTMutual.tpp>
#include "storage.h"

// Sensor-free prototype. All sample values are fictional game units, never BAC.
constexpr int POWER_PIN = 15, BACKLIGHT_PIN = 38;
constexpr uint32_t TICK_MS = 30000;
TFT_eSPI lcd;
TFT_eSprite frame(&lcd);
bool displayReady = false;
uint32_t lastTick = 0, lastFrame = 0, lastStatus = 0, frames = 0;
uint32_t carePresses = 0, samplePresses = 0;
PetStorage storage;
TouchLibCSTSelf selfTouch(Wire,18,17,0x15,21);
TouchLibCSTMutual mutualTouch(Wire,18,17,0x1A,21);
TouchLibInterface *touch = nullptr;
const char *touchName = "none";
enum Page { PET, READINGS, SETUP };
Page page = PET;
uint32_t touchTaps = 0;
int rawSample = 0, historyPage = 0, selectedAction = 0;
bool confirmClear = false;
const uint16_t BG = 0x10E4, PANEL = 0x1946, INK = 0xFFFF;
const uint16_t MINT = 0x87F4, LILAC = 0xBCBF, GOLD = 0xFE6D, MUTED = 0x94D5;

struct Pet {
  int food = 75, joy = 80, energy = 90, sample = 0;
  uint32_t feeds = 0;
  void care() {
    food = min(100, food + 15); joy = min(100, joy + 10);
    energy = min(100, energy + 5); ++feeds;
  }
  void tick() {
    food = max(0, food - 2); joy = max(0, joy - 1);
    energy = max(0, energy - (sample >= 70 ? 2 : 1));
  }
  void cycle() { sample = sample == 0 ? 45 : sample == 45 ? 85 : 0; }
  const char *mood() const {
    if (sample >= 70) return "DIZZY";
    if (sample >= 30) return "WOBBLY";
    if (food < 25) return "HUNGRY";
    if (energy < 25) return "SLEEPY";
    return "HAPPY";
  }
} pet;

void captureSample(int raw) {
  rawSample = raw;
  pet.sample = storage.score(raw);
  storage.record(raw,millis());
}
void cycleSample() { captureSample(rawSample == 0 ? 45 : rawSample == 45 ? 85 : 0); }
void resetPet() { pet = Pet(); rawSample = 0; lastTick = millis(); }
void calibrate(const String &action) {
  if (action == "zero") storage.data.zero = rawSample;
  else if (action == "default") { storage.data.zero = 0; storage.data.span = 100; }
  else if (action == "minus") storage.data.span = max(25,static_cast<int>(storage.data.span)-25);
  else if (action == "plus") storage.data.span = min(200,static_cast<int>(storage.data.span)+25);
  else return;
  storage.save(); pet.sample = storage.score(rawSample);
}

struct Button {
  int pin; bool raw = HIGH, stable = HIGH, held = false;
  uint32_t changed = 0, pressed = 0;
  explicit Button(int gpio) : pin(gpio) {}
  // Returns a tap on release or a single hold event after 1.2 seconds.
  int poll(uint32_t now) {
    bool value = digitalRead(pin);
    if (value != raw) { raw = value; changed = now; }
    int event = 0;
    if (now - changed >= 30 && stable != raw) {
      stable = raw;
      if (!stable) { pressed = now; held = false; }
      else if (!held) event = 1;
    }
    if (!stable && !held && now - pressed >= 1200) { held = true; event = 2; }
    return event;
  }
};
Button careButton{0}, sampleButton{14};

// Updated panel initialization from LILYGO's official tft example.
struct LcdCommand { uint8_t cmd, data[14], len; };
const LcdCommand panelCommands[] = {
  {0x11,{0},0x80}, {0x3A,{0x05},1}, {0xB2,{0x0B,0x0B,0,0x33,0x33},5},
  {0xB7,{0x75},1}, {0xBB,{0x28},1}, {0xC0,{0x2C},1}, {0xC2,{1},1},
  {0xC3,{0x1F},1}, {0xC6,{0x13},1}, {0xD0,{0xA7},1},
  {0xD0,{0xA4,0xA1},2}, {0xD6,{0xA1},1},
  {0xE0,{0xF0,0x05,0x0A,0x06,0x06,0x03,0x2B,0x32,0x43,0x36,0x11,0x10,0x2B,0x32},14},
  {0xE1,{0xF0,0x08,0x0C,0x0B,0x09,0x24,0x2B,0x22,0x43,0x38,0x15,0x16,0x2F,0x37},14}
};

void status() {
  // Never block gameplay or queue a partial JSON record when no terminal is open.
  if (Serial.availableForWrite() < 1024) return;
  Serial.printf("{\"app\":\"breath-pet\",\"version\":2,\"sensor\":\"SIMULATED\",\"food\":%d,\"joy\":%d,\"energy\":%d,\"sample\":%d,\"mood\":\"%s\",\"feeds\":%lu,\"display\":%s,\"width\":%d,\"height\":%d,\"psram\":%u,\"heap\":%u,\"frames\":%lu,\"care_presses\":%lu,\"sample_presses\":%lu,\"uptime_ms\":%lu,\"touch\":\"%s\",\"touch_taps\":%lu,\"page\":%d,\"raw\":%d,\"zero\":%u,\"span\":%u,\"history_count\":%u,\"boot\":%lu,\"storage_ok\":%s}\n",
    pet.food, pet.joy, pet.energy, pet.sample, pet.mood(), (unsigned long)pet.feeds,
    displayReady ? "true" : "false", lcd.width(), lcd.height(), ESP.getPsramSize(),
    ESP.getFreeHeap(), (unsigned long)frames, (unsigned long)carePresses,
    (unsigned long)samplePresses, (unsigned long)millis(),touchName,(unsigned long)touchTaps,
    static_cast<int>(page),rawSample,storage.data.zero,storage.data.span,storage.data.count,
    (unsigned long)storage.data.boot, storage.lastWriteOK ? "true" : "false");
}

void historyStatus() {
  Serial.print("{\"history\":[");
  for (int i=0;i<storage.data.count;i++) {
    const Reading &r = storage.data.readings[i];
    Serial.printf("%s{\"n\":%lu,\"boot\":%lu,\"seconds\":%lu,\"raw\":%u,\"score\":%u}",i ? "," : "",
      (unsigned long)r.number,(unsigned long)r.boot,(unsigned long)r.seconds,r.raw,r.score);
  }
  Serial.println("]}");
}

void selfTest() {
  Pet p;
  bool ok = p.sample == 0;
  p.cycle(); ok &= p.sample == 45 && String(p.mood()) == "WOBBLY";
  p.cycle(); ok &= p.sample == 85 && String(p.mood()) == "DIZZY";
  p.cycle(); ok &= p.sample == 0;
  for (int i=0;i<200;i++) p.tick();
  ok &= p.food == 0 && p.joy == 0 && p.energy == 0;
  for (int i=0;i<200;i++) p.care();
  ok &= p.food == 100 && p.joy == 100 && p.energy == 100;
  ok &= displayReady && lcd.width() == 320 && lcd.height() == 170 && ESP.getPsramSize() >= 8000000;
  PetStorage testStorage;
  ok &= testStorage.score(45) == 45;
  testStorage.data.zero = 45;
  ok &= testStorage.score(45) == 0 && testStorage.score(0) == 0;
  testStorage.data.span = 25;
  ok &= testStorage.score(85) == 100;
  Serial.printf("SELFTEST %s: simulated reactions, stat bounds, framebuffer, display geometry, PSRAM\n", ok ? "PASS" : "FAIL");
}

void label(const char *text, int x, int y, uint16_t color = INK, int font = 2) {
  frame.setTextColor(color); frame.drawString(text,x,y,font);
}
void meter(const char *name, int value, int y, uint16_t color) {
  label(name,180,y,MUTED); char val[5]; snprintf(val,sizeof(val),"%d",value);
  label(val,279,y,INK); frame.fillRoundRect(180,y+18,122,5,2,0x2968);
  if (value) frame.fillRoundRect(180,y+18,max(3,122*value/100),5,2,color);
}

void tile(const char *text,int x,int y,int width,uint16_t color=PANEL) {
  frame.fillRoundRect(x,y,width,30,5,color);
  frame.setTextColor(INK); frame.setTextDatum(MC_DATUM);
  frame.drawString(text,x+width/2,y+15,2); frame.setTextDatum(TL_DATUM);
}
void tabs() {
  const char *names[] = {"Pet","Readings","Setup"};
  for (int i=0;i<3;i++) tile(names[i],i*107,140,i==2?106:105,page==i?0x3A8C:PANEL);
}
void drawOtherPage() {
  if (page==READINGS) {
    char summary[38]; snprintf(summary,sizeof(summary),"SIMULATED HISTORY  %d/4",historyPage+1);
    label(summary,10,9,GOLD,1);
    if (!storage.data.count) label("Tap pet / FAKE to log a sample.",10,48,MUTED,2);
    for (int i=0;i<4;i++) {
      int n=historyPage*4+i; if (n>=storage.data.count) break;
      const Reading &r=storage.data.readings[n];
      char line[52]; snprintf(line,sizeof(line),"#%lu  raw %u > %u  [boot %lu, %lus]",
        (unsigned long)r.number,r.raw,r.score,(unsigned long)r.boot,(unsigned long)r.seconds);
      label(line,10,30+i*18,i==0?INK:MUTED,1);
    }
    tile("Prev",8,106,70); tile("Next",84,106,70);
    tile(confirmClear?"Tap to erase":"Clear history",166,106,146,confirmClear?0xA208:PANEL);
  } else {
    label("CALIBRATION / SIM ONLY",10,8,GOLD,2);
    char line[55]; snprintf(line,sizeof(line),"Raw %d  Zero %u  Span %u",rawSample,storage.data.zero,storage.data.span);
    label(line,10,35,INK,2);
    label("MQ-3 offline. Game units, not BAC.",10,57,MUTED,1);
    tile("Zero current",8,72,145); tile("Defaults",161,72,151);
    tile("Span -25",8,106,145); tile("Span +25",161,106,151);
  }
  if (page==READINGS) {
    int x=selectedAction==0?8:selectedAction==1?84:166;
    frame.drawRoundRect(x,106,selectedAction==2?146:70,30,5,GOLD);
  } else {
    frame.drawRoundRect(selectedAction%2==0?8:161,selectedAction<2?72:106,
      selectedAction%2==0?145:151,30,5,GOLD);
  }
}

void tap(int x,int y) {
  if (x<0 || x>=320 || y<0 || y>=170) return;
  if (y>=140) { page=static_cast<Page>(min(2,x/107)); confirmClear=false; selectedAction=0; }
  else if (page==PET) { if (x<170) pet.care(); else cycleSample(); }
  else if (page==READINGS && y>=106 && y<136) {
    if (x<78) { historyPage=max(0,historyPage-1); confirmClear=false; }
    else if (x<155) { historyPage=min(max(0,(storage.data.count-1)/4),historyPage+1); confirmClear=false; }
    else if (confirmClear) { storage.clear(); historyPage=0; confirmClear=false; }
    else confirmClear=true;
  } else if (page==SETUP) {
    if (y>=72 && y<102) calibrate(x<157?"zero":"default");
    else if (y>=106 && y<136) calibrate(x<157?"minus":"plus");
  }
  status();
}

void buttonAction() {
  if (page==PET) pet.care();
  else if (page==READINGS) tap(selectedAction==0?40:selectedAction==1?115:230,120);
  else tap(selectedAction%2==0?75:235,selectedAction<2?85:120);
}

void initTouch() {
  gpio_hold_dis(GPIO_NUM_21);
  pinMode(21,OUTPUT); digitalWrite(21,LOW); delay(500); digitalWrite(21,HIGH); delay(50);
  pinMode(16,INPUT_PULLUP); Wire.begin(18,17); Wire.setTimeOut(20);
  Wire.beginTransmission(0x15);
  if (Wire.endTransmission()==0 && selfTouch.init()) { touch=&selfTouch; touchName="CST816-family"; selfTouch.setRotation(1); }
  else {
    Wire.beginTransmission(0x1A);
    if (Wire.endTransmission()==0 && mutualTouch.init()) { touch=&mutualTouch; touchName="CST328"; mutualTouch.setRotation(1); }
  }
}

void pollTouch(uint32_t now) {
  static uint32_t lastPoll=0, lastSeen=0;
  static bool down=false;
  if (!touch || now-lastPoll<15) return;
  lastPoll=now;
  bool pressed=touch->read() && touch->getPointNum()>0;
  if (pressed) {
    TP_Point p=touch->getPoint(0);
    if (!down) { ++touchTaps; Serial.printf("TOUCH %u %u\n",p.x,p.y); tap(p.x,p.y); }
    down=true; lastSeen=now;
  } else if (now-lastSeen>90) down=false;
}
void draw(uint32_t now) {
  frame.fillSprite(BG);
  if (page!=PET) { drawOtherPage(); tabs(); frame.pushSprite(0,0); ++frames; return; }
  label("BREATH PET",12,8,MINT);
  frame.fillRoundRect(230,6,78,21,6,PANEL); label("SIM MODE",239,9,GOLD);
  frame.drawFastHLine(12,32,296,0x2968);
  frame.fillRoundRect(12,40,151,98,12,PANEL);
  int bounce = (now/350)%2 ? 2 : 0;
  int wobble = pet.sample ? (static_cast<int>((now/180)%3)-1)*3 : 0;
  int x = 55+wobble, y = 65+bounce;
  uint16_t body = pet.sample >= 70 ? GOLD : pet.sample ? LILAC : MINT;
  frame.fillEllipse(87,125,34,5,0x10E4);
  // Pixel creature with little ears and feet.
  frame.fillRect(x+8,y-8,12,12,body); frame.fillRect(x+44,y-8,12,12,body);
  frame.fillRect(x+8,y,48,8,body); frame.fillRect(x,y+8,64,40,body);
  frame.fillRect(x+8,y+48,48,8,body);
  frame.fillRect(x+4,y+52,16,8,body); frame.fillRect(x+44,y+52,16,8,body);
  bool blink = now%4000 < 170 || pet.energy < 25;
  if (pet.sample >= 70) {
    for (int ex : {x+16,x+44}) {
      frame.drawLine(ex-4,y+18,ex+4,y+26,BG); frame.drawLine(ex+4,y+18,ex-4,y+26,BG);
      frame.drawLine(ex-3,y+18,ex+5,y+26,BG); frame.drawLine(ex+5,y+18,ex-3,y+26,BG);
    }
  } else {
    frame.fillRect(x+14,y+19,6,blink?2:9,BG); frame.fillRect(x+43,y+19,6,blink?2:9,BG);
    if (!blink) { frame.fillRect(x+14,y+19,2,2,INK); frame.fillRect(x+43,y+19,2,2,INK); }
  }
  frame.fillRect(x+6,y+31,9,4,0xFBD7); frame.fillRect(x+50,y+31,9,4,0xFBD7);
  if (pet.sample) frame.drawRect(x+28,y+34,9,7,BG);
  else { frame.fillRect(x+25,y+37,15,3,BG); frame.fillRect(x+22,y+34,3,3,BG); frame.fillRect(x+40,y+34,3,3,BG); }
  label(pet.mood(),58,126,body,1);
  meter("FOOD",pet.food,42,MINT); meter("JOY",pet.joy,73,LILAC); meter("ENERGY",pet.energy,104,GOLD);
  char sim[20]; snprintf(sim,sizeof(sim),"FAKE %d/100",pet.sample); label(sim,185,130,GOLD,1);
  tabs();
  if (!storage.lastWriteOK) label("SAVE ERROR",12,34,TFT_RED,1);
  frame.pushSprite(0,0); ++frames;
}

void command(const String &cmd) {
  if (cmd == "status") { status(); return; }
  if (cmd == "touchscan") {
    initTouch();
    for (int a=1;a<127;a++) { Wire.beginTransmission(a); if (Wire.endTransmission()==0) Serial.printf("I2C 0x%02X\n",a); }
    Serial.printf("TOUCHSCAN %s irq=%d sda=%d scl=%d\n",touchName,digitalRead(16),digitalRead(18),digitalRead(17));
    return;
  }
  if (cmd == "selftest") { selfTest(); return; }
  if (cmd == "feed") pet.care();
  else if (cmd == "cycle") cycleSample();
  else if (cmd == "sample 0") captureSample(0);
  else if (cmd == "sample 45") captureSample(45);
  else if (cmd == "sample 85") captureSample(85);
  else if (cmd == "reset") resetPet();
  else if (cmd == "history") { historyStatus(); return; }
  else if (cmd == "history clear") { storage.clear(); historyPage=0; }
  else if (cmd == "cal zero") calibrate("zero");
  else if (cmd == "cal default") calibrate("default");
  else if (cmd == "cal minus") calibrate("minus");
  else if (cmd == "cal plus") calibrate("plus");
  else if (cmd == "page pet") { page=PET; confirmClear=false; }
  else if (cmd == "page readings") { page=READINGS; confirmClear=false; }
  else if (cmd == "page setup") { page=SETUP; confirmClear=false; }
  else if (cmd == "reboot") { Serial.println("REBOOT"); delay(100); ESP.restart(); return; }
  else if (cmd == "help") {
    Serial.println("status | feed | cycle | sample 0/45/85 | reset | selftest | history [clear] | cal zero/default/minus/plus | page pet/readings/setup | reboot"); return;
  } else { Serial.println("ERROR unknown command; type help"); return; }
  status();
}

void setup() {
  pinMode(POWER_PIN,OUTPUT); digitalWrite(POWER_PIN,HIGH);
  pinMode(0,INPUT_PULLUP); pinMode(14,INPUT_PULLUP);
  Serial.setTxBufferSize(2048);
  Serial.setTxTimeoutMs(0);
  Serial.begin(115200); // Do not wait for a host: pet also boots from standalone power.
  lcd.begin();
  for (const auto &c : panelCommands) {
    lcd.writecommand(c.cmd);
    for (int i=0;i<(c.len&0x7F);i++) lcd.writedata(c.data[i]);
    if (c.len&0x80) delay(120);
  }
  lcd.setRotation(3);
  storage.begin(); initTouch();
  pinMode(BACKLIGHT_PIN,OUTPUT); digitalWrite(BACKLIGHT_PIN,HIGH);
  frame.setColorDepth(16); displayReady = frame.createSprite(320,170) != nullptr;
  if (!displayReady) {
    lcd.fillScreen(TFT_BLACK); lcd.setTextColor(TFT_RED); lcd.drawString("Framebuffer error",10,20,2);
  }
  lastTick = millis();
  Serial.printf("BREATH PET boot; reset_reason=%d; MQ-3 disabled\n",esp_reset_reason());
  selfTest(); status();
}

void loop() {
  uint32_t now = millis();
  int care = careButton.poll(now), sample = sampleButton.poll(now);
  pollTouch(now);
  if (care) {
    ++carePresses;
    if (care == 2) { if (page==PET) resetPet(); else page=PET; } else buttonAction();
    status();
  }
  if (sample) {
    ++samplePresses;
    if (sample == 2) { page=static_cast<Page>((page+1)%3); confirmClear=false; selectedAction=0; }
    else if (page==PET) cycleSample();
    else { selectedAction=(selectedAction+1)%(page==READINGS?3:4); confirmClear=false; }
    status();
  }
  static String input;
  static bool overflow = false;
  int budget = 64;
  while (Serial.available() && budget-- > 0) {
    char c = Serial.read();
    if (c == '\n') {
      if (overflow) Serial.println("ERROR command too long");
      else { input.trim(); if (input.length()) command(input); }
      input = ""; overflow = false;
    } else if (c != '\r') {
      if (input.length() < 64) input += c; else overflow = true;
    }
  }
  if (now-lastTick >= TICK_MS) { lastTick=now; pet.tick(); }
  if (displayReady && now-lastFrame >= 80) { lastFrame=now; draw(now); }
  if (now-lastStatus >= 5000) { lastStatus=now; status(); }
  delay(2);
}
