#include "ta_vs_lan_screen.h"
#include <stdio.h>
#include <string.h>
#if defined(TA_VS_COMPACT_UI) && TA_VS_COMPACT_UI
#include "ta_vs_font5x7.h"
static void text(uint32_t*p,int x,int y,const char*s,uint32_t color,int requested){
 if(!p||!s||requested<1)return;int scale=requested>=4?3:2,first=x;const char*begin=s;
 for(;*s;s++){
  if(*s=='\n'){x=first;y+=8*scale;continue;}
  /* Wrap whole words instead of silently cropping long status messages. */
  if(*s!=' '&&(s==begin||x==first||s[-1]==' ')){unsigned word=0;while(s[word]&&s[word]!=' '&&s[word]!='\n')word++;if(x!=first&&x+(int)word*6*scale>486){x=first;y+=8*scale;}}
  if(x+5*scale>486){x=first;y+=8*scale;}if(x==first&&*s==' ')continue;
  const uint8_t*g=font_rows(*s);for(int row=0;row<7;row++)for(int col=0;col<5;col++)if(g[row]&(1u<<(4-col)))for(int dy=0;dy<scale;dy++)for(int dx=0;dx<scale;dx++){int xx=x+col*scale+dx,yy=y+row*scale+dy;if(xx>=0&&xx<512&&yy>=0&&yy<480)p[yy*512+xx]=color;}
  x+=6*scale;
 }
}
#else
static const char letters[]="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:-./_";
static const char*const glyph[]={
 "010101111101101","110101110101110","011100100100011","110101101101110","111100110100111","111100110100100","011100101101011","101101111101101","111010010010111","001001001101010","101101110101101","100100100100111","101111111101101","101111111111101","010101101101010","110101110100100","010101101111011","110101110101101","011100010001110","111010010010010","101101101101111","101101101101010","101101111111101","101101010101101","101101010010010","111001010100111",
 "111101101101111","010110010010111","110001010100111","110001010001110","101101111001001","111100110001110","011100111101111","111001010010010","111101111101111","111101111001110","000010000010000","000000111000000","000000000000010","001001010100100","000000000000111"};
static void text(uint32_t*p,int x,int y,const char*s,uint32_t color,int scale){
 for(;*s;s++,x+=4*scale){const char*c=strchr(letters,*s);if(!c)continue;const char*g=glyph[c-letters];for(int row=0;row<5;row++)for(int col=0;col<3;col++)if(g[row*3+col]=='1')for(int dy=0;dy<scale;dy++)for(int dx=0;dx<scale;dx++){int xx=x+col*scale+dx,yy=y+row*scale+dy;if(xx>=0&&xx<512&&yy>=0&&yy<480)p[yy*512+xx]=color;}}
}
#endif
static void ipline(uint32_t*p,int y,const char*label,uint32_t ip){char b[64];const uint8_t*a=(const uint8_t*)&ip;snprintf(b,sizeof b,"%s %u.%u.%u.%u",label,a[0],a[1],a[2],a[3]);text(p,30,y,b,0x009FB9CE,2);}
void ta_vs_lan_render(uint32_t*p,unsigned state,uint32_t localIp,uint32_t peerIp,unsigned remoteButtons,int error){
 if(!p)return;for(unsigned i=0;i<512*480;i++)p[i]=0x000B1524;
 text(p,30,42,"TETRIS ATTACK",0x00FFFFFF,5);
#if defined(TA_VS_ACCOUNT_UX) && TA_VS_ACCOUNT_UX
 text(p,30,88,"VERSUS N15",0x006FC9FF,3);
#elif defined(TA_VS_ACCOUNTS) && TA_VS_ACCOUNTS
 text(p,30,88,"VERSUS N14",0x006FC9FF,3);
#elif defined(TA_VS_NATIVE_VIEW) && TA_VS_NATIVE_VIEW
 text(p,30,88,"VERSUS N13",0x006FC9FF,3);
#elif defined(TA_VS_FIRST_LAUNCH) && TA_VS_FIRST_LAUNCH
 text(p,30,88,"VERSUS N12",0x006FC9FF,3);
#elif defined(TA_VS_REMATCH) && TA_VS_REMATCH
 text(p,30,88,"VERSUS N10",0x006FC9FF,3);
#elif defined(TA_VS_PUBLIC_ROOMS) && TA_VS_PUBLIC_ROOMS
 text(p,30,88,"VERSUS N9",0x006FC9FF,3);
#elif defined(TA_VS_ACTIVE_CAPTURE) && TA_VS_ACTIVE_CAPTURE
 text(p,30,88,"LAN VERSUS N8",0x006FC9FF,3);
#elif defined(TA_VS_LEAN_VIEW) && TA_VS_LEAN_VIEW
 text(p,30,88,"LAN VERSUS N7",0x006FC9FF,3);
#elif defined(TA_VS_NATIVE_GAME) && TA_VS_NATIVE_GAME
 text(p,30,88,"LAN VERSUS N6",0x006FC9FF,3);
#else
 text(p,30,88,"LAN CONNECTION PREVIEW N4",0x006FC9FF,3);
#endif
 text(p,30,124,"XEMU TO ORIGINAL XBOX",0x009FB9CE,2);
 static const char*const status[]={"CHOOSE CONNECTION","GETTING NETWORK ADDRESS","WAITING FOR JOIN","SEARCHING LOCAL NETWORK","CHECKING CONNECTION","CONNECTED","CONNECTION LOST","NETWORK ERROR"};
 if(state>TA_LAN_ERROR)state=TA_LAN_ERROR;text(p,30,178,status[state],state==TA_LAN_CONNECTED?0x006AFFA8:0x00FFFFFF,3);
 if(state==TA_LAN_SELECT){text(p,30,222,"A HOST LAN",0x006AFFA8,3);text(p,30,255,"X JOIN LAN",0x006FC9FF,3);}
 else {ipline(p,224,"LOCAL",localIp);if(peerIp)ipline(p,248,"PEER",peerIp);}
 if(state==TA_LAN_CONNECTED){text(p,30,280,"ROM AND RULES MATCH",0x006AFFA8,2);
#if defined(TA_VS_NATIVE_GAME) && TA_VS_NATIVE_GAME
 text(p,30,304,remoteButtons?"PEER READY":"WAITING FOR PEER READY",0x00FFFFFF,2);
#else
 text(p,30,304,remoteButtons?"PEER CONTROLLER ACTIVE":"PEER CONTROLLER IDLE",0x00FFFFFF,2);
#endif
}
 if(state==TA_LAN_ERROR){char b[64];snprintf(b,sizeof b,"ERROR %d",error);text(p,30,280,b,0x00FFAA88,3);}
 
#if defined(TA_VS_NATIVE_GAME) && TA_VS_NATIVE_GAME
 text(p,30,350,state==TA_LAN_CONNECTED?"A READY FOR VERSUS":"LOCAL BOARD AUTHORITY",0x00FFC46B,3);text(p,30,380,"EACH PLAYER CONTROLS LEFT BOARD",0x009FB9CE,2);
#else
 text(p,30,350,"VERSUS PLAY NOT ENABLED",0x00FFC46B,3);text(p,30,380,"GAME INTEGRATION IN PROGRESS",0x009FB9CE,2);
#endif
 if(state!=TA_LAN_SELECT)text(p,30,412,"B BACK",0x009FB9CE,2);
#if defined(TA_VS_FIRST_LAUNCH) && TA_VS_FIRST_LAUNCH
 text(p,260,412,"START OFFLINE PLAY",0x00FFFFFF,2);
#else
 text(p,260,412,"Y OFFLINE PLAY",0x00FFFFFF,2);
#endif
}

void ta_vs_lan_text(uint32_t*p,int x,int y,const char*s,uint32_t c,int scale){text(p,x,y,s,c,scale);}

/* Word wrapping and spacing are bounded before any glyph pixels are written. */
#if defined(TA_VS_COMPACT_UI) && TA_VS_COMPACT_UI
static unsigned ta_box_lines(const char*s,int width,int glyph,int advance){
 unsigned lines=1;int x=0;const char*begin=s;
 for(;*s;s++){
  if(*s=='\n'){x=0;lines++;continue;}
  if(*s!=' '&&(s==begin||s[-1]==' '||s[-1]=='\n')){
   unsigned word=0;while(s[word]&&s[word]!=' '&&s[word]!='\n')word++;
   if(x&&x+(int)(word-1)*advance+glyph>width){x=0;lines++;}
  }
  if(!x&&*s==' ')continue;
  if(x+glyph>width){x=0;lines++;}
  x+=advance;
 }
 return lines;
}
#endif
void ta_vs_lan_text_box(uint32_t*p,int x,int y,int width,int height,const char*s,uint32_t color,int requested){
 if(!p||!s||requested<1||width<=0||height<=0||x<0||y<0||x>=512||y>=480)return;
 if(width>512-x)width=512-x;if(height>480-y)height=480-y;
#if defined(TA_VS_COMPACT_UI) && TA_VS_COMPACT_UI
 int scale=requested>=4?3:2,advance=6*scale;
 for(;;){
  int normal=6*scale,tight=5*scale+1;unsigned lines=ta_box_lines(s,width,5*scale,normal);
  if((int)(lines*8u*(unsigned)scale)-(int)scale<=height){advance=normal;break;}
  lines=ta_box_lines(s,width,5*scale,tight);
  if((int)(lines*8u*(unsigned)scale)-(int)scale<=height){advance=tight;break;}
  if(scale==1){advance=normal;break;}scale--;
 }
 int cx=0,cy=0;const char*begin=s;
 for(;*s;s++){
  if(*s=='\n'){cx=0;cy+=8*scale;continue;}
  if(*s!=' '&&(s==begin||s[-1]==' '||s[-1]=='\n')){
   unsigned word=0;while(s[word]&&s[word]!=' '&&s[word]!='\n')word++;
   if(cx&&cx+(int)(word-1)*advance+5*scale>width){cx=0;cy+=8*scale;}
  }
  if(!cx&&*s==' ')continue;
  if(cx+5*scale>width){cx=0;cy+=8*scale;}
  if(cy+7*scale>height)break;
  const uint8_t*g=font_rows(*s);
  for(int row=0;row<7;row++)for(int col=0;col<5;col++)if(g[row]&(1u<<(4-col)))
   for(int dy=0;dy<scale;dy++)for(int dx=0;dx<scale;dx++){
    int xx=cx+col*scale+dx,yy=cy+row*scale+dy;
    if(xx<width&&yy<height)p[(y+yy)*512+x+xx]=color;
   }
  cx+=advance;
 }
#else
 (void)width;(void)height;text(p,x,y,s,color,requested);
#endif
}
