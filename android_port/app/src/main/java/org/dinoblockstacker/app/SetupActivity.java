package org.dinoblockstacker.app;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.view.Gravity;
import android.widget.*;
import java.io.*;
import java.security.MessageDigest;

/** Android-only ROM import. The game and every N17 menu stay native/shared. */
public final class SetupActivity extends MusicMenuActivity {
    private static final int PICK_ROM=17;
    private static final String ROM_HASH="579bf1a1988d4af06a69cc1d82a2478ebe51940c5ced7f97e83029a24e6aa778";
    private Button play,choose;private TextView status;private Spinner touchMode;
    @Override public void onCreate(Bundle b){super.onCreate(b);
        LinearLayout box=new LinearLayout(this);box.setOrientation(1);box.setGravity(Gravity.CENTER);box.setPadding(36,16,36,16);box.setBackgroundColor(0xff101821);
        TextView title=new TextView(this);title.setText("DINO BLOCK STACKER");title.setTextSize(26);title.setTextColor(0xff7dd9e8);title.setGravity(Gravity.CENTER);box.addView(title);
        status=new TextView(this);status.setTextSize(16);status.setTextColor(0xffdbe3ef);status.setGravity(Gravity.CENTER);status.setPadding(0,8,0,12);box.addView(status);
        choose=new Button(this);choose.setText("Import compatible game ROM");choose.setOnClickListener(v->{Intent i=new Intent(Intent.ACTION_OPEN_DOCUMENT);i.addCategory(Intent.CATEGORY_OPENABLE);i.setType("*/*");startActivityForResult(i,PICK_ROM);});box.addView(choose);
        play=new Button(this);play.setText("Play / Online / LAN");play.setOnClickListener(v->{int mode=touchMode.getSelectedItemPosition();getPreferences(0).edit().putInt("touch_mode",mode).apply();Intent i=new Intent(this,GameActivity.class);i.putExtra("touch_mode",mode);startActivity(i);});box.addView(play);
        TextView touchLabel=new TextView(this);touchLabel.setText("ON-SCREEN CONTROLS");touchLabel.setTextColor(0xffdbe3ef);touchLabel.setTextSize(14);box.addView(touchLabel);
        touchMode=new Spinner(this);String[] modes={"Always show (recommended)","Auto-hide with physical controller","Off"};ArrayAdapter<String> modeAdapter=new ArrayAdapter<>(this,android.R.layout.simple_spinner_item,modes);modeAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);touchMode.setAdapter(modeAdapter);touchMode.setSelection(Math.min(2,Math.max(0,getPreferences(0).getInt("touch_mode",0))));box.addView(touchMode);
        Button customize=new Button(this);customize.setText("Customize touch button layout");customize.setOnClickListener(v->startActivity(new Intent(this,TouchLayoutActivity.class)));box.addView(customize);
        Button boards=new Button(this);boards.setText("Customize board layouts");boards.setOnClickListener(v->startActivity(new Intent(this,BoardLayoutActivity.class)));box.addView(boards);
        TextView hint=new TextView(this);hint.setText("USB / Bluetooth controller: D-pad or left stick moves | A selects | B returns\nUse CONTROLS in the game menu to rebind gameplay and Cycle Junk.\nXbox / Android public rooms, accounts, rankings and LAN matches.\nFirst launch prepares original game data once. No ROM is included.");hint.setTextColor(0xffbdc7d5);hint.setTextSize(14);hint.setGravity(Gravity.CENTER);hint.setPadding(0,10,0,0);box.addView(hint);
        ScrollView scroll=new ScrollView(this);scroll.setFillViewport(true);scroll.addView(box);setContentView(scroll);update();
    }
    private void update(){boolean ready=new File(getFilesDir(),"tetris_attack.sfc").length()==1048576;play.setEnabled(ready);status.setText(ready?"Dino Block Stacker | ROM ready":"Import the compatible, unheadered 1 MB ROM to begin.");}
    @Override protected void onActivityResult(int request,int result,Intent data){super.onActivityResult(request,result,data);if(request!=PICK_ROM||result!=RESULT_OK||data==null||data.getData()==null)return;Uri uri=data.getData();choose.setEnabled(false);play.setEnabled(false);status.setText("Checking your ROM...");
        new Thread(()->{File temp=new File(getFilesDir(),"rom-import.tmp");String error=null;
            try(InputStream in=getContentResolver().openInputStream(uri);FileOutputStream out=new FileOutputStream(temp)){
                if(in==null)throw new IOException("Cannot open selected file");MessageDigest sha=MessageDigest.getInstance("SHA-256");byte[] buffer=new byte[32768];int count,total=0;
                while((count=in.read(buffer))!=-1){total+=count;if(total>1048576)throw new IOException("Select the unheadered 1 MB .sfc ROM, not a ZIP");out.write(buffer,0,count);sha.update(buffer,0,count);}out.getFD().sync();StringBuilder hash=new StringBuilder();for(byte x:sha.digest())hash.append(String.format(java.util.Locale.ROOT,"%02x",x&255));
                if(total!=1048576||!ROM_HASH.equals(hash.toString()))throw new IOException("This ROM does not match the Xbox version. Select the compatible unheadered ROM listed in the README.");
            }catch(Exception e){error=e.getMessage();}
            if(error==null&&!temp.renameTo(new File(getFilesDir(),"tetris_attack.sfc")))error="Could not save imported ROM.";if(error!=null)temp.delete();final String message=error;runOnUiThread(()->{choose.setEnabled(true);update();if(message!=null)status.setText(message);});
        },"ROM import").start();
    }
}
