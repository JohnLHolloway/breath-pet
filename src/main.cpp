#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <math.h>
#include <ModulesCSTSelf.tpp>
#include <ModulesCSTMutual.tpp>
#include "party.h"
#include "mq3.h"
#include "fun.h"

constexpr int POWER_PIN=15,BACKLIGHT_PIN=38;
TFT_eSPI lcd;
TFT_eSprite frame(&lcd);
PartyStorage storage;
FunStorage fun;
Mq3Monitor mq3;
SensorFeed sensorFeed;
bool inputLive=!BREATH_PET_TEST_MODE;
uint32_t responseId=0;
TouchLibCSTSelf selfTouch(Wire,18,17,0x15,21);
TouchLibCSTMutual mutualTouch(Wire,18,17,0x1A,21);
TouchLibInterface *touch=nullptr;
const char *touchName="none";
bool displayReady=false;
uint32_t frames=0,touchTaps=0,carePresses=0,samplePresses=0;
uint32_t lastTick=0,lastFrame=0,lastStatus=0,samplingStart=0;
uint32_t fedAt[MAX_PLAYERS]={},restedAt[MAX_PLAYERS]={};
bool fed[MAX_PLAYERS]={},rested[MAX_PLAYERS]={};
enum Page {TANK,NAME_PICK,PET_PICK,PET,FEED,SAMPLING,RESULT,HISTORY,MENU,CALIBRATION,NEW_NIGHT,SENSOR,WARDROBE,PLAY,AWARDS,DECOR,COUNTDOWN};
const char *const PAGE_NAMES[]={"tank","name","choose_pet","pet","feed","sampling","result","history","menu","calibration","new_night","sensor","wardrobe","play","awards","decor","countdown"};
Page page=TANK;
int cursor=6,selected=-1,joiningSlot=-1,nameIndex=0,petIndex=0,historyPage=0;
int demoIndex=1,lastRaw=0,resultDelta=0;
const int DEMO_VALUES[]={0,25,50,85};
const char *notice="";
uint32_t noticeAt=0;
int catches=0;
#if BREATH_PET_TEST_MODE
int bubbleOverride=-1;
#endif
int bubblePosition(uint32_t now) {
#if BREATH_PET_TEST_MODE
  if(bubbleOverride>=0) return bubbleOverride;
#endif
  int p=(now/18)%200; return p<=100?p:200-p;
}

void status(bool waitForSpace=false);
void message(const char *text);
void tap(int x,int y);
#include "hardware.h"

bool hasPlayer() { return selected>=0 && selected<MAX_PLAYERS && storage.data.players[selected].active; }
Player &player() { return storage.data.players[selected]; }
void show(Page next) {
  if((page==SAMPLING || page==COUNTDOWN) && next!=SAMPLING && next!=COUNTDOWN) sensorFeed.cancel();
  if ((next==SENSOR && page!=SENSOR) || (next==FEED && inputLive && page!=FEED)) mq3.begin(millis());
  if (next!=SENSOR && !(inputLive && (next==FEED || next==SAMPLING || next==COUNTDOWN))) mq3.active=false;
  page=next; cursor=0; notice="";
  if(next==PLAY) catches=0;
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
  storage.newNight(); fun.sync(storage.data); selected=-1; joiningSlot=-1;
  memset(fed,0,sizeof(fed)); memset(rested,0,sizeof(rested));
  lastTick=millis(); show(TANK);
}
bool capture(int raw) {
  if (inputLive || !hasPlayer() || raw<0 || raw>100 || cooldown(selected)) return false;
  lastRaw=raw; resultDelta=storage.record(selected,raw,millis());
  fun.wake(selected,player().lastScore,millis(),esp_random());
  fed[selected]=true; fedAt[selected]=millis(); show(RESULT); return true;
}
void beginFeed() {
  if (!hasPlayer()) return;
  if (cooldown(selected)) { message("PET IS FULL. WAIT A MOMENT."); return; }
  if(inputLive && !sensorFeed.start(mq3,millis())) { message("KEEP CUP AWAY; WAIT FOR CLEAN AIR"); return; }
  samplingStart=millis(); show(COUNTDOWN);
}
void finishLiveFeed() {
  if(!sensorFeed.valid()) { show(FEED); message("NO READING SAVED: CHECK SIGNAL"); return; }
  int raw=sensorScore(sensorFeed.baseline,sensorFeed.peak,storage.data.sensorSpanMv);
  resultDelta=storage.record(selected,raw,millis(),1,sensorFeed.baseline,sensorFeed.peak);
  fun.wake(selected,player().lastScore,millis(),esp_random());
  fed[selected]=true; fedAt[selected]=millis(); show(RESULT);
}
void restPet() {
  if (!hasPlayer()) return;
  if (rested[selected] && millis()-restedAt[selected]<10000) { message("RESTING... TRY AGAIN SOON."); return; }
  storage.rest(selected); rested[selected]=true; restedAt[selected]=millis(); fun.nap(selected); message("NAP TIME / PLAY OR FEED TO WAKE");
}
void calibrate(int action) {
  if(inputLive) {
    if(action==0) storage.data.sensorSpanMv=max(100,int(storage.data.sensorSpanMv)-100);
    if(action==1) storage.data.sensorSpanMv=min(2000,int(storage.data.sensorSpanMv)+100);
    if(action==2) storage.data.sensorSpanMv=MQ3_DEFAULT_SPAN_MV;
    if(action==3) { show(MENU); return; }
    storage.save(); return;
  }
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
  else if (page==CALIBRATION || page==SENSOR || page==AWARDS || page==DECOR) show(MENU);
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
    case PET: cursor=(cursor+1)%5; break;
    case FEED: if(!inputLive) demoIndex=(demoIndex+1)%4; break;
    case RESULT: cursor=1-cursor; break;
    case HISTORY: historyPage=(historyPage+1)%max(1,(int(player().count)+2)/3); break;
    case MENU: cursor=(cursor+1)%7; break;
    case SENSOR: cursor=(cursor+1)%3; break;
    case CALIBRATION: cursor=(cursor+1)%4; break;
    case NEW_NIGHT: cursor=1-cursor; break;
    case WARDROBE: cursor=(cursor+1)%4; break;
    case PLAY: cursor=1-cursor; break;
    case AWARDS: cursor=(cursor+1)%3; break;
    case DECOR: cursor=(cursor+1)%3; break;
    case COUNTDOWN: case SAMPLING: break;
  }
}
void activate() {
  notice="";
  switch (page) {
    case TANK: if (cursor==7) show(MENU); else if (cursor==6) addPet(); else selectSlot(cursor); break;
    case NAME_PICK: show(PET_PICK); break;
    case PET_PICK: {
      int slot=storage.join(PICKER_NAMES[nameIndex],petIndex,joiningSlot);
      if (slot>=0) { fun.sync(storage.data); selected=slot; show(PET); } else message("NAME TAKEN OR TANK FULL");
      break;
    }
    case PET:
      if (cursor==0) show(FEED);
      else if (cursor==1) { historyPage=0; show(HISTORY); }
      else if(cursor==2) restPet();
      else if(cursor==3) show(WARDROBE);
      else show(PLAY);
      break;
    case FEED: beginFeed(); break;
    case RESULT: show(cursor==0?TANK:PET); break;
    case HISTORY: show(PET); break;
    case MENU:
      if (cursor==0) show(TANK); else if (cursor==1) show(CALIBRATION); else if (cursor==2) show(NEW_NIGHT); else if(cursor==3) show(SENSOR);
      else if(cursor==5) show(AWARDS);
      else if(cursor==6) show(DECOR);
      else { inputLive=!inputLive; message(inputLive?"LIVE MQ-3 FEEDING ENABLED":"DEMO FEEDING ENABLED"); }
      break;
    case SENSOR:
      if (cursor==2) show(MENU);
      else if (cursor==1) { mq3.hasBaseline=false; message("BASELINE CLEARED"); }
      else if (mq3.zero()) message("TEMPORARY AIR BASELINE SET");
      else message("WAIT FOR A QUIET, VALID SIGNAL");
      break;
    case CALIBRATION: calibrate(cursor); break;
    case NEW_NIGHT: if (cursor==1) newNight(); else show(MENU); break;
    case WARDROBE: if(cursor==3) show(PET); else fun.dress(selected,cursor); break;
    case PLAY:
      if(cursor) { show(PET); break; }
      if(abs(bubblePosition(millis())-50)<=18) {
        if(++catches>=3) { fun.play(selected,millis(),esp_random()); show(PET); message("THREE CATCHES / HELLO AGAIN!"); }
        else message("POP! CATCH THE NEXT ONE");
      } else message("MISSED / TRY AGAIN");
      break;
    case AWARDS: case DECOR: show(MENU); break;
    case COUNTDOWN: case SAMPLING: break;
  }
}

void swimPosition(int slot,uint32_t now,int &x,int &y) {
  const auto &f=fun.data.pets[slot];
  if(f.idle>=20) { x=80+slot*39; y=116; return; }
  float speed=f.idle>=10?130.0f:f.vibe>=70?29.0f:f.vibe>=25?48.0f:70.0f;
  float phase=fmodf(now/speed+slot*71.0f,408.0f);
  x=82+int(phase<=204?phase:408-phase);
  y=70+(slot%2)*35+int(sinf(now/(f.vibe>=70?220.0f:1300.0f)+slot*1.8f)*(f.vibe>=70?10:7));
  // Shared, short scenes gather awake pets without confining them to boxes.
  if(now%24000<4500) {
    if(f.personality==3) { x=92+int((now%4500)/27); y=78; }
    else { x=75+slot*37+int(sinf(now/330.0f+slot)*9); y=94; }
  }
}
void tap(int x,int y) {
  if (x<0 || x>=320 || y<0 || y>=170) return;
  if (x<47) { if (y<85) next(); else activate(); }
  else if (y>=139) activate();
  else if (page==TANK) {
    if (y>=32 && y<139) {
      int closest=-1,distance=1600;
      for (int i=0;i<MAX_PLAYERS;i++) if (storage.data.players[i].active) {
        int px,py; swimPosition(i,millis(),px,py); int d=(x-px)*(x-px)+(y-py)*(y-py);
        if (d<distance) { distance=d; closest=i; }
      }
      if (closest>=0) selectSlot(closest);
    }
  } else if (page!=SAMPLING && page!=COUNTDOWN) next();
  status();
}

#include "ui.h"

void status(bool waitForSpace) {
  uint32_t started=millis();
  while(Serial.availableForWrite()<2048) {
    if(!waitForSpace || millis()-started>=500) return;
    delay(1);
  }
  char record[2048];
  size_t length=snprintf(record,sizeof(record),"{\"app\":\"breath-pet\",\"version\":6,\"sensor\":\"%s\",\"page\":\"%s\",\"cursor\":%d,\"selected\":%d,\"players\":%d,\"night\":%lu,\"boot\":%lu,\"zero\":%u,\"span\":%u,\"display\":%s,\"psram\":%u,\"frames\":%lu,\"uptime_ms\":%lu,\"touch\":\"%s\",\"touch_taps\":%lu,\"care_presses\":%lu,\"sample_presses\":%lu,\"storage_ok\":%s,\"demo_raw\":%d,\"picker_name\":\"%s\",\"picker_pet\":%d,\"cooldown_ms\":%lu",
    inputLive?"MQ3":"SIMULATED",PAGE_NAMES[page],cursor,selected,storage.count(),(unsigned long)storage.data.night,
    (unsigned long)storage.data.boot,storage.data.zero,storage.data.span,displayReady?"true":"false",
    ESP.getPsramSize(),(unsigned long)frames,(unsigned long)millis(),touchName,(unsigned long)touchTaps,
    (unsigned long)carePresses,(unsigned long)samplePresses,storage.lastWriteOK?"true":"false",
    DEMO_VALUES[demoIndex],PICKER_NAMES[nameIndex],petIndex,(unsigned long)(hasPlayer()?cooldown(selected):0));
  if (hasPlayer()) length+=snprintf(record+length,sizeof(record)-length,",\"name\":\"%s\",\"pet_type\":%u,\"health\":%u,\"food\":%u,\"joy\":%u,\"score\":%u,\"feeds\":%lu,\"history_count\":%u,\"mood\":\"%s\"",
    player().name,player().type,player().health,player().food,player().joy,player().lastScore,
    (unsigned long)player().feeds,player().count,mood(player()));
  if(hasPlayer()) {
    const auto &f=fun.data.pets[selected]; const auto *c=fun.collection(player().name,false);
    length+=snprintf(record+length,sizeof(record)-length,",\"energy\":%d,\"state\":\"%s\",\"idle_minutes\":%u,\"checkins\":%u,\"naps\":%u,\"games\":%u,\"encounters\":%u,\"hat\":%u,\"item\":%u,\"colour\":%u,\"hats\":%u,\"items\":%u,\"colours\":%u,\"visits\":%u,\"reward_ready\":%s,\"loot\":%d",
      fun.energy(selected),fun.state(selected),f.idle,f.checkins,f.naps,f.games,f.encounters,c?c->hat:0,c?c->item:0,c?c->colour:0,c?c->hats:1,c?c->items:1,c?c->colours:1,c?c->visits:0,fun.rewardReady(selected)?"true":"false",fun.lastLoot);
  }
  length+=snprintf(record+length,sizeof(record)-length,",\"fun_storage_ok\":%s,\"upgrades\":%u,\"catches\":%d",fun.lastWriteOK?"true":"false",fun.data.upgrades,catches);
  length+=snprintf(record+length,sizeof(record)-length,",\"test_mode\":%s,\"mq3_active\":%s,\"mq3_mv\":%d,\"mq3_adc\":%d,\"mq3_samples\":%lu,\"mq3_spread_mv\":%d,\"mq3_can_zero\":%s,\"mq3_baseline_mv\":%d",
    BREATH_PET_TEST_MODE?"true":"false",mq3.active?"true":"false",mq3.millivolts,mq3.raw,(unsigned long)mq3.samples,mq3.spread(),mq3.canZero()?"true":"false",mq3.hasBaseline?mq3.baseline:-1);
  length+=snprintf(record+length,sizeof(record)-length,",\"feed_ready\":%s,\"feed_baseline_mv\":%d,\"feed_peak_mv\":%d,\"sensor_span_mv\":%u,\"recovering\":%s,\"request_id\":%lu}\n",
    (page==FEED && inputLive && sensorFeed.ready(mq3))?"true":"false",sensorFeed.baseline,sensorFeed.peak,storage.data.sensorSpanMv,sensorFeed.recovered(mq3)?"false":"true",(unsigned long)responseId);
  // Small writes also handle the hardware USB CDC ring buffer wrapping.
  size_t offset=0; uint32_t progress=millis();
  while(offset<length) {
    size_t n=Serial.write(reinterpret_cast<const uint8_t *>(record)+offset,min(size_t(256),length-offset));
    offset+=n;
    if(n) progress=millis();
    else if(millis()-progress>=500) break;
    delay(1);
  }
}
void historyStatus() {
  if (!hasPlayer()) { Serial.println("ERROR select a pet first"); return; }
  Serial.printf("{\"request_id\":%lu,\"owner\":\"%s\",\"history\":[",(unsigned long)responseId,player().name);
  for (int i=0;i<player().count;i++) {
    const Sample &r=player().readings[i];
    Serial.printf("%s{\"n\":%lu,\"boot\":%lu,\"seconds\":%lu,\"raw\":%u,\"score\":%u,\"health_delta\":%d,\"source\":\"%s\",\"baseline_mv\":%u,\"peak_mv\":%u,\"span_mv\":%u}",i?",":"",
      (unsigned long)r.number,(unsigned long)r.boot,(unsigned long)r.seconds,r.raw,r.score,r.healthDelta,r.source?"MQ3":"DEMO",r.baselineMv,r.peakMv,r.spanMv);
  }
  Serial.println("]}");
}
void selfTest() {
  Player p; bool ok=true;
  feedPlayer(p,0); ok &= p.food==85 && p.health==100;
  p=Player(); feedPlayer(p,25); Player q; feedPlayer(q,69);
  ok &= p.food==q.food && p.joy==q.joy;
  feedPlayer(p,85); ok &= p.health==100;
  for (int i=0;i<30;i++) feedPlayer(p,100);
  ok &= p.health==100 && p.joy==100 && p.food==100;
  ok &= gameScore(20,45,100)==0 && gameScore(85,45,25)==100;
  static const PartyData freshParty;
  static const LegacyPartyData freshLegacy;
  static PartyStorage scratch; scratch.data=freshParty;
  int first=scratch.join("CAPTAIN",0),second=scratch.join("GOOSE",1);
  ok &= first==0 && second==1 && scratch.join("CAPTAIN",2)==-1;
  for (int i=0;i<20;i++) scratch.record(first,i,1000+i);
  ok &= scratch.data.players[first].count==16 && scratch.data.players[first].readings[0].raw==19;
  ok &= scratch.data.players[first].readings[15].raw==4 && scratch.data.players[second].count==0;
  ok &= storage.valid(storage.data) && displayReady && ESP.getPsramSize()>8000000;
  Mq3Monitor bench; bench.active=true;
  for(int i=0;i<99;i++) bench.ingest(1000,1300);
  ok &= !bench.zero(); bench.ingest(1020,1320);
  ok &= bench.zero() && bench.baseline==1000;
  bench.ingest(1500,1900); ok &= !bench.canZero();
  for(int i=0;i<100;i++) bench.ingest(0,0);
  ok &= !bench.canZero();
  for(int i=0;i<100;i++) bench.ingest(2900,4000);
  ok &= !bench.canZero();
  static LegacyPartyData old; old=freshLegacy; old.boot=4; old.zero=12; old.span=75;
  strcpy(old.players[0].name,"CAPTAIN"); old.players[0].active=1; old.players[0].feeds=1; old.players[0].count=1;
  old.players[0].readings[0].raw=25; old.players[0].readings[0].number=7;
  static PartyData migrated; migrated=PartyStorage::migrate(old);
  ok &= storage.valid(migrated) && migrated.boot==4 && migrated.zero==12 && migrated.span==75;
  ok &= !strcmp(migrated.players[0].name,"CAPTAIN") && migrated.players[0].readings[0].number==7 && !migrated.players[0].readings[0].source;
  bench=Mq3Monitor(); bench.active=true; SensorFeed trial;
  for(int i=0;i<99;i++) bench.ingest(100,100);
  ok &= !trial.start(bench,1000); bench.ingest(100,100);
  ok &= trial.start(bench,1000);
  for(int i=0;i<100;i++) trial.accept(400);
  ok &= trial.valid() && !trial.done(10999) && trial.done(11000) && trial.peak==400;
  ok &= sensorScore(100,399,1200)==0 && sensorScore(100,400,1200)==23 && sensorScore(139,852,1200)==57 && sensorScore(139,999,1200)==70 && sensorScore(100,2700,1200)==100;
  trial.cancel(); for(int i=0;i<100;i++) bench.ingest(400,400);
  ok &= !trial.ready(bench); for(int i=0;i<100;i++) bench.ingest(110,110);
  ok &= trial.start(bench,20000); trial.accept(0); ok &= trial.invalid && !trial.valid();
  // Clean-air boundaries apply even before the first feed; a stable plume is rejected.
  trial=SensorFeed(); bench=Mq3Monitor(); bench.active=true;
  for(int i=0;i<100;i++) bench.ingest(250,300);
  ok &= trial.ready(bench); bench.ingest(251,301); ok &= !trial.ready(bench);
  for(int i=0;i<100;i++) bench.ingest(400,500);
  ok &= !trial.ready(bench);
  for(int i=0;i<100;i++) bench.ingest(200,250);
  ok &= trial.start(bench,30000); trial.cancel();
  for(int i=0;i<100;i++) bench.ingest(250,300);
  ok &= trial.ready(bench) && trial.recovered(bench);
  scratch.record(second,23,2000,1,100,400);
  const Sample &measured=scratch.data.players[second].readings[0];
  ok &= measured.source==1 && measured.score==23 && measured.baselineMv==100 && measured.peakMv==400 && measured.spanMv==1200;
  Serial.printf("SELFTEST %s game rules, stat bounds, calibration, sensor baseline guards, saved schema, framebuffer, PSRAM\n",ok?"PASS":"FAIL");
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
    while (Serial.availableForWrite()<int(count)) { if (millis()-progress>3000) { Serial.setTxTimeoutMs(10); return; } delay(1); }
    size_t sent=Serial.write(pixels+offset,count);
    offset+=sent;
    if (sent) progress=millis();
    else if (millis()-progress>3000) { Serial.setTxTimeoutMs(10); return; }
  }
  Serial.println("\nEND_FRAME");
  Serial.setTxTimeoutMs(10);
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
  if (cmd=="status") { status(true); return; }
  if (cmd=="history") { historyStatus(); return; }
  if (cmd=="selftest") { selfTest(); return; }
  if (cmd=="screen") { screenshot(); return; }
  if (cmd=="touchscan") {
    initTouch();
    for (int a=1;a<127;a++) { Wire.beginTransmission(a); if (!Wire.endTransmission()) Serial.printf("I2C 0x%02X\n",a); }
    Serial.printf("TOUCHSCAN %s\n",touchName); return;
  }
#if BREATH_PET_TEST_MODE
  if(numberAfter(cmd,"test minutes ",value) && value<=60) { for(int i=0;i<value;i++) fun.tick(); lastTick=millis(); status(true); return; }
  if(numberAfter(cmd,"test bubble ",value) && value<=100) { bubbleOverride=value; status(true); return; }
  if(cmd=="test fun reset") { static const FunData fresh; fun.data=fresh; fun.sync(storage.data); fun.save(); status(true); return; }
  if(numberAfter(cmd,"test sensor ",value) && value<=999) { mq3.injectedMv=value; status(true); return; }
  if(cmd=="test sensor high") { mq3.injectedMv=2900; status(true); return; }
  if(cmd=="test sensor off") { mq3.injectedMv=-1; status(true); return; }
#endif
  if (cmd=="ui next") next();
  else if (cmd=="ui select") activate();
  else if (cmd=="ui back") back();
  else if (cmd=="ui menu") show(MENU);
  else if (cmd=="tank") show(TANK);
  else if (cmd=="input live" || cmd=="input demo") { inputLive=cmd=="input live"; show(TANK); }
  else if (cmd=="sensor open") show(SENSOR);
  else if (cmd=="sensor zero") {
    if (!mq3.zero()) { Serial.println("ERROR need 10s quiet input between 50 and 2700 mV"); return; }
  }
  else if (cmd=="sensor clear") mq3.hasBaseline=false;
  else if (cmd=="night new CONFIRM") newNight();
  else if (cmd.startsWith("join ")) {
    int separator=cmd.indexOf(' ',5);
    String name=cmd.substring(5,separator<0?cmd.length():separator); name.toUpperCase();
    String kind=separator<0?"":cmd.substring(separator+1);
    if (kind.length()!=1 || kind[0]<'0' || kind[0]>'5') { Serial.println("ERROR join NAME 0..5"); return; }
    int slot=storage.join(name.c_str(),kind[0]-'0');
    if (slot<0) { Serial.println("ERROR invalid/duplicate name or full tank"); return; }
    fun.sync(storage.data); selected=slot; show(PET);
  }
  else if (numberAfter(cmd,"select ",value) && value<MAX_PLAYERS && storage.data.players[value].active) { selected=value; show(PET); }
  else if (numberAfter(cmd,"sample ",value) && value<=100) {
    if (!capture(value)) { Serial.println("ERROR select pet or wait for cooldown"); return; }
  }
  else if (cmd=="rest" && hasPlayer()) restPet();
  else if (cmd=="cal zero") {
    if(inputLive) { Serial.println("ERROR live feeds set a fresh baseline automatically"); return; }
    calibrate(0);
  }
  else if (cmd=="cal default") calibrate(inputLive?2:1);
  else if (cmd=="cal minus") calibrate(inputLive?0:2);
  else if (cmd=="cal plus") calibrate(inputLive?1:3);
  else if (cmd=="reboot") { Serial.println("REBOOT"); delay(100); ESP.restart(); return; }
  else if (cmd=="help") { Serial.println("status | join NAME TYPE(0..5) | select SLOT(0..5) | sample 0..100 | history | rest | tank | ui next/select/back/menu | cal zero/default/minus/plus | sensor open/zero/clear | input live/demo | night new CONFIRM | touchscan | selftest | screen | reboot"); return; }
  else { Serial.println("ERROR unknown or invalid command"); return; }
  status(true);
}

void setup() {
  pinMode(POWER_PIN,OUTPUT); digitalWrite(POWER_PIN,HIGH);
  pinMode(0,INPUT_PULLUP); pinMode(14,INPUT_PULLUP);
  Serial.setTxBufferSize(8192); Serial.setTxTimeoutMs(10); Serial.begin(115200);
  lcd.begin();
  for (const auto &c:panelCommands) {
    lcd.writecommand(c.cmd);
    for (int i=0;i<(c.len&0x7F);i++) lcd.writedata(c.data[i]);
    if (c.len&0x80) delay(120);
  }
  lcd.setRotation(3); storage.begin(); fun.begin(storage.data); show(TANK); initTouch();
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
  if (a) { ++carePresses; if (a==2) back(); else next(); status(); }
  if (b) { ++samplePresses; if (b==2) show(MENU); else activate(); status(); }
  static String input; static bool overflow=false;
  int budget=64;
  while (Serial.available() && budget-->0) {
    char c=Serial.read();
    if (c=='\n') {
      input.trim(); responseId=0;
      if(input.startsWith("@")) {
        int space=input.indexOf(' ');
        if(space>1 && space<=10) {
          bool digits=true; for(int i=1;i<space;i++) digits &= isDigit(input[i]);
          if(digits) { responseId=strtoul(input.substring(1,space).c_str(),nullptr,10); input=input.substring(space+1); }
        }
      }
      if(responseId) Serial.printf("CMD %lu\n",(unsigned long)responseId);
      else Serial.println("CMD");
      if (overflow) Serial.println("ERROR command too long");
      else { input.trim(); if (input.length()) command(input); }
      input=""; overflow=false;
      responseId=0;
    } else if (c!='\r') { if (input.length()<64) input+=c; else overflow=true; }
  }
  now=millis(); // Commands may reset timers or take time (e.g. screenshot).
  bool sampled=mq3.poll(now);
  if(page==COUNTDOWN && now-samplingStart>=5000) {
    samplingStart=now; sensorFeed.started=now; show(SAMPLING);
  }
  if(page==SAMPLING && inputLive) {
    if(sampled) sensorFeed.accept(mq3.millivolts);
    if(sensorFeed.invalid || sensorFeed.done(now)) finishLiveFeed();
  } else if (page==SAMPLING && now-samplingStart>=10000) capture(DEMO_VALUES[demoIndex]);
  if (now-lastTick>=60000) { lastTick+=60000; storage.tick(); fun.tick(); }
  if (displayReady && now-lastFrame>=80) { lastFrame=now; draw(now); }
  if (now-lastStatus>=5000) { lastStatus=now; status(); }
  delay(2);
}
