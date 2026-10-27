"""Generate Windows adapters from the same public frontend as the console ports."""
from pathlib import Path
import hashlib,json,re
ROOT=Path(__file__).resolve().parents[1]; PORT=ROOT/'pc_port'; OUT=PORT/'generated'
def write(p,s):
 p.parent.mkdir(parents=True,exist_ok=True)
 if not p.exists() or p.read_text(encoding='utf-8')!=s:p.write_text(s,encoding='utf-8',newline='\n')
def replace_function(s,name,body):
 start=s.index('static bool '+name+'(');brace=s.index('{',start);depth=1;i=brace+1
 while depth:
  if s[i]=='{':depth+=1
  elif s[i]=='}':depth-=1
  i+=1
 return s[:start]+body+s[i:]
identity=json.loads((ROOT/'release_identity.json').read_text())
key=(ROOT/'config/server-public-key.hex').read_text(encoding='utf-8-sig').strip()
if not re.fullmatch('[0-9a-fA-F]{64}',key) or not any(bytes.fromhex(key)):raise ValueError('Invalid public server pin')
write(OUT/'ta_pc_assets.h','#pragma once\n#define TA_N17_SERVER '+json.dumps(identity['server'])+'\n#define TA_N17_PORT '+str(identity['server_udp_port'])+'u\nstatic const unsigned char ta_n17_account_key[]={'+','.join(str(b) for b in bytes.fromhex(key))+'};\nstatic const unsigned char ta_n17_start_cache[]={0};\n')
records=[]
for file in sorted((ROOT/'src/xbox').glob('*.h')):
 s=file.read_text(encoding='utf-8')
 if file.name in {'ta_menu_touch.h','ta_n22_accounts.h','ta_n22_social.h','ta_n22_presence.h'}:s=s.replace('TA_ANDROID','TA_PC')
 if file.name=='ta_n22_accounts.h':
  s=replace_function(s,'accountKeyboard',r"""static int pcAccountKeyboardTick(void){return pump(false);}
static bool accountKeyboard(const char*title,const char*label,char*out,unsigned maximum,bool password,unsigned minimum=4){
 return TA_PC_TextInput(out,maximum+1,title,label,password,minimum,pcAccountKeyboardTick)!=0;
}""")
  s=s.replace('"R1/TARGET","AUX"','"RB/TARGET","LB"').replace('"WHITE","BLACK"','"RB/TARGET","LB"')
  s=s.replace('SWIPE OR D-PAD TO SCROLL','MOUSE WHEEL / D-PAD TO SCROLL').replace('REMOVED FROM THIS XBOX ONLY','REMOVED FROM THIS PC ONLY')
 if file.name=='ta_n22_social.h':s=s.replace('ta_android_chat_input','ta_pc_chat_input')
 # Route configuration is carried by the public release identity, not disc paths.
 s=re.sub(r'FILE\s*\*\s*f=fopen\("D:\\\\ta-(?:party-)?server\.txt","rb"\);','FILE*f=NULL;',s)
 write(OUT/'shared/src/xbox'/file.name,s)
 records.append({'source':file.relative_to(ROOT).as_posix(),'sha256':hashlib.sha256(file.read_bytes()).hexdigest()})
file=ROOT/'src/xbox/ta_lan_game.cpp';s=file.read_text(encoding='utf-8')
s=s.replace('#include "ta_platform.h"','#include "ta_pc_platform.h"').replace('#include <winsockx.h>','').replace('../../out/N17Assets/ta_n17_assets.h','ta_pc_assets.h')
s=s.replace('FILE*f=fopen("D:\\\\ta-server.txt","rb");','FILE*f=NULL;')
s+='\nextern "C" void TA_PC_FrontendClose(void){partyClose();releaseGame();freeMenuCache();ta_vs_local_bind(NULL);leavePublic();ta_vs_udp_close(&transport);publicMode=false;}\n'
s+='extern "C" void TA_PC_ShutdownFrontend(void){TA_PC_FrontendClose();taPresenceEnabled=false;TA_Background_Set(NULL);taPresenceDrop(GetTickCount(),0);}\n'
write(OUT/'shared/src/xbox/ta_pc_frontend.cpp',s)
remove={'XBOX','_XBOX','ZAMN_R23_MMX_PPU','ZAMN_R31_MMX_TRANSPARENT_EARLYOUT','TA_LOG_INTERVAL_FRAMES'}
zero={'TA_R37_STREAM_UPLOAD','TA_R102_MODE2_DUAL_PUT','TA_R124_MODE2_STREAM','TA_R126_DIRECT_TARGET','TA_R126_GPU_SUBTRACT','TA_R127_GPU_INDEXED','TA_R128_GPU_ATLAS_CACHE','TA_R129_GPU_DIRECT_PALETTE'}
defs=[d for d in json.loads((ROOT/'rxdk.project.json').read_text())['configurations']['PlayN22PublicNoLog']['defines'] if d.split('=')[0] not in remove|zero]
defs += [d+'=0' for d in sorted(zero)]+['TA_PC=1','TA_VS_HOST=1','TA_NO_LOG=1','WIN32_LEAN_AND_MEAN','NOMINMAX','_CRT_SECURE_NO_WARNINGS','_WINSOCK_DEPRECATED_NO_WARNINGS','_WIN32_WINNT=0x0A00','SDL_MAIN_HANDLED=1']
defs=list(dict.fromkeys(defs))
write(OUT/'pc_defines.cmake','set(TA_PC_DEFINES\n'+''.join(' "'+d+'"\n' for d in defs)+')\n')
write(OUT/'defines.json',json.dumps(defs,indent=2)+'\n')
write(OUT/'port_manifest.json',json.dumps({'version':identity['version'],'target':'Windows x64','frontend_sha256':hashlib.sha256(file.read_bytes()).hexdigest(),'shared_headers':records,'protocol_changes':'additive encrypted account command 17','rom_embedded':False,'build_status':'not_built','features':['shared menus, animated artwork and trophies','offline game and two local players','LAN 1v1','online ranked 1v1','online 2-4 player Auto Target and Manual Target','real opponent boards and local scene backdrop','profiles, both leaderboards and six egg ranks','localized Events, recurring days and interested players','profile timezones, Discord and weekly availability','world and lobby chat','rematches, speed requests and host transfer','population counters','custom controls for two players','keyboard, mouse and two gamepads','desktop text entry and gamepad keyboard']},indent=2)+'\n')
print('Prepared Windows frontend and shared build definitions.')
