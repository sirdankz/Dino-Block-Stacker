# Original music

Composed and owned by **sirdankz**.

- `Ness_2.wav`: frontend menus, including rooms, profiles, chat, settings and leaderboards.
- `Ness_2ResultsScreen.wav`: 1v1 and party results/rematch screens.

The original 48 kHz, stereo, 16-bit PCM exports are preserved unchanged. Build preparation creates streamed playback copies with a 100 ms tail/head crossfade. Playback copies omit the overlapped 100 ms so the final sample joins the start without replaying the crossfade region. Scene changes use short volume ramps.

Music use is limited to Dino Block Stacker and forks derived from this port under [LICENSE-MUSIC.txt](LICENSE-MUSIC.txt). Reuse in unrelated projects requires separate permission.
