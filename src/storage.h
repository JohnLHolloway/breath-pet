#pragma once
#include <Arduino.h>
#include <Preferences.h>

// Versioned NVS blob. Only user sample/settings actions write it, never animation.
struct Reading {
  uint32_t number = 0, boot = 0, seconds = 0;
  uint16_t raw = 0, score = 0;
};
struct SavedData {
  uint32_t magic = 0x42505432, version = 1, boot = 0, sequence = 0;
  uint16_t count = 0, zero = 0, span = 100, reserved = 0;
  Reading readings[16];
};
class PetStorage {
  Preferences prefs;
public:
  SavedData data;
  bool ready = false, lastWriteOK = false;
  bool begin() {
    ready = prefs.begin("breath-pet", false);
    if (!ready) return false;
    SavedData loaded;
    if (prefs.getBytesLength("data") == sizeof(loaded) &&
        prefs.getBytes("data", &loaded, sizeof(loaded)) == sizeof(loaded) &&
        loaded.magic == data.magic && loaded.version == 1 && loaded.count <= 16 &&
        loaded.zero <= 100 && loaded.span >= 25 && loaded.span <= 200) data = loaded;
    ++data.boot;
    return save();
  }
  bool save() { return lastWriteOK = ready && prefs.putBytes("data", &data, sizeof(data)) == sizeof(data); }
  int score(int raw) const { return constrain((raw-static_cast<int>(data.zero))*100/static_cast<int>(data.span),0,100); }
  void record(int raw, uint32_t ms) {
    for (int i=15;i>0;--i) data.readings[i] = data.readings[i-1];
    Reading &r = data.readings[0];
    r.number = ++data.sequence; r.boot = data.boot; r.seconds = ms/1000;
    r.raw = raw; r.score = score(raw); data.count = min(16,static_cast<int>(data.count)+1); save();
  }
  void clear() { data.count = 0; for (auto &r : data.readings) r = Reading(); save(); }
};
