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
    static native void nativeTouchButtons(int buttons);
    static native void nativeRightStick(int x,int y);
    static native void nativeViewport(int width,int height);
    static native float[] nativeBoardDefaults(int profile,int width,int height,boolean full);
    static native void nativeBoardLayout(int profile,boolean custom,boolean full,float[] pose);
    static native int nativeBoardProfile();
    static native int nativeLeaderboardMode();
    static native void nativeLeaderboardTouch(int rows,int action,int mode);
    static native int nativeMenuMode();
    static native void nativeMenuTouch(int mode,float x,float y,int rows,int action);
    private final MenuGesture menuGesture=new MenuGesture();
    private final LeaderboardGesture leaderboardGesture=new LeaderboardGesture();
    private final int[] leaderboardLocation=new int[2];
    static native void nativePrepare();
    static native void nativePause(boolean pause);
    static native void nativeStop();
    static native int nativeCopyFrame(ByteBuffer pixels);
    static native void nativeRefresh();
    private FrameLayout gameRoot;private BoardLayoutEditor boardEditor;private int editedProfile;
    private Thread gameThread;private GLSurfaceView surface;private TouchPad touch;private int touchMode;
    private volatile AudioTrack audio;private WifiManager.MulticastLock multicast;private InputManager input;
    private volatile boolean chatPending;
    private volatile String chatResult;
    private int chatGeneration;private AlertDialog chatDialog;
    private final Map<Integer,Pad> pads=new LinkedHashMap<>();private int touchMask;
    private static final class Pad {int slot,keys,axes;final Set<Integer> held=new HashSet<>();Pad(int s){slot=s;}}
    @Override public void onCreate(Bundle b){super.onCreate(b);getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);immersive();
        FrameLayout root=new FrameLayout(this);gameRoot=root;surface=new GLSurfaceView(this);surface.setEGLContextClientVersion(2);surface.setPreserveEGLContextOnPause(true);surface.setRenderer(new Screen(this));root.addView(surface);
        touchMode=getIntent().getIntExtra("touch_mode",0);touch=new TouchPad(this,mask->{touchMask=mask;sendInputs();});root.addView(touch,new FrameLayout.LayoutParams(-1,-1));setContentView(root);
        input=(InputManager)getSystemService(INPUT_SERVICE);input.registerInputDeviceListener(this,null);for(int id:InputDevice.getDeviceIds())onInputDeviceAdded(id);updateTouch();
        try{WifiManager wifi=(WifiManager)getApplicationContext().getSystemService(WIFI_SERVICE);if(wifi!=null){multicast=wifi.createMulticastLock("TA LAN discovery");multicast.setReferenceCounted(false);multicast.acquire();}}catch(RuntimeException ignored){}
        android.util.DisplayMetrics metrics=getResources().getDisplayMetrics();BoardLayouts.loadAll(this,metrics.widthPixels,metrics.heightPixels);
        nativePrepare();startAudio();gameThread=new Thread(()->{String error=nativeRun(new File(getFilesDir(),"tetris_attack.sfc").getPath(),new File(getFilesDir(),"00000001").getPath());if(error!=null)runOnUiThread(()->{Toast.makeText(this,error,Toast.LENGTH_LONG).show();finish();});},"Dino Block Stacker game");gameThread.start();
    }
    private void immersive(){getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN|View.SYSTEM_UI_FLAG_HIDE_NAVIGATION|View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY|View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN|View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION|View.SYSTEM_UI_FLAG_LAYOUT_STABLE);}
    @Override public void onWindowFocusChanged(boolean focus){super.onWindowFocusChanged(focus);if(focus)immersive();else releaseInputs();}
    private void startAudio(){try{int min=AudioTrack.getMinBufferSize(48000,AudioFormat.CHANNEL_OUT_STEREO,AudioFormat.ENCODING_PCM_16BIT);audio=new AudioTrack.Builder().setAudioAttributes(new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME).setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build()).setAudioFormat(new AudioFormat.Builder().setSampleRate(48000).setChannelMask(AudioFormat.CHANNEL_OUT_STEREO).setEncoding(AudioFormat.ENCODING_PCM_16BIT).build()).setTransferMode(AudioTrack.MODE_STREAM).setBufferSizeInBytes(Math.max(6400,min)).build();audio.play();}catch(RuntimeException e){audio=null;}}
    @SuppressWarnings("unused") private void writeAudio(short[] samples){AudioTrack a=audio;if(a!=null)try{a.write(samples,0,samples.length,AudioTrack.WRITE_NON_BLOCKING);}catch(RuntimeException ignored){}}
    @Override protected void onResume(){super.onResume();if(surface!=null)surface.onResume();nativePause(false);AudioTrack a=audio;if(a!=null)try{a.play();}catch(RuntimeException ignored){}}
    @Override protected void onPause(){nativePause(true);releaseInputs();AudioTrack a=audio;if(a!=null)try{a.pause();a.flush();}catch(RuntimeException ignored){}if(surface!=null)surface.onPause();super.onPause();}
    @Override protected void onDestroy(){cancelChatInput();nativeStop();input.unregisterInputDeviceListener(this);if(gameThread!=null)try{gameThread.join(1500);}catch(InterruptedException e){Thread.currentThread().interrupt();}AudioTrack a=audio;audio=null;if(a!=null)a.release();if(multicast!=null&&multicast.isHeld())multicast.release();super.onDestroy();}
    @SuppressWarnings("unused") private void beginChatInput(String title){
        chatResult=null;chatPending=true;
        runOnUiThread(()->{
            if(!chatPending||isFinishing()||isDestroyed()){chatResult="";chatPending=false;return;}
            int generation=++chatGeneration;
            EditText field=new EditText(this);field.setSingleLine(true);field.setHint("Message (32 characters)");
            android.text.InputFilter ascii=(source,start,end,dest,dstart,dend)->{
                for(int i=start;i<end;i++)if(source.charAt(i)<32||source.charAt(i)>126)return "";return null;
            };
            field.setFilters(new android.text.InputFilter[]{ascii,new android.text.InputFilter.LengthFilter(32)});
            field.setInputType(android.text.InputType.TYPE_CLASS_TEXT|android.text.InputType.TYPE_TEXT_FLAG_CAP_SENTENCES);
            field.setImeOptions(android.view.inputmethod.EditorInfo.IME_ACTION_SEND);
            FrameLayout wrap=new FrameLayout(this);int margin=(int)(24*getResources().getDisplayMetrics().density);
            wrap.setPadding(margin,0,margin,0);wrap.addView(field);
            chatDialog=new AlertDialog.Builder(this).setTitle(title).setView(wrap)
                .setPositiveButton("Send",null).setNegativeButton("Cancel",(d,w)->{}).create();
            AlertDialog dialog=chatDialog;
            Runnable send=()->{String value=field.getText().toString().trim();if(value.isEmpty()){field.setError("Enter a message");return;}if(chatGeneration==generation)chatResult=value;dialog.dismiss();};
            dialog.setOnDismissListener(d->{if(chatGeneration==generation){if(chatResult==null)chatResult="";chatPending=false;chatDialog=null;}immersive();});
            dialog.show();dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(v->send.run());
            field.setOnEditorActionListener((v,action,event)->{if(action==android.view.inputmethod.EditorInfo.IME_ACTION_SEND){send.run();return true;}return false;});
            field.requestFocus();dialog.getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_VISIBLE);
        });
    }
    @SuppressWarnings("unused") private String pollChatText(){return chatPending?null:chatResult==null?"":chatResult;}
    @SuppressWarnings("unused") private void cancelChatInput(){
        chatPending=false;runOnUiThread(()->{++chatGeneration;if(chatDialog!=null){chatDialog.dismiss();chatDialog=null;}immersive();});
    }
    private boolean dispatchMenuTouch(MotionEvent event,int mode){
        if(menuGesture.active()&&menuGesture.session()!=mode){menuGesture.cancel();return true;}
        if(touch!=null)touch.setLeaderboardMode(true);
        int action=event.getActionMasked();surface.getLocationOnScreen(leaderboardLocation);
        float imageH=Math.min(surface.getHeight()*.9f,surface.getWidth()*.9f*3f/4f),imageW=imageH*4f/3f;
        if(imageW<=0||imageH<=0){menuGesture.cancel();return true;}
        int index=action==MotionEvent.ACTION_DOWN?0:event.findPointerIndex(menuGesture.pointer());
        if(action==MotionEvent.ACTION_CANCEL||index<0){menuGesture.cancel();return true;}
        float sx=event.getRawX()-event.getX()+event.getX(index)-leaderboardLocation[0];
        float sy=event.getRawY()-event.getY()+event.getY(index)-leaderboardLocation[1];
        float x=(sx-(surface.getWidth()-imageW)*.5f)*512f/imageW,y=(sy-(surface.getHeight()-imageH)*.5f)*480f/imageH;
        if(action==MotionEvent.ACTION_DOWN){touchMask=0;if(touch!=null)touch.clearPressed();sendInputs();menuGesture.begin(mode,event.getPointerId(0),x,y);}
        else if(action==MotionEvent.ACTION_MOVE&&menuGesture.active()){int rows=menuGesture.move(x,y);if(rows!=0)nativeMenuTouch(mode,x,y,rows,1);}
        else if(action==MotionEvent.ACTION_UP&&menuGesture.active()){if(menuGesture.finish(x,y))nativeMenuTouch(mode,x,y,0,0);}
        else if(action==MotionEvent.ACTION_POINTER_UP&&event.getPointerId(event.getActionIndex())==menuGesture.pointer())menuGesture.cancel();
        return true;
    }
    @Override public boolean dispatchTouchEvent(MotionEvent event){
        if(boardEditor!=null||chatPending)return super.dispatchTouchEvent(event);
        int menuMode=nativeMenuMode();if(nativeLeaderboardMode()==0&&menuMode!=0)return dispatchMenuTouch(event,menuMode);
        if(menuGesture.active()){menuGesture.cancel();return true;}
        int mode=nativeLeaderboardMode();
        if(leaderboardGesture.active()&&leaderboardGesture.session()!=mode){leaderboardGesture.cancel();return true;}
        if(mode==0)return super.dispatchTouchEvent(event);
        if(touch!=null)touch.setLeaderboardMode(true);
        int action=event.getActionMasked();
        surface.getLocationOnScreen(leaderboardLocation);
        float imageH=Math.min(surface.getHeight()*.9f,surface.getWidth()*.9f*3f/4f),imageW=imageH*4f/3f;
        if(imageW<=0||imageH<=0){leaderboardGesture.cancel();return true;}
        float ox=(surface.getWidth()-imageW)*.5f,oy=(surface.getHeight()-imageH)*.5f;
        int index=action==MotionEvent.ACTION_DOWN?0:event.findPointerIndex(leaderboardGesture.pointer());
        if(action==MotionEvent.ACTION_CANCEL||index<0){leaderboardGesture.cancel();return true;}
        float sx=event.getRawX()-event.getX()+event.getX(index)-leaderboardLocation[0];
        float sy=event.getRawY()-event.getY()+event.getY(index)-leaderboardLocation[1];
        float x=(sx-ox)*512f/imageW,y=(sy-oy)*480f/imageH;
        if(action==MotionEvent.ACTION_DOWN){touchMask=0;if(touch!=null)touch.clearPressed();sendInputs();leaderboardGesture.begin(mode,event.getPointerId(0),x,y);}
        else if(action==MotionEvent.ACTION_MOVE&&leaderboardGesture.active()){int rows=leaderboardGesture.move(x,y);if(rows!=0)nativeLeaderboardTouch(rows,0,mode);}
        else if(action==MotionEvent.ACTION_UP&&leaderboardGesture.active()){int button=leaderboardGesture.finish(x,y);if(button!=0)nativeLeaderboardTouch(0,button,mode);}
        else if(action==MotionEvent.ACTION_POINTER_UP&&event.getPointerId(event.getActionIndex())==leaderboardGesture.pointer())leaderboardGesture.cancel();
        return true;
    }
    private void openBoardEditor(){
        int profile=nativeBoardProfile();if(profile<1||profile>8){Toast.makeText(this,"Board layouts are also available from the setup screen.",Toast.LENGTH_SHORT).show();return;}
        releaseInputs();touch.setVisibility(View.GONE);editedProfile=profile;
        boardEditor=new BoardLayoutEditor(this,profile,true,BoardLayouts::apply,saved->closeBoardEditor());gameRoot.addView(boardEditor,new FrameLayout.LayoutParams(-1,-1));
    }
    private void closeBoardEditor(){if(boardEditor==null)return;gameRoot.removeView(boardEditor);boardEditor=null;editedProfile=0;releaseInputs();updateTouch();}
    private void checkBoardEditor(int profile){if(boardEditor!=null&&profile!=editedProfile)boardEditor.cancel();}
    @Override public void onBackPressed(){
        if(boardEditor!=null){boardEditor.cancel();return;}
        int menuMode=nativeMenuMode();if(menuMode!=0){nativeMenuTouch(menuMode,0,0,0,2);return;}
        int ladderMode=nativeLeaderboardMode();if(ladderMode!=0){nativeLeaderboardTouch(0,0x100,ladderMode);return;}
        int profile=nativeBoardProfile();String[] options=profile>0?new String[]{"Resume","Board layout","Leave game"}:new String[]{"Resume","Leave game"};
        new AlertDialog.Builder(this).setTitle("Game options").setItems(options,(dialog,which)->{if(which==0)return;if(profile>0&&which==1){openBoardEditor();return;}new AlertDialog.Builder(this).setTitle("Leave Dino Block Stacker?").setMessage("Leaving disconnects the current online or LAN match.").setPositiveButton("Leave",(d,w)->finish()).setNegativeButton("Stay",null).show();}).show();
    }
    private static boolean controller(InputDevice d){return d!=null&&((d.getSources()&InputDevice.SOURCE_GAMEPAD)==InputDevice.SOURCE_GAMEPAD||(d.getSources()&InputDevice.SOURCE_JOYSTICK)==InputDevice.SOURCE_JOYSTICK);}
    private Pad pad(int id){Pad p=pads.get(id);if(p!=null)return p;boolean[] used=new boolean[2];for(Pad q:pads.values())used[q.slot]=true;int slot=!used[0]?0:!used[1]?1:-1;if(slot<0)return null;p=new Pad(slot);pads.put(id,p);return p;}
    private void sendInputs(){nativeTouchButtons(touchMask);int[] mask={0,0};for(Pad p:pads.values())mask[p.slot]|=p.keys|p.axes;nativeButtons(0,mask[0]);nativeButtons(1,mask[1]);}
    private void releaseInputs(){menuGesture.cancel();leaderboardGesture.cancel();nativeRightStick(0,0);nativeTouchButtons(0);touchMask=0;for(Pad p:pads.values()){p.keys=p.axes=0;p.held.clear();}nativeButtons(0,0);nativeButtons(1,0);if(touch!=null)touch.clearPressed();}
    private void updateTouch(){boolean physicalPad=false;for(int id:InputDevice.getDeviceIds()){InputDevice d=InputDevice.getDevice(id);if(controller(d)&&!d.isVirtual()){physicalPad=true;break;}}boolean show=boardEditor==null&&(touchMode==0||(touchMode==1&&!physicalPad));if(!show)touch.clearPressed();touch.setVisibility(show?View.VISIBLE:View.GONE);}
    @Override public void onInputDeviceAdded(int id){if(controller(InputDevice.getDevice(id)))pad(id);if(touch!=null)updateTouch();}
    @Override public void onInputDeviceRemoved(int id){pads.remove(id);sendInputs();updateTouch();}
    @Override public void onInputDeviceChanged(int id){Pad p=pads.get(id);if(p!=null){p.keys=p.axes=0;p.held.clear();}sendInputs();updateTouch();}
    static int keyBit(int k){return ControllerMap.keyBit(k);}
    @Override public boolean dispatchKeyEvent(KeyEvent e){if(boardEditor!=null||chatPending)return super.dispatchKeyEvent(e);int bit=keyBit(e.getKeyCode());if(bit!=0&&(e.getAction()==KeyEvent.ACTION_DOWN||e.getAction()==KeyEvent.ACTION_UP)){Pad p=pad(e.getDeviceId());if(p!=null){if(e.getAction()==KeyEvent.ACTION_DOWN)p.held.add(e.getKeyCode());else p.held.remove(e.getKeyCode());p.keys=0;for(int k:p.held)p.keys|=keyBit(k);sendInputs();}return true;}return super.dispatchKeyEvent(e);}
    private static float axis(MotionEvent e,int a){InputDevice d=e.getDevice();InputDevice.MotionRange range=d==null?null:d.getMotionRange(a,e.getSource());float value=e.getAxisValue(a);return Math.abs(value)>Math.max(.25f,range==null?0:range.getFlat())?value:0;}
    @Override public boolean onGenericMotionEvent(MotionEvent e){if(boardEditor!=null)return super.onGenericMotionEvent(e);if((e.getSource()&InputDevice.SOURCE_JOYSTICK)==InputDevice.SOURCE_JOYSTICK&&e.getAction()==MotionEvent.ACTION_MOVE){Pad p=pad(e.getDeviceId());if(p!=null){float x=axis(e,MotionEvent.AXIS_X),y=axis(e,MotionEvent.AXIS_Y),hx=axis(e,MotionEvent.AXIS_HAT_X),hy=axis(e,MotionEvent.AXIS_HAT_Y);int mask=0;if(x<0||hx<0)mask|=0x40;if(x>0||hx>0)mask|=0x80;if(y<0||hy<0)mask|=0x10;if(y>0||hy>0)mask|=0x20;if(e.getAxisValue(MotionEvent.AXIS_LTRIGGER)>.25f||e.getAxisValue(MotionEvent.AXIS_BRAKE)>.25f)mask|=0x400;if(e.getAxisValue(MotionEvent.AXIS_RTRIGGER)>.25f||e.getAxisValue(MotionEvent.AXIS_GAS)>.25f)mask|=0x800;if(p.slot==0)nativeRightStick((int)(e.getAxisValue(MotionEvent.AXIS_Z)*32767),(int)(-e.getAxisValue(MotionEvent.AXIS_RZ)*32767));p.axes=mask;sendInputs();}return true;}return super.onGenericMotionEvent(e);}
    private static final class Screen implements GLSurfaceView.Renderer {
        private final GameActivity owner;private int lastLeaderboardMode,lastBoardProfile=-1;
        Screen(GameActivity activity){owner=activity;}
        private final ByteBuffer pixels=ByteBuffer.allocateDirect(512*480*4).order(ByteOrder.nativeOrder());private final FloatBuffer vertices=ByteBuffer.allocateDirect(16*4).order(ByteOrder.nativeOrder()).asFloatBuffer();private int program,texture,width,height,rows=480;
        private int shader(int type,String source){int s=GLES20.glCreateShader(type);GLES20.glShaderSource(s,source);GLES20.glCompileShader(s);int[] ok={0};GLES20.glGetShaderiv(s,GLES20.GL_COMPILE_STATUS,ok,0);if(ok[0]==0)throw new IllegalStateException(GLES20.glGetShaderInfoLog(s));return s;}
        @Override public void onSurfaceCreated(GL10 unused,EGLConfig config){program=GLES20.glCreateProgram();GLES20.glAttachShader(program,shader(GLES20.GL_VERTEX_SHADER,"attribute vec2 pos;attribute vec2 uv;varying mediump vec2 tc;void main(){gl_Position=vec4(pos,0.0,1.0);tc=uv;}"));GLES20.glAttachShader(program,shader(GLES20.GL_FRAGMENT_SHADER,"precision mediump float;uniform sampler2D image;varying mediump vec2 tc;void main(){gl_FragColor=texture2D(image,tc);}"));GLES20.glLinkProgram(program);int[] linked={0};GLES20.glGetProgramiv(program,GLES20.GL_LINK_STATUS,linked,0);if(linked[0]==0)throw new IllegalStateException(GLES20.glGetProgramInfoLog(program));int[] t={0};GLES20.glGenTextures(1,t,0);texture=t[0];GLES20.glBindTexture(GLES20.GL_TEXTURE_2D,texture);GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D,GLES20.GL_TEXTURE_MIN_FILTER,GLES20.GL_NEAREST);GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D,GLES20.GL_TEXTURE_MAG_FILTER,GLES20.GL_NEAREST);GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D,GLES20.GL_TEXTURE_WRAP_S,GLES20.GL_CLAMP_TO_EDGE);GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D,GLES20.GL_TEXTURE_WRAP_T,GLES20.GL_CLAMP_TO_EDGE);GLES20.glTexImage2D(GLES20.GL_TEXTURE_2D,0,GLES20.GL_RGBA,512,480,0,GLES20.GL_RGBA,GLES20.GL_UNSIGNED_BYTE,null);nativeRefresh();}
        @Override public void onSurfaceChanged(GL10 unused,int w,int h){width=w;height=h;nativeViewport(w,h);}
        @Override public void onDrawFrame(GL10 unused){int profile=nativeBoardProfile();if(profile!=lastBoardProfile){lastBoardProfile=profile;owner.runOnUiThread(()->owner.checkBoardEditor(profile));}int mode=(nativeLeaderboardMode()!=0||nativeMenuMode()!=0)?1:0;if(mode!=lastLeaderboardMode){lastLeaderboardMode=mode;owner.runOnUiThread(()->{if(owner.touch!=null)owner.touch.setLeaderboardMode(mode!=0);});}GLES20.glViewport(0,0,width,height);GLES20.glClearColor(0,0,0,1);GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT);GLES20.glBindTexture(GLES20.GL_TEXTURE_2D,texture);int updated=nativeCopyFrame(pixels);if(updated!=0){rows=updated;pixels.position(0);GLES20.glTexSubImage2D(GLES20.GL_TEXTURE_2D,0,0,0,512,rows,GLES20.GL_RGBA,GLES20.GL_UNSIGNED_BYTE,pixels);}boolean party=rows!=224&&rows!=480;float aspect=party?(float)width/height:4f/3f;float margin=party?1f:.9f;int h=(int)(height*margin),w=(int)(h*aspect);if(w>width*margin){w=(int)(width*margin);h=(int)(w/aspect);}GLES20.glViewport((width-w)/2,(height-h)/2,w,h);GLES20.glUseProgram(program);float bottom=(float)rows/480;vertices.clear();vertices.put(new float[]{-1,-1,0,bottom,1,-1,1,bottom,-1,1,0,0,1,1,1,0}).position(0);int pos=GLES20.glGetAttribLocation(program,"pos"),uv=GLES20.glGetAttribLocation(program,"uv");vertices.position(0);GLES20.glVertexAttribPointer(pos,2,GLES20.GL_FLOAT,false,16,vertices);vertices.position(2);GLES20.glVertexAttribPointer(uv,2,GLES20.GL_FLOAT,false,16,vertices);GLES20.glEnableVertexAttribArray(pos);GLES20.glEnableVertexAttribArray(uv);GLES20.glUniform1i(GLES20.glGetUniformLocation(program,"image"),0);GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP,0,4);}
    }
}
