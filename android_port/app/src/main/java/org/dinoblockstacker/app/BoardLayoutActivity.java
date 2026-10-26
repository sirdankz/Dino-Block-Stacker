package org.dinoblockstacker.app;

import android.app.Activity;
import android.os.Bundle;
import android.view.*;
import android.widget.*;

/** Pre-match editor for all independent mobile board layouts. */
public final class BoardLayoutActivity extends MusicMenuActivity {
    private BoardLayoutEditor editor;
    @Override public void onCreate(Bundle state){super.onCreate(state);FrameLayout root=new FrameLayout(this);
        editor=new BoardLayoutEditor(this,1,false,s->{},saved->{if(!saved)finish();});root.addView(editor);
        Spinner modes=new Spinner(this);ArrayAdapter<String> adapter=new ArrayAdapter<>(this,android.R.layout.simple_spinner_item,BoardLayouts.NAMES);adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);modes.setAdapter(adapter);modes.setBackgroundColor(0xffdbe3ef);
        FrameLayout.LayoutParams picker=new FrameLayout.LayoutParams(Math.round(320*getResources().getDisplayMetrics().density),-2,Gravity.TOP|Gravity.LEFT);picker.topMargin=Math.round(50*getResources().getDisplayMetrics().density);root.addView(modes,picker);
        modes.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener(){public void onItemSelected(AdapterView<?>p,View v,int position,long id){editor.setProfile(position+1);}public void onNothingSelected(AdapterView<?>p){}});
        setContentView(root);getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN|View.SYSTEM_UI_FLAG_HIDE_NAVIGATION|View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
    }
    @Override public void onBackPressed(){editor.cancel();}
}
