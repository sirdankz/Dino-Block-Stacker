package org.dinoblockstacker.app;

import android.app.Activity;
import android.graphics.Color;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.HorizontalScrollView;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

/** Customize each on-screen button without starting or pausing the game. */
public final class TouchLayoutActivity extends Activity {
    private TouchPad touchPad;
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN |
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE);

        FrameLayout root = new FrameLayout(this);
        root.setBackgroundColor(0xff101821);
        touchPad = new TouchPad(this, null);
        touchPad.setEditing(true);
        root.addView(touchPad, new FrameLayout.LayoutParams(-1, -1));

        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setBackgroundColor(0xe8101821);
        int pad = Math.round(getResources().getDisplayMetrics().density * 5f);
        row.setPadding(pad, pad, pad, pad);
        TextView selection = new TextView(this);
        selection.setTextColor(Color.WHITE);
        selection.setTextSize(13);
        selection.setMinWidth(Math.round(getResources().getDisplayMetrics().density * 105f));
        row.addView(selection);
        touchPad.setSelectionListener((label, percent) -> selection.setText(label + "  " + percent + "%"));

        row.addView(button("Smaller", v -> touchPad.resizeSelected(.85f)));
        row.addView(button("Larger", v -> touchPad.resizeSelected(1.15f)));
        row.addView(button("Reset", v -> { touchPad.resetToDefaults(); Toast.makeText(this, "Defaults restored in editor. Save to keep them.", Toast.LENGTH_SHORT).show(); }));
        row.addView(button("Cancel", v -> finish()));
        row.addView(button("Save", v -> { touchPad.saveLayout(); Toast.makeText(this, "Touch layout saved", Toast.LENGTH_SHORT).show(); finish(); }));

        HorizontalScrollView toolbar = new HorizontalScrollView(this);
        toolbar.setHorizontalScrollBarEnabled(false);
        toolbar.addView(row);
        FrameLayout.LayoutParams top = new FrameLayout.LayoutParams(-1, -2, Gravity.TOP);
        root.addView(toolbar, top);

        TextView hint = new TextView(this);
        hint.setText("Tap a button to select it. Drag to move. Use Smaller / Larger to resize each button.");
        hint.setTextSize(12);
        hint.setGravity(Gravity.CENTER);
        hint.setTextColor(0xffdbe3ef);
        hint.setBackgroundColor(0xe8101821);
        FrameLayout.LayoutParams hintLayout = new FrameLayout.LayoutParams(-1, -2, Gravity.TOP);
        hintLayout.topMargin = Math.round(getResources().getDisplayMetrics().density * 56f);
        root.addView(hint, hintLayout);
        setContentView(root);
    }

    private Button button(String text, View.OnClickListener callback) {
        Button button = new Button(this);
        button.setText(text);
        button.setTextSize(12);
        button.setAllCaps(false);
        button.setOnClickListener(callback);
        return button;
    }
}
