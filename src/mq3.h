#pragma once
#include <Arduino.h>

// Bench diagnostics only. Voltages never enter the pet score or saved history.
struct Mq3Monitor {
  static constexpr int PIN=1, WINDOW=100;
  uint16_t readings[WINDOW]={};
  int count=0,head=0,millivolts=0,raw=0,baseline=0;
  bool active=false,hasBaseline=false;
  uint32_t started=0,lastSample=0,samples=0;

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
  void poll(uint32_t now) {
    if (!active || now-lastSample<100) return;
    lastSample=now;
    uint32_t mv=0,adc=0;
    for(int i=0;i<4;i++) { mv+=analogReadMilliVolts(PIN); adc+=analogRead(PIN); }
    ingest((mv+2)/4,(adc+2)/4);
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
