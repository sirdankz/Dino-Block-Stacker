#ifndef TA_VS_RANK_H
#define TA_VS_RANK_H
/* Six competitive divisions centered around the existing 1000-point starting
   rating. Rating awards, losses, standings and historical records are unchanged. */
static unsigned ta_rank_floor(unsigned tier){static const unsigned limits[]={0,800,950,1100,1250,1400};return limits[tier<6?tier:5];}
static unsigned ta_rank_tier(unsigned rating){unsigned tier=0;while(tier<5&&rating>=ta_rank_floor(tier+1))++tier;return tier;}
static const char*ta_rank_name(unsigned rating){static const char*const names[]={"ROTTEN EGG","BRONZE EGG","SILVER EGG","GOLD EGG","PLATINUM EGG","MASTER EGG"};return names[ta_rank_tier(rating)];}
static unsigned ta_rank_next(unsigned rating){unsigned tier=ta_rank_tier(rating);return tier<5?ta_rank_floor(tier+1):0;}
static unsigned ta_rank_progress(unsigned rating){unsigned tier=ta_rank_tier(rating);return tier<5?(rating-ta_rank_floor(tier))*100/(ta_rank_floor(tier+1)-ta_rank_floor(tier)):100;}
#endif
