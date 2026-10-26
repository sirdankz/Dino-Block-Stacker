# Dino Block Stacker — Windows PC

Windows x64 port, version 1.1.8. Windows 10 version 1903 or newer, or Windows 11.

## Features

- Shared animated menus, supplied artwork, rank emblems and winner trophies
- Original offline game modes and two-player local play
- LAN 1v1, ranked online 1v1, and online 2–4-player Auto/Focus rooms
- Real opponent boards, a centered local board in party modes, and the local character background
- Accounts, persistent sign-in, independent 1v1/party leaderboards, and all six egg ranks
- World/lobby chat, population counters, rematches, speed requests and host transfer
- Keyboard, mouse, and up to two compatible gamepads; customizable controls for both players
- Windowed/fullscreen presentation, 60 Hz simulation and separately paced remote animations

## Running

Extract the Windows ZIP, keep `SDL3.dll` beside `DinoBlockStacker.exe`, and launch the executable. On first launch, select the compatible unheadered 1 MiB Tetris Attack (USA) ROM. A compatible `game.sfc` beside the executable is also accepted. The `--rom "path"` launch argument selects a file directly.

Expected ROM SHA-256: `579bf1a1988d4af06a69cc1d82a2478ebe51940c5ced7f97e83029a24e6aa778`.

No ROM is distributed. Initial startup prepares game data locally. All existing ports use the same hosted rooms and accounts; no additional server port is required. LAN hosting may require allowing the application through Windows Firewall on the private network.

## Controls

The shared menu retains its controller button labels. Menus also support clicking rows and actions, right-click to return, and the mouse wheel to scroll. On a keyboard, Enter selects, Escape returns, and Tab opens the chat shortcut.

| Action | Player 1 | Player 2 |
| --- | --- | --- |
| Move | Arrow keys | Keypad 8 / 2 / 4 / 6 |
| Swap | Z or S | Keypad 1 or decimal |
| Raise | X, Q or W | Keypad 3, 7 or 5 |
| Start / offline pause | Enter or Escape | Keypad Enter |
| Back / Select | Backspace or Tab | Keypad minus |
| Cycle junk target | C | Keypad 0 |
| Auxiliary / LB | E | Keypad divide |

Gamepads use A to swap, X or triggers to raise, RB to cycle the junk target, and the right stick to select a Focus target. The in-game CONTROLS menu changes bindings independently for both players. Arbitrary keyboard keys can be assigned with SDL scancode names in `keyboard.ini`; changes apply on the next launch.

- F5: START / ready in a lobby.
- F11 or Alt+Enter: toggle fullscreen.
- Ctrl+O: leave the current session and select a different compatible ROM.
- Hold F10, or gamepad Back+Start: return from offline play to the frontend.
- Chat/profile entry: type directly, Backspace deletes, Ctrl+V pastes, Enter confirms and Escape cancels. The on-screen grid supports mouse and gamepad input. Gamepad X deletes, Y adds a space in chat, and Start confirms.
- Leaderboards: mouse wheel, D-pad or left stick to scroll.

## Saves

Profiles, imported ROM, control bindings, window settings, generated startup caches, and offline battery saves use `%LOCALAPPDATA%\DinoBlockStacker`. Nothing needs to be written beside the executable. Local profiles contain sign-in tokens and are private user data.

## Building

Requirements: Python 3.10+, Visual Studio 2022 C++ Build Tools with an x64 toolchain and Windows 10/11 SDK, LLVM with `clang-cl` and `lld-link`, CMake 3.22+, and Ninja. Visual Studio's bundled CMake/Ninja/LLVM installations are detected, as are normal PATH installations and the standard standalone LLVM location.

From the source root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\BUILD_DINO_PC.ps1
```

SDL3 3.4.18 is downloaded from its official release and verified against the pinned SHA-256. Compiler and dependency files remain outside the release package. The build uses the static MSVC runtime and includes the SDL runtime DLL.

Outputs are `out\DinoBlockStackerPC\Windows-x64\DinoBlockStacker.exe`, a ROM-free Windows ZIP, checksums, and local build logs. `-OutDir`, `-RomPath`, and `-Jobs` are optional. A ROM supplied with `-RomPath` is copied only into the local runnable folder after the ROM-free ZIP is finalized. Publication must use the explicitly named ZIP.

Lobby exits require confirmation, with Stay selected initially. START readies each player; party hosts begin automatically when everyone is ready. B BACK and OPEN CHAT occupy separate, non-overlapping targets. Closing the Windows application or importing a ROM from a lobby uses the same confirmation.
