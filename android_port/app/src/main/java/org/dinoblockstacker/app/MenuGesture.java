package org.dinoblockstacker.app;

/** Taps activate native hit regions; vertical drags scroll menu choices. */
final class MenuGesture {
    private int session,pointer=-1;
    private float downX,downY,lastY,remainder;
    private boolean dragged;
    int pointer(){return pointer;}
    boolean active(){return pointer>=0;}
    int session(){return session;}
    void cancel(){pointer=-1;session=0;remainder=0;dragged=false;}
    void begin(int mode,int id,float x,float y){cancel();session=mode;pointer=id;downX=x;downY=lastY=y;}
    int move(float x,float y){
        if(!active())return 0;
        if(Math.abs(x-downX)>10||Math.abs(y-downY)>10)dragged=true;
        if(!dragged||downY<143||downY>=395)return 0;
        remainder+=lastY-y;lastY=y;
        int rows=(int)(remainder/34);remainder-=rows*34;return Math.max(-8,Math.min(8,rows));
    }
    boolean finish(float x,float y){boolean tap=active()&&!dragged&&Math.abs(x-downX)<=10&&Math.abs(y-downY)<=10;cancel();return tap;}
}
