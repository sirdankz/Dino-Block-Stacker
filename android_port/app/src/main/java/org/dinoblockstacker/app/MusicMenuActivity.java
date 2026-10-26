package org.dinoblockstacker.app;

import android.app.Activity;

/** Music for the Android launcher and native Android layout/settings screens. */
public class MusicMenuActivity extends Activity {
    @Override protected void onResume(){super.onResume();MenuMusicPlayer.resume(this);}
    @Override protected void onPause(){MenuMusicPlayer.pause();super.onPause();}
}
