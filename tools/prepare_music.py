"""Prepare streamed PCM loops from the separately licensed original music."""
from pathlib import Path
import argparse,array,hashlib,json,shutil,sys,wave
ROOT=Path(__file__).resolve().parents[1];ASSETS=ROOT/'assets/music';FADE=4800
TRACKS={'menu.wav':'Ness_2.wav','results.wav':'Ness_2ResultsScreen.wav'}
def digest(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return h.hexdigest()
def prepare(out):
 out=Path(out);out.mkdir(parents=True,exist_ok=True)
 sources={n:digest(ASSETS/n) for n in TRACKS.values()};manifest=out/'manifest.json'
 if manifest.exists():
  old=json.loads(manifest.read_text())
  if old.get('schema')==1 and old.get('sources')==sources and all((out/n).is_file() and digest(out/n)==old.get('tracks',{}).get(n,{}).get('sha256') for n in TRACKS):
   shutil.copy2(ASSETS/'LICENSE-MUSIC.txt',out/'LICENSE-MUSIC.txt');return out
 record={'schema':1,'owner':'sirdankz','license':'LICENSE-MUSIC.txt','sample_rate':48000,'crossfade_frames':FADE,'sources':sources,'tracks':{}}
 for output,source in TRACKS.items():
  with wave.open(str(ASSETS/source),'rb') as src:
   if (src.getnchannels(),src.getsampwidth(),src.getframerate(),src.getcomptype())!=(2,2,48000,'NONE'):raise ValueError(source+': expected 48 kHz stereo 16-bit PCM')
   frames=src.getnframes()
   if frames<FADE*4:raise ValueError(source+': track is too short for the loop crossfade')
   head=array.array('h',src.readframes(FADE));src.setpos(frames-FADE);tail=array.array('h',src.readframes(FADE))
   if sys.byteorder!='little':head.byteswap();tail.byteswap()
   mixed=array.array('h')
   for i in range(FADE):
    for channel in range(2):
     a=int(tail[i*2+channel]);b=int(head[i*2+channel]);mixed.append(round((a*(FADE-1-i)+b*i)/(FADE-1)))
   if sys.byteorder!='little':mixed.byteswap()
   target=out/output;temporary=target.with_suffix('.tmp');src.setpos(FADE)
   with wave.open(str(temporary),'wb') as dst:
    dst.setnchannels(2);dst.setsampwidth(2);dst.setframerate(48000);remaining=frames-2*FADE
    while remaining:
     count=min(remaining,65536);data=src.readframes(count)
     if len(data)!=count*4:raise ValueError('Truncated music source: '+source)
     dst.writeframesraw(data);remaining-=count
    dst.writeframesraw(mixed.tobytes())
   temporary.replace(target);record['tracks'][output]={'source':source,'frames':frames-FADE,'bytes':target.stat().st_size,'sha256':digest(target)}
 shutil.copy2(ASSETS/'LICENSE-MUSIC.txt',out/'LICENSE-MUSIC.txt');manifest.write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8');return out
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args();print('Prepared original music loops: '+str(prepare(a.out)))
