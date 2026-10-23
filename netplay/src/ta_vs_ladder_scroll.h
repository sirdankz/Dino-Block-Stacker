#ifndef TA_VS_LADDER_SCROLL_H
#define TA_VS_LADDER_SCROLL_H
#include "ta_vs_account.h"
#include <string.h>
#define TA_LADDER_VISIBLE 6u
#define TA_LADDER_ROW_HEIGHT 34u
#define TA_LADDER_PAGE 8u
typedef struct{unsigned valid,offset,count;TaAccountProfile rows[8];}TaLadderPage;
typedef struct{unsigned first,total,known;TaLadderPage pages[3];}TaLadderScroll;
static inline void ta_ladder_reset(TaLadderScroll*s){memset(s,0,sizeof *s);}
static inline unsigned ta_ladder_max_first(const TaLadderScroll*s){return s->total>TA_LADDER_VISIBLE?s->total-TA_LADDER_VISIBLE:0;}
static inline void ta_ladder_clamp(TaLadderScroll*s){unsigned max=ta_ladder_max_first(s);if(s->first>max)s->first=max;}
static inline int ta_ladder_move(TaLadderScroll*s,int delta){unsigned old=s->first;long long next=(long long)s->first+delta,max=ta_ladder_max_first(s);s->first=next<0?0:next>max?(unsigned)max:(unsigned)next;return s->first!=old;}
static inline const TaAccountProfile*ta_ladder_row(const TaLadderScroll*s,unsigned index){unsigned page=(index/8u)*8u;const TaLadderPage*p=&s->pages[(page/8u)%3u];return p->valid&&p->offset==page&&index-page<p->count?&p->rows[index-page]:NULL;}
static inline int ta_ladder_missing(const TaLadderScroll*s){if(!s->known)return 0;unsigned end=s->first+TA_LADDER_VISIBLE;if(end>s->total)end=s->total;for(unsigned i=s->first;i<end;i++)if(!ta_ladder_row(s,i))return (int)((i/8u)*8u);return -1;}
static inline int ta_ladder_accept(TaLadderScroll*s,unsigned requested,const uint8_t*b,unsigned n){
 if(!b||n<5||b[4]>8||n!=5u+b[4]*44u)return 0;
 unsigned offset=((unsigned)b[0]<<8)|b[1],total=((unsigned)b[2]<<8)|b[3],count=b[4];
 if(offset>65528u||offset%8u||(!count&&offset<total)||count>total||((offset<total)&&count>total-offset))return 0;
 unsigned expected=offset<total?(total-offset<8u?total-offset:8u):0u;if(count!=expected)return 0;
 if(offset!=requested&&!(requested>=total&&offset==(total?((total-1)/8)*8:0)))return 0;
 TaLadderPage page;memset(&page,0,sizeof page);page.valid=1;page.offset=offset;page.count=count;
 for(unsigned i=0;i<count;i++)if(ta_account_profile(&page.rows[i],b+5+i*44,44))return 0;
 if(s->known&&s->total!=total)memset(s->pages,0,sizeof s->pages);
 s->known=1;s->total=total;ta_ladder_clamp(s);s->pages[(offset/8u)%3u]=page;return 1;
}
static inline void ta_ladder_refresh(TaLadderScroll*s){memset(s->pages,0,sizeof s->pages);}
#endif
