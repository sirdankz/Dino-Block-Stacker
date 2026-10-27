package org.dinoblockstacker.app;

import android.content.Context;
import android.content.SharedPreferences;

/** Device-local display settings. No gameplay or network state is stored here. */
final class BoardLayouts {
    static final String[] NAMES={"Ranked 1v1","LAN 1v1","Auto target | 2 players","Manual target | 2 players","Auto target | 3 players","Manual target | 3 players","Auto target | 4 players","Manual target | 4 players"};
    private static final String PREFS="dino_board_layouts_v1";
    static final class State {
        int id;boolean custom,full;float[] pose;
        State copy(){State s=new State();s.id=id;s.custom=custom;s.full=full;s.pose=pose.clone();return s;}
    }
    static int count(int id){return id<=2?2:2+(id-3)/2;}
    static State load(Context c,int id,int w,int h){
        SharedPreferences p=c.getSharedPreferences(PREFS,Context.MODE_PRIVATE);State s=defaults(id,w,h,id>2||p.getBoolean(id+".full",false));
        if(!p.getBoolean(id+".custom",false))return s;
        s.custom=true;for(int i=0;i<12;i++){float value=p.getFloat(id+"."+i,s.pose[i]);if(!Float.isNaN(value)&&!Float.isInfinite(value))s.pose[i]=value;}
        return s;
    }
    static State defaults(int id,int w,int h,boolean full){State s=new State();s.id=id;s.full=id>2||full;s.pose=GameActivity.nativeBoardDefaults(id,w,h,s.full);if(s.pose==null)s.pose=new float[12];return s;}
    static void apply(State s){GameActivity.nativeBoardLayout(s.id,s.custom,s.full,s.pose);}
    static boolean save(Context c,State s){
        SharedPreferences.Editor p=c.getSharedPreferences(PREFS,Context.MODE_PRIVATE).edit();p.putBoolean(s.id+".custom",s.custom);p.putBoolean(s.id+".full",s.full);
        for(int i=0;i<12;i++)p.putFloat(s.id+"."+i,s.pose[i]);return p.commit();
    }
    static void loadAll(Context c,int w,int h){for(int id=1;id<=8;id++)apply(load(c,id,w,h));}
}
