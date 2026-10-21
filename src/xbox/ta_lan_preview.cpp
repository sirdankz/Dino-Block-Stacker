#include "ta_platform.h"
#include <winsockx.h>
extern "C" {
#include "ta_vs_lan.h"
#include "ta_vs_udp.h"
#include "ta_vs_lan_screen.h"
#include "ta_rom_sha256.h"
}
static TaVsUdp transport;
static TaVsLan lobby;
static uint32_t screen[512*480];
/* Native connection integration only. No game hook is enabled until the
 * shared-board correctness boundary passes. Neither controller packets nor
 * remote snapshots are ever written into SNES RAM by this frontend. */
extern "C" void TA_LanPreview_Run(const uint8_t*rom,unsigned bytes){
 unsigned state=TA_LAN_SELECT,prior=~0u,remoteButtons=0;uint32_t localIp=0;int error=0;uint16_t previous=0,lastSent=65535;unsigned long lastStatus=0,lastSnapshot=0;
 TA_Log("[TA N4 LAN PREVIEW] boot identity=N4_LAN_CONNECTION_PREVIEW xemu_console_lan=1 versus_enabled=0 profiling=off\n");
 for(;;){
  uint16_t pads[2];TA_Input_Poll(pads);uint16_t pressed=pads[0]&~previous;previous=pads[0];uint32_t now=GetTickCount();
  if(pressed&(1u<<9)){ta_vs_udp_close(&transport);TA_Log("[TA N4 LAN] offline selected; network closed; original R129 game path\n");return;}
  if(state!=TA_LAN_SELECT&&(pressed&(1u<<8))){ta_vs_udp_close(&transport);memset(&lobby,0,sizeof lobby);state=TA_LAN_SELECT;error=0;remoteButtons=0;localIp=0;lastSent=65535;}
  if(state==TA_LAN_SELECT&&(pressed&3)){
   unsigned role=(pressed&1)?0:1;error=ta_vs_udp_open(&transport,role?0:TA_VS_LAN_PORT);
   if(!error)error=ta_vs_udp_enable_broadcast(&transport);
   if(error)state=TA_LAN_ERROR;
   else {
    TaVsIdentity id;memset(&id,0,sizeof id);ta_rom_sha256(rom,bytes,id.rom);
    static const uint8_t rules[]="TA_NATIVE_VS_N4_LAN_CONNECTION_ONLY_NO_GAME_START";ta_rom_sha256(rules,sizeof rules-1,id.rules);
    uint64_t nonce=__builtin_readcyclecounter()^((uint64_t)now<<32);if(!nonce)nonce=1;id.seed=(uint32_t)nonce;
    error=ta_vs_lan_init(&lobby,&id,role,nonce,now);state=error?TA_LAN_ERROR:TA_LAN_ADDRESS;
   }
  }
  if(state!=TA_LAN_SELECT&&state!=TA_LAN_ERROR&&state!=TA_LAN_LOST){
   XNADDR address;DWORD flags=XNetGetTitleXnAddr(&address);
   if(flags&(XNET_GET_XNADDR_STATIC|XNET_GET_XNADDR_DHCP|XNET_GET_XNADDR_AUTO|XNET_GET_XNADDR_PPPOE)){
    localIp=address.ina.s_addr;
    for(unsigned k=0;k<8;k++){
     uint8_t packet[TA_VS_PACKET_BYTES];uint32_t from;uint16_t port;
     int n=ta_vs_udp_receive_any(&transport,packet,sizeof packet,&from,&port);if(!n)break;
     if(n<0){if(n==-WSAEMSGSIZE){lobby.rejected++;continue;}error=n;state=TA_LAN_ERROR;break;}
     int result=ta_vs_lan_receive(&lobby,now,from,port,packet,(size_t)n);if(result){lobby.rejected++;if(result==TA_VS_IDENTITY_MISMATCH){error=result;state=TA_LAN_ERROR;break;}}
    }
    if(state!=TA_LAN_ERROR){
     bool ready=ta_vs_lan_ready(&lobby)!=0;
     state=ready?TA_LAN_CONNECTED:lobby.pinned?TA_LAN_HANDSHAKE:lobby.role?TA_LAN_JOIN_WAIT:TA_LAN_HOST_WAIT;
     if(ready&&(pads[0]!=lastSent||(uint32_t)(now-lastSnapshot)>=250u)){uint8_t status[4]={1,(uint8_t)pads[0],(uint8_t)(pads[0]>>8),0};ta_vs_set_snapshot(&lobby.session,status,sizeof status);lastSent=pads[0];lastSnapshot=now;}
     if(ready){size_t n;uint32_t revision;const uint8_t*p=ta_vs_remote_snapshot(&lobby.session,&n,&revision);if(p&&n==4&&p[0]==1&&!p[3])remoteButtons=p[1]|((unsigned)p[2]<<8);}
     if(ta_vs_lan_timed_out(&lobby,now))state=TA_LAN_LOST;
     else for(unsigned k=0;k<4;k++){
      uint8_t packet[TA_VS_PACKET_BYTES];uint32_t to;uint16_t port;size_t n=ta_vs_lan_packet(&lobby,now,packet,sizeof packet,&to,&port);if(!n)break;
      int sent=ta_vs_udp_send_to(&transport,to,port,packet,n);if(sent<0){error=sent;state=TA_LAN_ERROR;break;}
     }
    }
   }
  }
  if(state!=prior){TA_Log("[TA N4 LAN] state=%u role=%u local_ip=%08lX peer_ip=%08lX port=%u rejected=%u error=%d versus_enabled=0\n",state,(unsigned)lobby.role,(unsigned long)localIp,(unsigned long)lobby.peerAddress,(unsigned)lobby.peerPort,lobby.rejected,error);prior=state;}
  if(state==TA_LAN_CONNECTED&&(uint32_t)(now-lastStatus)>=5000){TA_Log("[TA N4 LAN] alive sent=%lu received=%lu rejected=%u peer_buttons=%04X versus_enabled=0\n",(unsigned long)transport.sent,(unsigned long)transport.received,lobby.rejected,remoteButtons);lastStatus=now;}
  ta_vs_lan_render(screen,state,localIp,lobby.pinned?lobby.peerAddress:0,remoteButtons,error);TA_Video_Submit((const uint8_t*)screen);
 }
}
