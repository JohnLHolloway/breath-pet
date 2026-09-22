#pragma once
#include <Arduino.h>
#include <Preferences.h>

#ifndef BREATH_PET_TEST_MODE
#define BREATH_PET_TEST_MODE 0
#endif

constexpr int MAX_PLAYERS=6, HISTORY_SIZE=16, PET_TYPES=6;
constexpr uint32_t FEED_COOLDOWN_MS=5000;
constexpr int MQ3_CLEAN_MAX_MV=250, MQ3_RESPONSE_MIN_MV=400, MQ3_DEFAULT_SPAN_MV=1200;
const char *const PET_NAMES[]={"BLOB","AXOLOTL","BAT","CAT","GHOST","FROG"};
const char *const PICKER_NAMES[]={"CAPTAIN","GOOSE","BEAN","CHAOS","PICKLE","NUGGET","BUBBLES","SPUD","MOCHI","GREMLIN","WAFFLES","NOODLE","GOBLIN","PEACH","SQUID","BISCUIT"};
constexpr int NAME_COUNT=sizeof(PICKER_NAMES)/sizeof(PICKER_NAMES[0]);

// Exact v1 layout, retained only to migrate existing pets and demo history.
struct LegacySample { uint32_t number=0,boot=0,seconds=0; uint8_t raw=0,score=0; int8_t healthDelta=0; uint8_t reserved=0; };
struct LegacyPlayer {
  char name[9]={};
  uint8_t active=0,type=0,health=100,food=70,joy=70,count=0,lastScore=0,reserved=0;
  uint32_t feeds=0; LegacySample readings[HISTORY_SIZE];
};
struct LegacyPartyData {
  uint32_t magic=0x42505433,version=1,boot=0,night=1,sequence=0;
  uint16_t zero=0,span=100; LegacyPlayer players[MAX_PLAYERS];
};

struct Sample {
  uint32_t number=0,boot=0,seconds=0;
  uint8_t raw=0,score=0;
  int8_t healthDelta=0;
  uint8_t source=0; // 0 = demo, 1 = measured MQ-3 voltage.
  uint16_t baselineMv=0,peakMv=0,spanMv=0;
};
struct Player {
  char name[9]={};
  uint8_t active=0,type=0,health=100,food=70,joy=70,count=0,lastScore=0,reserved=0;
  uint32_t feeds=0;
  Sample readings[HISTORY_SIZE];
};
struct PartyData {
  uint32_t magic=0x42505433,version=2,boot=0,night=1,sequence=0;
  uint16_t zero=0,span=100;
  uint16_t sensorSpanMv=MQ3_DEFAULT_SPAN_MV;
  Player players[MAX_PLAYERS];
};

// Fictional game units, never BAC. Zero feeds too; the benefit is capped.
inline int gameScore(int raw,int zero,int span) { return constrain((raw-zero)*100/max(25,span),0,100); }
inline int sensorScore(int baseline,int peak,int span) { return peak<MQ3_RESPONSE_MIN_MV?0:constrain(max(0,peak-baseline-20)*100/max(100,span),0,100); }
inline int feedPlayer(Player &p,int score) {
  int previous=p.health;
  p.food=min(100,int(p.food)+15); p.joy=min(100,int(p.joy)+12);
  p.lastScore=score; ++p.feeds; return int(p.health)-previous;
}
inline const char *mood(const Player &p) {
  return p.lastScore>=70?"WILD":p.lastScore>=25?"PARTY":"CHILL";
}

class PartyStorage {
  Preferences prefs;
public:
  PartyData data;
  bool ready=false,lastWriteOK=false;
  bool valid(const PartyData &d) const {
    if (d.magic!=0x42505433 || d.version!=2 || d.zero>100 || d.span<25 || d.span>200 || d.sensorSpanMv<100 || d.sensorSpanMv>2000) return false;
    for (const auto &p:d.players) {
      if (p.active>1 || p.type>=PET_TYPES || p.health>100 || p.food>100 || p.joy>100 || p.count>HISTORY_SIZE || p.name[8]!=0) return false;
      if (p.active && !p.name[0]) return false;
      for (int i=0;i<p.count;i++) {
        const Sample &r=p.readings[i];
        if (r.raw>100 || r.score>100 || r.source>1) return false;
        if (r.source && (r.baselineMv>2700 || r.peakMv>2700 || r.peakMv<r.baselineMv || r.spanMv<100 || r.spanMv>2000)) return false;
      }
    }
    return true;
  }
  static PartyData migrate(const LegacyPartyData &old) {
    PartyData d; d.magic=old.magic; d.boot=old.boot; d.night=old.night; d.sequence=old.sequence;
    d.zero=old.zero; d.span=old.span;
    for(int i=0;i<MAX_PLAYERS;i++) {
      const LegacyPlayer &a=old.players[i]; Player &b=d.players[i];
      memcpy(b.name,a.name,sizeof(b.name)); b.active=a.active; b.type=a.type; b.health=a.health;
      b.food=a.food; b.joy=a.joy; b.count=a.count; b.lastScore=a.lastScore; b.feeds=a.feeds;
      for(int j=0;j<HISTORY_SIZE;j++) {
        const LegacySample &x=a.readings[j]; Sample &y=b.readings[j];
        y.number=x.number; y.boot=x.boot; y.seconds=x.seconds; y.raw=x.raw; y.score=x.score; y.healthDelta=x.healthDelta;
      }
    }
    return d;
  }
  bool begin() {
    ready=prefs.begin(BREATH_PET_TEST_MODE?"breath-test":"breath-pet",false); if (!ready) return false;
    static PartyData loaded;
    if (prefs.getBytesLength("party-v2")==sizeof(loaded) &&
        prefs.getBytes("party-v2",&loaded,sizeof(loaded))==sizeof(loaded) && valid(loaded)) {
      data=loaded;
      // The verified v2 record supersedes this old key; reclaim scarce NVS space.
      prefs.remove("party-v1");
    }
    else if (!prefs.getBytesLength("party-v2")) {
      static LegacyPartyData old;
      if (prefs.getBytesLength("party-v1")==sizeof(old) && prefs.getBytes("party-v1",&old,sizeof(old))==sizeof(old) && old.version==1) {
        loaded=migrate(old); if(valid(loaded)) data=loaded;
      }
    }
    ++data.boot; return save();
  }
  bool save() { return lastWriteOK=ready && prefs.putBytes("party-v2",&data,sizeof(data))==sizeof(data); }
  int count() const { int n=0; for (const auto &p:data.players) n+=p.active?1:0; return n; }
  int score(int raw) const { return gameScore(raw,data.zero,data.span); }
  bool nameUsed(const char *name) const {
    for (const auto &p:data.players) if (p.active && strcmp(p.name,name)==0) return true;
    return false;
  }
  int join(const char *name,int type,int slot=-1) {
    size_t len=strlen(name);
    if (!len || len>8 || type<0 || type>=PET_TYPES || nameUsed(name)) return -1;
    for (size_t i=0;i<len;i++) if (!(name[i]>='A' && name[i]<='Z') && !(name[i]>='0' && name[i]<='9')) return -1;
    if (slot<0) for (int i=0;i<MAX_PLAYERS;i++) if (!data.players[i].active) { slot=i; break; }
    if (slot<0 || slot>=MAX_PLAYERS || data.players[slot].active) return -1;
    Player &p=data.players[slot]; p=Player(); strcpy(p.name,name); p.active=1; p.type=type;
    save(); return slot;
  }
  int record(int slot,int raw,uint32_t ms,int source=0,int baseline=0,int peak=0) {
    Player &p=data.players[slot]; int calibrated=source?sensorScore(baseline,peak,data.sensorSpanMv):score(raw),delta=feedPlayer(p,calibrated);
    for (int i=HISTORY_SIZE-1;i>0;i--) p.readings[i]=p.readings[i-1];
    Sample &r=p.readings[0]; r=Sample();
    r.number=++data.sequence; r.boot=data.boot; r.seconds=ms/1000;
    r.raw=raw; r.score=calibrated; r.healthDelta=delta;
    r.source=source; r.baselineMv=baseline; r.peakMv=peak; r.spanMv=source?data.sensorSpanMv:0;
    p.count=min(HISTORY_SIZE,int(p.count)+1); save(); return delta;
  }
  void rest(int slot) {
    Player &p=data.players[slot]; p.health=min(100,int(p.health)+8);
    p.joy=min(100,int(p.joy)+4); p.lastScore=0; save();
  }
  void tick() {
    for (auto &p:data.players) if (p.active) {
      p.food=max(0,int(p.food)-1); p.joy=max(0,int(p.joy)-1);
      // No neglect damage: attention is represented by the sleep/energy system.
    }
    if (count()) save();
  }
  void newNight() {
    uint32_t boot=data.boot,night=data.night+1; uint16_t zero=data.zero,span=data.span,sensorSpan=data.sensorSpanMv;
    data=PartyData(); data.boot=boot; data.night=night; data.zero=zero; data.span=span; data.sensorSpanMv=sensorSpan; save();
  }
};
