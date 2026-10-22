/* Temporary native pixel emblems. Replace this function with supplied art later. */
static unsigned eggTier(unsigned r){return r<1000?0:r<1200?1:r<1400?2:r<1600?3:r<1800?4:5;}
static const char*eggShort(unsigned r){static const char*n[]={"ROTTEN","BRONZE","SILVER","GOLD","PLAT","MASTER"};return n[eggTier(r)];}
static void eggEmblem(int x,int y,unsigned rating,unsigned scale){
 static const uint32_t shell[]={0x008b9870,0x00c99161,0x00d7e2ed,0x00ffda6f,0x00a5f0e8,0x00d2b4ff};
 static const uint32_t spots[]={0x004e6034,0x008e542f,0x007d9db9,0x00c58627,0x00439f9a,0x008749c5};
 unsigned t=eggTier(rating);
 for(int yy=0;yy<18;yy++)for(int xx=0;xx<14;xx++){
  static const int radius[18]={0,1,2,3,4,4,5,5,6,6,6,6,6,5,5,4,2,0};
  int dx=xx-7;if(dx<0)dx=-dx;int inside=dx<=radius[yy];
  if(!inside)continue;bool edge=dx==radius[yy]||yy==0||yy==17||(yy>0&&dx>radius[yy-1])||(yy<17&&dx>radius[yy+1]);
  bool dot=((xx-4)*(xx-4)+(yy-6)*(yy-6)<7)||((xx-9)*(xx-9)+(yy-11)*(yy-11)<8)||((xx-4)*(xx-4)+(yy-14)*(yy-14)<4);
  uint32_t c=edge?0x002a3445:dot?spots[t]:shell[t];if(!edge&&!dot&&xx<5&&yy<9)c=0x00fff9e0;
  accountRect(x+xx*scale,y+yy*scale,scale,scale,c);
 }
}
