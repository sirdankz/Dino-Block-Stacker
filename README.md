# Dino Block Stacker

**Dino Block Stacker** is an unofficial, ROM-free Original Xbox and Android port compatible with the SNES game *Tetris Attack*. It supports local play, LAN multiplayer, and public online features through the project's hosted service.

The application does **not** include the original game ROM, graphics, characters, music, or sound effects. Game content is loaded locally from a separately supplied, compatible ROM. This project is independent of and is not affiliated with or endorsed by Nintendo, Microsoft, or the original rights holders.

## Features

- Native Original Xbox executable (`.xbe`)
- Android application (`.apk`) for compatible 64-bit ARM and x86 devices
- Offline play and LAN multiplayer
- Public online play using the project's hosted service
- Gamepad support on both platforms and on-screen controls on Android
- ROM import and compatibility verification before gameplay

## Downloads

Prebuilt releases are available from the repository's **Releases** page:

| Platform | Release file |
| --- | --- |
| Original Xbox ZIP | `DinoBlockStacker-v1.0.0-Xbox.zip` |
| Android | `DinoBlockStacker-v1.0.0.apk` |
| Source archive | `DinoBlockStacker-v1.0.0-Xbox.zip` |

**No game ROM is included with any download.** A compatible, unheadered 1 MiB SNES ROM is required. ROM compatibility is checked against the expected SHA-256 value. Each user is responsible for obtaining and using game files lawfully.

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

The imported ROM is stored locally by the app. Public online features require network access and availability of the project's hosted service. The server implementation is not distributed in this repository.

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

Output: `out\DinoBlockStackerAndroid\DinoBlockStacker-1.0.0.apk`

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

The repository contains **client software only**. Dedicated server source, service configuration, deployment scripts, private keys, proprietary game data, generated binaries, and development logs are not included. The online client contains the publicly reachable service endpoint and public verification key required for connectivity.

## License and acknowledgments

Project contributions are distributed under the **GNU General Public License v3.0 or later**; see [`LICENSE`](LICENSE) and [`COPYING`](COPYING). Third-party software retains its respective license terms and notices. Details are provided in [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

The license applies only to material that its contributors are authorized to license. It does not grant rights to Nintendo game content, trademarks, or third-party software outside the scope of the applicable licenses. Building or distributing software linked against RXDK also requires compliance with the applicable RXDK and component licenses.

*Dino Block Stacker is an independent fan-made project and is not an official Nintendo or Microsoft product.*

## Release history

See [CHANGELOG.md](CHANGELOG.md) for release notes.
