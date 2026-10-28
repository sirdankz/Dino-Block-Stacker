from pathlib import Path
import hashlib,json,re,sys,subprocess
r=Path(__file__).resolve().parents[2];out=r/'android_port/generated';out.mkdir(parents=True,exist_ok=True)
subprocess.run([sys.executable,str(r/'tools/prepare_music.py'),'--out',str(r/'android_port/app/src/main/assets/music')],check=True)
key=Path(sys.argv[1]).read_text(encoding='utf8').strip()
if not re.fullmatch('[0-9a-fA-F]{64}',key) or not any(bytes.fromhex(key)):raise ValueError('Expected your server PUBLIC key, 64 hex characters')
identity=json.loads((r/'release_identity.json').read_text());ip=identity['server'];port=identity['server_udp_port']
if len(ip.split('.'))!=4 or any(not s.isdigit() or not 0<=int(s)<=255 for s in ip.split('.')) or not 1<=port<=65535:raise ValueError('Bad room endpoint')
header='#pragma once\n#define TA_N17_SERVER '+json.dumps(ip)+'\n#define TA_N17_PORT '+str(port)+'u\nstatic const unsigned char ta_n17_account_key[]={'+','.join(str(x) for x in bytes.fromhex(key))+'};\n/* ROM-derived data is generated on the phone after ROM import. */\nstatic const unsigned char ta_n17_start_cache[]={0};\n'
(out/'ta_android_assets.h').write_text(header,encoding='ascii',newline='\n')
s=(r/'src/xbox/ta_lan_game.cpp').read_text()
def replace(old,new):
 global s
 assert s.count(old)==1,(old,s.count(old));s=s.replace(old,new)
replace('#include "ta_platform.h"','#include "ta_android_platform.h"')
replace('#include <winsockx.h>','')
assert s.count('#include "../../out/N17Assets/ta_n17_assets.h"') in (1,2)
s=s.replace('#include "../../out/N17Assets/ta_n17_assets.h"','#include "ta_android_assets.h"')
# No D: files on Android; retain embedded endpoint/pin. Save files use app-private storage.
replace('FILE*f=fopen("D:\\\\ta-server.txt","rb");','FILE*f=NULL;')
s+='\nextern "C" void ta_android_frontend_close(void){TA_Background_Set(NULL);taPresenceEnabled=false;taPresenceDrop(GetTickCount(),0);partyClose();releaseGame();freeMenuCache();ta_vs_local_bind(NULL);if(publicMode&&transport.opened&&publicRoom.haveCookie){ta_vs_public_begin_leave(&publicRoom);uint8_t packet[TA_PUB_BYTES];uint32_t to;uint16_t port;size_t n=ta_vs_public_packet(&publicRoom,GetTickCount(),packet,sizeof packet,&to,&port);if(n)ta_vs_udp_send_to(&transport,to,port,packet,n);}ta_vs_udp_close(&transport);publicMode=false;}\n'
(out/'ta_android_frontend.cpp').write_text(s,encoding='utf8',newline='\n')
# Keep native CPU, APU rational timing, guest guards and network definitions.
# Select existing portable CPU-renderer fallbacks for x86-only MMX/NV2A output.
c=json.loads((r/'rxdk.project.json').read_text())['configurations']['PlayN22PublicLogged']
remove={'XBOX','_XBOX','TA_R130_PALETTE_DIRTY','TA_R130_MATH_MASK','TA_R130_MODE1_INDEXED','NDEBUG','ZAMN_R23_MMX_PPU','ZAMN_R31_MMX_TRANSPARENT_EARLYOUT','TA_R37_STREAM_UPLOAD','TA_R102_MODE2_DUAL_PUT','TA_R124_MODE2_STREAM','TA_R126_DIRECT_TARGET','TA_R126_GPU_SUBTRACT','TA_R127_GPU_INDEXED','TA_R128_GPU_ATLAS_CACHE','TA_R129_GPU_DIRECT_PALETTE','TA_LOG_INTERVAL_FRAMES'}
defs=[d for d in c['defines'] if d.split('=')[0] not in remove]+[n+'=0' for n in sorted(remove) if n not in {'XBOX','_XBOX','NDEBUG','ZAMN_R23_MMX_PPU','ZAMN_R31_MMX_TRANSPARENT_EARLYOUT','TA_LOG_INTERVAL_FRAMES'}]+['TA_ANDROID=1']
(out/'android_defines.cmake').write_text('set(TA_ANDROID_DEFINES\n'+''.join('  "'+d+'"\n' for d in defs)+')\n',encoding='ascii',newline='\n')
(out/'android_defines.json').write_text(json.dumps(defs,indent=2)+'\n')
(out/'adapter_manifest.json').write_text(json.dumps({'N17_frontend_SHA256':hashlib.sha256((r/'src/xbox/ta_lan_game.cpp').read_bytes()).hexdigest(),'same_N17_menu_headers':True,'ROM_in_APK':False,'public_endpoint':[ip,port],'public_pin_SHA256':hashlib.sha256(bytes.fromhex(key)).hexdigest(),'Android_architecture_fallbacks':sorted(remove),'guest_CPU_APU_timing_and_native_rules_unchanged':True},indent=2)+'\n')
print('Dino Block Stacker shared N22 frontend prepared; no game ROM bundled.')
