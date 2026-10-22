#include "ta_vs_ladder_scroll.h"
#include <assert.h>
#include <stdio.h>
#include <limits.h>
static unsigned checks;
#define CHECK(x) do{assert(x);checks++;}while(0)
static void be32(uint8_t*p,unsigned n){p[0]=n>>24;p[1]=n>>16;p[2]=n>>8;p[3]=n;}
static unsigned page(uint8_t*b,unsigned offset,unsigned total){unsigned n=offset<total?(total-offset<8?total-offset:8):0;memset(b,0,357);b[0]=offset>>8;b[1]=offset;b[2]=total>>8;b[3]=total;b[4]=n;for(unsigned i=0;i<n;i++){uint8_t*p=b+5+i*44;snprintf((char*)p,16,"USER_%u",offset+i);be32(p+16,1000+offset+i);be32(p+40,offset+i+1);}return 5+n*44;}
static void fill(TaLadderScroll*s){uint8_t b[357];for(unsigned i=0;i<3;i++){int m=ta_ladder_missing(s);if(m<0)return;unsigned n=page(b,(unsigned)m,s->total);CHECK(ta_ladder_accept(s,(unsigned)m,b,n));}CHECK(ta_ladder_missing(s)<0);}
int main(void){TaLadderScroll s;uint8_t b[357];ta_ladder_reset(&s);CHECK(ta_ladder_missing(&s)==0);CHECK(!ta_ladder_move(&s,1));unsigned n=page(b,0,100);CHECK(ta_ladder_accept(&s,0,b,n));CHECK(ta_ladder_missing(&s)<0);
 CHECK(ta_ladder_move(&s,1)&&s.first==1);CHECK(ta_ladder_missing(&s)<0);CHECK(ta_ladder_move(&s,1)&&s.first==2);CHECK(ta_ladder_missing(&s)<0);CHECK(ta_ladder_move(&s,1)&&s.first==3);CHECK(ta_ladder_missing(&s)==8);fill(&s);CHECK(ta_ladder_row(&s,8)->rank==9);
 for(int i=0;i<250;i++){int d=i%2? -3:5;ta_ladder_move(&s,d);fill(&s);for(unsigned j=0;j<TA_LADDER_VISIBLE;j++){const TaAccountProfile*p=ta_ladder_row(&s,s.first+j);CHECK(p&&p->rank==s.first+j+1);}}
 CHECK(ta_ladder_move(&s,INT_MAX)&&s.first==100-TA_LADDER_VISIBLE);fill(&s);CHECK(ta_ladder_row(&s,99)->rank==100);CHECK(!ta_ladder_move(&s,INT_MAX));CHECK(ta_ladder_move(&s,INT_MIN)&&s.first==0);fill(&s);
 TaLadderScroll before=s;n=page(b,0,100);CHECK(!ta_ladder_accept(&s,0,b,n-1));b[4]=9;CHECK(!ta_ladder_accept(&s,0,b,n));page(b,0,100);b[4]=7;CHECK(!ta_ladder_accept(&s,0,b,5+7*44));n=page(b,8,100);CHECK(!ta_ladder_accept(&s,0,b,n));CHECK(memcmp(&s,&before,sizeof s)==0);
 ta_ladder_move(&s,64);n=page(b,64,20);CHECK(ta_ladder_accept(&s,64,b,n));CHECK(s.total==20&&s.first==20-TA_LADDER_VISIBLE);fill(&s);CHECK(ta_ladder_row(&s,19)->rank==20);
 n=page(b,0,100);CHECK(ta_ladder_accept(&s,0,b,n));ta_ladder_move(&s,64);n=page(b,16,20);CHECK(ta_ladder_accept(&s,64,b,n));CHECK(s.first==20-TA_LADDER_VISIBLE);fill(&s);
 n=page(b,0,0);CHECK(ta_ladder_accept(&s,0,b,n));CHECK(s.first==0&&s.total==0&&ta_ladder_missing(&s)<0);
 n=page(b,0,65535);CHECK(ta_ladder_accept(&s,0,b,n));ta_ladder_move(&s,INT_MAX);CHECK(s.first==65535-TA_LADDER_VISIBLE);fill(&s);CHECK(ta_ladder_row(&s,65534)->rank==65535);ta_ladder_refresh(&s);CHECK(ta_ladder_missing(&s)==65528);fill(&s);
 puts("Leaderboard cache: continuous boundaries, 250 back/forward moves, cache eviction, first/last, count changes, both existing endpoint responses, malformed packets, empty and 65535-entry lists passed.");printf("Assertions: %u\n",checks);return 0;}