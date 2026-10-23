package org.dinoblockstacker.app;

import android.content.Context;
import android.graphics.*;
import android.view.*;
import android.widget.*;

/** Reusable editor; the live version overlays real boards without pausing networking. */
final class BoardLayoutEditor extends FrameLayout {
    interface Changed {void apply(BoardLayouts.State state);}
    interface Finished {void close(boolean saved);}
    private final boolean live;
    private final Changed changed;
    private final Finished closed;
    private final BoardCanvas canvas;
    private final TextView status;
    private final CheckBox full;
    private final Button save;
    private BoardLayouts.State original,state;
    private int profile,selected;
    private boolean ready,suppress;
    private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF viewport=new RectF();
    BoardLayoutEditor(Context context,int id,boolean live,Changed callback,Finished dismiss){
        super(context);this.profile=id;this.live=live;this.changed=callback;this.closed=dismiss;setClickable(true);
        if(!live)setBackgroundColor(0xff122238);
        canvas=new BoardCanvas(context);addView(canvas,new LayoutParams(-1,-1));
        LinearLayout tools=new LinearLayout(context);tools.setGravity(Gravity.CENTER_VERTICAL);tools.setPadding(dp(6),0,dp(6),0);tools.setBackgroundColor(0xee101821);
        TextView title=new TextView(context);title.setText(live?BoardLayouts.NAMES[id-1]+" | game keeps running":"Board layout preview");title.setTextColor(0xffdbe3ef);title.setTextSize(12);title.setPadding(dp(8),0,dp(8),0);tools.addView(title);
        full=new CheckBox(context);full.setText("Full screen");full.setTextColor(0xffdbe3ef);full.setTextSize(12);tools.addView(full);
        full.setOnCheckedChangeListener((button,value)->{if(suppress||!ready||profile>2)return;state.full=value;state.custom=true;fitAll();notifyChanged();});
        tools.addView(button("Smaller",()->resize(.9f)));tools.addView(button("Larger",()->resize(1.1f)));tools.addView(button("Reset",()->{state=BoardLayouts.defaults(profile,getWidth(),getHeight(),profile>2);selected=0;syncToggle();notifyChanged();}));
        tools.addView(button("Cancel",this::cancel));save=button("Save",this::save);tools.addView(save);
        HorizontalScrollView bar=new HorizontalScrollView(context);bar.setHorizontalScrollBarEnabled(false);bar.addView(tools);addView(bar,new LayoutParams(-1,dp(48),Gravity.TOP));
        status=new TextView(context);status.setTextColor(0xffdbe3ef);status.setTextSize(12);status.setGravity(Gravity.CENTER);status.setBackgroundColor(0xee101821);status.setPadding(dp(6),dp(5),dp(6),dp(5));addView(status,new LayoutParams(-1,-2,Gravity.BOTTOM));
    }
    private int dp(int v){return Math.round(v*getResources().getDisplayMetrics().density);}
    private Button button(String text,Runnable action){Button b=new Button(getContext());b.setText(text);b.setTextSize(12);b.setAllCaps(false);b.setOnClickListener(v->action.run());return b;}
    @Override protected void onSizeChanged(int w,int h,int oldw,int oldh){super.onSizeChanged(w,h,oldw,oldh);if(w<=0||h<=0)return;if(!ready){original=BoardLayouts.load(getContext(),profile,w,h);state=original.copy();ready=true;syncToggle();}fitAll();refresh();}
    void setProfile(int id){if(ready)changed.apply(original.copy());profile=id;selected=0;ready=false;post(()->{if(getWidth()>0){original=BoardLayouts.load(getContext(),profile,getWidth(),getHeight());state=original.copy();ready=true;syncToggle();fitAll();refresh();}});}
    private void syncToggle(){suppress=true;full.setChecked(state.full);full.setVisibility(profile<=2?VISIBLE:GONE);suppress=false;}
    private void viewBounds(){float w=getWidth(),h=getHeight();if(profile>2||state.full)viewport.set(0,0,w,h);else{float ih=Math.min(h*.9f,w*.9f*3/4),iw=ih*4/3;viewport.set((w-iw)*.5f,(h-ih)*.5f,(w+iw)*.5f,(h+ih)*.5f);}}
    private float header(){return profile>2?Math.max(.025f,8f/sourceRows()):0;}
    private float sourceRows(){if(profile<=2&&!state.full)return 224;int rows=Math.max(128,Math.min(384,512*getHeight()/Math.max(1,getWidth())));return rows==224?226:rows;}
    private float aspect(int slot){return 7f/12*(profile>2&&slot==0?224f/208:1);}
    private void fit(int slot){viewBounds();float view=viewport.width()/Math.max(1,viewport.height()),hn=header();int o=slot*3;float h=clamp(state.pose[o+2],.12f,Math.min(1-hn,view/aspect(slot))),w=h*aspect(slot)/view;state.pose[o+2]=h;state.pose[o]=clamp(state.pose[o],w/2,1-w/2);state.pose[o+1]=clamp(state.pose[o+1],hn+h/2,1-h/2);}
    private void fitAll(){if(!ready)return;for(int i=0;i<BoardLayouts.count(profile);i++)fit(i);}
    private RectF rect(int slot,boolean label){viewBounds();int o=slot*3;float h=state.pose[o+2]*viewport.height(),w=h*aspect(slot);float cx=viewport.left+state.pose[o]*viewport.width(),cy=viewport.top+state.pose[o+1]*viewport.height();return new RectF(cx-w/2,cy-h/2-(label?header()*viewport.height():0),cx+w/2,cy+h/2);}
    private boolean overlap(){for(int i=0;i<BoardLayouts.count(profile);i++)for(int j=i+1;j<BoardLayouts.count(profile);j++){RectF a=rect(i,true),b=rect(j,true);a.inset(1,1);b.inset(1,1);if(RectF.intersects(a,b))return true;}return false;}
    private void refresh(){if(!ready)return;boolean clash=overlap();save.setEnabled(!clash);status.setText(clash?"Boards overlap - drag them apart before saving.":(selected==0?"Your board":"Opponent "+selected)+" | drag to move | pinch or use Smaller / Larger | Save keeps this mode only");canvas.invalidate();}
    private void notifyChanged(){if(!ready)return;fitAll();changed.apply(state.copy());refresh();}
    private void resize(float factor){if(!ready)return;state.custom=true;state.pose[selected*3+2]*=factor;fit(selected);notifyChanged();}
    void cancel(){if(ready)changed.apply(original.copy());closed.close(false);}
    private void save(){if(!ready||overlap())return;if(!BoardLayouts.save(getContext(),state)){Toast.makeText(getContext(),"Layout could not be saved",Toast.LENGTH_SHORT).show();return;}changed.apply(state.copy());original=state.copy();Toast.makeText(getContext(),"Saved: "+BoardLayouts.NAMES[profile-1],Toast.LENGTH_SHORT).show();closed.close(true);}
    private static float clamp(float v,float lo,float hi){return Math.max(lo,Math.min(hi,v));}
    private final class BoardCanvas extends View {
        private int pointer=-1;private float dx,dy;
        private final ScaleGestureDetector pinch;
        BoardCanvas(Context c){super(c);pinch=new ScaleGestureDetector(c,new ScaleGestureDetector.SimpleOnScaleGestureListener(){@Override public boolean onScale(ScaleGestureDetector d){resize(d.getScaleFactor());return true;}});}
        @Override protected void onDraw(Canvas c){if(!ready)return;boolean clash=overlap();viewBounds();
            if(!live){paint.setStyle(Paint.Style.FILL);paint.setColor(0xff1c3550);c.drawRect(viewport,paint);}
            for(int i=BoardLayouts.count(profile)-1;i>=0;i--){RectF r=rect(i,false);if(!live){paint.setStyle(Paint.Style.FILL);paint.setColor(i==0?0xff315475:0xff24425c);c.drawRect(r,paint);}
                paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(dp(i==selected?3:1));paint.setColor(clash?0xfff1a0aa:i==selected?0xffffd166:0xff7dd9e8);c.drawRect(r,paint);
                paint.setStyle(Paint.Style.FILL);paint.setTextSize(dp(13));paint.setTypeface(Typeface.DEFAULT_BOLD);c.drawText(i==0?"YOU":"OPPONENT "+i,r.left+dp(5),r.top+dp(18),paint);
            }
        }
        @Override public boolean onTouchEvent(android.view.MotionEvent e){if(!ready)return true;pinch.onTouchEvent(e);int a=e.getActionMasked();
            if(a==MotionEvent.ACTION_DOWN){pointer=-1;for(int i=0;i<BoardLayouts.count(profile);i++)if(rect(i,false).contains(e.getX(),e.getY())){selected=i;pointer=e.getPointerId(0);break;}if(pointer<0)return true;viewBounds();dx=e.getX()-(viewport.left+state.pose[selected*3]*viewport.width());dy=e.getY()-(viewport.top+state.pose[selected*3+1]*viewport.height());refresh();}
            else if(a==MotionEvent.ACTION_MOVE&&!pinch.isInProgress()&&e.getPointerCount()==1){int n=e.findPointerIndex(pointer);if(n>=0){viewBounds();state.custom=true;state.pose[selected*3]=(e.getX(n)-dx-viewport.left)/Math.max(1,viewport.width());state.pose[selected*3+1]=(e.getY(n)-dy-viewport.top)/Math.max(1,viewport.height());fit(selected);notifyChanged();}}
            else if(a==MotionEvent.ACTION_UP||a==MotionEvent.ACTION_CANCEL||a==MotionEvent.ACTION_POINTER_UP)pointer=-1;return true;
        }
    }
}
