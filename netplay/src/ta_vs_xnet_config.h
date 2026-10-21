#ifndef TA_VS_XNET_CONFIG_H
#define TA_VS_XNET_CONFIG_H
#include <stddef.h>
/* RXDK libxnet private configuration ABI, 492 bytes. Configure only a fresh stack.
 * No XNetSaveConfigParams call: IP/mask/gateway choices are title-session only.
 * Provenance: Team-Resurgent/RXDK-Libs libs/libxnet/inc/winsockp.h. */
#pragma pack(push,1)
typedef struct {
 unsigned char seed[20];IN_ADDR ip,mask,gateway,dnsPrimary,dnsSecondary;
 char dhcpHost[64],pppUser[64],pppPassword[64],pppServer[64];
 unsigned char reserved[192];DWORD signature;
}TaVsXnetConfig;
#pragma pack(pop)
typedef char TaVsXnetConfigSize[(sizeof(TaVsXnetConfig)==492)?1:-1];
typedef char TaVsXnetConfigIpOffset[(offsetof(TaVsXnetConfig,ip)==20)?1:-1];
typedef char TaVsXnetConfigSignatureOffset[(offsetof(TaVsXnetConfig,signature)==488)?1:-1];
INT WSAAPI XNetConfig(const TaVsXnetConfig*,DWORD);
#endif
