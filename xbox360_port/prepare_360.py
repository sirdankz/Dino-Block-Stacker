"""Generate a PowerPC build tree from the shared public client sources."""
from pathlib import Path
import hashlib,json,re,sys,xml.etree.ElementTree as E
ROOT=Path(__file__).resolve().parents[1]
PORT=ROOT/'xbox360_port';OUT=PORT/'generated';TREE=OUT/'shared'
C_GROUPS={
 'third_party/lakesnes/snes':None,
 'src/native':['ta_rom_sha256','ta_profile','ta_r82_cost','ta_native'],
 'third_party/monocypher':['monocypher'],
 'netplay/src':['ta_vs_party','ta_vs_local_game','ta_vs_session','ta_vs_attack','ta_vs_board_rng','ta_vs_board_rows','ta_vs_picture','ta_vs_lan','ta_vs_lan_screen','ta_vs_public','ta_vs_match','ta_vs_round','ta_vs_preview','ta_vs_start','ta_vs_account']}
def write(path,s):
 path.parent.mkdir(parents=True,exist_ok=True)
 if path.is_file() and path.read_text(encoding='utf-8')==s:return
 path.write_text(s,encoding='utf-8',newline='\n')
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def cpp_c(s):
 # Xbox 360 XNet reports STATIC, DHCP and PPPOE. Original Xbox AUTO
 # has no 360 SDK equivalent and must not be treated as a ready flag.
 s=re.sub(r'\|\s*XNET_GET_XNADDR_AUTO\b','',s)
 # XDK's v16 compiler supports C++98, including declarations in for loops.
 # C pointer initializers need explicit conversions outside allocation calls.
 pattern=r'\b((?:const\s+)?(?:struct\s+)?[A-Za-z_]\w*\s*\*+)\s*([A-Za-z_]\w*)\s*=\s*([A-Za-z_]\w*(?:->\w+|\.\w+)*)\s*;'
 s=re.sub(pattern,lambda m:m[1]+' '+m[2]+'=('+m[1]+')'+m[3]+';',s)
 # Preserve alignment requirements instead of dropping GNU alignment attributes.
 s=re.sub(r'(?m)^(\s*)((?:static\s+)?(?:const\s+)?[^;\n]+?)\s+__attribute__\(\(aligned\((\d+)\)\)\)',r'\1__declspec(align(\3)) \2',s)
 return s
identity=json.loads((ROOT/'release_identity.json').read_text())
keyPath=Path(sys.argv[1]) if len(sys.argv)>1 else ROOT/'config/server-public-key.hex'
key=keyPath.read_text(encoding='utf-8-sig').strip()
if not re.fullmatch('[0-9a-fA-F]{64}',key) or not any(bytes.fromhex(key)):raise ValueError('Expected nonzero 32-byte server PUBLIC key')
assets='#pragma once\n#define TA_N17_SERVER '+json.dumps(identity['server'])+'\n#define TA_N17_PORT '+str(identity['server_udp_port'])+'u\nstatic const unsigned char ta_n17_account_key[]={'+','.join(str(v) for v in bytes.fromhex(key))+'};\nstatic const unsigned char ta_n17_start_cache[]={0};\n'
write(OUT/'ta_360_assets.h',assets)
compiled=[];manifest=[]
for group,names in C_GROUPS.items():
 directory=ROOT/group
 for file in sorted(directory.glob('*')):
  if file.suffix!='.h' and not(file.suffix=='.c' and (names is None or file.stem in names)):continue
  s=file.read_text(encoding='utf-8');original=s
  if file.name=='ta_bus_end_limit.h':s=s.replace('#define TA_R44_BUS_TABLE_LINKAGE\n','#define TA_R44_BUS_TABLE_LINKAGE extern\n')
  if file.name=='ta_vs_start.c':s=s.replace('TAN11C01','TAN36C01')
  s=cpp_c(s)
  dest=TREE/group/file.name
  if file.suffix=='.c':
   dest=dest.with_suffix('.cpp');s='#define TA_360_C_TRANSLATION 1\n#include "ta_360_compat.h"\nextern "C" {\n'+s+'\n}\n';compiled.append(dest)
  write(dest,s);manifest.append({'source':file.relative_to(ROOT).as_posix(),'sha256':digest(file),'generated':dest.relative_to(OUT).as_posix(),'adapted':s!=original})
# Headers are copied from the same source revision as Android/Original Xbox.
for file in sorted((ROOT/'src/xbox').glob('*.h')):
 s=file.read_text(encoding='utf-8')
 s=s.replace('"D:\\\\ta-server.txt"','"game:\\\\ta-server.txt"').replace('"D:\\\\ta-party-server.txt"','"game:\\\\ta-party-server.txt"')
 s=s.replace('const uint8_t*ip=(const uint8_t*)&localIp;','const uint8_t*ip=TA_360_IPBytes(localIp);')
 if file.name=='ta_n22_accounts.h':
  s=s.replace('#if defined(TA_ANDROID)\n int source=','#if defined(TA_ANDROID) || defined(TA_XBOX360)\n int source=')
  s=s.replace('#if !defined(TA_ANDROID)\n if(!source)','#if !defined(TA_ANDROID) && !defined(TA_XBOX360)\n if(!source)')
  s=s.replace(' "WHITE","BLACK"};',' "RB","LB"};')
 s=cpp_c(s);write(TREE/'src/xbox'/file.name,s);manifest.append({'source':file.relative_to(ROOT).as_posix(),'sha256':digest(file),'generated':(TREE/'src/xbox'/file.name).relative_to(OUT).as_posix()})
frontend=ROOT/'src/xbox/ta_lan_game.cpp';s=frontend.read_text(encoding='utf-8')
assert s.count('#include "ta_platform.h"')==1
s=s.replace('#include "ta_platform.h"','#include "ta_360_platform.h"').replace('#include "../../out/N17Assets/ta_n17_assets.h"','#include "ta_360_assets.h"')
s=s.replace('const uint8_t*ip=(const uint8_t*)&publicRoom.localAddress;','const uint8_t*ip=TA_360_IPBytes(publicRoom.localAddress);')
s=s.replace('"D:\\\\ta-server.txt"','"game:\\\\ta-server.txt"')
s+='\nextern "C" void TA_360_FrontendClose(void){partyClose();releaseGame();freeMenuCache();ta_vs_local_bind(NULL);leavePublic();ta_vs_udp_close(&transport);publicMode=false;}\n'
frontOut=TREE/'src/xbox/ta_360_frontend.cpp';write(frontOut,cpp_c(s));compiled.append(frontOut)
# x86 MMX/NV2A code is absent; CPU/APU timing and game rules retain shared flags.
remove={'ZAMN_R23_MMX_PPU','ZAMN_R31_MMX_TRANSPARENT_EARLYOUT','TA_LOG_INTERVAL_FRAMES'}
zero={'TA_R37_STREAM_UPLOAD','TA_R102_MODE2_DUAL_PUT','TA_R124_MODE2_STREAM','TA_R126_DIRECT_TARGET','TA_R126_GPU_SUBTRACT','TA_R127_GPU_INDEXED','TA_R128_GPU_ATLAS_CACHE','TA_R129_GPU_DIRECT_PALETTE'}
defs=[d for d in json.loads((ROOT/'rxdk.project.json').read_text())['configurations']['PlayN22PublicNoLog']['defines'] if d.split('=')[0] not in remove|zero]
defs+=[n+'=0' for n in sorted(zero)]+['TA_XBOX360=1','_XBOX_CRT_DEPRECATE_INSECURE','__STDC_CONSTANT_MACROS','__STDC_LIMIT_MACROS']
write(OUT/'defines.json',json.dumps(defs,indent=2)+'\n')
compiled += sorted(PORT.glob('ta_360_*.cpp'))
# VS2010 Xbox 360 platform integration comes with the installed XDK.
ns='http://schemas.microsoft.com/developer/msbuild/2003';E.register_namespace('',ns)
def node(parent,name,value=None,**attributes):
 e=E.SubElement(parent,'{'+ns+'}'+name,attributes)
 if value is not None:e.text=str(value)
 return e
project=E.Element('{'+ns+'}Project',{'DefaultTargets':'Build','ToolsVersion':'4.0'})
group=node(project,'ItemGroup',Label='ProjectConfigurations');cfg=node(group,'ProjectConfiguration',Include='Release|Xbox 360');node(cfg,'Configuration','Release');node(cfg,'Platform','Xbox 360')
group=node(project,'PropertyGroup',Label='Globals');node(group,'ProjectGuid','{5B29CD30-5441-3045-9236-010100000006}');node(group,'ProjectName','DinoBlockStacker360');node(group,'RootNamespace','DinoBlockStacker360');node(group,'Keyword','Win32Proj')
node(project,'Import',Project='$(VCTargetsPath)\\Microsoft.Cpp.Default.props')
group=node(project,'PropertyGroup',Label='Configuration');node(group,'ConfigurationType','Application');node(group,'PlatformToolset','2010-01');node(group,'CharacterSet','MultiByte');node(group,'UseOfAtl','false')
node(project,'Import',Project='$(VCTargetsPath)\\Microsoft.Cpp.props')
group=node(project,'PropertyGroup');node(group,'OutDir',str(ROOT/'out/Xbox360NoLog')+'\\');node(group,'IntDir',str(OUT/'obj')+'\\');node(group,'TargetName','default');node(group,'ImageXexOutput','$(OutDir)default-devkit.xex');node(group,'LinkIncremental','false')
group=node(project,'ItemDefinitionGroup');cl=node(group,'ClCompile')
for k,v in {'WarningLevel':'Level3','Optimization':'MaxSpeed','IntrinsicFunctions':'true','RuntimeLibrary':'MultiThreaded','ExceptionHandling':'false','RuntimeTypeInfo':'false','BufferSecurityCheck':'true','FunctionLevelLinking':'true','ForceConformanceInForLoopScope':'true','CompileAs':'CompileAsCpp','PreprocessorDefinitions':';'.join(defs)+';%(PreprocessorDefinitions)','AdditionalIncludeDirectories':';'.join(map(str,[PORT/'compat',PORT,OUT,TREE/'src/native',TREE/'src/xbox',TREE/'netplay/src',TREE/'third_party/lakesnes/snes']))+';%(AdditionalIncludeDirectories)','ObjectFileName':'$(IntDir)%(Filename).obj'}.items():node(cl,k,v)
link=node(group,'Link')
for k,v in {'AdditionalDependencies':'d3d9.lib;d3dx9.lib;xgraphics.lib;xapilib.lib;xaudio2.lib;xmcore.lib;xnet.lib;xboxkrnl.lib;%(AdditionalDependencies)','AdditionalOptions':'/XEX:NO %(AdditionalOptions)','GenerateDebugInformation':'false','OptimizeReferences':'true','EnableCOMDATFolding':'true','StackReserveSize':'1048576','StackCommitSize':'1048576'}.items():node(link,k,v)
image=node(group,'ImageXex');node(image,'TitleID','0x'+identity['xbox_title_id']);node(image,'CrossPlatformSystemLink','true');node(image,'HeapSize','0x04000000');node(image,'SuppressStartupBanner','true')
group=node(project,'ItemGroup')
for file in compiled:node(group,'ClCompile',Include=str(file))
node(project,'Import',Project='$(VCTargetsPath)\\Microsoft.Cpp.targets')
E.indent(project);write(OUT/'DinoBlockStacker360.vcxproj',E.tostring(project,encoding='utf-8',xml_declaration=True).decode('utf-8'))
record={'version':identity['version'],'target':'Xbox 360 PowerPC / RGH-JTAG and Bad Avatar','frontend_sha256':digest(frontend),'shared_sources':manifest,'translation_units':len(compiled),'public_pin_sha256':hashlib.sha256(bytes.fromhex(key)).hexdigest(),'protocol_changes':False,'rom_embedded':False,'server_implementation_included':False,'build_status':'not_built','features':['shared animated menus and supplied art','offline original game and two local controllers','LAN 1v1','online ranked 1v1','online 2-4 player Auto and Focus','real remote board images','local character backdrop','accounts and persistent profile resume','world and lobby chat','both leaderboards and six egg ranks','rematches, speed requests and host transfer','population counters','two-port custom controller bindings'],'architecture_fallbacks':sorted(remove|zero)}
write(OUT/'port_manifest.json',json.dumps(record,indent=2)+'\n')
print('Prepared %d translation units from shared public client sources.'%len(compiled))
print(OUT/'DinoBlockStacker360.vcxproj')
