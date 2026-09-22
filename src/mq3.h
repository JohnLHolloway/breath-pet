#pragma once
#include <Arduino.h>

// Shared ADC acquisition for bench diagnostics and live feeding.
struct Mq3Monitor {
  static constexpr int PIN=1, WINDOW=100;
  uint16_t readings[WINDOW]={};
  int count=0,head=0,millivolts=0,raw=0,baseline=0;
  bool active=false,hasBaseline=false;
  uint32_t started=0,lastSample=0,samples=0;
#if BREATH_PET_TEST_MODE
  int injectedMv=-1; // Test firmware only; never available in a normal build.
#endif

  void begin(uint32_t now) {
    pinMode(PIN,INPUT);
    analogReadResolution(12);
    analogSetPinAttenuation(PIN,ADC_11db);
    active=true; count=0; head=0; hasBaseline=false;
    started=now; lastSample=now; samples=0;
  }
  void ingest(int mv,int adc) {
    millivolts=mv; raw=adc;
    readings[head]=mv; head=(head+1)%WINDOW;
    count=min(WINDOW,count+1); ++samples;
  }
  bool poll(uint32_t now) {
    if (!active || now-lastSample<100) return false;
    lastSample=now;
#if BREATH_PET_TEST_MODE
    if(injectedMv>=0) { ingest(injectedMv,injectedMv*4095/3100); return true; }
#endif
    uint32_t mv=0,adc=0;
    for(int i=0;i<4;i++) { mv+=analogReadMilliVolts(PIN); adc+=analogRead(PIN); }
    ingest((mv+2)/4,(adc+2)/4);
    return true;
  }
  int spread() const {
    if (!count) return 0;
    int lo=readings[0],hi=lo;
    for(int i=1;i<count;i++) { lo=min(lo,int(readings[i])); hi=max(hi,int(readings[i])); }
    return hi-lo;
  }
  int mean() const {
    uint32_t sum=0; for(int i=0;i<count;i++) sum+=readings[i];
    return count?sum/count:0;
  }
  bool canZero() const {
    return active && count==WINDOW && millivolts>=50 && millivolts<=2700 && spread()<=50;
  }
  bool zero() {
    if (!canZero()) return false;
    baseline=mean(); hasBaseline=true; return true;
  }
  const char *condition() const {
    if (!active) return "OFF";
    if (!count) return "WAITING";
    if (millivolts>2700) return "HIGH INPUT - CHECK DIVIDER";
    if (millivolts<50) return "LOW INPUT - CHECK WIRING";
    if (count<WINDOW) return "COLLECTING 10s WINDOW";
    if (spread()>50) return "DRIFTING - LET IT SETTLE";
    return "QUIET SIGNAL / NOT CALIBRATED";
  }
};

struct SensorFeed {
  static constexpr uint32_t DURATION=10000;
  bool running=false,invalid=false,needsRecovery=false;
  int baseline=0,peak=0,samples=0;
  uint32_t started=0;
  bool recovered(const Mq3Monitor &m) const { return !needsRecovery || (m.millivolts<=MQ3_CLEAN_MAX_MV && m.mean()<=MQ3_CLEAN_MAX_MV); }
  bool ready(const Mq3Monitor &m) const {
    if(!m.canZero() || m.spread()>25) return false;
    // Every sample in the fresh window must be inside the clean-air band.
    for(int i=0;i<m.count;i++) if(m.readings[i]<50 || m.readings[i]>MQ3_CLEAN_MAX_MV) return false;
    return true;
  }
  bool start(const Mq3Monitor &m,uint32_t now) {
    if(!ready(m)) return false;
    baseline=m.mean(); peak=baseline; samples=0; invalid=false;
    started=now; running=true; needsRecovery=true; return true;
  }
  void accept(int mv) {
    if(!running) return;
    ++samples; peak=max(peak,mv);
    if(mv<20 || mv>2700) invalid=true;
  }
  bool done(uint32_t now) const { return running && now-started>=DURATION; }
  bool valid() const { return !invalid && samples>=80; }
  void cancel() { running=false; }
};
