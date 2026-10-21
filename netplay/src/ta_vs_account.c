/* N14: menu/result-only authenticated account channel. Monocypher 4.0.2.
   Pinned X25519 server key; fresh OS-random client key per connection.
   BLAKE2b-256 transcript KDF; IETF ChaCha20-Poly1305, directional counters.
   Retries resend identical ciphertext; a sequence never encrypts new plaintext. */
#include "ta_vs_account.h"
#include "../../third_party/monocypher/monocypher.h"
#include <string.h>
static uint32_t r32(const uint8_t*p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static void w64(uint8_t*p,uint64_t x){for(unsigned i=0;i<8;i++)p[7-i]=(uint8_t)(x>>(8*i));}
int ta_account_key(uint8_t out[32],const uint8_t secret[32],const uint8_t peer[32],const uint8_t client[32],const uint8_t server[32]){
 uint8_t input[116],shared[32],zero[32]={0};crypto_x25519(shared,secret,peer);
 if(!crypto_verify32(shared,zero))return -1;
 memcpy(input,"TA-N14-ACCOUNT-KEY-v1",20);memcpy(input+20,shared,32);memcpy(input+52,client,32);memcpy(input+84,server,32);
 crypto_blake2b(out,32,input,sizeof input);crypto_wipe(shared,sizeof shared);crypto_wipe(input,sizeof input);return 0;
}
static void ctx(crypto_aead_ctx*c,const uint8_t*k,uint64_t seq,unsigned server){uint8_t n[12];memcpy(n,server?"TAS1":"TAC1",4);w64(n+4,seq);crypto_aead_init_ietf(c,k,n);}
void ta_account_seal(uint8_t*out,uint8_t tag[16],const uint8_t key[32],uint64_t seq,unsigned server,const uint8_t*ad,unsigned an,const uint8_t*in,unsigned n){crypto_aead_ctx c;ctx(&c,key,seq,server);crypto_aead_write(&c,out,tag,ad,an,in,n);crypto_wipe(&c,sizeof c);}
int ta_account_open(uint8_t*out,const uint8_t tag[16],const uint8_t key[32],uint64_t seq,unsigned server,const uint8_t*ad,unsigned an,const uint8_t*in,unsigned n){crypto_aead_ctx c;ctx(&c,key,seq,server);int r=crypto_aead_read(&c,out,tag,ad,an,in,n);crypto_wipe(&c,sizeof c);return r;}
int ta_account_init(TaAccountLink*a,const uint8_t secret[32],const uint8_t server[32]){
 memset(a,0,sizeof *a);crypto_x25519_public_key(a->publicKey,secret);memcpy(a->serverKey,server,32);
 if(ta_account_key(a->key,secret,server,a->publicKey,server))return -1;a->ready=1;return 0;
}
int ta_account_request(TaAccountLink*a,const TaVsPublic*p,unsigned cmd,const void*data,unsigned n){
 if(!a||!a->ready||a->pending||!p->haveCookie||n>TA_ACCOUNT_PACKET-89||!cmd||a->sequence==UINT64_MAX)return -1;
 uint8_t*o=a->tx,plain[TA_ACCOUNT_PACKET];unsigned bytes=57+n;memcpy(o,"TAP9",4);o[4]=1;o[5]=16;o[6]=(uint8_t)(bytes>>8);o[7]=(uint8_t)bytes;w64(o+8,p->nonce);memcpy(o+16,p->cookie,16);memcpy(o+32,a->publicKey,32);w64(o+64,++a->sequence);
 plain[0]=(uint8_t)cmd;if(n)memcpy(plain+1,data,n);ta_account_seal(o+88,o+72,a->key,a->sequence,0,o,72,plain,n+1);crypto_wipe(plain,sizeof plain);
 a->txBytes=89+n;a->replyBytes=0;a->pending=1;a->sent=0;return 0;
}
size_t ta_account_packet(TaAccountLink*a,uint32_t now,const uint8_t**out){if(!a->pending||a->sent&&(uint32_t)(now-a->lastSend)<400)return 0;*out=a->tx;a->sent=1;a->lastSend=now;return a->txBytes;}
int ta_account_receive(TaAccountLink*a,const TaVsPublic*p,const uint8_t*b,size_t n){
 uint8_t seq[8];w64(seq,a->sequence);
 if(!a->pending||n<90||n>TA_ACCOUNT_PACKET||memcmp(b,"TAP9",4)||b[4]!=1||b[5]!=16||(((unsigned)b[6]<<8)|b[7])!=n-32||memcmp(b+8,a->tx+8,24)||memcmp(b+32,a->publicKey,32)||memcmp(b+64,seq,8))return -1;
 if(ta_account_open(a->reply,b+72,a->key,a->sequence,1,b,72,b+88,(unsigned)n-88))return -1;
 a->replyBytes=(unsigned)n-88;a->pending=0;crypto_wipe(a->tx,sizeof a->tx);(void)p;return 0;
}
int ta_account_profile(TaAccountProfile*p,const uint8_t*b,size_t n){
 if(!p||!b||n<TA_ACCOUNT_PROFILE_BYTES)return -1;memset(p,0,sizeof *p);memcpy(p->name,b,16);
 unsigned i;for(i=0;i<16&&p->name[i];i++)if(!((p->name[i]>='A'&&p->name[i]<='Z')||(p->name[i]>='0'&&p->name[i]<='9')||p->name[i]=='-'||p->name[i]=='_'))return -1;
 if(i<3||i==16)return -1;p->rating=r32(b+16);p->wins=r32(b+20);p->losses=r32(b+24);p->draws=r32(b+28);p->streak=(int32_t)r32(b+32);p->best=r32(b+36);p->rank=r32(b+40);return 0;
}
const char*ta_account_tier(uint32_t r){return r<1000?"BRONZE":r<1200?"SILVER":r<1400?"GOLD":r<1600?"PLATINUM":r<1800?"DIAMOND":"MASTER";}
