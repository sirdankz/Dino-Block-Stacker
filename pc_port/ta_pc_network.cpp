#include "ta_pc_internal.h"
#include <iphlpapi.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <chrono>

static std::atomic<uint32_t> localAddress{0};
static std::thread addressWorker;
static std::mutex addressMutex;
static std::condition_variable addressWake;
static bool addressStopping=true;

static uint32_t queryLocalAddress(){
 // Route and adapter discovery can wait inside Windows or a VPN provider.
 // Only this worker performs those calls; frame/input/presence reads are cached.
 uint32_t address=0;SOCKET s=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
 if(s!=INVALID_SOCKET){sockaddr_in dst={};dst.sin_family=AF_INET;dst.sin_port=htons(34987);InetPtonA(AF_INET,"172.233.145.244",&dst.sin_addr);
  if(connect(s,(sockaddr*)&dst,sizeof dst)==0){sockaddr_in local={};int len=sizeof local;if(getsockname(s,(sockaddr*)&local,&len)==0)address=local.sin_addr.s_addr;}closesocket(s);
 }
 if(address)return address;
 ULONG size=16384;std::vector<unsigned char> bytes(size);
 ULONG status=GetAdaptersAddresses(AF_INET,GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_MULTICAST|GAA_FLAG_SKIP_DNS_SERVER,NULL,(IP_ADAPTER_ADDRESSES*)bytes.data(),&size);
 if(status==ERROR_BUFFER_OVERFLOW&&size<=1024u*1024u){bytes.resize(size);status=GetAdaptersAddresses(AF_INET,GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_MULTICAST|GAA_FLAG_SKIP_DNS_SERVER,NULL,(IP_ADAPTER_ADDRESSES*)bytes.data(),&size);}
 if(status==NO_ERROR)for(auto a=(IP_ADAPTER_ADDRESSES*)bytes.data();a&&!address;a=a->Next)if(a->OperStatus==IfOperStatusUp&&a->IfType!=IF_TYPE_SOFTWARE_LOOPBACK)for(auto u=a->FirstUnicastAddress;u;u=u->Next)if(u->Address.lpSockaddr&&u->Address.lpSockaddr->sa_family==AF_INET){auto ip=((sockaddr_in*)u->Address.lpSockaddr)->sin_addr.s_addr;if(ip&&((ip&255)!=127)){address=ip;break;}}
 return address;
}
void pcNetworkShutdown(){
 {std::lock_guard<std::mutex> lock(addressMutex);addressStopping=true;}
 addressWake.notify_all();if(addressWorker.joinable())addressWorker.join();localAddress.store(0,std::memory_order_release);
}
void pcNetworkInit(){
 pcNetworkShutdown();{std::lock_guard<std::mutex> lock(addressMutex);addressStopping=false;}
 addressWorker=std::thread([]{
  std::unique_lock<std::mutex> lock(addressMutex);
  while(!addressStopping){
   lock.unlock();uint32_t address=0;try{address=queryLocalAddress();}catch(...){/* Retry after temporary OS/allocation failure. */}
   localAddress.store(address,std::memory_order_release);lock.lock();
   addressWake.wait_for(lock,std::chrono::seconds(3),[]{return addressStopping;});
  }
 });
}
extern "C" DWORD XNetGetTitleXnAddr(XNADDR*out){
 if(!out)return 0;uint32_t address=localAddress.load(std::memory_order_acquire);memset(out,0,sizeof *out);out->ina.s_addr=address;return address?XNET_GET_XNADDR_STATIC:0;
}
