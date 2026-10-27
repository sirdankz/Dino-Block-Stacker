# Changelog

## 1.2.9

- Enclose all three event-entry lines in the selection highlight and matching touch area.
- Rename public party hosting choices to 2-4 PLAYERS RANK - AUTO TARGET and 2-4 PLAYERS RANK - MANUAL TARGET.
- Label the rebindable junk-recipient control TARGET for both player ports.
- Use Auto Target and Manual Target labels in public room lists, lobbies and Android layout settings.

## 1.2.8

- Update time-setup wording and offer manual current date/time entry without selecting a location.
- Save a 12-hour/24-hour preference per profile and apply it to event dates, availability and time controls.
- Use Y as space when entering event text; expose console case switching on L.
- Rebalance both egg-tier scales around the existing 1,000 rating baseline, preserving points and records.
- Add progress toward the next egg division and a rank guide.

## 1.2.7

- Replace fixed GMT/UTC timezone choices with geographic locations and familiar US/Canada choices.
- Apply local daylight-saving rules automatically, including non-DST locations such as Arizona and Hawaii.
- Require legacy fixed-offset profiles to select a location once, without guessing from their offset.
- Repair legacy fixed-offset recurring events after location selection while preserving one-time event instants.

## 1.2.6

- Add Events with localized dates, repeating days, interested-player lists, and organizer editing/cancellation.
- Require online profiles to choose an IANA time zone. Event dates and weekly availability use server time and daylight-saving rules.
- Add public player profiles, optional Discord usernames, and overnight weekly availability.
- Display 14 compact chat messages with blue usernames, a scrollbar, paged history, touch scrolling, and sender profile access.

## 1.2.5

- Set first-run and restored menu/results music to 10% and game audio to 50%, retaining 100% master volume.
- Preserve existing saved volume settings and use the same fallback music level on the Android launcher.

## 1.2.4

- Add Windows GRAPHICS OPTIONS with Automatic, OpenGL, Direct3D 11, Direct3D 12 and Software modes.
- Apply and save renderer choices without restarting the game or room; retain the current renderer if a choice is unavailable.
- Keep automatic stutter recovery in Automatic mode and preserve manual selections across launches.
- Migrate the previous OpenGL preference and show the active renderer and save/application status in the menu.

## 1.2.3

- Detect repeated Windows graphics stalls and recover through OpenGL without resetting game or room state.
- Save the OpenGL preference after successful presentation and restore it on later launches.
- Preserve board textures, text entry and display settings during recovery; retain the existing renderer if replacement fails.
- Ignore startup, resizing and focus changes when detecting stalls; respect explicit renderer overrides.

## 1.2.2

- Correct panel identities and preview symbols for Circle and Upside-down Triangle.
- Drive Windows menu/results music from the audio device callback and buffer original game sound independently of presentation.
- Remove additional frame-limiter waits after overdue frames and rotate frame textures to reduce upload contention.
- Update Windows population and 1v1 opponent textures only when changed; add optional frame timing capture.

## 1.2.1

- Group controls, volume and block-color settings under OPTIONS on all four ports.
- Add PC-only fullscreen/windowed mode and window-size presets, retaining F11 and Alt+Enter shortcuts.
- Save independent master, menu/results music and game audio volume levels locally.
- Add Classic, Pastel, Sunset, Ocean, Candy and High Contrast block-color presets with per-preset swaps, rotation and reset.
- Recolor stationary and animated block palettes without altering guest state, gameplay timing, matching rules or background artwork; preserve the READY fade.
- Support direct mouse/touch adjustment buttons and scrolling color editors.

## 1.2.0

- Move Windows route/adapter refresh to a background worker so presence checks cannot wait on the render/input thread.
- Use precise nanosecond frame deadlines and limit catch-up after long stalls on PC.
- Cache the PC menu/results soundtrack at startup, use a precise playback clock, and retain queued audio across temporary output delays.

## 1.1.9

- Play the original Ness_2 track throughout menus and Ness_2ResultsScreen throughout results and rematch screens on all four ports.
- Crossfade loop boundaries over 100 ms and fade scene changes over 150 ms, with streamed PCM playback on native frontends.
- Include original recordings, generated playback loops and a separate music license crediting sirdankz and limiting reuse to this port and its forks.
- Move the tiled menu background down and left like falling rain.

## 1.1.8

- Add the Windows x64 port with shared menus, artwork, local play, LAN, online 1v1/party modes, accounts, chat, ranks and rematches.
- Support keyboard, mouse, two gamepads, direct text entry, fullscreen and local Windows saves.
- Confirm lobby, waiting-queue and rematch-room exits with Stay selected initially; keep networking active during the prompt.
- Use START to ready players and begin party rounds automatically when everyone is ready.
- Separate B BACK and OPEN CHAT labels and mouse/touch targets across all four ports.
- Add Windows exploit mitigations and bound clipboard and battery-save reads.

## 1.1.7

- Show the winning player name beside the supplied eight-frame animated trophy on 1v1 and party results screens across all three ports.
- Preserve party winner identity while rematch votes and room membership change; simultaneous top-outs automatically replay without recording a rating result.
- Apply the revised menu frame and restore the original 10x14 counter lettering inside its dotted tab.
- Reflow party result rows and move touchscreen speed controls with their labels.

## 1.1.6

- Add the Xbox 360 port with shared console menus, artwork, offline play, LAN, online modes, chat, profiles, leaderboards, and rematches.
- Support persistent Xbox 360 saves on writable game folders and Xenia saved-game containers.
- Preserve native network-order IPv4 bytes for Xbox 360 public-room and party connections.

- Use a compact OPEN CHAT legend with a black Back/Select icon; separate status text from button prompts and remove repeated option-card prompts.

- Open World Chat from Browse Rooms with BACK and choose Lobby Chat or World Chat in waiting lobbies.
- Keep lobby connections responsive during chat and Android keyboard input; return when a match starts or room membership changes.
- Return lone remaining players to a public lobby as host after preserving match results.
- Add Android menu taps and swipe navigation, including rooms, profiles, keyboards, options, lobby controls, and rematches.
- Guard touch actions across menu transitions, result input delays, and refreshed room lists.

## 1.1.5

- Inset menu text and highlights from border outlines, dots, and bevels.
- Fit footer instructions and action highlights inside the footer panel.
- Reflow six leaderboard rows, rank icons, columns, and scrollbar within the content panel.
- Bound long labels and status messages; fit match-option portraits within their panels.

## 1.1.4

- Use one continuous translucent menu surface without stacked content or footer shades.
- Preserve border artwork, dotted trim, the moving background, and selection highlights.
- Reduce menu counter lettering and inset it away from the dotted top-tab trim.

## 1.1.3

- Remove the counter background panel on Xbox and Android, preserving the underlying artwork and existing text placement.

## 1.1.2

- Inset menu population counters within the top tab without covering its border.
- Match Xbox counter placement and scaling to the menu frame.
- Retain the counter font size and compact labels.

## 1.1.1

- Add supplied pixel-art menu frames and panels on Xbox and Android.
- Scroll the tileable menu background diagonally while frames remain fixed.
- Preserve artwork transparency and cache composition between animation steps.
- Fit population counters inside the new top tab without reducing font size.

## 1.1.0

- Expand Android gameplay touch targets beyond the visible button edges.
- Prefer direct taps and select the nearest button when expanded areas overlap.
- Preserve multi-touch input, saved control layouts, and precise editor selection.

## 1.0.9

- Restore the original offline board arrangement for Android ranked and LAN 1v1 by default.
- Add saved Android board positioning and resizing for each multiplayer mode and player count.
- Add a pre-match layout editor and a live editor through Android Back â†’ Board layout.
- Use the local character background to fill party scenery on Xbox and Android.
- Retain cached opponent updates while editing layout geometry.

## 1.0.8

- Display active and full rooms with clear availability labels.
- Add next-round waiting slots and join notifications to party rooms.
- Require sign-in for party hosting and joining; clear stale authentication on reconnection.
- Wait for server-confirmed 1v1 departures and preserve the results screen.
- Reopen an advertised 1v1 room after an opponent leaves.

## 1.0.7

- Replace all six temporary rank emblems with supplied pixel artwork at 34x34.
- Use the star emblem for Master Egg.
- Fit six taller leaderboard rows and align Android swipe steps with the row height.

## 1.0.6

- Add 24x24 Rotten Egg artwork and consistently sized rank emblems.
- Replace leaderboard paging buttons with D-pad/stick scrolling and automatic cached loading.
- Add Android swipe scrolling and touch Refresh/Back actions.
- Hide gameplay touch controls while browsing leaderboards.

## 1.0.5

Restored full Xbox menu framing so population counters clear the title on console and xemu.

## 1.0.4

Larger population counters centered above menu titles.

## 1.0.3

Public 3-4 player AUTO/FOCUS parties, centered boards, same-room rematches, speed requests, separate party standings, and egg ranks.

## 1.0.2

Reorganized online menus, world chat, population counters, translucent selections, and improved input handling.

## 1.0.1

Customizable Android touch controls with per-button position and size, visibility modes, and saved layouts.

## 1.0.0

Initial Original Xbox and Android release with offline, LAN and hosted online play.
