# Menu artwork

The original 512x480 RGBA PNGs define the shared Original Xbox, Xbox 360, and Android menu skin:

- `tetris_general_background.png`: opaque, tileable backdrop.
- `tetris_general_menu1.png`: fixed outer frame and population tab.
- `tetris_general_menu2.png`: fixed content panel.

The original PNGs remain unchanged. The compiled skin omits the inner panel's flat black fill so the outer frame's translucent surface continues through the header, content area, and footer. Border outlines and dotted trim retain their original artwork. Legacy rectangle shading is disabled for this skin; selected items use the shared highlight. The backdrop scrolls diagonally by one source pixel every 50 ms and wraps at the image edges. Menu text, selection highlights, counters, and input remain live above the artwork. Menu animation does not run during gameplay.

`src/xbox/ta_menu_sprites.h` contains the generated lossless palettes and row runs. Regenerate it after replacing PNGs with `python tools/compile_menu_art.py` (Pillow required). Runtime composition uses cached pixels and palette lookups, with no image decoding or extra emulator frames.

Menu text and highlights use the inset regions in `netplay/src/ta_vs_menu_layout.h`. Bounded labels tighten character spacing or reduce font size as needed. Footer lines use source Y410 and Y432. Six leaderboard rows start at Y178 with 34px pitch; 34x34 emblems remain inside the content frame. Match-option portraits use 80px copies; the character picker retains its 96px preview.

The revised outer-frame tab has a clear interior at X78..433, Y10..23. Menu population glyphs use their original 10x14 size at source Y10, with no background panel. Long counter values use compact labels to stay inside the horizontal trim.
