#ifndef TA_VS_NETWORK_RECOVERY_H
#define TA_VS_NETWORK_RECOVERY_H
/* Retry only an unanswered static-address public connection, once. Never an
 * authenticated endpoint, an active match, a DHCP connection or ordinary LAN. */
static inline int ta_vs_network_recover(unsigned online,unsigned cookie,unsigned peer,unsigned attempted,unsigned isStatic){return online&&!cookie&&!peer&&!attempted&&isStatic;}
#endif
