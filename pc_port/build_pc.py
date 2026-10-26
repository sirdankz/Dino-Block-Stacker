"""Build and package the Windows x64 client with pinned SDL3 dependencies."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys,urllib.request,zipfile
ROOT=Path(__file__).resolve().parents[1];PORT=ROOT/'pc_port'
def run(args,env=None,log=None):
 print('Running:',Path(str(args[0])).name,flush=True)
 if log:
  with log.open('w',encoding='utf-8') as output:
   process=subprocess.Popen(list(map(str,args)),env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,errors='replace')
   for line in process.stdout:print(line,end='');output.write(line)
   status=process.wait()
 else:status=subprocess.call(list(map(str,args)),env=env)
 if status:raise RuntimeError('Build step failed (exit %d).%s'%(status,' Diagnostics: '+str(log) if log else ''))
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def executable(name,candidates):
 found=shutil.which(name)
 if found:return Path(found)
 for path in candidates:
  if path.is_file():return path
 raise RuntimeError(name+' was not found. See pc_port/README.md for build requirements.')
def windows_environment():
 pf=Path(os.environ.get('ProgramFiles','C:/Program Files'));p86=Path(os.environ.get('ProgramFiles(x86)','C:/Program Files (x86)'))
 where=executable('vswhere.exe',[p86/'Microsoft Visual Studio/Installer/vswhere.exe'])
 vs=subprocess.check_output([str(where),'-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip()
 if not vs:raise RuntimeError('Install Visual Studio 2022 C++ Build Tools and a Windows 10/11 SDK.')
 base=Path(vs);bat=base/'VC/Auxiliary/Build/vcvars64.bat'
 if not bat.is_file():raise RuntimeError('The x64 Visual Studio compiler environment is missing.')
 raw=subprocess.check_output(f'cmd.exe /d /s /c ""{bat}" >nul && set"',text=True,errors='replace');env=os.environ.copy()
 for line in raw.splitlines():
  if '=' in line and not line.startswith('='):
   k,v=line.split('=',1);env[k]=v
 compiler=executable('clang-cl.exe',[pf/'LLVM/bin/clang-cl.exe',base/'VC/Tools/Llvm/x64/bin/clang-cl.exe',base/'VC/Tools/Llvm/bin/clang-cl.exe'])
 linker=compiler.with_name('lld-link.exe')
 if not linker.exists():raise RuntimeError('The LLVM lld-link linker must be installed with clang-cl.')
 cmake=executable('cmake.exe',[base/'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe',Path('C:/SHSDK/cmake/3.22.1/bin/cmake.exe'),pf/'CMake/bin/cmake.exe'])
 ninja=executable('ninja.exe',[cmake.with_name('ninja.exe'),base/'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe',Path('C:/SHSDK/cmake/3.22.1/bin/ninja.exe')])
 env['PATH']=str(compiler.parent)+';'+str(cmake.parent)+';'+str(ninja.parent)+';'+env.get('PATH','')
 return env,compiler,linker,cmake,ninja
def sdl_dependency():
 lock=json.loads((PORT/'dependencies.json').read_text());cache=ROOT/'out/PCDependencies';cache.mkdir(parents=True,exist_ok=True);archive=cache/Path(lock['url']).name
 if not archive.exists() or digest(archive)!=lock['sha256']:
  print('Downloading official SDL3 '+lock['version']+'...',flush=True);request=urllib.request.Request(lock['url'],headers={'User-Agent':'DinoBlockStacker-build'});temp=archive.with_suffix('.download')
  with urllib.request.urlopen(request,timeout=120) as response,temp.open('wb') as output:shutil.copyfileobj(response,output)
  if digest(temp)!=lock['sha256']:raise RuntimeError('The SDL3 archive checksum does not match the pinned official release.')
  temp.replace(archive)
 with zipfile.ZipFile(archive) as z:
  for name in z.namelist():
   if not (cache/name).resolve().is_relative_to(cache.resolve()):raise RuntimeError('Unsafe dependency archive path.')
  z.extractall(cache)
 return cache/lock['folder']
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--out-dir',type=Path,default=ROOT/'out/DinoBlockStackerPC');parser.add_argument('--rom',type=Path);parser.add_argument('--jobs',type=int,default=0);args=parser.parse_args()
 if os.name!='nt':raise RuntimeError('The Windows PC build requires Windows.')
 if sys.version_info<(3,10):raise RuntimeError('Python 3.10 or newer is required.')
 out=args.out_dir.resolve();out.mkdir(parents=True,exist_ok=True);build=out/'build';app=out/'Windows-x64';app.mkdir(exist_ok=True)
 version=json.loads((ROOT/'release_identity.json').read_text())['version'];env,compiler,linker,cmake,ninja=windows_environment();sdk=sdl_dependency()
 run([sys.executable,ROOT/'tools/prepare_music.py','--out',app/'music'])
 run([sys.executable,PORT/'prepare_pc.py'])
 run([cmake,'-S',PORT,'-B',build,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_C_COMPILER='+str(compiler),'-DCMAKE_CXX_COMPILER='+str(compiler),'-DCMAKE_LINKER='+str(linker),'-DCMAKE_MAKE_PROGRAM='+str(ninja),'-DSDL3_DIR='+str(sdk/'cmake')],env,out/'configure-pc.log')
 run([cmake,'--build',build,'--parallel',str(args.jobs or min(os.cpu_count() or 2,8))],env,out/'build-pc.log')
 files={'DinoBlockStacker.exe':build/'DinoBlockStacker.exe','SDL3.dll':sdk/'lib/x64/SDL3.dll','README.md':PORT/'README.md','LICENSE-MUSIC.txt':ROOT/'LICENSE-MUSIC.txt','LICENSE':ROOT/'LICENSE','COPYING':ROOT/'COPYING','THIRD_PARTY_NOTICES.md':ROOT/'THIRD_PARTY_NOTICES.md','LICENSES-LAKESNES-MIT.txt':ROOT/'LICENSES-LAKESNES-MIT.txt','LICENSES-SDL3.txt':sdk/'LICENSE.txt'}
 for name,source in files.items():shutil.copy2(source,app/name)
 for name in ['menu.wav','results.wav','manifest.json','LICENSE-MUSIC.txt']:files['music/'+name]=app/'music'/name
 # Retain Monocypher's original BSD/CC0 notices with distributed binaries.
 source=(ROOT/'third_party/monocypher/monocypher.c').read_text(encoding='utf-8');start=0;end=source.index('#include "monocypher.h"')
 (app/'LICENSES-MONOCYPHER.txt').write_text(source[start:end]+'\n',encoding='utf-8');files['LICENSES-MONOCYPHER.txt']=app/'LICENSES-MONOCYPHER.txt'
 manifest=json.loads((PORT/'generated/port_manifest.json').read_text());manifest['build_status']='built';manifest['runtime_validation']='pending';manifest['compiler']=subprocess.check_output([str(compiler),'--version'],text=True).splitlines()[0];manifest['sdl_version']=json.loads((PORT/'dependencies.json').read_text())['version']
 (app/'port_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8');files['port_manifest.json']=app/'port_manifest.json'
 archive=out/f'DinoBlockStacker-v{version}-Windows-x64-NoLog.zip'
 # Explicit allowlist: existing game files, profiles, logs and caches cannot enter the ZIP.
 with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
  for name in sorted(files):z.write(app/name,'DinoBlockStacker/'+name)
 checks=[archive,app/'DinoBlockStacker.exe',app/'SDL3.dll'];(out/'SHA256SUMS.txt').write_text(''.join(digest(p)+'  '+p.relative_to(out).as_posix()+'\n' for p in checks),encoding='ascii')
 if args.rom:
  data=args.rom.read_bytes()
  if len(data)!=1048576 or hashlib.sha256(data).hexdigest()!='579bf1a1988d4af06a69cc1d82a2478ebe51940c5ced7f97e83029a24e6aa778':raise RuntimeError('The supplied ROM is incompatible. The ROM-free PC build and ZIP are ready.')
  shutil.copy2(args.rom,app/'game.sfc')
 print('Windows executable: '+str(app/'DinoBlockStacker.exe'));print('ROM-free package: '+str(archive));print('Runtime validation: pending.')
if __name__=='__main__':
 try:main()
 except Exception as e:print('ERROR: '+str(e),file=sys.stderr);sys.exit(1)
