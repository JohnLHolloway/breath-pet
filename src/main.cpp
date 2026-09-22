#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <math.h>
#include <ModulesCSTSelf.tpp>
#include <ModulesCSTMutual.tpp>
#include "party.h"

constexpr int POWER_PIN=15,BACKLIGHT_PIN=38;
TFT_eSPI lcd;
TFT_eSprite frame(&lcd);
PartyStorage storage;
TouchLibCSTSelf selfTouch(Wire,18,17,0x15,21);
TouchLibCSTMutual mutualTouch(Wire,18,17,0x1A,21);
TouchLibInterface *touch=nullptr;
const char *touchName="none";
bool displayReady=false;
uint32_t frames=0,touchTaps=0,carePresses=0,samplePresses=0;
uint32_t lastTick=0,lastFrame=0,lastStatus=0,samplingStart=0;
uint32_t fedAt[MAX_PLAYERS]={},restedAt[MAX_PLAYERS]={};
bool fed[MAX_PLAYERS]={},rested[MAX_PLAYERS]={};
enum Page {TANK,NAME_PICK,PET_PICK,PET,FEED,SAMPLING,RESULT,HISTORY,MENU,CALIBRATION,NEW_NIGHT};
const char *const PAGE_NAMES[]={"tank","name","choose_pet","pet","feed","sampling","result","history","menu","calibration","new_night"};
Page page=TANK;
int cursor=6,selected=-1,joiningSlot=-1,nameIndex=0,petIndex=0,historyPage=0;
int demoIndex=1,lastRaw=0,resultDelta=0;
const int DEMO_VALUES[]={0,25,50,85};
const char *notice="";
uint32_t noticeAt=0;

void status();
void message(const char *text);
void tap(int x,int y);
#include "hardware.h"

bool hasPlayer() { return selected>=0 && selected<MAX_PLAYERS && storage.data.players[selected].active; }
Player &player() { return storage.data.players[selected]; }
void show(Page next) {
  page=next; cursor=0; notice="";
  if (next==TANK) {
    cursor=6;
    for (int i=0;i<MAX_PLAYERS;i++) if (storage.data.players[i].active) { cursor=i; break; }
  }
}
bool tankTarget(int i) { return i==7 || (i==6?storage.count()<MAX_PLAYERS:storage.data.players[i].active); }
void addPet() {
  for (int i=0;i<MAX_PLAYERS;i++) if (!storage.data.players[i].active) {
    joiningSlot=i; nameIndex=0;
    while (storage.nameUsed(PICKER_NAMES[nameIndex])) nameIndex=(nameIndex+1)%NAME_COUNT;
    petIndex=i%PET_TYPES; show(NAME_PICK); return;
  }
  message("TANK FULL: SIX PETS TONIGHT");
}
void message(const char *text) { notice=text; noticeAt=millis(); }
uint32_t cooldown(int slot) {
  if (!fed[slot]) return 0;
  uint32_t elapsed=millis()-fedAt[slot];
  return elapsed<FEED_COOLDOWN_MS?FEED_COOLDOWN_MS-elapsed:0;
}
void newNight() {
  storage.newNight(); selected=-1; joiningSlot=-1;
  memset(fed,0,sizeof(fed)); memset(rested,0,sizeof(rested));
  lastTick=millis(); show(TANK);
}
bool capture(int raw) {
  if (!hasPlayer() || raw<0 || raw>100 || cooldown(selected)) return false;
  lastRaw=raw; resultDelta=storage.record(selected,raw,millis());
  fed[selected]=true; fedAt[selected]=millis(); show(RESULT); return true;
}
void beginFeed() {
  if (!hasPlayer()) return;
  if (cooldown(selected)) { message("PET IS FULL. WAIT A MOMENT."); return; }
  samplingStart=millis(); show(SAMPLING);
}
void restPet() {
  if (!hasPlayer()) return;
  if (rested[selected] && millis()-restedAt[selected]<10000) { message("RESTING... TRY AGAIN SOON."); return; }
  storage.rest(selected); rested[selected]=true; restedAt[selected]=millis(); message("A LITTLE REST: +8 HEALTH");
}
void calibrate(int action) {
  if (action==0) storage.data.zero=lastRaw;
  if (action==1) { storage.data.zero=0; storage.data.span=100; }
  if (action==2) storage.data.span=max(25,int(storage.data.span)-25);
  if (action==3) storage.data.span=min(200,int(storage.data.span)+25);
  storage.save();
}
void nextName() {
  do { nameIndex=(nameIndex+1)%NAME_COUNT; } while (storage.nameUsed(PICKER_NAMES[nameIndex]));
}
void selectSlot(int slot) {
  if (slot<0 || slot>=MAX_PLAYERS) return;
  if (storage.data.players[slot].active) { selected=slot; show(PET); }
  else {
    joiningSlot=slot; nameIndex=0;
    if (storage.nameUsed(PICKER_NAMES[nameIndex])) nextName();
    petIndex=slot%PET_TYPES; show(NAME_PICK);
  }
}
void back() {
  if (page==NAME_PICK || page==PET || page==MENU || page==NEW_NIGHT) show(TANK);
  else if (page==PET_PICK) show(NAME_PICK);
  else if (page==CALIBRATION) show(MENU);
  else if (page==TANK) show(MENU);
  else if (hasPlayer()) show(PET);
  else show(TANK);
}
void next() {
  notice="";
  switch (page) {
    case TANK: do { cursor=(cursor+1)%8; } while (!tankTarget(cursor)); break;
    case NAME_PICK: nextName(); break;
    case PET_PICK: petIndex=(petIndex+1)%PET_TYPES; break;
    case PET: cursor=(cursor+1)%3; break;
    case FEED: demoIndex=(demoIndex+1)%4; break;
    case RESULT: show(PET); break;
    case HISTORY: historyPage=(historyPage+1)%max(1,(int(player().count)+3)/4); break;
    case MENU: cursor=(cursor+1)%3; break;
    case CALIBRATION: cursor=(cursor+1)%4; break;
    case NEW_NIGHT: cursor=1-cursor; break;
    case SAMPLING: break;
  }
}
void activate() {
  notice="";
  switch (page) {
    case TANK: if (cursor==7) show(MENU); else if (cursor==6) addPet(); else selectSlot(cursor); break;
    case NAME_PICK: show(PET_PICK); break;
    case PET_PICK: {
      int slot=storage.join(PICKER_NAMES[nameIndex],petIndex,joiningSlot);
      if (slot>=0) { selected=slot; show(PET); } else message("NAME TAKEN OR TANK FULL");
      break;
    }
    case PET:
      if (cursor==0) show(FEED);
      else if (cursor==1) { historyPage=0; show(HISTORY); }
      else restPet();
      break;
    case FEED: beginFeed(); break;
    case RESULT: show(TANK); break;
    case HISTORY: show(PET); break;
    case MENU:
      if (cursor==0) show(TANK); else if (cursor==1) show(CALIBRATION); else show(NEW_NIGHT);
      break;
    case CALIBRATION: calibrate(cursor); break;
    case NEW_NIGHT: if (cursor==1) newNight(); else show(MENU); break;
    case SAMPLING: break;
  }
}

void swimPosition(int slot,uint32_t now,int &x,int &y) {
  float phase=fmodf(now/70.0f+slot*89.0f,528.0f);
  x=28+int(phase<=264?phase:528-phase);
  y=66+(slot%2)*48+int(sinf(now/1300.0f+slot*1.8f)*9);
}
void tap(int x,int y) {
  if (x<0 || x>=320 || y<0 || y>=170) return;
  if (page==TANK) {
    if (x>=276 && y<30) show(MENU);
    else if (x>=194 && y<30) addPet();
    else if (y>=35 && y<146) {
      int closest=-1,distance=1600;
      for (int i=0;i<MAX_PLAYERS;i++) if (storage.data.players[i].active) {
        int px,py; swimPosition(i,millis(),px,py); int d=(x-px)*(x-px)+(y-py)*(y-py);
        if (d<distance) { distance=d; closest=i; }
      }
      if (closest>=0) selectSlot(closest);
    }
  } else if (page==PET && y>=112 && y<146) { cursor=min(2,x/107); activate(); }
  else if (page==MENU && y>=38 && y<140) { cursor=min(2,(y-38)/34); activate(); }
  else if (page==CALIBRATION && y>=75 && y<145) { cursor=(y>=110?2:0)+(x>=160?1:0); activate(); }
  else if (page==NEW_NIGHT && y>=100 && y<145) { cursor=x>=160?1:0; activate(); }
  else if (y>=148) back();
  else if (page!=SAMPLING) { if (x<160) next(); else activate(); }
  status();
}

#include "ui.h"

void status() {
  if (Serial.availableForWrite()<2048) return;
  Serial.printf("{\"app\":\"breath-pet\",\"version\":3,\"sensor\":\"SIMULATED\",\"page\":\"%s\",\"cursor\":%d,\"selected\":%d,\"players\":%d,\"night\":%lu,\"boot\":%lu,\"zero\":%u,\"span\":%u,\"display\":%s,\"psram\":%u,\"frames\":%lu,\"uptime_ms\":%lu,\"touch\":\"%s\",\"touch_taps\":%lu,\"care_presses\":%lu,\"sample_presses\":%lu,\"storage_ok\":%s,\"demo_raw\":%d,\"picker_name\":\"%s\",\"picker_pet\":%d,\"cooldown_ms\":%lu",
    PAGE_NAMES[page],cursor,selected,storage.count(),(unsigned long)storage.data.night,
    (unsigned long)storage.data.boot,storage.data.zero,storage.data.span,displayReady?"true":"false",
    ESP.getPsramSize(),(unsigned long)frames,(unsigned long)millis(),touchName,(unsigned long)touchTaps,
    (unsigned long)carePresses,(unsigned long)samplePresses,storage.lastWriteOK?"true":"false",
    DEMO_VALUES[demoIndex],PICKER_NAMES[nameIndex],petIndex,(unsigned long)(hasPlayer()?cooldown(selected):0));
  if (hasPlayer()) Serial.printf(",\"name\":\"%s\",\"pet_type\":%u,\"health\":%u,\"food\":%u,\"joy\":%u,\"score\":%u,\"feeds\":%lu,\"history_count\":%u,\"mood\":\"%s\"",
    player().name,player().type,player().health,player().food,player().joy,player().lastScore,
    (unsigned long)player().feeds,player().count,mood(player()));
  Serial.println("}");
}
void historyStatus() {
  if (!hasPlayer()) { Serial.println("ERROR select a pet first"); return; }
  Serial.printf("{\"owner\":\"%s\",\"history\":[",player().name);
  for (int i=0;i<player().count;i++) {
    const Sample &r=player().readings[i];
    Serial.printf("%s{\"n\":%lu,\"boot\":%lu,\"seconds\":%lu,\"raw\":%u,\"score\":%u,\"health_delta\":%d}",i?",":"",
      (unsigned long)r.number,(unsigned long)r.boot,(unsigned long)r.seconds,r.raw,r.score,r.healthDelta);
  }
  Serial.println("]}");
}
void selfTest() {
  Player p; bool ok=true;
  feedPlayer(p,0); ok &= p.food==78 && p.health==100;
  p=Player(); feedPlayer(p,25); Player q; feedPlayer(q,69);
  ok &= p.food==q.food && p.joy==q.joy;
  feedPlayer(p,85); ok &= p.health==83;
  for (int i=0;i<30;i++) feedPlayer(p,100);
  ok &= p.health==0 && p.joy==0 && p.food==100;
  ok &= gameScore(20,45,100)==0 && gameScore(85,45,25)==100;
  PartyStorage scratch;
  int first=scratch.join("CAPTAIN",0),second=scratch.join("GOOSE",1);
  ok &= first==0 && second==1 && scratch.join("CAPTAIN",2)==-1;
  for (int i=0;i<20;i++) scratch.record(first,i,1000+i);
  ok &= scratch.data.players[first].count==16 && scratch.data.players[first].readings[0].raw==19;
  ok &= scratch.data.players[first].readings[15].raw==4 && scratch.data.players[second].count==0;
  ok &= storage.valid(storage.data) && displayReady && ESP.getPsramSize()>8000000;
  Serial.printf("SELFTEST %s game rules, stat bounds, calibration, saved schema, framebuffer, PSRAM\n",ok?"PASS":"FAIL");
}

// Captures the actual rendered framebuffer for layout QA; no panel readback.
void screenshot() {
  if (!displayReady) { Serial.println("ERROR no frame"); return; }
  draw(millis());
  const uint8_t *pixels=static_cast<const uint8_t *>(frame.getPointer());
  Serial.setTxTimeoutMs(50);
  Serial.println("FRAME 320 170 RGB565BE");
  uint32_t progress=millis();
  for (size_t offset=0;offset<320*170*2;) {
    size_t count=min(size_t(256),size_t(320*170*2)-offset);
    while (Serial.availableForWrite()<int(count)) { if (millis()-progress>3000) { Serial.setTxTimeoutMs(0); return; } delay(1); }
    size_t sent=Serial.write(pixels+offset,count);
    offset+=sent;
    if (sent) progress=millis();
    else if (millis()-progress>3000) { Serial.setTxTimeoutMs(0); return; }
  }
  Serial.println("\nEND_FRAME");
  Serial.setTxTimeoutMs(0);
}
bool numberAfter(const String &s,const char *prefix,int &value) {
  if (!s.startsWith(prefix)) return false;
  String tail=s.substring(strlen(prefix));
  if (!tail.length()) return false;
  for (size_t i=0;i<tail.length();i++) if (!isDigit(tail[i])) return false;
  if (tail.length()>3) return false;
  value=tail.toInt(); return true;
}
void command(const String &cmd) {
  int value;
  if (cmd=="status") { status(); return; }
  if (cmd=="history") { historyStatus(); return; }
  if (cmd=="selftest") { selfTest(); return; }
  if (cmd=="screen") { screenshot(); return; }
  if (cmd=="touchscan") {
    initTouch();
    for (int a=1;a<127;a++) { Wire.beginTransmission(a); if (!Wire.endTransmission()) Serial.printf("I2C 0x%02X\n",a); }
    Serial.printf("TOUCHSCAN %s\n",touchName); return;
  }
  if (cmd=="ui next") next();
  else if (cmd=="ui select") activate();
  else if (cmd=="ui back") back();
  else if (cmd=="ui menu") show(MENU);
  else if (cmd=="tank") show(TANK);
  else if (cmd=="night new CONFIRM") newNight();
  else if (cmd.startsWith("join ")) {
    int separator=cmd.indexOf(' ',5);
    String name=cmd.substring(5,separator<0?cmd.length():separator); name.toUpperCase();
    String kind=separator<0?"":cmd.substring(separator+1);
    if (kind.length()!=1 || kind[0]<'0' || kind[0]>'5') { Serial.println("ERROR join NAME 0..5"); return; }
    int slot=storage.join(name.c_str(),kind[0]-'0');
    if (slot<0) { Serial.println("ERROR invalid/duplicate name or full tank"); return; }
    selected=slot; show(PET);
  }
  else if (numberAfter(cmd,"select ",value) && value<MAX_PLAYERS && storage.data.players[value].active) { selected=value; show(PET); }
  else if (numberAfter(cmd,"sample ",value) && value<=100) {
    if (!capture(value)) { Serial.println("ERROR select pet or wait for cooldown"); return; }
  }
  else if (cmd=="rest" && hasPlayer()) restPet();
  else if (cmd=="cal zero") calibrate(0);
  else if (cmd=="cal default") calibrate(1);
  else if (cmd=="cal minus") calibrate(2);
  else if (cmd=="cal plus") calibrate(3);
  else if (cmd=="reboot") { Serial.println("REBOOT"); delay(100); ESP.restart(); return; }
  else if (cmd=="help") { Serial.println("status | join NAME TYPE(0..5) | select SLOT(0..5) | sample 0..100 | history | rest | tank | ui next/select/back/menu | cal zero/default/minus/plus | night new CONFIRM | touchscan | selftest | screen | reboot"); return; }
  else { Serial.println("ERROR unknown or invalid command"); return; }
  status();
}

void setup() {
  pinMode(POWER_PIN,OUTPUT); digitalWrite(POWER_PIN,HIGH);
  pinMode(0,INPUT_PULLUP); pinMode(14,INPUT_PULLUP);
  Serial.setTxBufferSize(8192); Serial.setTxTimeoutMs(0); Serial.begin(115200);
  lcd.begin();
  for (const auto &c:panelCommands) {
    lcd.writecommand(c.cmd);
    for (int i=0;i<(c.len&0x7F);i++) lcd.writedata(c.data[i]);
    if (c.len&0x80) delay(120);
  }
  lcd.setRotation(3); storage.begin(); show(TANK); initTouch();
  pinMode(BACKLIGHT_PIN,OUTPUT); digitalWrite(BACKLIGHT_PIN,HIGH);
  frame.setColorDepth(16); displayReady=frame.createSprite(320,170)!=nullptr;
  if (!displayReady) { lcd.fillScreen(TFT_BLACK); lcd.setTextColor(TFT_RED); lcd.drawString("Framebuffer error",10,20,2); }
  lastTick=millis();
  // Give any saved pets a short pause after restarting, too.
  for (int i=0;i<MAX_PLAYERS;i++) if (storage.data.players[i].feeds) { fed[i]=true; fedAt[i]=millis(); }
  selfTest(); status();
}
void loop() {
  uint32_t now=millis();
  int a=careButton.poll(now),b=sampleButton.poll(now);
  pollTouch(now);
  if (a) { ++carePresses; if (a==2) back(); else activate(); status(); }
  if (b) { ++samplePresses; if (b==2) show(MENU); else next(); status(); }
  static String input; static bool overflow=false;
  int budget=64;
  while (Serial.available() && budget-->0) {
    char c=Serial.read();
    if (c=='\n') {
      Serial.println("CMD"); // Separates command replies from periodic status records.
      if (overflow) Serial.println("ERROR command too long");
      else { input.trim(); if (input.length()) command(input); }
      input=""; overflow=false;
    } else if (c!='\r') { if (input.length()<64) input+=c; else overflow=true; }
  }
  now=millis(); // Commands may reset timers or take time (e.g. screenshot).
  if (page==SAMPLING && now-samplingStart>=3000) capture(DEMO_VALUES[demoIndex]);
  if (now-lastTick>=60000) { lastTick=now; storage.tick(); }
  if (displayReady && now-lastFrame>=80) { lastFrame=now; draw(now); }
  if (now-lastStatus>=5000) { lastStatus=now; status(); }
  delay(2);
}
