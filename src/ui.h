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
  text(title,10,7,MINT,2);
  frame.fillRoundRect(252,5,58,22,5,PANEL); center("DEMO",281,16,GOLD,1);
  frame.drawFastHLine(10,31,300,LINE);
}
void footer(const char *help) {
  frame.fillRect(0,149,320,21,BG); frame.drawFastHLine(10,149,300,LINE);
  center(help,160,160,MUTED,1);
}
void tile(const char *s,int x,int y,int w,int h,bool active=false) {
  frame.fillRoundRect(x,y,w,h,5,active?0x226A:PANEL);
  if (active) frame.drawRoundRect(x,y,w,h,5,GOLD);
  center(s,x+w/2,y+h/2,INK,2);
}
void bar(const char *name,int value,int x,int y,int width,uint16_t color) {
  text(name,x,y,MUTED,1); char n[8]; snprintf(n,sizeof(n),"%d",value); text(n,x+width-18,y,INK,1);
  frame.fillRoundRect(x,y+12,width,5,2,LINE);
  if (value>0) frame.fillRoundRect(x,y+12,max(3,width*value/100),5,2,color);
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
  text("THE TANK",10,7,MINT,2);
  char count[16]; snprintf(count,sizeof(count),"%d / 6 pets",storage.count()); text(count,106,14,MUTED,1);
  tile(storage.count()<MAX_PLAYERS?"Add Pet":"Full",194,4,76,25,cursor==6);
  tile("Menu",276,4,40,25,cursor==7);
  frame.fillRoundRect(5,35,310,111,7,PANEL); frame.drawRoundRect(5,35,310,111,7,LINE);
  frame.fillRect(12,141,296,3,0x328C);
  for (int i=0;i<8;i++) {
    int bx=18+i*40,by=45+int((now/95+i*17)%91);
    frame.drawCircle(bx,by,i%2+1,LINE);
    if (i%2==0) { frame.drawLine(bx,141,bx-3,134,MINT); frame.drawLine(bx,141,bx+4,130,LINE); }
  }
  if (!storage.count()) {
    center("Your tank is empty.",160,76,MINT,2);
    center("Choose Add Pet to adopt your first pet.",160,101,MUTED,1);
  }
  for (int i=0;i<MAX_PLAYERS;i++) {
    Player &p=storage.data.players[i]; if (!p.active) continue;
    int x,y; swimPosition(i,now,x,y);
    if (cursor==i) frame.drawEllipse(x,y,20,17,GOLD);
    creature(p.type,x,y,2,p.health,p.lastScore,now+i*217);
    center(p.name,x,y-23,cursor==i?GOLD:PET_COLORS[p.type],1);
  }
  footer("14: next   BOOT: pick   hold 14: menu");
}
void drawName() {
  header("WHO'S PLAYING?");
  center("CHOOSE A NICKNAME",160,48,MUTED,1);
  frame.fillRoundRect(20,63,280,43,8,PANEL);
  center(PICKER_NAMES[nameIndex],160,84,MINT,4);
  center("<  NEXT NAME          PICK THIS  >",160,126,GOLD,1);
  footer("14: next name   BOOT: choose   hold BOOT: back");
}
void drawPetPicker(uint32_t now) {
  header("ADOPT A PET"); text(PICKER_NAMES[nameIndex],14,44,MUTED,2);
  creature(petIndex,79,92,4,100,0,now);
  center(PET_NAMES[petIndex],226,65,PET_COLORS[petIndex],4);
  center("YOURS FOR THE EVENING",222,91,MUTED,1);
  tile("Adopt!",164,108,132,29,true);
  footer("14: next pet   BOOT: adopt   hold BOOT: back");
}
void drawPet(uint32_t now) {
  Player &p=player(); header(p.name);
  creature(p.type,72,70,4,p.health,p.lastScore,now);
  center(mood(p),72,103,p.lastScore>=70?GOLD:MINT,1);
  bar("HEALTH",p.health,160,39,146,p.health<35?RED:MINT);
  bar("FOOD",p.food,160,63,146,GOLD); bar("JOY",p.joy,160,87,146,LILAC);
  tile("Feed",6,114,99,30,cursor==0); tile("History",110,114,100,30,cursor==1); tile("Rest",215,114,99,30,cursor==2);
  footer("14: next   BOOT: select   hold BOOT: tank");
}
void drawFeed(uint32_t now) {
  header("FEED YOUR PET");
  text(player().name,12,41,PET_COLORS[player().type],2);
  text("MQ-3 offline / pretend breath",12,63,MUTED,1);
  if (page==SAMPLING) {
    int remaining=max(1,3-int((now-samplingStart)/1000));
    char number[5]; snprintf(number,sizeof(number),"%d",remaining);
    center(number,265,76,MINT,4);
    center("SAMPLING DEMO...",160,107,GOLD,2);
    frame.fillRoundRect(16,132,288,6,3,LINE);
    frame.fillRoundRect(16,132,min(288,int((now-samplingStart)*288/3000)),6,3,MINT);
    footer("hold BOOT: cancel sample");
  } else {
    char value[24]; snprintf(value,sizeof(value),"%d",DEMO_VALUES[demoIndex]);
    center(value,263,55,GOLD,4); center("fake units",263,81,MUTED,1);
    tile("Next fake value",8,101,148,38,false); tile("Start demo",164,101,148,38,true);
    footer("14: fake value   BOOT: start   hold BOOT: back");
  }
}
void drawResult(uint32_t now) {
  Player &p=player(); header("FEED RESULT / DEMO");
  creature(p.type,67,80,4,p.health,p.lastScore,now);
  text(p.name,139,40,PET_COLORS[p.type],2);
  text(resultDelta<0?"TOO MUCH!":"NOM NOM!",139,62,resultDelta<0?RED:MINT,4);
  char detail[40]; snprintf(detail,sizeof(detail),"Fake score: %u / 100",p.lastScore); text(detail,139,93,MUTED,1);
  if (resultDelta<0) snprintf(detail,sizeof(detail),"%d HEALTH - try Rest",resultDelta);
  else snprintf(detail,sizeof(detail),"Food + joy boosted");
  text(detail,139,111,resultDelta<0?GOLD:MINT,1);
  footer("BOOT: tank   14: your pet   hold BOOT: back");
}
void drawHistory() {
  Player &p=player(); header("YOUR PET'S HISTORY");
  char summary[44]; snprintf(summary,sizeof(summary),"%s / SIM ONLY / PAGE %d",p.name,historyPage+1); text(summary,10,40,GOLD,1);
  if (!p.count) center("No samples yet. Feed your pet!",160,88,MUTED,2);
  for (int i=0;i<4;i++) {
    int n=historyPage*4+i; if (n>=p.count) break;
    Sample &r=p.readings[n]; char row[64],age[24];
    if (r.boot==storage.data.boot) {
      uint32_t seconds=millis()/1000-r.seconds;
      snprintf(age,sizeof(age),"%lum %lus ago",(unsigned long)(seconds/60),(unsigned long)(seconds%60));
    } else snprintf(age,sizeof(age),"earlier session");
    snprintf(row,sizeof(row),"#%lu %u units | HP %+d | %s",(unsigned long)r.number,r.score,r.healthDelta,age);
    text(row,10,63+i*19,i?MUTED:INK,1);
  }
  footer("14: next page   BOOT: back to pet");
}
void drawMenu() {
  header("EVENING MENU");
  tile("Back to the tank",12,38,296,30,cursor==0);
  tile("Demo calibration",12,73,296,30,cursor==1);
  tile("Start a new evening",12,108,296,30,cursor==2);
  footer("14: next   BOOT: select   hold BOOT: tank");
}
void drawCalibration() {
  header("DEMO CALIBRATION");
  char line[50]; snprintf(line,sizeof(line),"Raw %d  Zero %u  Span %u",lastRaw,storage.data.zero,storage.data.span); text(line,10,40,INK,2);
  text("Game response only. Not a BAC calibration.",10,62,MUTED,1);
  tile("Zero current",8,77,147,29,cursor==0); tile("Defaults",163,77,149,29,cursor==1);
  tile("Span -25",8,112,147,29,cursor==2); tile("Span +25",163,112,149,29,cursor==3);
  footer("14: next   BOOT: change   hold BOOT: back");
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
    case NEW_NIGHT:
      header("NEW EVENING?"); center("Clear all six pets and their history?",160,56,INK,2);
      center("This cannot be undone.",160,80,MUTED,1);
      tile("Keep pets",8,107,147,35,cursor==0); tile("New evening",163,107,149,35,cursor==1);
      footer("14: choose   BOOT: confirm   hold BOOT: cancel"); break;
  }
  if (notice[0] && now-noticeAt<2500) {
    frame.fillRoundRect(9,120,302,26,5,0x42A8); center(notice,160,133,INK,1);
  }
  if (!storage.lastWriteOK) text("SAVE ERROR",10,32,RED,1);
  frame.pushSprite(0,0); ++frames;
}

