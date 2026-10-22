package org.dinoblockstacker.app;

/** Leaderboard-only gestures in the native 512x480 menu coordinate system. */
final class LeaderboardGesture {
    // Matches TA_LADDER_ROW_HEIGHT in the native leaderboard layout.
    private static final float ROW_HEIGHT=36f;
    private int session, pointer=-1, button;
    private float lastY, remainder, downX, downY;
    boolean active(){return pointer>=0;}
    int pointer(){return pointer;}
    int session(){return session;}
    void cancel(){session=0;pointer=-1;button=0;remainder=0;}
    boolean begin(int mode,int id,float x,float y){
        cancel();if(mode==0||x<26||x>=486)return false;
        if(y>=143&&y<395)button=0;
        else if(y>=403&&y<435)button=x<256?1:0x100;
        else return false;
        session=mode;pointer=id;lastY=downY=y;downX=x;return true;
    }
    int move(float x,float y){
        if(!active())return 0;
        if(button!=0){if(Math.abs(x-downX)>12||Math.abs(y-downY)>12)button=-1;return 0;}
        remainder+=lastY-y;lastY=y;
        int rows=(int)(remainder/ROW_HEIGHT);remainder-=rows*ROW_HEIGHT;return Math.max(-64,Math.min(64,rows));
    }
    int finish(float x,float y){
        int action=button>0&&x>=26&&x<486&&y>=403&&y<435&&(x<256?1:0x100)==button&&Math.abs(x-downX)<=12&&Math.abs(y-downY)<=12?button:0;
        cancel();return action;
    }
}
