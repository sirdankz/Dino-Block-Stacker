package org.dinoblockstacker.app;
import android.view.KeyEvent;
public final class ControllerMap {
    public static int keyBit(int k){switch(k){case KeyEvent.KEYCODE_DPAD_UP:return 0x10;case KeyEvent.KEYCODE_DPAD_DOWN:return 0x20;case KeyEvent.KEYCODE_DPAD_LEFT:return 0x40;case KeyEvent.KEYCODE_DPAD_RIGHT:return 0x80;case KeyEvent.KEYCODE_BUTTON_A:case KeyEvent.KEYCODE_DPAD_CENTER:case KeyEvent.KEYCODE_Z:return 1;case KeyEvent.KEYCODE_BUTTON_X:case KeyEvent.KEYCODE_X:return 2;case KeyEvent.KEYCODE_BUTTON_B:case KeyEvent.KEYCODE_C:return 0x100;case KeyEvent.KEYCODE_BUTTON_Y:return 0x200;case KeyEvent.KEYCODE_BUTTON_START:case KeyEvent.KEYCODE_ENTER:return 8;case KeyEvent.KEYCODE_BUTTON_SELECT:case KeyEvent.KEYCODE_SPACE:return 4;case KeyEvent.KEYCODE_BUTTON_L1:case KeyEvent.KEYCODE_BUTTON_L2:return 0x400;case KeyEvent.KEYCODE_BUTTON_R1:case KeyEvent.KEYCODE_BUTTON_R2:return 0x800;default:return 0;}}
}
