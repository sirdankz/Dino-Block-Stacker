# Dino Block Stacker

**Dino Block Stacker** is an unofficial, ROM-free(NO ROM INCLUDED MUST PROVIDE YOUR OWN LEGALLY OBTAINED ROM)
Original Xbox, Xbox 360, Android, & Windows PC port compatible with the SNES game *Tetris Attack*. It supports local play, LAN multiplayer, and public online features through the project's hosted service.

The application does **not** include the original game ROM, graphics, characters, music, or sound effects. Game content is loaded locally from a separately supplied, compatible ROM. This project is independent of and is not affiliated with or endorsed by Nintendo, Microsoft, or the original rights holders.

## Features

- Original menu/results music by sirdankz
- Offline play and LAN multiplayer
- Automatic replays after simultaneous top-outs; completed matches have one winner
- Pixel-art(Made by Galactusx) menu frames with a scrolling tiled background, trophy animated sprite, & leaderboards
- Online accounts, in-game and online population counters
- Lobby and world chat in waiting rooms, with a dedicated chat shortcut in Browse Rooms
- Direct tapping and swipe navigation for Android menus
- Xbox world-chat shortcuts: START send, X delete, Y space, L/R jump to SPACE/SEND
- Gamepad support on all four platforms and on-screen controls on Android
- Windows keyboard, mouse, and two-gamepad support
- Customizable Android touchscreen buttons 

## Options

OPTIONS includes controls, volume and six customizable panel-color schemes on all four ports. Default levels are 100% master, 10% menu/results music and 50% game audio. Saved custom levels remain in effect; RESTORE VOLUME DEFAULTS applies these defaults.

Windows also includes fullscreen and GRAPHICS OPTIONS: Automatic, OpenGL, Direct3D 11, Direct3D 12 and Software. Graphics choices apply immediately and save locally. Automatic remembers OpenGL recovery after repeated graphics stalls; manual choices remain selected.

## Downloads

Prebuilt releases are available from the repository's **Releases** page:

| Platform | Release file |
| --- | --- |
| Original Xbox ZIP | `DinoBlockStacker-v1.2.5-Xbox-NoLog.zip` |
| Android | `DinoBlockStacker-v1.2.5.apk` |
| Original Xbox executable | `default.xbe` |
| Xbox 360 ZIP | `DinoBlockStacker-v1.2.5-Xbox360-NoLog.zip` |
| Xbox 360 executable | `default.xex` |
| Windows PC ZIP | `DinoBlockStacker-v1.2.5-Windows-x64-NoLog.zip` |
| Source archive | GitHub source archives |

Keep the packaged `music/` folder beside the desktop/console executable for the menu and results soundtrack. Android includes both tracks in the APK.

**No game ROM is included with any download.** A compatible, unheadered 1 MiB SNES ROM is required. ROM compatibility is checked against the expected SHA-256 value. Each user is responsible for obtaining and using game files lawfully.

On Controller, START sends a world-chat message directly; X deletes, Y inserts a space, and L/R focus the SPACE/SEND actions.

## Installation

### Original Xbox

1. Extract the Original Xbox release ZIP into a game folder on the console, such as `E:\Games\DinoBlockStacker\`.
2. Place the compatible, unheadered ROM in the same folder and name it `game.sfc`.
3. Launch `default.xbe` from the console dashboard.

Expected layout:

```text
DinoBlockStacker/
  default.xbe
  game.sfc       # ; not part of the release(MUST PROVIDE YOUR OWN LEGALLY OBTAINED ROM)
```

### Xbox 360

1. Extract the Xbox 360 release ZIP into a game folder on writable HDD or USB storage.
2. Place the compatible, unheadered ROM beside `default.xex` and name it `game.sfc`.
3. Launch `default.xex` on an RGH/JTAG system or through an active Bad Avatar homebrew loader. Xenia is also supported.


Expected layout:

```text
DinoBlockStacker/
  default.xex
  game.sfc       # ; not part of the release(MUST PROVIDE YOUR OWN LEGALLY OBTAINED ROM)
```
Profiles, settings, bindings, and caches use `DinoBlockStacker360Data` beside the game when writable. On Xenia or read-only game mounts, they use the normal Xbox saved-game container under the configured content directory. No Xenia write-permission setting change is required.

The Xbox 360 port shares the console menus and board layouts. RB cycles junk targets; the right stick selects a Focus target. Hold BACK+START during offline play to return to the menus.

### Android

1. Install the release APK on a compatible Android device (Android 7.0 / API 24 or newer).
2. Open **Dino Block Stacker** and select **Import compatible game ROM**.
3. Choose a compatible, unheadered 1 MiB SNES ROM using Android's system file picker.
4. Select an available game mode. Touch controls and supported USB/Bluetooth gamepads can be used.

Android 1v1 uses the original offline board layout by default. Board positions and sizes save separately by multiplayer mode and player count from **Customize board layouts**, or Android Back > **Board layout** during a match.

The imported ROM is stored locally by the app. Public online features require network access and availability of the project's hosted service. 

### Windows PC

1. Extract the Windows PC release ZIP and keep `SDL3.dll` beside `DinoBlockStacker.exe`.
2. Launch `DinoBlockStacker.exe` and select the compatible ROM, or place `game.sfc` beside it.
3. Play with keyboard, mouse, or up to two compatible gamepads.

Enter selects, Escape returns, F5 readies, Tab opens the chat shortcut, and F11 toggles fullscreen. Settings and saves use `%LOCALAPPDATA%\DinoBlockStacker`. See [PC controls](pc_port/README.md#controls) for the full layout.

## Building from source

Build scripts are provided for Windows. All public build targets can be compiled without a game ROM; original game content is supplied at runtime.

### Original Xbox

Requirements:

- Team Resurgent RXDK, including its Original Xbox runtime libraries and command-line tools
- Python 3.14
- RXDK installed at `C:\ProgramData\RXDK`

From the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\BUILD_DINO_XBOX.ps1
```

Output: `out\DinoBlockStackerXbox\PlayN22PublicNoLog\default.xbe`

The build script also supports an optional locally supplied `-RomPath` and private `-MakeIso` output. **ROM-containing builds and ISOs are not release artifacts and must not be redistributed without the appropriate rights.**

### Xbox 360

Requirements:

- Windows and Python 3
- Xbox 360 XDK with the VS2010 Xbox 360 platform integration
- XexTool, downloaded automatically from the checksum-pinned upstream BadUpdate tools release into an ignored local cache, or supplied with `-XexToolPath`

From the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\BUILD_DINO_XBOX360.ps1
```

Output: `out\DinoBlockStacker360\default.xex`, a ROM-free Xbox 360 ZIP, and checksums. The script accepts `-OutDir`, `-RomPath`, `-AccountKeyPath`, `-SdkPath`, and `-XexToolPath`. The XDK and conversion-tool binaries are external dependencies and are not included in the repository.

### Android

Requirements:

- JDK 17 or 21
- Gradle 8.9
- Android SDK platform 34 and build-tools 34.0.0
- Android NDK 27.3.13750724 and CMake 3.22.1
- Python 3.14

From the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\BUILD_DINO_ANDROID.ps1
```

Output: `out\DinoBlockStackerAndroid\DinoBlockStacker-<version>.apk` (from `release_identity.json`).

The script accepts `-SdkRoot`, `-GradleRoot`, and `-JdkRoot` overrides. The default Python interpreter is `C:\Python314\python.exe`, with `python` on `PATH` used as a fallback.

Android release signing uses a local keystore at `%USERPROFILE%\.android\dinoblockstacker-release.jks` and a separate `.pass` file. These credentials are excluded from the repository. Subsequent APK updates must be signed using the same release key to remain compatible with existing installations.

### Windows PC

Requirements: Windows 10 version 1903 or newer or Windows 11, Python 3.10+, Visual Studio 2022 C++ Build Tools with a Windows SDK, LLVM, CMake 3.22+, and Ninja. The build downloads and verifies the pinned official SDL3 dependency. See [PC build documentation](pc_port/README.md#building) for details.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\BUILD_DINO_PC.ps1
```

Output: `out\DinoBlockStackerPC\Windows-x64\DinoBlockStacker.exe` and a ROM-free Windows ZIP. The script accepts `-OutDir`, `-RomPath`, and `-Jobs`.

## Source layout

| Path | Contents |
| --- | --- |
| `assets/ranks/` | Rank emblem artwork |
| `assets/menus/` | Menu frame and background artwork |
| `assets/trophy/` | Animated winner trophy artwork |
| `assets/music/` | Original menu/results recordings and separate music license |
| `src/` | Shared native game runtime and Original Xbox frontend |
| `android_port/` | Android application, native platform code, and build configuration |
| `xbox360_port/` | Xbox 360 platform adapter and shared-source build preparation |
| `pc_port/` | Windows PC platform adapters and build configuration |
| `netplay/` | LAN and hosted-online **client** functionality |
| `third_party/` | Included third-party source code and notices |
| `config/server-public-key.hex` | Public verification key used by online clients |
| `rxdk.project.json` | Original Xbox build configuration |
| `release_identity.json` | Version, platform identity, and public endpoint configuration |
| `BUILD_DINO_XBOX.ps1` | Original Xbox build entry point |
| `BUILD_DINO_ANDROID.ps1` | Android build entry point |
| `BUILD_DINO_XBOX360.ps1` | Xbox 360 build entry point |
| `BUILD_DINO_PC.ps1` | Windows PC build entry point |

Online play connects to a separately maintained hosted service. Public connection settings and verification keys are included in the client source.

## License and acknowledgments

Project contributions are distributed under the **GNU General Public License v3.0 or later**; see [`LICENSE`](LICENSE) and [`COPYING`](COPYING). Third-party software retains its respective license terms and notices. Details are provided in [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

The original menu and results music is copyright 2026 **sirdankz**, separately licensed under [`LICENSE-MUSIC.txt`](LICENSE-MUSIC.txt) for this port and its forks. Use in unrelated projects requires separate permission; the program GPL does not apply to these recordings.

The license applies only to material that its contributors are authorized to license. It does not grant rights to Nintendo game content, trademarks, or third-party software outside the scope of the applicable licenses. Building or distributing software linked against RXDK also requires compliance with the applicable RXDK and component licenses.

*Dino Block Stacker is an independent fan-made project and is not an official Nintendo or Microsoft product.*

