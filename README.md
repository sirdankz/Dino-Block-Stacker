# Dino Block Stacker

**Dino Block Stacker** is an unofficial, ROM-free Original Xbox and Android port compatible with the SNES game *Tetris Attack*. It supports local play, LAN multiplayer, and public online features through the project's hosted service.

The application does **not** include the original game ROM, graphics, characters, music, or sound effects. Game content is loaded locally from a separately supplied, compatible ROM. This project is independent of and is not affiliated with or endorsed by Nintendo, Microsoft, or the original rights holders.

## Features

- Native Original Xbox executable (`.xbe`)
- Android application (`.apk`) for compatible 64-bit ARM and x86 devices
- Offline play and LAN multiplayer
- Public 3-4 player AUTO/FOCUS parties with centered original-game boards
- Same-room rematches, departure statuses, host speed changes and guest speed requests
- Separate persistent 1v1/party leaderboards with six egg ranks and temporary emblems
- Public online play using the project's hosted service
- Online accounts, in-game and online population counters
- Online world chat with account authentication and anti-abuse moderation
- Translucent selected-menu highlights and improved results-menu input protection
- Xbox world-chat shortcuts: START send, X delete, Y space, L/R jump to SPACE/SEND
- Gamepad support on both platforms and on-screen controls on Android
- Customizable Android touchscreen buttons with per-button position and size; Always Show, Auto-hide, and Off modes
- ROM import and compatibility verification before gameplay

## Downloads

Prebuilt releases are available from the repository's **Releases** page:

| Platform | Release file |
| --- | --- |
| Original Xbox ZIP | `DinoBlockStacker-v1.0.5-Xbox.zip` |
| Android | `DinoBlockStacker-v1.0.5.apk` |
| Original Xbox executable | `default.xbe` |
| Source archive | GitHub source archives |

**No game ROM is included with any download.** A compatible, unheadered 1 MiB SNES ROM is required. ROM compatibility is checked against the expected SHA-256 value. Each user is responsible for obtaining and using game files lawfully.

## Online menu

The VERSUS menu contains **LAN JOIN**, **LAN HOST**, **ONLINE**, and **OFFLINE PLAY**.
The ONLINE menu contains **PUBLIC ROOMS**, **PROFILE**, **WORLD CHAT**, and **LEADERBOARDS**.
PUBLIC ROOMS provides browse and host actions. Press B to return through each menu.
Counts refer to registered accounts, authenticated players in an active online round, and unique authenticated accounts currently connected to the online service.
World chat accepts up to 32 ASCII characters per message; profanity is allowed, but racist slurs (including common obfuscations and compounds) are rejected by the server.
Chat messages and moderation decisions are handled by the private hosted service, not by other game clients.

On Xbox, START sends a world-chat message directly; X deletes, Y inserts a space, and L/R focus the SPACE/SEND actions. Results menu choices are blocked for approximately one second and until A/B buttons have been released.

## Installation

### Original Xbox

1. Extract the Original Xbox release ZIP into a game folder on the console, such as `E:\Games\DinoBlockStacker\`.
2. Place the compatible, unheadered ROM in the same folder and name it `game.sfc`.
3. Launch `default.xbe` from the console dashboard.

Expected layout:

```text
DinoBlockStacker/
  default.xbe
  game.sfc       # Supplied separately; not part of the release
```

### Android

1. Install the release APK on a compatible Android device (Android 7.0 / API 24 or newer).
2. Open **Dino Block Stacker** and select **Import compatible game ROM**.
3. Choose a compatible, unheadered 1 MiB SNES ROM using Android's system file picker.
4. Select an available game mode. Touch controls and supported USB/Bluetooth gamepads can be used.

The imported ROM is stored locally by the app. Public online features require network access and availability of the project's hosted service. 

## Building from source

Build scripts are provided for Windows. Both public build targets can be compiled without a game ROM; original game content is supplied at runtime.

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

## Source layout

| Path | Contents |
| --- | --- |
| `src/` | Shared native game runtime and Original Xbox frontend |
| `android_port/` | Android application, native platform code, and build configuration |
| `netplay/` | LAN and hosted-online **client** functionality |
| `third_party/` | Included third-party source code and notices |
| `config/server-public-key.hex` | Public verification key used by online clients |
| `rxdk.project.json` | Original Xbox build configuration |
| `release_identity.json` | Version, platform identity, and public endpoint configuration |
| `BUILD_DINO_XBOX.ps1` | Original Xbox build entry point |
| `BUILD_DINO_ANDROID.ps1` | Android build entry point |

Online play connects to a separately maintained hosted service. Public connection settings and verification keys are included in the client source.

## License and acknowledgments

Project contributions are distributed under the **GNU General Public License v3.0 or later**; see [`LICENSE`](LICENSE) and [`COPYING`](COPYING). Third-party software retains its respective license terms and notices. Details are provided in [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

The license applies only to material that its contributors are authorized to license. It does not grant rights to Nintendo game content, trademarks, or third-party software outside the scope of the applicable licenses. Building or distributing software linked against RXDK also requires compliance with the applicable RXDK and component licenses.

*Dino Block Stacker is an independent fan-made project and is not an official Nintendo or Microsoft product.*

## Release history

See [CHANGELOG.md](CHANGELOG.md) for release notes.

## Client checks

```powershell
python netplay/tests/run_client_tests.py
```
