package org.dinoblockstacker.app;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.view.MotionEvent;
import android.view.View;
import java.util.ArrayList;

/** Shared gameplay overlay and per-button touch layout editor. No native code required. */
public final class TouchPad extends View {
    public interface MaskListener { void onMaskChanged(int mask); }
    public interface SelectionListener { void onSelectionChanged(String label, int percent); }

    private static final String PREFS = "dino_touch_layout_v1";
    private final SharedPreferences preferences;
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final ArrayList<Control> controls = new ArrayList<>();
    private final MaskListener maskListener;
    private SelectionListener selectionListener;
    private float unit;
    private int pressedMask;
    private boolean editing;
    private int selected = -1;
    private int dragPointer = -1;
    private float dragDx, dragDy;

    private static final class Control {
        final String id, label;
        final int bit;
        final float defaultX, defaultY, baseWidth, baseHeight;
        final RectF rect = new RectF();
        float nx, ny, scale = 1f;
        Control(String name, String text, int flag, float x, float y, float width, float height, int viewW, int viewH) {
            id = name; label = text; bit = flag;
            defaultX = x / viewW; defaultY = y / viewH;
            baseWidth = width; baseHeight = height;
            nx = defaultX; ny = defaultY;
        }
    }

    public TouchPad(Context context, MaskListener callback) {
        super(context);
        preferences = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        maskListener = callback;
        setFocusable(false);
        setClickable(true);
    }

    public void setEditing(boolean enabled) {
        editing = enabled;
        dragPointer = -1;
        clearPressed();
        invalidate();
    }

    public void setSelectionListener(SelectionListener listener) {
        selectionListener = listener;
        notifySelection();
    }

    private static float clamp(float value, float low, float high) {
        return Math.min(high, Math.max(low, value));
    }

    private void add(String id, String label, int bit, float x, float y, float width, float height, int w, int h) {
        Control c = new Control(id, label, bit, x, y, width, height, w, h);
        c.nx = preferences.getFloat(id + ".x", c.defaultX);
        c.ny = preferences.getFloat(id + ".y", c.defaultY);
        c.scale = clamp(preferences.getFloat(id + ".scale", 1f), .55f, 2.5f);
        controls.add(c);
        updateRect(c);
    }

    private void updateRect(Control c) {
        int width = getWidth(), height = getHeight();
        if (width <= 0 || height <= 0) return;
        float halfW = c.baseWidth * c.scale * .5f;
        float halfH = c.baseHeight * c.scale * .5f;
        float px = clamp(c.nx * width, Math.min(halfW, width * .5f), Math.max(width - halfW, width * .5f));
        float py = clamp(c.ny * height, Math.min(halfH, height * .5f), Math.max(height - halfH, height * .5f));
        c.nx = px / width;
        c.ny = py / height;
        c.rect.set(px - halfW, py - halfH, px + halfW, py + halfH);
    }

    private void buildDefaultControls(boolean loadSaved) {
        int w = getWidth(), h = getHeight();
        if (w <= 0 || h <= 0) return;
        float image = Math.min(w * .9f, h * .9f * 4f / 3f), margin = (w - image) * .5f;
        unit = Math.min(h / 9f, Math.max(margin / 3.2f, h / 14f));
        float cx = Math.max(unit * 1.7f, margin / 2f);
        float cy = h - unit * 2.2f;
        float right = w - cx;
        controls.clear();
        add("up", "UP", 0x10, cx, cy - unit, unit, unit, w, h);
        add("down", "DOWN", 0x20, cx, cy + unit, unit, unit, w, h);
        add("left", "LEFT", 0x40, cx - unit, cy, unit, unit, w, h);
        add("right", "RIGHT", 0x80, cx + unit, cy, unit, unit, w, h);
        add("a", "A", 1, right, cy + unit, unit, unit, w, h);
        add("b", "B", 0x100, right + unit, cy, unit, unit, w, h);
        add("x", "X", 2, right - unit, cy, unit, unit, w, h);
        add("y", "Y", 0x200, right, cy - unit, unit, unit, w, h);
        add("l", "L", 0x400, cx, cy - unit * 2.4f, unit * 1.8f, unit * .7f, w, h);
        add("r", "R", 0x800, right, cy - unit * 2.4f, unit * 1.8f, unit * .7f, w, h);
        add("back", "BACK", 4, w * .44f, h - unit * .6f, unit * 1.8f, unit * .7f, w, h);
        add("start", "START", 8, w * .56f, h - unit * .6f, unit * 1.8f, unit * .7f, w, h);
        if (!loadSaved) for (Control c : controls) { c.nx = c.defaultX; c.ny = c.defaultY; c.scale = 1f; updateRect(c); }
        selected = -1;
        notifySelection();
        invalidate();
    }

    @Override protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        buildDefaultControls(true);
    }

    /** Reset the editor's pending layout only. The user must press Save to persist. */
    public void resetToDefaults() {
        buildDefaultControls(false);
    }

    public void saveLayout() {
        SharedPreferences.Editor out = preferences.edit();
        for (Control c : controls) {
            out.putFloat(c.id + ".x", c.nx);
            out.putFloat(c.id + ".y", c.ny);
            out.putFloat(c.id + ".scale", c.scale);
        }
        out.apply();
    }

    public void resizeSelected(float factor) {
        if (selected < 0 || selected >= controls.size()) return;
        Control c = controls.get(selected);
        c.scale = clamp(c.scale * factor, .55f, 2.5f);
        updateRect(c);
        notifySelection();
        invalidate();
    }

    public void clearPressed() {
        if (pressedMask != 0) {
            pressedMask = 0;
            if (maskListener != null) maskListener.onMaskChanged(0);
        }
        invalidate();
    }

    private void notifySelection() {
        if (selectionListener == null) return;
        if (selected < 0 || selected >= controls.size()) selectionListener.onSelectionChanged("Select a button", 100);
        else {
            Control c = controls.get(selected);
            selectionListener.onSelectionChanged(c.label, Math.round(c.scale * 100f));
        }
    }

    private int controlAt(float x, float y) {
        for (int i = controls.size() - 1; i >= 0; --i) if (controls.get(i).rect.contains(x, y)) return i;
        return -1;
    }

    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        paint.setTypeface(Typeface.create(Typeface.MONOSPACE, Typeface.BOLD));
        paint.setTextAlign(Paint.Align.CENTER);
        for (int i = 0; i < controls.size(); i++) {
            Control c = controls.get(i);
            paint.setStyle(Paint.Style.FILL);
            paint.setColor(editing ? 0xc02d4054 : ((pressedMask & c.bit) != 0 ? 0xc0457080 : 0x97080d14));
            canvas.drawRoundRect(c.rect, unit * .18f, unit * .18f, paint);
            if (editing && i == selected) {
                paint.setStyle(Paint.Style.STROKE);
                paint.setStrokeWidth(Math.max(2f, unit * .045f));
                paint.setColor(0xff7dd9e8);
                canvas.drawRoundRect(c.rect, unit * .18f, unit * .18f, paint);
                paint.setStyle(Paint.Style.FILL);
            }
            paint.setColor(c.bit == 1 ? 0xff96e3b2 : c.bit == 0x100 ? 0xfff1a0aa : c.bit == 0x200 ? 0xfff1cb72 : 0xffdbe3ef);
            float font = Math.min(c.rect.height() * .38f, c.rect.width() / Math.max(2.1f, c.label.length() * .65f));
            paint.setTextSize(Math.max(9f, font));
            canvas.drawText(c.label, c.rect.centerX(), c.rect.centerY() - (paint.ascent() + paint.descent()) * .5f, paint);
        }
    }

    @Override public boolean onTouchEvent(MotionEvent event) {
        int action = event.getActionMasked();
        if (editing) {
            if (action == MotionEvent.ACTION_DOWN) {
                selected = controlAt(event.getX(), event.getY());
                dragPointer = selected < 0 ? -1 : event.getPointerId(0);
                if (selected >= 0) {
                    Control c = controls.get(selected);
                    dragDx = c.rect.centerX() - event.getX();
                    dragDy = c.rect.centerY() - event.getY();
                }
                notifySelection();
                invalidate();
            } else if (action == MotionEvent.ACTION_MOVE && dragPointer != -1 && selected >= 0) {
                int pointer = event.findPointerIndex(dragPointer);
                if (pointer >= 0) {
                    Control c = controls.get(selected);
                    c.nx = (event.getX(pointer) + dragDx) / Math.max(1f, getWidth());
                    c.ny = (event.getY(pointer) + dragDy) / Math.max(1f, getHeight());
                    updateRect(c);
                    invalidate();
                }
            } else if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL ||
                    (action == MotionEvent.ACTION_POINTER_UP && event.getPointerId(event.getActionIndex()) == dragPointer)) {
                dragPointer = -1;
            }
            return true;
        }
        int mask = 0;
        int skip = action == MotionEvent.ACTION_POINTER_UP ? event.getActionIndex() : -1;
        if (action != MotionEvent.ACTION_CANCEL && action != MotionEvent.ACTION_UP) {
            for (int i = 0; i < event.getPointerCount(); i++) {
                if (i == skip) continue;
                int index = controlAt(event.getX(i), event.getY(i));
                if (index >= 0) mask |= controls.get(index).bit;
            }
        }
        if (pressedMask != mask) {
            pressedMask = mask;
            if (maskListener != null) maskListener.onMaskChanged(mask);
        }
        invalidate();
        return true;
    }
}
