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
    frame.drawRoundRect(3,y,39,33,5,(page==SAMPLING || page==COUNTDOWN)?MUTED:GOLD);
    center((page==SAMPLING || page==COUNTDOWN)?"...":(i?"OK":"NEXT"),22,y+16,pressed?BG:GOLD,2);
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
void creature(int type,int cx,int cy,int scale,int health,int score,uint32_t now,bool asleep=false,uint16_t tint=0) {
  int bob=(now/400)%2, x=cx-7*scale,y=cy-6*scale-bob;
  uint16_t color=tint?tint:PET_COLORS[type];
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
  if (asleep) { pixel(4,eyeY+1,2,1,BG); pixel(8,eyeY+1,2,1,BG);
  } else if (score>=70) {
    pixel(4,eyeY,2,2,BG); pixel(8,eyeY+1,3,1,BG); pixel(9,eyeY,1,1,BG);
  } else { pixel(4,eyeY,2,blink?1:2,BG); pixel(8,eyeY,2,blink?1:2,BG); }
  pixel(3,8,2,1,RED); pixel(9,8,2,1,RED);
  if (asleep) pixel(6,8,2,2,BG);
  else { pixel(6,9,2,1,BG); pixel(5,8,1,1,BG); pixel(8,8,1,1,BG); }
}

void dressed(int slot,int x,int y,int scale,uint32_t now) {
  auto &p=storage.data.players[slot]; auto &f=fun.data.pets[slot]; auto *c=fun.collection(p.name,false);
  bool sleeping=f.idle>=20,drowsy=f.idle>=10;
  const uint16_t colours[]={0,GOLD,LILAC,0x4DFF};
  int hat=c?c->hat:0,item=c?c->item:0;
  // The prankster borrows a silly hat for a moment; inventories never change.
  if(!sleeping && f.personality==1 && now%24000<2400) hat=2;
  creature(p.type,x,y,scale,100,f.vibe,now,sleeping || (drowsy && now%1800<900),colours[c?c->colour:0]);
  int top=y-7*scale;
  if(hat==1) { frame.fillTriangle(x-3*scale,top+scale,x+3*scale,top+scale,x,top-4*scale,GOLD); frame.fillCircle(x,top-4*scale,scale,RED); }
  if(hat==2) { frame.fillRect(x-5*scale,top,10*scale,scale,GOLD); frame.fillRoundRect(x-3*scale,top-3*scale,6*scale,3*scale,scale,GOLD); }
  if(hat==3) { frame.fillRect(x-4*scale,top,8*scale,2*scale,GOLD); for(int k=-3;k<=3;k+=3) frame.fillTriangle(x+(k-1)*scale,top,x+(k+1)*scale,top,x+k*scale,top-3*scale,GOLD); }
  if(hat==4) { frame.fillRect(x-4*scale,y-2*scale,3*scale,2*scale,BG); frame.fillRect(x+scale,y-2*scale,3*scale,2*scale,BG); frame.drawLine(x-scale,y-scale,x+scale,y-scale,INK); }
  if(hat==5) { frame.fillRect(x-4*scale,top,8*scale,scale,LILAC); frame.fillRect(x-2*scale,top-4*scale,4*scale,4*scale,LILAC); frame.fillRect(x-2*scale,top-scale,4*scale,scale,GOLD); }
  int hx=x+5*scale,hy=y+scale;
  if(item==1 || (f.personality==2 && drowsy && !sleeping)) { frame.fillRect(hx,hy,3*scale,4*scale,RED); frame.fillRect(hx,hy,3*scale,scale,INK); }
  if(item==2) { frame.fillTriangle(hx,hy,hx+4*scale,hy,hx+2*scale,hy+5*scale,GOLD); frame.fillCircle(hx+2*scale,hy+scale,scale,RED); }
  if(item==3) { frame.drawEllipse(x,y+3*scale,8*scale,2*scale,GOLD); frame.drawEllipse(x,y+3*scale,8*scale-1,2*scale-1,RED); }
  if(item==4) { frame.drawLine(hx,hy+4*scale,hx+scale,hy,MINT); frame.drawCircle(hx+scale,hy-scale,2*scale,LILAC); }
  if(sleeping) { if(scale>2) text("Zzz",x+12,y-19,MUTED,1); }
  else if(fun.wakeAt[slot] && now-fun.wakeAt[slot]<3000) { frame.drawCircle(x,y,12+int((now-fun.wakeAt[slot])/150)%12,GOLD); if(scale>2) center("HELLO!",x,y+27,GOLD,1); }
  else if(f.vibe>=70) { for(int j=0;j<3;j++) frame.fillCircle(x-12+j*12,y-19-int((now/90+j*7)%12),1+j%2,j%2?GOLD:LILAC); }
  else if(f.vibe>=25 || f.personality==0) frame.drawCircle(x+16,y-10-int((now/150)%12),2,MINT);
}
void decorations(uint32_t now) {
  if(fun.data.upgrades&1) { frame.fillRoundRect(57,113,14,21,3,LILAC); frame.fillCircle(64,127,4,BG); text("~",73,113,GOLD,1); }
  if(fun.data.upgrades&2) { frame.fillTriangle(260,124,298,124,279,135,GOLD); frame.drawLine(278,123,278,103,INK); frame.fillTriangle(279,103,279,120,296,120,LILAC); }
  if(fun.data.upgrades&4) { frame.drawLine(185,34,185,42,MUTED); frame.fillCircle(185,47,6,LILAC); frame.drawLine(179,47,191,47,INK); frame.drawLine(185,41,185,53,INK); }
  bool disco=fun.celebrating && now-fun.discoAt<15000;
  if(disco) for(int i=0;i<18;i++) { int x=56+(i*31)%255,y=36+(now/35+i*13)%97; frame.fillRect(x,y,2,3,i%2?GOLD:LILAC); }
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
  decorations(now);
  for (int i=0;i<MAX_PLAYERS;i++) {
    Player &p=storage.data.players[i]; if (!p.active) continue;
    int x,y; swimPosition(i,now,x,y);
    if (cursor==i) frame.drawEllipse(x,y,20,17,GOLD);
    dressed(i,x,y,2,now);
    if(fun.data.pets[i].idle<20 && now%24000<4500 && i>0) { frame.drawLine(x-12,y-1,x-18,y-5,MINT); }
    // Only label the focused pet so names don't collide as the tank moves.

  }
  int usedX[MAX_PLAYERS],usedY[MAX_PLAYERS],usedW[MAX_PLAYERS],used=0;
  for(int slot=0;slot<MAX_PLAYERS;slot++) {
    if(!storage.data.players[slot].active) continue;
    int px,py; swimPosition(slot,now,px,py);
    const char *state=fun.data.pets[slot].idle>=20?"Zzz":fun.state(slot); int width=strlen(state)*6+4;
    int lx=constrain(px-width/2,55,312-width),ly=constrain(py+16,38,125);
    for(int tries=0;tries<8;tries++) {
      bool overlap=false;
      for(int j=0;j<used;j++) if(lx<usedX[j]+usedW[j] && lx+width>usedX[j] && ly<usedY[j]+10 && ly+10>usedY[j]) overlap=true;
      if(!overlap) break;
      const int offsets[]={-26,5,-15,17};
      ly=constrain(py+offsets[tries%4],38,125);
      if(tries>=3) lx=constrain(px-width/2+(tries%2?12:-12),55,312-width);
    }
    usedX[used]=lx; usedY[used]=ly; usedW[used++]=width;
    frame.fillRoundRect(lx,ly,width,10,2,BG); text(state,lx+2,ly+1,slot==cursor?GOLD:MUTED,1);
  }
  char label[32];
  if (cursor==6) strcpy(label,"ADD A PET");
  else if (cursor==7) strcpy(label,"Evening menu");
  else snprintf(label,sizeof(label),"%s / %s",storage.data.players[cursor].name,fun.state(cursor));
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
  dressed(selected,103,76,4,now);
  center(PERSONALITIES[fun.data.pets[selected].personality],103,120,MUTED,1);
  const auto *c=fun.collection(p.name,false);
  text(fun.state(selected),164,40,MINT,2);
  text(HAT_NAMES[c?c->hat:0],164,63,INK,2);
  text(ITEM_NAMES[c?c->item:0],164,83,GOLD,2);
  text(COLOUR_NAMES[c?c->colour:0],164,103,LILAC,1);
  bar("ENERGY",fun.energy(selected),164,116,142,MINT);
  const char *actions[]={"Wake / feed your pet","View sample history","Take a nap","Wardrobe","Play bubble catch"}; action(actions[cursor],cursor,5);
}
void drawFeed(uint32_t now) {
  header("FEED YOUR PET"); text(player().name,58,41,PET_COLORS[player().type],2);
  char value[40];
  if(page==COUNTDOWN) {
    snprintf(value,sizeof(value),"%d",max(1,5-int((now-samplingStart)/1000)));
    center(value,184,83,GOLD,6); center("GET READY / WAIT FOR BLOW",184,123,MUTED,1);
    return;
  }
  if(page==SAMPLING) {
    center("BLOW!",184,72,MINT,4);
    int score=inputLive?sensorScore(sensorFeed.baseline,sensorFeed.peak,storage.data.sensorSpanMv):DEMO_VALUES[demoIndex];
    frame.fillRoundRect(64,100,241,14,5,LINE);
    if(score) frame.fillRoundRect(64,100,max(5,241*score/100),14,5,score>=70?LILAC:GOLD);
    snprintf(value,sizeof(value),"%ds left / keep a steady sample",max(0,10-int((now-samplingStart)/1000))); center(value,184,128,MUTED,1);
    frame.fillRoundRect(55,151,258,8,3,LINE);
    int w=min(258,int((now-samplingStart)*258/10000)); if(w) frame.fillRoundRect(55,151,max(3,w),8,3,MINT);
    return;
  }
  if(inputLive) {
    center("KEEP SENSOR IN CLEAN AIR",184,75,GOLD,2);
    const char *hint=!mq3.count?"Checking sensor...":sensorFeed.ready(mq3)?"Ready / countdown starts with OK":"Let the sensor settle and recover";
    center(hint,184,104,MUTED,1);
    action(sensorFeed.ready(mq3)?"Start feeding":"Wait for clean air");
  } else {
    center("PRETEND SAMPLE",184,73,GOLD,2);
    snprintf(value,sizeof(value),"Game level: %d",DEMO_VALUES[demoIndex]); center(value,184,103,MUTED,2);
    action("Start demo feed",demoIndex,4);
  }
}

void drawResult(uint32_t now) {
  Player &p=player(); header("FEED RESULT"); dressed(selected,101,78,4,now);
  text(p.name,157,42,PET_COLORS[p.type],2);
  text(fun.state(selected),157,66,MINT,2);
  const Sample &r=p.readings[0];
  char detail[40]; snprintf(detail,sizeof(detail),"%s score: %u",r.source?"Game":"Demo",p.lastScore); text(detail,157,92,MUTED,1);
  if(fun.lastLoot) snprintf(detail,sizeof(detail),"NEW: %s",fun.lastLoot<10?HAT_NAMES[fun.lastLoot]:ITEM_NAMES[fun.lastLoot-10]);
  else snprintf(detail,sizeof(detail),"Awake + happy again");
  text(detail,157,108,resultDelta<0?GOLD:MINT,1);
  center("Game reaction / not a BAC reading",184,128,MUTED,1);
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
    snprintf(row,sizeof(row),"#%lu %s / Level %u",(unsigned long)r.number,r.source?"MQ3":"DEMO",r.score);
    text(row,58,52+i*27,INK,1);
    snprintf(row,sizeof(row),"%s / %s",r.score>=70?"WILD":r.score>=25?"PARTY":"CHILL",age);
    text(row,58,63+i*27,MUTED,1);
  }
  action("Back to your pet",historyPage,max(1,(int(p.count)+2)/3));
}
const char *const MENU_ITEMS[]={"Back to the tank","Response settings","Start new evening","MQ-3 setup","Change input mode","Evening awards","Tank upgrades"};
void drawMenu() {
  header("EVENING MENU");
  const char *titles[]={"THE TANK","GAME RESPONSE","NEW EVENING","MQ-3 SETUP",inputLive?"LIVE MQ-3":"DEMO INPUT","THE AWARDS","TANK TOYS"};
  const char *details[]={"Visit your pets or add a friend","Adjust game sensitivity / not BAC","Clear pets after confirmation","Check live voltage in clean air",inputLive?"OK switches to pretend readings":"OK switches to the real sensor","Best dressed, naps and new friends","Decorations earned together"};
  center(titles[cursor],184,65,MINT,4); center(details[cursor],184,97,MUTED,1);
  center("NEXT to browse / OK to choose",184,120,MUTED,1);
  action(MENU_ITEMS[cursor],cursor,7);
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
    const char *actions[]={"More responsive","Less responsive","Default (1200 mV)","Back to menu"}; action(actions[cursor],cursor,4); return;
  }
  header("DEMO CALIBRATION");
  char line[48]; snprintf(line,sizeof(line),"RAW %d     ZERO %u",lastRaw,storage.data.zero); center(line,184,51,MUTED,2);
  snprintf(line,sizeof(line),"Span %u",storage.data.span); center(line,184,83,MINT,4);
  center("Game response only / not BAC",184,116,MUTED,1);
  const char *actions[]={"Zero current input","Restore defaults","More responsive","Less responsive"}; action(actions[cursor],cursor,4);
}
void drawWardrobe(uint32_t now) {
  header("WARDROBE"); dressed(selected,106,82,4,now);
  const auto *c=fun.collection(player().name,false);
  text(player().name,161,41,MINT,2);
  text(HAT_NAMES[c?c->hat:0],161,66,INK,2);
  text(ITEM_NAMES[c?c->item:0],161,87,GOLD,2);
  text(COLOUR_NAMES[c?c->colour:0],161,110,LILAC,1);
  const char *a[]={"Try next hat","Try next hand item","Try next colour","Back to your pet"}; action(a[cursor],cursor,4);
}
void drawPlay(uint32_t now) {
  header("BUBBLE CATCH"); char label[32]; snprintf(label,sizeof(label),"%d / 3 caught",catches); center(label,184,49,MINT,2);
  center("Press OK inside the gold zone",184,72,MUTED,1);
  frame.fillRoundRect(65,90,240,27,4,PANEL); frame.fillRect(65+240*32/100,90,240*36/100,27,LINE);
  frame.drawRect(65+240*32/100,90,240*36/100,27,GOLD);
  frame.drawCircle(65+bubblePosition(now)*240/100,103,6,MINT);
  center("Three catches wake your pet",184,126,MUTED,1);
  action(cursor?"Back to your pet":"Catch the bubble",cursor,2);
}
void drawAwards(uint32_t now) {
  header("EVENING AWARDS"); const char *names[]={"BEST DRESSED","MOST NAPS","SOCIAL BUTTERFLY"};
  center(names[cursor],184,46,GOLD,2); int ties=0,slot=fun.winner(cursor,ties);
  if(slot<0) center("Adopt a pet to begin",184,85,MINT,2);
  else {
    dressed(slot,98,91,3,now); text(fun.data.pets[slot].name,153,73,MINT,2);
    char line[40]; snprintf(line,sizeof(line),"%d %s",fun.awardValue(slot,cursor),cursor==0?"collectibles":cursor==1?"naps":"friendly encounters"); text(line,153,96,MUTED,1);
    if(ties>1) { snprintf(line,sizeof(line),"Shared award: %d pets tied",ties); center(line,184,126,LILAC,1); }
  }
  action("Back to menu",cursor,3);
}
void drawDecor() {
  header("TANK UPGRADES"); const char *names[]={"JUKEBOX","PIRATE SHIP","DISCO BALL"}; const int goals[]={2,5,8};
  center(names[cursor],184,62,MINT,4);
  bool owned=fun.data.upgrades&(1<<cursor); center(owned?"UNLOCKED / in your tank":"Keep checking in or playing",184,96,owned?GOLD:MUTED,1);
  char line[45]; snprintf(line,sizeof(line),"%lu / %d shared check-ins",(unsigned long)fun.data.totalRewards,goals[cursor]); center(line,184,118,MUTED,1);
  action("Back to menu",cursor,3);
}

void draw(uint32_t now) {
  frame.fillSprite(BG);
  switch (page) {
    case TANK: drawTank(now); break;
    case NAME_PICK: drawName(); break;
    case PET_PICK: drawPetPicker(now); break;
    case PET: drawPet(now); break;
    case FEED: case COUNTDOWN: case SAMPLING: drawFeed(now); break;
    case RESULT: drawResult(now); break;
    case HISTORY: drawHistory(); break;
    case MENU: drawMenu(); break;
    case CALIBRATION: drawCalibration(); break;
    case SENSOR: drawSensor(now); break;
    case WARDROBE: drawWardrobe(now); break;
    case PLAY: drawPlay(now); break;
    case AWARDS: drawAwards(now); break;
    case DECOR: drawDecor(); break;
    case NEW_NIGHT:
      header("NEW EVENING?"); center("Clear tonight's pets & history?",184,61,INK,2);
      center("Clothes & tank toys stay saved",184,88,MUTED,1);
      action(cursor==0?"Keep my pets":"Clear & start again",cursor,2); break;
  }
  if (notice[0] && now-noticeAt<2500) {
    frame.fillRoundRect(54,107,260,28,5,0x42A8); center(notice,184,121,INK,1);
  }
  if (!storage.lastWriteOK || !fun.lastWriteOK) text("SAVE ERROR",55,32,RED,1);
  controls(); frame.pushSprite(0,0); ++frames;
}
