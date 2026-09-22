#pragma once
#include "party.h"

constexpr int COLLECTIONS=32, HATS=6, ITEMS=5, COLOURS=4;
const char *const HAT_NAMES[]={"No hat","Party cone","Cowboy hat","Crown","Sunglasses","Top hat"};
const char *const ITEM_NAMES[]={"Empty hands","Red cup","Pizza slice","Floatie","Bubble wand"};
const char *const COLOUR_NAMES[]={"Original","Sunshine","Lilac","Ocean"};
const char *const PERSONALITIES[]={"Bubble chaser","Hat prankster","Shy cup buddy","Parade leader"};
struct Collection {
  char name[9]={}; uint8_t hats=1,items=1,colours=1,hat=0,item=0,colour=0;
  uint16_t visits=0; uint32_t lastNight=0;
};
struct FunPet {
  char name[9]={}; uint16_t idle=0,naps=0,checkins=0,games=0,encounters=0;
  uint8_t vibe=0,personality=0,rewarded=0; uint32_t lastReward=0;
};
struct FunData {
  uint32_t magic=0x46554e31,night=0,minutes=0,totalRewards=0;
  uint8_t upgrades=0; FunPet pets[MAX_PLAYERS]; Collection closet[COLLECTIONS];
};
class FunStorage {
  Preferences prefs;
public:
  FunData data; bool ready=false,lastWriteOK=true;
  uint32_t wakeAt[MAX_PLAYERS]={},discoAt=0; bool celebrating=false;
  int lastLoot=0; // 0 none, 1..5 hat, 11..14 item, 21..23 colour.
  static int bits(uint8_t n) { int count=0; while(n) { count+=n&1; n>>=1; } return count; }
  bool valid(const FunData &d) const {
    if(d.magic!=0x46554e31 || d.upgrades>7) return false;
    for(auto &p:d.pets) if(p.name[8] || p.vibe>100 || p.personality>3 || p.rewarded>1 || p.lastReward>d.minutes) return false;
    for(auto &c:d.closet) if(c.name[8] || c.hats>=64 || c.items>=32 || c.colours>=16 || c.hat>=HATS || c.item>=ITEMS || c.colour>=COLOURS || !(c.hats&(1<<c.hat)) || !(c.items&(1<<c.item)) || !(c.colours&(1<<c.colour))) return false;
    return true;
  }
  bool save() { if(!ready) return false; return lastWriteOK=prefs.putBytes("fun-v1",&data,sizeof(data))==sizeof(data); }
  Collection *collection(const char *name,bool create=true) {
    for(auto &c:data.closet) if(!strcmp(c.name,name)) return &c;
    if(create) for(auto &c:data.closet) if(!c.name[0]) { strcpy(c.name,name); return &c; }
    return nullptr;
  }
  void sync(const PartyData &party) {
    bool changed=false;
    if(data.night!=party.night) {
      // The party's saved evening ID also completes a reset after power loss
      // between the party and collection writes. A normal reboot keeps both.
      static const FunData fresh;
      data=fresh; data.night=party.night;
      memset(wakeAt,0,sizeof(wakeAt)); discoAt=0; lastLoot=0; celebrating=false;
      changed=true;
    }
    for(int i=0;i<MAX_PLAYERS;i++) {
      const auto &p=party.players[i]; if(!p.active) continue;
      FunPet &f=data.pets[i];
      if(strcmp(f.name,p.name)) {
        f=FunPet(); strcpy(f.name,p.name);
        // Stable nicknames give repeat visitors the same personality.
        uint32_t hash=0; for(const char *s=p.name;*s;s++) hash=hash*31+*s;
        f.personality=hash%4;
        if(!strcmp(p.name,"GOOSE")) f.personality=1;
        if(!strcmp(p.name,"BEAN")) f.personality=2;
        if(!strcmp(p.name,"CAPTAIN")) f.personality=3;
        Collection *c=collection(p.name);
        if(c && c->lastNight!=party.night) { c->lastNight=party.night; c->visits=1; }
        changed=true;
      }
    }
    if(changed) save();
  }
  void begin(const PartyData &party) {
    ready=prefs.begin(BREATH_PET_TEST_MODE?"fun-test":"pet-fun",false);
    static FunData loaded;
    if(ready && prefs.getBytesLength("fun-v1")==sizeof(loaded) && prefs.getBytes("fun-v1",&loaded,sizeof(loaded))==sizeof(loaded) && valid(loaded)) data=loaded;
    sync(party); if(ready) save(); else lastWriteOK=false;
  }
  int energy(int slot) const { return max(0,100-int(data.pets[slot].idle)*5); }
  const char *state(int slot) const {
    const auto &p=data.pets[slot];
    return p.idle>=20?"ASLEEP":p.idle>=10?"DROWSY":p.vibe>=70?"WILD":p.vibe>=25?"PARTY":"CHILL";
  }
  bool rewardReady(int slot) const { const auto &p=data.pets[slot]; return !p.rewarded || data.minutes-p.lastReward>=10; }
  int dressCount(int slot) { auto *c=collection(data.pets[slot].name,false); return c?bits(c->hats)+bits(c->items)+bits(c->colours)-3:0; }
  void wake(int slot,int vibe,uint32_t now,uint32_t randomBits) {
    auto &p=data.pets[slot]; p.idle=0; p.vibe=constrain(vibe,0,100); wakeAt[slot]=now; lastLoot=0;
    if(rewardReady(slot)) {
      p.rewarded=1; p.lastReward=data.minutes; p.checkins=min(65535,int(p.checkins)+1); ++data.totalRewards;
      Collection *c=collection(p.name);
      if(c) {
        if(!(c->items&2)) { c->items|=2; c->item=1; lastLoot=11; }
        else {
          int options[12],n=0;
          for(int i=1;i<HATS;i++) if(!(c->hats&(1<<i))) options[n++]=i;
          // Third check-in guarantees a hat if any remain.
          if(p.checkins!=3 || !n) {
            for(int i=1;i<ITEMS;i++) if(!(c->items&(1<<i))) options[n++]=10+i;
            // Colours must be earnable within this evening, like other clothes.
            for(int i=1;i<COLOURS;i++) if(!(c->colours&(1<<i))) options[n++]=20+i;
          }
          if(n) {
            lastLoot=options[randomBits%n];
            if(lastLoot<10) { c->hats|=1<<lastLoot; c->hat=lastLoot; }
            else if(lastLoot<20) { c->items|=1<<(lastLoot-10); c->item=lastLoot-10; }
            else { c->colours|=1<<(lastLoot-20); c->colour=lastLoot-20; }
          }
        }
      }
      if(data.totalRewards>=2) data.upgrades|=1;
      if(data.totalRewards>=5) data.upgrades|=2;
      if(data.totalRewards>=8) data.upgrades|=4;
      int present=0; bool all=true;
      for(auto &f:data.pets) if(f.name[0]) { ++present; all &= f.checkins>0 && f.idle<10; }
      if(present>=2 && all) { celebrating=true; discoAt=now; data.upgrades|=4; }
    }
    save();
  }
  void nap(int slot) { auto &p=data.pets[slot]; if(p.idle<20) p.naps=min(65535,int(p.naps)+1); p.idle=20; save(); }
  void tick() {
    ++data.minutes;
    for(auto &p:data.pets) if(p.name[0]) { if(p.idle==19) p.naps=min(65535,int(p.naps)+1); p.idle=min(65535,int(p.idle)+1); }
    // A shared encounter every two powered minutes. Sleeping pets sit it out.
    if(data.minutes%2==0) {
      int awake[MAX_PLAYERS],n=0; for(int i=0;i<MAX_PLAYERS;i++) if(data.pets[i].name[0] && data.pets[i].idle<20) awake[n++]=i;
      if(n>=2) { int a=(data.minutes/2)%n,b=(a+1)%n; for(int i:{awake[a],awake[b]}) data.pets[i].encounters=min(65535,int(data.pets[i].encounters)+1); }
    }
    save();
  }
  void play(int slot,uint32_t now,uint32_t randomBits) { data.pets[slot].games=min(65535,int(data.pets[slot].games)+1); wake(slot,25,now,randomBits); }
  void dress(int slot,int category) {
    auto *c=collection(data.pets[slot].name); if(!c) return;
    uint8_t &value=category==0?c->hat:category==1?c->item:c->colour;
    uint8_t mask=category==0?c->hats:category==1?c->items:c->colours;
    int limit=category==0?HATS:category==1?ITEMS:COLOURS;
    do { value=(value+1)%limit; } while(!(mask&(1<<value)));
    save();
  }
  int awardValue(int slot,int category) { return category==0?dressCount(slot):category==1?data.pets[slot].naps:data.pets[slot].encounters; }
  int winner(int category,int &ties) {
    int best=-1,value=-1; ties=0;
    for(int i=0;i<MAX_PLAYERS;i++) if(data.pets[i].name[0]) { int n=awardValue(i,category); if(n>value) { best=i; value=n; ties=1; } else if(n==value) ++ties; }
    return best;
  }
};
