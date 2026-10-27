# Dino Block Stacker — Windows PC

Windows x64 port, version 1.2.9. Windows 10 version 1903 or newer, or Windows 11.

## Features

- Shared animated menus, supplied artwork, rank emblems and winner trophies
- Original offline game modes and two-player local play
- LAN 1v1, ranked online 1v1, and online 2–4-player Auto/Manual Target rooms
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
| TARGET (cycle junk recipient) | C | Keypad 0 |
| Auxiliary / LB | E | Keypad divide |

Gamepads use A to swap, X or triggers to raise, RB to cycle the junk target, and the right stick to select a manual target. The in-game CONTROLS menu changes bindings independently for both players. Arbitrary keyboard keys can be assigned with SDL scancode names in `keyboard.ini`; changes apply on the next launch.

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

## Custom soundtrack

Keep the packaged `music/` folder beside the executable. The original menu and results tracks by sirdankz are licensed separately under `LICENSE-MUSIC.txt`, for use in this port and its forks.

## Frame timing

The Windows frame limiter uses precise 60 Hz deadlines and skips the sleep after an overdue frame. Three rotating frame textures reduce contention with uploads still in use by the GPU. Population and 1v1 opponent textures are updated only when their content changes. Party opponent animations retain their existing independent update rates.

Menu and results music runs from the audio device callback, independently of the game/menu thread. Both soundtrack tracks are cached at startup (about 38 MiB combined). Original game audio uses a fixed queue with a three-frame cushion and short underrun/recovery fades. Route and network-adapter refresh remains on a background worker.

With AUTOMATIC selected, renderer recovery watches graphics costs independently of CPU work and the frame limiter. Three graphics stalls of at least 50 ms within 60 seconds request an OpenGL switch. Startup, resize, display-mode and focus changes receive a two-second grace period. Switching rebuilds display resources while retaining the game, room, board images and text-entry state. A failed replacement keeps the current renderer and is attempted at most once per launch.

Graphics choices and successful automatic OpenGL recovery are atomically saved in `renderer.ini` in `%LOCALAPPDATA%\DinoBlockStacker`. Later launches try the saved renderer first; if it cannot initialize, normal renderer selection remains available. An explicit `SDL_RENDER_DRIVER` override takes precedence and disables automatic switching for that run. Removing `renderer.ini` restores normal automatic selection. Timing captures include `renderer_opengl` to identify the active path.

Optional diagnosis: launch `DinoBlockStacker.exe --frame-timings`, reproduce a hitch and close normally. The first 18,000 presented frames are retained in memory, then written to `%LOCALAPPDATA%\DinoBlockStacker\frame-timings.csv` on exit. The CSV records frame intervals, work/upload/render/wait durations and audio buffer counters. It contains no account names, chat or network payloads. Normal launches do not collect timing data. This flag does not change frame pacing or game rules.

## Options

OPTIONS contains CONTROLS, FULLSCREEN OPTIONS, GRAPHICS OPTIONS, VOLUME OPTIONS and CUSTOM BLOCK COLOR SCHEME. Fullscreen can also be toggled with F11 or Alt+Enter. Window presets are 960x720, 1280x720, 1280x960 and 1920x1080. Window dimensions survive fullscreen toggles and relaunches. Master, menu/results music and original game audio have independent volume controls. Defaults are 100% master, 10% menu/results music and 50% game audio; existing saved levels remain in effect. Six color presets support saved swaps, rotation and reset through the shared mouse/controller editor.

Panel identities are Heart, Circle, Triangle, Star, Upside-down Triangle and Diamond. The green panel carries a circle; the dark-blue panel carries the downward triangle. Color reassignment retains each panel's shape.

## Graphics options

GRAPHICS OPTIONS offers AUTOMATIC, OPENGL, DIRECT3D 11, DIRECT3D 12 and SOFTWARE. The current renderer appears above the choices, and an asterisk marks the selected mode. Mouse, keyboard and gamepad selection use the shared menu controls.

A applies a choice immediately and saves it after a successful frame. Manual choices stay selected and disable automatic recovery. AUTOMATIC retains the remembered OpenGL recovery preference and can switch after repeated graphics stalls. Software renders on the CPU and provides a compatibility option.

An unavailable choice leaves the existing renderer and saved selection intact. Saved renderer settings from v1.2.3 migrate automatically, preserving a remembered OpenGL recovery. Other platforms retain their existing graphics paths.
