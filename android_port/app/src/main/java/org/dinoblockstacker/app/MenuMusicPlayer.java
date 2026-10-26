package org.dinoblockstacker.app;

import android.content.Context;
import android.content.res.AssetFileDescriptor;
import android.media.MediaPlayer;
import android.os.Handler;
import android.os.Looper;

/** The original track is separately licensed; generated WAV loops are gapless. */
final class MenuMusicPlayer {
    private static final Handler handler=new Handler(Looper.getMainLooper());
    private static MediaPlayer player;
    private static int generation;
    private static final float DEFAULT_GAIN=0.1f;
    private static float savedGain(Context context){
        try(java.io.FileInputStream input=new java.io.FileInputStream(new java.io.File(context.getFilesDir(),"00000001/options-v1.bin"))){
            byte[] b=new byte[56];int at=0,n;while(at<b.length&&(n=input.read(b,at,b.length-at))>0)at+=n;if(at!=56||input.read()!=-1)return DEFAULT_GAIN;
            if(!new String(b,0,8,java.nio.charset.StandardCharsets.US_ASCII).equals("DINOOPT1"))return DEFAULT_GAIN;
            int h=0x811c9dc5;for(int i=0;i<52;i++)h=(h^(b[i]&255))*16777619;int crc=(b[52]&255)|((b[53]&255)<<8)|((b[54]&255)<<16)|((b[55]&255)<<24);
            int master=b[8]&255,music=b[9]&255;if(h!=crc||master>100||music>100)return DEFAULT_GAIN;return master*music/10000f;
        }catch(java.io.IOException ignored){return DEFAULT_GAIN;}
    }
    static void resume(Context context){
        final float volume=savedGain(context);
        handler.removeCallbacksAndMessages(null);
        if(player!=null){try{player.setVolume(volume,volume);return;}catch(RuntimeException invalid){stop();++generation;}}
        final int token=++generation;MediaPlayer music=new MediaPlayer();
        try(AssetFileDescriptor asset=context.getAssets().openFd("music/menu.wav")){
            music.setDataSource(asset.getFileDescriptor(),asset.getStartOffset(),asset.getLength());
            music.setLooping(true);music.setVolume(0,0);music.prepare();player=music;
            music.setOnErrorListener((p,what,extra)->{stop();return true;});music.start();
            for(int step=1;step<=10;step++){final float gain=step*volume/10f;handler.postDelayed(()->{if(generation==token&&player==music)music.setVolume(gain,gain);},step*15L);}
        }catch(Exception error){if(player==music)player=null;music.release();}
    }
    static void pause(){int token=++generation;handler.postDelayed(()->{if(generation==token)stop();},120);}
    static void handoff(){++generation;handler.removeCallbacksAndMessages(null);}
    static void stop(){++generation;handler.removeCallbacksAndMessages(null);MediaPlayer old=player;player=null;if(old!=null){try{old.stop();}catch(RuntimeException ignored){}old.release();}}
    private MenuMusicPlayer(){}
}
