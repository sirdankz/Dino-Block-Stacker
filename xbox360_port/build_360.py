"""Build and package the shared client for a homebrew-enabled retail Xbox 360."""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,urllib.request,zipfile,io
ROOT=Path(__file__).resolve().parents[1];PORT=ROOT/'xbox360_port'
ROM_SHA='579bf1a1988d4af06a69cc1d82a2478ebe51940c5ced7f97e83029a24e6aa778'
TOOLS_URL='https://github.com/grimdoomer/Xbox360BadUpdate/releases/download/v1.2/Tools.zip'
TOOLS_SHA='a26424909879e11dbf3657df7b286c8288c6178a285e24abcb1c6963573f989e'
XEXTOOL_SHA='33c4048ca120d3209fb1f6e4198e870b8852c00e7868f0d9b9685ac1dc839cdd'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(args,log=None,env=None):
 if log:
  with log.open('w',encoding='utf-8') as f:
   proc=subprocess.Popen([str(a) for a in args],cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,encoding='utf-8',errors='replace')
   for line in proc.stdout:print(line,end='');f.write(line)
   status=proc.wait();f.flush()
   if status:
    errors=[]
    for line in log.read_text(encoding='utf-8',errors='replace').splitlines():
     if re.search(r'\b(?:fatal )?error (?:C|LNK|MSB|IM)\d+',line):
      concise=re.sub(r'^\s*(?:\d+>)?','',line).split(' [')[0]
      if concise not in errors:errors.append(concise)
    if errors:print('\nCompiler errors:\n'+'\n'.join(errors[:12]),flush=True)
    raise RuntimeError('Build failed (exit %d). Full diagnostics: %s'%(status,log))
 else:subprocess.run([str(a) for a in args],cwd=ROOT,env=env,check=True)
def converter(path):
 if path:
  tool=Path(path).resolve()
  if not tool.is_file():raise ValueError('XexTool not found: '+str(tool))
  return tool
 cache=ROOT/'out/Xbox360Tools';tool=cache/'XexTool.exe'
 if tool.is_file() and sha(tool)==XEXTOOL_SHA:return tool
 cache.mkdir(parents=True,exist_ok=True)
 print('Downloading the pinned upstream homebrew XEX conversion tool...')
 req=urllib.request.Request(TOOLS_URL,headers={'User-Agent':'DinoBlockStacker-build'})
 data=urllib.request.urlopen(req,timeout=90).read()
 if hashlib.sha256(data).hexdigest()!=TOOLS_SHA:raise ValueError('Upstream tools archive checksum mismatch')
 with zipfile.ZipFile(io.BytesIO(data)) as z:
  names=[n for n in z.namelist() if n.lower().endswith('/xextool.exe') or n.lower()=='xextool.exe']
  if len(names)!=1:raise ValueError('Expected one XexTool in the upstream archive')
  executable=z.read(names[0])
 if hashlib.sha256(executable).hexdigest()!=XEXTOOL_SHA:raise ValueError('XexTool checksum mismatch')
 tool.write_bytes(executable);return tool
ap=argparse.ArgumentParser();ap.add_argument('--sdk',default=os.environ.get('XEDK',r'C:\Program Files (x86)\Microsoft Xbox 360 SDK'));ap.add_argument('--out',default=str(ROOT/'out/DinoBlockStacker360'));ap.add_argument('--public-key',default=str(ROOT/'config/server-public-key.hex'));ap.add_argument('--rom',default='');ap.add_argument('--xextool',default='');a=ap.parse_args()
sdk=Path(a.sdk).resolve();out=Path(a.out).resolve();out.mkdir(parents=True,exist_ok=True)
for file in ['bin/win32/cl.exe','bin/win32/link.exe','bin/win32/imagexex.exe','include/xbox/xtl.h','lib/xbox/d3d9.lib']:
 if not(sdk/file).is_file():raise ValueError('Xbox 360 SDK component missing: '+str(sdk/file))
msbuild=Path(os.environ.get('WINDIR',r'C:\Windows'))/'Microsoft.NET/Framework/v4.0.30319/MSBuild.exe'
vctargets=Path(os.environ.get('ProgramFiles(x86)',r'C:\Program Files (x86)'))/'MSBuild/Microsoft.Cpp/v4.0'
if not msbuild.is_file() or not(vctargets/'Platforms/Xbox 360/Microsoft.Cpp.Xbox 360.targets').is_file():raise ValueError('Install the Xbox 360 SDK VS2010 build integration')
rom=Path(a.rom).resolve() if a.rom else None
if rom and (not rom.is_file() or sha(rom)!=ROM_SHA):raise ValueError('Incompatible ROM: expected the supported 1 MB unheadered game.sfc')
run([sys.executable,PORT/'prepare_360.py',a.public_key])
tool=converter(a.xextool)
env=os.environ.copy();env['XEDK']=str(sdk)+'\\';env['XDKInstallDir']=str(sdk)+'\\';env['PATH']=str(sdk/'bin/win32')+os.pathsep+env.get('PATH','')
env['INCLUDE']=str(sdk/'include/xbox');env['LIB']=str(sdk/'lib/xbox')
run([msbuild,PORT/'generated/DinoBlockStacker360.vcxproj','/t:Build','/m','/nologo','/p:Configuration=Release','/p:Platform=Xbox 360','/p:XDKInstallDir='+str(sdk)+'\\','/p:VCTargetsPath='+str(vctargets)+'\\'],out/'build-xbox360.log',env)
dev=ROOT/'out/Xbox360NoLog/default-devkit.xex'
if not dev.is_file() or dev.read_bytes()[:4]!=b'XEX2':raise RuntimeError('Build did not produce an Xbox 360 XEX2 executable')
# Convert a new copy. Never replace or modify another game or console file.
xex=out/'default.xex';run([tool,'-m','r','-r','a','-e','u','-c','b','-o',xex,dev],out/'package-xbox360.log')
if not xex.is_file() or xex.read_bytes()[:4]!=b'XEX2':raise RuntimeError('Retail-format XEX conversion failed')
run([tool,'-l',xex],out/'xex-info.txt')
version=json.loads((ROOT/'release_identity.json').read_text())['version']
readme=out/'README.txt';readme.write_text("Dino Block Stacker v{version} - Xbox 360\n\nRetail-format homebrew executable for RGH/JTAG and an active Bad Avatar homebrew loader. Xenia is also supported. Logging is disabled.\n\nExtract the game folder to HDD/USB storage and launch default.xex. Place the compatible, unheadered 1 MiB game.sfc beside it. No ROM is included.\n\nProfiles, settings, bindings, and caches use DinoBlockStacker360Data beside the executable when writable. On Xenia/read-only game mounts, they use the normal Xbox saved-game container under the configured content directory.\n\nControls: D-pad/left stick navigate; A select; B back; BACK opens chat where shown; RB cycles junk targets; right stick selects a Focus target. Both controller ports have custom bindings. Hold BACK+START in offline play to return to the menus.\n\nOnline modes use the project's hosted UDP services without Xbox LIVE.\n\nStartup, saved-game storage, LAN play, and online connectivity have been checked in Xenia Canary Netplay. Physical-console verification remains pending.\n".format(version=version),encoding='utf-8')
run([sys.executable,ROOT/'tools/prepare_music.py','--out',out/'music'])
publicFiles=[xex,readme]+[out/'music'/n for n in ['menu.wav','results.wav','manifest.json','LICENSE-MUSIC.txt']]
for name in ['LICENSE-MUSIC.txt','LICENSE','COPYING','THIRD_PARTY_NOTICES.md','LICENSES-LAKESNES-MIT.txt']:
 source=ROOT/name
 if source.is_file():dest=out/name;shutil.copy2(source,dest);publicFiles.append(dest)
notices=out/'ThirdParty';notices.mkdir(exist_ok=True)
for name in ['third_party/lakesnes/LICENSE.txt','third_party/monocypher/LICENCE.md','third_party/monocypher/LICENCE']:
 source=ROOT/name
 if source.is_file():dest=notices/(source.parent.name+'-'+source.name);shutil.copy2(source,dest);publicFiles.append(dest)
archive=out/('DinoBlockStacker-v'+version+'-Xbox360-NoLog.zip')
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
 for file in publicFiles:z.write(file,file.relative_to(out))
checks=out/'SHA256SUMS.txt';checks.write_text(''.join(sha(f)+'  '+f.name+'\n' for f in [xex,archive]),encoding='ascii')
record=json.loads((PORT/'generated/port_manifest.json').read_text());record.update(build_status='compiled_and_retail_packaged',hardware_validation='pending',xex_sha256=sha(xex),rom_in_public_package=False)
(out/'port_manifest.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')
if rom:shutil.copy2(rom,out/'game.sfc')
print('\nXbox 360 executable: '+str(xex));print('ROM-free package: '+str(archive));print('Hardware validation: pending.')
