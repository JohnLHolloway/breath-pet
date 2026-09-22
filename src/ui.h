#pragma once

const uint16_t BG=0x08C3,PANEL=0x1146,LINE=0x22AB,INK=0xFFFF;
const uint16_t MINT=0x87F4,LILAC=0xBCBF,GOLD=0xFE6D,MUTED=0x9D96,RED=0xFB6F;
const uint16_t PET_COLORS[]={0x87F4,0xFCB9,0xBCBF,0xFE6D,0xC7FF,0x8EEB};

void text(const char *s,int x,int y,uint16_t color=INK,int font=2) {
  frame.setTextDatum(TL_DATUM); frame.setTextColor(color); frame.drawString(s,x,y,font);
}
void center(const char *s,int x,int y,uint16_t color=INK,int font=2) {
  frame.setTextDatum(MC_DATUM); frame.setTextColor(color); frame.drawString(s,x,y,font); frame.setTextDatum(TL_DATUM);
}
void header(const char *title) {
  text(title,55,7,MINT,2); text(BREATH_PET_TEST_MODE?"TEST":((page==SENSOR || inputLive)?"LIVE":"DEMO"),287,12,GOLD,1);
  frame.drawFastHLine(54,30,259,LINE);
}
// The rail lines up with the two physical buttons, with the device held landscape.
void controls() {
  frame.fillRect(0,0,46,170,0x0862); frame.drawFastVLine(46,0,170,LINE);
  for (int i=0;i<2;i++) {
    int y=i?99:25; bool pressed=digitalRead(i?14:0)==LOW;
    frame.fillRoundRect(3,y,39,33,5,pressed?GOLD:PANEL);
    frame.drawRoundRect(3,y,39,33,5,page==SAMPLING?MUTED:GOLD);
    center(page==SAMPLING?"...":(i?"OK":"NEXT"),22,y+16,pressed?BG:GOLD,2);
  }
  center("hold",22,65,MUTED,1); center("BACK",22,76,MUTED,1);
  center("hold",22,139,MUTED,1); center("MENU",22,150,MUTED,1);
}
// Every selectable screen uses this same action, focus, and position indicator.
void action(const char *label,int index=0,int count=1) {
  frame.fillRoundRect(53,139,262,28,5,GOLD);
  text(">",60,144,BG,2); text(label,74,144,BG,2);
  char position[12]; snprintf(position,sizeof(position),"%d/%d",index+1,count);
  frame.setTextDatum(TR_DATUM); frame.setTextColor(BG); frame.drawString(position,308,149,1); frame.setTextDatum(TL_DATUM);
}
void bar(const char *name,int value,int x,int y,int width,uint16_t color) {
  text(name,x,y,MUTED,1); char n[8]; snprintf(n,sizeof(n),"%d",value); text(n,x+width-18,y,INK,1);
  frame.fillRoundRect(x,y+12,width,6,2,LINE);
  if (value>0) frame.fillRoundRect(x,y+12,max(3,width*value/100),6,2,color);
}
void creature(int type,int cx,int cy,int scale,int health,int score,uint32_t now) {
  int bob=(now/400)%2, x=cx-7*scale,y=cy-6*scale-bob;
  uint16_t color=PET_COLORS[type];
  auto pixel=[&](int px,int py,int w,int h,uint16_t c) { frame.fillRect(x+px*scale,y+py*scale,w*scale,h*scale,c); };
  frame.fillEllipse(cx,cy+6*scale,5*scale,max(1,scale),0x0862);
  // Shared pixel body with distinct silhouettes for the six selectable pets.
  pixel(4,2,6,1,color); pixel(3,3,8,7,color); pixel(4,10,6,1,color);
  if (type==0) { pixel(2,5,1,4,color); pixel(11,5,1,4,color); }
  if (type==1) { for (int yy=3;yy<=7;yy+=2) { pixel(0,yy,3,1,RED); pixel(11,yy,3,1,RED); } }
  if (type==2) { pixel(0,3,3,5,color); pixel(11,3,3,5,color); pixel(1,7,2,2,color); pixel(11,7,2,2,color); pixel(3,0,2,3,color); pixel(9,0,2,3,color); }
  if (type==3) { pixel(3,0,2,4,color); pixel(9,0,2,4,color); pixel(3,1,1,2,RED); pixel(10,1,1,2,RED); pixel(11,7,2,1,color); pixel(12,5,1,3,color); }
  if (type==4) { pixel(3,9,8,2,color); for (int xx=3;xx<=9;xx+=3) pixel(xx,11,2,1,color); }
  if (type==5) { pixel(3,0,3,3,color); pixel(8,0,3,3,color); pixel(2,9,2,2,color); pixel(10,9,2,2,color); }
  if (type!=4) { pixel(3,10,3,2,color); pixel(8,10,3,2,color); }
  bool blink=now%4300<150;
  int eyeY=type==5?2:5;
  if (score>=70 || health<35) {
    for (int ex:{4,8}) { pixel(ex,eyeY,1,1,BG); pixel(ex+1,eyeY+1,1,1,BG); pixel(ex+2,eyeY,1,1,BG); pixel(ex,eyeY+2,1,1,BG); pixel(ex+2,eyeY+2,1,1,BG); }
  } else { pixel(4,eyeY,2,blink?1:2,BG); pixel(8,eyeY,2,blink?1:2,BG); }
  pixel(3,8,2,1,RED); pixel(9,8,2,1,RED);
  if (score>=70 || health<35) pixel(6,8,2,2,BG);
  else { pixel(6,9,2,1,BG); pixel(5,8,1,1,BG); pixel(8,8,1,1,BG); }
}


void drawTank(uint32_t now) {
  header("THE TANK");
  char count[12]; snprintf(count,sizeof(count),"%d/6",storage.count()); text(count,247,12,MUTED,1);
  frame.fillRoundRect(53,34,262,102,6,PANEL);
  for (int i=0;i<7;i++) {
    int bx=61+i*39,by=38+int((now/95+i*17)%93);
    frame.drawCircle(bx,by,i%2+1,LINE);
    if (i%2==0) { frame.drawLine(bx,134,bx-3,127,MINT); frame.drawLine(bx,134,bx+4,123,LINE); }
  }
  if (!storage.count()) {
    center("A little empty in here.",184,72,MINT,2);
    center("Adopt your first pet below",184,97,MUTED,1);
  }
  for (int i=0;i<MAX_PLAYERS;i++) {
    Player &p=storage.data.players[i]; if (!p.active) continue;
    int x,y; swimPosition(i,now,x,y);
    if (cursor==i) frame.drawEllipse(x,y,20,17,GOLD);
    creature(p.type,x,y,2,p.health,p.lastScore,now+i*217);
    // Only label the focused pet so names don't collide as the tank moves.
    if (cursor==i) center(p.name,x,y-23,GOLD,1);
  }
  char label[32];
  if (cursor==6) strcpy(label,"ADD A PET");
  else if (cursor==7) strcpy(label,"Evening menu");
  else snprintf(label,sizeof(label),"Visit %s",storage.data.players[cursor].name);
  int index=0,total=0;
  for(int i=0;i<8;i++) if(tankTarget(i)) { if(i==cursor) index=total; ++total; }
  action(label,index,total);
}
void drawName() {
  header("WHO'S PLAYING?");
  center("CHOOSE A NICKNAME",184,48,MUTED,1);
  center(PICKER_NAMES[nameIndex],184,82,MINT,4);
  center("NEXT to browse / OK to choose",184,115,MUTED,1);
  int index=0,total=0;
  for(int i=0;i<NAME_COUNT;i++) if(!storage.nameUsed(PICKER_NAMES[i])) { if(i==nameIndex) index=total; ++total; }
  char label[32]; snprintf(label,sizeof(label),"Use %s",PICKER_NAMES[nameIndex]); action(label,index,total);
}
void drawPetPicker(uint32_t now) {
  header("ADOPT A PET"); text(PICKER_NAMES[nameIndex],58,41,MUTED,2);
  creature(petIndex,103,88,4,100,0,now);
  center(PET_NAMES[petIndex],232,76,PET_COLORS[petIndex],4);
  center("YOUR EVENING BUDDY",232,104,MUTED,1);
  action("Adopt this pet",petIndex,PET_TYPES);
}
void drawPet(uint32_t now) {
  Player &p=player(); header(p.name);
  creature(p.type,103,76,4,p.health,p.lastScore,now);
  center(mood(p),103,114,p.lastScore>=70?GOLD:MINT,1);
  bar("HEALTH",p.health,165,43,142,p.health<35?RED:MINT);
  bar("FOOD",p.food,165,71,142,GOLD); bar("JOY",p.joy,165,99,142,LILAC);
  const char *actions[]={"Feed your pet","View sample history","Rest (+8 health)"}; action(actions[cursor],cursor,3);
}
void drawFeed(uint32_t now) {
  header("FEED YOUR PET"); text(player().name,58,42,PET_COLORS[player().type],2);
  if(inputLive) {
    char line[48]; snprintf(line,sizeof(line),"%d mV",mq3.millivolts); center(line,256,52,MINT,2);
    if(page==SAMPLING) {
      center("CUP NEAR SENSOR NOW",184,81,GOLD,2);
      snprintf(line,sizeof(line),"Rise +%d mV / %ds left",max(0,sensorFeed.peak-sensorFeed.baseline),max(0,12-int((now-samplingStart)/1000)));
      center(line,184,106,MUTED,2); center("Remove cup after 5-10 seconds",184,128,MUTED,1);
      frame.fillRoundRect(55,145,258,12,4,LINE);
      int w=min(258,int((now-samplingStart)*258/12000)); if(w>0) frame.fillRoundRect(55,145,max(4,w),12,4,MINT);
    } else {
      center("KEEP CUP AWAY",184,78,GOLD,2);
      const char *hint=!mq3.count?"Checking sensor...":!sensorFeed.recovered(mq3)?"Let sensor recover in clean air":!mq3.canZero()?mq3.condition():mq3.spread()>25?"Wait for signal to settle":"Ready: press OK, then bring cup";
      center(hint,184,106,MUTED,1);
      action(sensorFeed.ready(mq3)?"Start live feed":"Wait for clean air");
    }
    return;
  }
  text("Simulated input",58,68,MUTED,1); text("Live test is in Menu",58,84,MUTED,1);
  char value[8];
  if (page==SAMPLING) {
    snprintf(value,sizeof(value),"%d",max(1,3-int((now-samplingStart)/1000)));
    center(value,272,73,MINT,6); center("Sampling demo...",184,113,GOLD,2);
    frame.fillRoundRect(55,143,258,12,4,LINE);
    int w=min(258,int((now-samplingStart)*258/3000)); if(w>0) frame.fillRoundRect(55,143,max(4,w),12,4,MINT);
  } else {
    snprintf(value,sizeof(value),"%d",DEMO_VALUES[demoIndex]); center(value,272,71,GOLD,6);
    center("fake units",272,107,MUTED,1); action("Start demo feed",demoIndex,4);
  }
}
void drawResult(uint32_t now) {
  Player &p=player(); header("FEED RESULT"); creature(p.type,101,78,4,p.health,p.lastScore,now);
  text(p.name,157,42,PET_COLORS[p.type],2);
  text(resultDelta<0?"TOO MUCH!":"NOM NOM!",157,66,resultDelta<0?RED:MINT,2);
  const Sample &r=p.readings[0];
  char detail[40]; snprintf(detail,sizeof(detail),"%s score: %u",r.source?"Game":"Demo",p.lastScore); text(detail,157,92,MUTED,1);
  if (resultDelta<0) snprintf(detail,sizeof(detail),"Health %d / try Rest",resultDelta);
  else snprintf(detail,sizeof(detail),"Food + joy boosted");
  text(detail,157,108,resultDelta<0?GOLD:MINT,1);
  if(r.source) { snprintf(detail,sizeof(detail),"MQ-3 rise +%u mV",r.peakMv-r.baselineMv); center(detail,184,128,MUTED,1); }
  action(cursor==0?"Back to the tank":"Visit your pet",cursor,2);
}
void drawHistory() {
  Player &p=player(); header("SAMPLE HISTORY");
  char summary[44]; snprintf(summary,sizeof(summary),"%s / page %d of %d",p.name,historyPage+1,max(1,(int(p.count)+2)/3)); text(summary,57,36,GOLD,1);
  if (!p.count) { center("No samples yet",184,80,MINT,2); center("Feed your pet to begin",184,106,MUTED,1); }
  for (int i=0;i<3;i++) {
    int n=historyPage*3+i; if (n>=p.count) break;
    Sample &r=p.readings[n]; char row[64],age[24];
    if (r.boot==storage.data.boot) {
      uint32_t seconds=millis()/1000-r.seconds;
      snprintf(age,sizeof(age),"%lum %lus ago",(unsigned long)(seconds/60),(unsigned long)(seconds%60));
    } else snprintf(age,sizeof(age),"earlier session");
    snprintf(row,sizeof(row),"#%lu %s / Score %u / HP %+d",(unsigned long)r.number,r.source?"MQ3":"DEMO",r.score,r.healthDelta);
    text(row,58,52+i*27,INK,1);
    if(r.source) snprintf(row,sizeof(row),"+%u mV / %s",r.peakMv-r.baselineMv,age);
    else snprintf(row,sizeof(row),"Fake %u / %s",r.raw,age);
    text(row,58,63+i*27,MUTED,1);
  }
  action("Back to your pet",historyPage,max(1,(int(p.count)+2)/3));
}
const char *const MENU_ITEMS[]={"Back to the tank","Response settings","Start new evening","MQ-3 setup","Change input mode"};
void drawMenu() {
  header("EVENING MENU");
  const char *titles[]={"THE TANK","GAME RESPONSE","NEW EVENING","MQ-3 SETUP",inputLive?"LIVE MQ-3":"DEMO INPUT"};
  const char *details[]={"Visit your pets or add a friend","Adjust game sensitivity / not BAC","Clear pets after confirmation","Check live voltage in clean air",inputLive?"OK switches to pretend readings":"OK switches to the real sensor"};
  center(titles[cursor],184,65,MINT,4); center(details[cursor],184,97,MUTED,1);
  center("NEXT to browse / OK to choose",184,120,MUTED,1);
  action(MENU_ITEMS[cursor],cursor,5);
}
void drawSensor(uint32_t now) {
  header("MQ-3 BENCH TEST");
  char value[48]; snprintf(value,sizeof(value),"%d mV",mq3.millivolts); text(value,57,39,MINT,4);
  snprintf(value,sizeof(value),"ADC %d",mq3.raw); text(value,236,43,MUTED,1);
  snprintf(value,sizeof(value),"%lus open",(unsigned long)((now-mq3.started)/1000)); text(value,236,56,MUTED,1);
  if(mq3.hasBaseline) snprintf(value,sizeof(value),"Air %d mV / change %+d mV",mq3.baseline,mq3.millivolts-mq3.baseline);
  else snprintf(value,sizeof(value),"GPIO1 / no air baseline / not BAC");
  text(value,57,72,MUTED,1);
  frame.fillRect(55,87,258,27,PANEL);
  for(int i=1;i<mq3.count;i++) {
    int a=(mq3.head-mq3.count+i-1+Mq3Monitor::WINDOW)%Mq3Monitor::WINDOW;
    int b=(a+1)%Mq3Monitor::WINDOW;
    int ya=112-constrain(int(mq3.readings[a]),0,3100)*24/3100;
    int yb=112-constrain(int(mq3.readings[b]),0,3100)*24/3100;
    frame.drawLine(56+(i-1)*256/99,ya,56+i*256/99,yb,GOLD);
  }
  center(mq3.condition(),184,124,mq3.canZero()?MINT:GOLD,1);
  const char *actions[]={"Zero in clean air","Clear air baseline","Back to menu"}; action(actions[cursor],cursor,3);
}
void drawCalibration() {
  if(inputLive) {
    header("GAME RESPONSE"); char line[32]; snprintf(line,sizeof(line),"%u mV",storage.data.sensorSpanMv);
    center(line,184,68,MINT,4); center("Rise above 20mV noise allowance",184,100,MUTED,1);
    center("Full-scale game score / not BAC",184,118,MUTED,1);
    const char *actions[]={"More responsive","Less responsive","Default (600 mV)","Back to menu"}; action(actions[cursor],cursor,4); return;
  }
  header("DEMO CALIBRATION");
  char line[48]; snprintf(line,sizeof(line),"RAW %d     ZERO %u",lastRaw,storage.data.zero); center(line,184,51,MUTED,2);
  snprintf(line,sizeof(line),"Span %u",storage.data.span); center(line,184,83,MINT,4);
  center("Game response only / not BAC",184,116,MUTED,1);
  const char *actions[]={"Zero current input","Restore defaults","More responsive","Less responsive"}; action(actions[cursor],cursor,4);
}
void draw(uint32_t now) {
  frame.fillSprite(BG);
  switch (page) {
    case TANK: drawTank(now); break;
    case NAME_PICK: drawName(); break;
    case PET_PICK: drawPetPicker(now); break;
    case PET: drawPet(now); break;
    case FEED: case SAMPLING: drawFeed(now); break;
    case RESULT: drawResult(now); break;
    case HISTORY: drawHistory(); break;
    case MENU: drawMenu(); break;
    case CALIBRATION: drawCalibration(); break;
    case SENSOR: drawSensor(now); break;
    case NEW_NIGHT:
      header("NEW EVENING?"); center("Clear all pets & history?",184,61,INK,2);
      center("This cannot be undone.",184,88,MUTED,1);
      action(cursor==0?"Keep my pets":"Clear & start again",cursor,2); break;
  }
  if (notice[0] && now-noticeAt<2500) {
    frame.fillRoundRect(54,107,260,28,5,0x42A8); center(notice,184,121,INK,1);
  }
  if (!storage.lastWriteOK) text("SAVE ERROR",55,32,RED,1);
  controls(); frame.pushSprite(0,0); ++frames;
}
