/* Menu-only composition. No additional rendering during live versus. */
static void ta_n17_panel(int x,int y,int w,int h){
 for(int j=y;j<y+h&&j<480;j++)for(int i=x;i<x+w&&i<512;i++)if(i>=0&&j>=0){uint32_t c=screen[j*512+i];screen[j*512+i]=((c>>2)&0x003f3f3fu);}
}
static void ta_n17_backdrop(void){
 static uint32_t background[512*480];static bool ready;
 if(!ready){for(unsigned y=0;y<480;y++)for(unsigned x=0;x<512;x++){unsigned r=21+y/48,g=31+y/40,b=44+y/32;unsigned tile=((x+18)/62+(y+7)/62)%5;if((x+18)%62<54&&(y+7)%62<54){if(tile==0)g+=9;else if(tile==1)b+=12;else if(tile==2){r+=12;g+=5;}else if(tile==3){r+=7;b+=11;}else{g+=7;b+=7;}}background[y*512+x]=(r<<16)|(g<<8)|b;}ready=true;}
 memcpy(screen,background,sizeof background);ta_n17_panel(20,23,472,435);
}
