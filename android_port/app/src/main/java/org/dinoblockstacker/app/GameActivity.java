package org.dinoblockstacker.app;

import android.app.*;
import android.content.Context;
import android.graphics.*;
import android.hardware.input.InputManager;
import android.media.*;
import android.net.wifi.WifiManager;
import android.opengl.*;
import android.os.Bundle;
import android.view.*;
import android.widget.*;
import java.io.File;
import java.nio.*;
import java.util.*;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public final class GameActivity extends Activity implements InputManager.InputDeviceListener {
    static{System.loadLibrary("dinoblockstacker");}
    private native String nativeRun(String rom,String cache);
    static native void nativeButtons(int player,int buttons);
    static native void nativePrepare();
    static native void nativePause(boolean pause);
    static native void nativeStop();
    static native int nativeCopyFrame(ByteBuffer pixels);
    static native void nativeRefresh();
    private Thread gameThread;private GLSurfaceView surface;private TouchPad touch;private boolean touchAllowed;
    private volatile AudioTrack audio;private WifiManager.MulticastLock multicast;private InputManager input;
    private final Map<Integer,Pad> pads=new LinkedHashMap<>();private int touchMask;
    private static final class Pad {int slot,keys,axes;final Set<Integer> held=new HashSet<>();Pad(int s){slot=s;}}
    @Override public void onCreate(Bundle b){super.onCreate(b);getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);immersive();
        FrameLayout root=new FrameLayout(this);surface=new GLSurfaceView(this);surface.setEGLContextClientVersion(2);surface.setPreserveEGLContextOnPause(true);surface.setRenderer(new Screen());root.addView(surface);
        touchAllowed=getIntent().getBooleanExtra("touch",true);touch=new TouchPad(this);root.addView(touch);setContentView(root);
        input=(InputManager)getSystemService(INPUT_SERVICE);input.registerInputDeviceListener(this,null);for(int id:InputDevice.getDeviceIds())onInputDeviceAdded(id);updateTouch();
        try{WifiManager wifi=(WifiManager)getApplicationContext().getSystemService(WIFI_SERVICE);if(wifi!=null){multicast=wifi.createMulticastLock("TA LAN discovery");multicast.setReferenceCounted(false);multicast.acquire();}}catch(RuntimeException ignored){}
        nativePrepare();startAudio();gameThread=new Thread(()->{String error=nativeRun(new File(getFilesDir(),"tetris_attack.sfc").getPath(),new File(getFilesDir(),"00000001").getPath());if(error!=null)runOnUiThread(()->{Toast.makeText(this,error,Toast.LENGTH_LONG).show();finish();});},"Dino Block Stacker game");gameThread.start();
    }
    private void immersive(){getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN|View.SYSTEM_UI_FLAG_HIDE_NAVIGATION|View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY|View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN|View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION|View.SYSTEM_UI_FLAG_LAYOUT_STABLE);}
    @Override public void onWindowFocusChanged(boolean focus){super.onWindowFocusChanged(focus);if(focus)immersive();else releaseInputs();}
    private void startAudio(){try{int min=AudioTrack.getMinBufferSize(48000,AudioFormat.CHANNEL_OUT_STEREO,AudioFormat.ENCODING_PCM_16BIT);audio=new AudioTrack.Builder().setAudioAttributes(new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME).setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build()).setAudioFormat(new AudioFormat.Builder().setSampleRate(48000).setChannelMask(AudioFormat.CHANNEL_OUT_STEREO).setEncoding(AudioFormat.ENCODING_PCM_16BIT).build()).setTransferMode(AudioTrack.MODE_STREAM).setBufferSizeInBytes(Math.max(6400,min)).build();audio.play();}catch(RuntimeException e){audio=null;}}
    @SuppressWarnings("unused") private void writeAudio(short[] samples){AudioTrack a=audio;if(a!=null)try{a.write(samples,0,samples.length,AudioTrack.WRITE_NON_BLOCKING);}catch(RuntimeException ignored){}}
    @Override protected void onResume(){super.onResume();if(surface!=null)surface.onResume();nativePause(false);AudioTrack a=audio;if(a!=null)try{a.play();}catch(RuntimeException ignored){}}
    @Override protected void onPause(){nativePause(true);releaseInputs();AudioTrack a=audio;if(a!=null)try{a.pause();a.flush();}catch(RuntimeException ignored){}if(surface!=null)surface.onPause();super.onPause();}
    @Override protected void onDestroy(){nativeStop();input.unregisterInputDeviceListener(this);if(gameThread!=null)try{gameThread.join(1500);}catch(InterruptedException e){Thread.currentThread().interrupt();}AudioTrack a=audio;audio=null;if(a!=null)a.release();if(multicast!=null&&multicast.isHeld())multicast.release();super.onDestroy();}
    @Override public void onBackPressed(){new AlertDialog.Builder(this).setTitle("Leave Dino Block Stacker?").setMessage("Leaving disconnects your current online or LAN match.").setPositiveButton("Leave",(d,w)->finish()).setNegativeButton("Stay",null).show();}
    private static boolean controller(InputDevice d){return d!=null&&((d.getSources()&InputDevice.SOURCE_GAMEPAD)==InputDevice.SOURCE_GAMEPAD||(d.getSources()&InputDevice.SOURCE_JOYSTICK)==InputDevice.SOURCE_JOYSTICK);}
    private Pad pad(int id){Pad p=pads.get(id);if(p!=null)return p;boolean[] used=new boolean[2];for(Pad q:pads.values())used[q.slot]=true;int slot=!used[0]?0:!used[1]?1:-1;if(slot<0)return null;p=new Pad(slot);pads.put(id,p);return p;}
    private void sendInputs(){int[] mask={touchMask,0};for(Pad p:pads.values())mask[p.slot]|=p.keys|p.axes;nativeButtons(0,mask[0]);nativeButtons(1,mask[1]);}
    private void releaseInputs(){touchMask=0;for(Pad p:pads.values()){p.keys=p.axes=0;p.held.clear();}nativeButtons(0,0);nativeButtons(1,0);if(touch!=null)touch.invalidate();}
    private void updateTouch(){boolean has=false;for(int id:InputDevice.getDeviceIds())if(controller(InputDevice.getDevice(id))){has=true;break;}touch.setVisibility(touchAllowed&&!has?View.VISIBLE:View.GONE);if(has){touchMask=0;sendInputs();}}
    @Override public void onInputDeviceAdded(int id){if(controller(InputDevice.getDevice(id)))pad(id);if(touch!=null)updateTouch();}
    @Override public void onInputDeviceRemoved(int id){pads.remove(id);sendInputs();updateTouch();}
    @Override public void onInputDeviceChanged(int id){Pad p=pads.get(id);if(p!=null){p.keys=p.axes=0;p.held.clear();}sendInputs();updateTouch();}
    static int keyBit(int k){return ControllerMap.keyBit(k);}
    @Override public boolean dispatchKeyEvent(KeyEvent e){int bit=keyBit(e.getKeyCode());if(bit!=0&&(e.getAction()==KeyEvent.ACTION_DOWN||e.getAction()==KeyEvent.ACTION_UP)){Pad p=pad(e.getDeviceId());if(p!=null){if(e.getAction()==KeyEvent.ACTION_DOWN)p.held.add(e.getKeyCode());else p.held.remove(e.getKeyCode());p.keys=0;for(int k:p.held)p.keys|=keyBit(k);sendInputs();}return true;}return super.dispatchKeyEvent(e);}
    private static float axis(MotionEvent e,int a){InputDevice d=e.getDevice();InputDevice.MotionRange range=d==null?null:d.getMotionRange(a,e.getSource());float value=e.getAxisValue(a);return Math.abs(value)>Math.max(.25f,range==null?0:range.getFlat())?value:0;}
    @Override public boolean onGenericMotionEvent(MotionEvent e){if((e.getSource()&InputDevice.SOURCE_JOYSTICK)==InputDevice.SOURCE_JOYSTICK&&e.getAction()==MotionEvent.ACTION_MOVE){Pad p=pad(e.getDeviceId());if(p!=null){float x=axis(e,MotionEvent.AXIS_X),y=axis(e,MotionEvent.AXIS_Y),hx=axis(e,MotionEvent.AXIS_HAT_X),hy=axis(e,MotionEvent.AXIS_HAT_Y);int mask=0;if(x<0||hx<0)mask|=0x40;if(x>0||hx>0)mask|=0x80;if(y<0||hy<0)mask|=0x10;if(y>0||hy>0)mask|=0x20;if(e.getAxisValue(MotionEvent.AXIS_LTRIGGER)>.25f||e.getAxisValue(MotionEvent.AXIS_BRAKE)>.25f)mask|=0x400;if(e.getAxisValue(MotionEvent.AXIS_RTRIGGER)>.25f||e.getAxisValue(MotionEvent.AXIS_GAS)>.25f)mask|=0x800;p.axes=mask;sendInputs();}return true;}return super.onGenericMotionEvent(e);}
    private static final class Screen implements GLSurfaceView.Renderer {
        private final ByteBuffer pixels=ByteBuffer.allocateDirect(512*480*4).order(ByteOrder.nativeOrder());private final FloatBuffer vertices=ByteBuffer.allocateDirect(16*4).order(ByteOrder.nativeOrder()).asFloatBuffer();private int program,texture,width,height,rows=480;
        private int shader(int type,String source){int s=GLES20.glCreateShader(type);GLES20.glShaderSource(s,source);GLES20.glCompileShader(s);int[] ok={0};GLES20.glGetShaderiv(s,GLES20.GL_COMPILE_STATUS,ok,0);if(ok[0]==0)throw new IllegalStateException(GLES20.glGetShaderInfoLog(s));return s;}
        @Override public void onSurfaceCreated(GL10 unused,EGLConfig config){program=GLES20.glCreateProgram();GLES20.glAttachShader(program,shader(GLES20.GL_VERTEX_SHADER,"attribute vec2 pos;attribute vec2 uv;varying mediump vec2 tc;void main(){gl_Position=vec4(pos,0.0,1.0);tc=uv;}"));GLES20.glAttachShader(program,shader(GLES20.GL_FRAGMENT_SHADER,"precision mediump float;uniform sampler2D image;varying mediump vec2 tc;void main(){gl_FragColor=texture2D(image,tc);}"));GLES20.glLinkProgram(program);int[] linked={0};GLES20.glGetProgramiv(program,GLES20.GL_LINK_STATUS,linked,0);if(linked[0]==0)throw new IllegalStateException(GLES20.glGetProgramInfoLog(program));int[] t={0};GLES20.glGenTextures(1,t,0);texture=t[0];GLES20.glBindTexture(GLES20.GL_TEXTURE_2D,texture);GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D,GLES20.GL_TEXTURE_MIN_FILTER,GLES20.GL_NEAREST);GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D,GLES20.GL_TEXTURE_MAG_FILTER,GLES20.GL_NEAREST);GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D,GLES20.GL_TEXTURE_WRAP_S,GLES20.GL_CLAMP_TO_EDGE);GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D,GLES20.GL_TEXTURE_WRAP_T,GLES20.GL_CLAMP_TO_EDGE);GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D,0,GLES20.GL_RGBA,512,480,0,GLES20.GL_RGBA,GLES20.GL_UNSIGNED_BYTE,null);nativeRefresh();}
        @Override public void onSurfaceChanged(GL10 unused,int w,int h){width=w;height=h;}
        @Override public void onDrawFrame(GL10 unused){GLES20.glViewport(0,0,width,height);GLES20.glClearColor(0,0,0,1);GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);GLES20.glBindTexture(GLES20.GL_TEXTURE_2D,texture);int updated=nativeCopyFrame(pixels);if(updated!=0){rows=updated;pixels.position(0);GLES20.glTexSubImage2D(GLES20.GL_TEXTURE_2D,0,0,0,512,rows,GLES20.GL_RGBA,GLES20.GL_UNSIGNED_BYTE,pixels);}int h=(int)(height*.9f),w=h*4/3;if(w>width*.9f){w=(int)(width*.9f);h=w*3/4;}GLES20.glViewport((width-w)/2,(height-h)/2,w,h);GLES20.glUseProgram(program);float bottom=(float)rows/480;vertices.clear();vertices.put(new float[]{-1,-1,0,bottom,1,-1,1,bottom,-1,1,0,0,1,1,1,0}).position(0);int pos=GLES20.glGetAttribLocation(program,"pos"),uv=GLES20.glGetAttribLocation(program,"uv");vertices.position(0);GLES20.glVertexAttribPointer(pos,2,GLES20.GL_FLOAT,false,16,vertices);vertices.position(2);GLES20.glVertexAttribPointer(uv,2,GLES20.GL_FLOAT,false,16,vertices);GLES20.glEnableVertexAttribArray(pos);GLES20.glEnableVertexAttribArray(uv);GLES20.glUniform1i(GLES20.glGetUniformLocation(program,"image"),0);GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP,0,4);}
    }
    private final class TouchPad extends View {
        private final Paint paint=new Paint(3);private final ArrayList<Control> controls=new ArrayList<>();private float cx,cy,unit;
        private final class Control {final RectF rect;final String text;final int bit;Control(float x,float y,float w,float h,String s,int b){rect=new RectF(x-w/2,y-h/2,x+w/2,y+h/2);text=s;bit=b;}}
        TouchPad(Context c){super(c);setFocusable(false);}
        @Override protected void onSizeChanged(int w,int h,int oldw,int oldh){float image=Math.min(w*.9f,h*.9f*4/3),margin=(w-image)/2;unit=Math.min(h/9f,Math.max(margin/3.2f,h/14f));cx=Math.max(unit*1.7f,margin/2);cy=h-unit*2.2f;controls.clear();float right=w-cx;
            controls.add(new Control(cx,cy-unit,unit,unit,"UP",0x10));controls.add(new Control(cx,cy+unit,unit,unit,"DOWN",0x20));controls.add(new Control(cx-unit,cy,unit,unit,"LEFT",0x40));controls.add(new Control(cx+unit,cy,unit,unit,"RIGHT",0x80));
            controls.add(new Control(right,cy+unit,unit,unit,"A",1));controls.add(new Control(right+unit,cy,unit,unit,"B",0x100));controls.add(new Control(right-unit,cy,unit,unit,"X",2));controls.add(new Control(right,cy-unit,unit,unit,"Y",0x200));
            controls.add(new Control(cx,cy-unit*2.4f,unit*1.8f,unit*.7f,"L",0x400));controls.add(new Control(right,cy-unit*2.4f,unit*1.8f,unit*.7f,"R",0x800));controls.add(new Control(w*.44f,h-unit*.6f,unit*1.8f,unit*.7f,"BACK",4));controls.add(new Control(w*.56f,h-unit*.6f,unit*1.8f,unit*.7f,"START",8));
        }
        @Override protected void onDraw(Canvas c){paint.setTypeface(Typeface.create(Typeface.MONOSPACE,Typeface.BOLD));paint.setTextAlign(Paint.Align.CENTER);paint.setTextSize(unit*.2f);for(Control b:controls){paint.setColor((touchMask&b.bit)!=0?0xb0457080:0x8c080d14);c.drawRoundRect(b.rect,unit*.18f,unit*.18f,paint);paint.setColor(b.bit==1?0xff96e3b2:b.bit==0x100?0xfff1a0aa:b.bit==0x200?0xfff1cb72:0xffdbe3ef);c.drawText(b.text,b.rect.centerX(),b.rect.centerY()-(paint.ascent()+paint.descent())/2,paint);}}
        @Override public boolean onTouchEvent(MotionEvent e){int mask=0;int action=e.getActionMasked(),skip=action==MotionEvent.ACTION_POINTER_UP?e.getActionIndex():-1;if(action!=MotionEvent.ACTION_CANCEL&&action!=MotionEvent.ACTION_UP)for(int i=0;i<e.getPointerCount();i++){if(i==skip)continue;float x=e.getX(i),y=e.getY(i);for(Control b:controls)if(b.rect.contains(x,y))mask|=b.bit;}touchMask=mask;sendInputs();invalidate();return true;}
    }
}
