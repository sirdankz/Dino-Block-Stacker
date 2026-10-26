# Changelog

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
- Add a pre-match layout editor and a live editor through Android Back → Board layout.
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
