#pragma once

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

void initTouch() {
  touch=nullptr; touchName="none";
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
