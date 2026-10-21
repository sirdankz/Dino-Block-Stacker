/* R77: register all documented R73 sprite continuation entries. */
#ifndef TA_R77_DISPATCH_H
#define TA_R77_DISPATCH_H
#include "ta_r73_chain.h"
#ifndef TA_R77_SPRITE_ENTRY_FIX
#define TA_R77_SPRITE_ENTRY_FIX 0
#endif
#define TA_R77_ENTRY_FIX_ON (TA_R73_CHAIN_ON && TA_R77_SPRITE_ENTRY_FIX)
#endif
