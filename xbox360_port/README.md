# Dino Block Stacker — Xbox 360

PowerPC/Xenos platform adapter for the shared Dino Block Stacker client. The shared menu and gameplay implementation is generated directly from the Original Xbox source; it is not a separate frontend.

## Features

The port enables the supplied animated menu artwork, original offline modes, two local controllers with custom bindings, LAN and ranked 1v1, online parties of two to four players, Auto Target and Manual Target junk targeting, real remote boards, local character backdrops, account profiles, lobby/world chat, both leaderboards, six egg ranks, rematches, speed requests, host transfer, and population counters.

The console uses gamepad navigation and the shared console board layout. Android touch controls and its Android layout editor remain platform-specific.

## Build

Requirements: Windows, Python 3, the Xbox 360 XDK with its VS2010 platform integration. The PowerPC compiler and libraries remain external dependencies and are not distributed in this source package.

Run `BUILD_DINO_XBOX360.ps1` at the source root. The script prepares the shared sources, compiles the game, converts a new executable copy to retail homebrew format, and creates a ROM-free Xbox360-NoLog ZIP and SHA-256 checksums. The pinned XexTool dependency is downloaded from the upstream BadUpdate tools release into the ignored `out/Xbox360Tools` cache when absent; its archive and executable checksums are verified.

Optional parameters: `-OutDir`, `-RomPath`, `-AccountKeyPath`, `-SdkPath`, `-XexToolPath`. A supplied ROM must match the supported unheadered 1 MB SHA-256; it is copied only to the local game folder after the public ZIP is created.

## Run

Target: RGH/JTAG, or a Bad Avatar system after its homebrew loader is active. Launch `default.xex` from a writable HDD/USB folder with the supported `game.sfc` beside it. Profiles, bindings, settings and caches use `DinoBlockStacker360Data` beside the game when writable. On Xenia and read-only launch media, a persistent saved-game container is opened through `XContentCreateEx` with `OPENALWAYS`, mounted as `DinoSave:`, and used through `DinoSave:\Data`. Xenia stores the container beneath its configured content directory. Existing writable HDD/USB saves remain supported; no emulator setting changes are required. Cache magic is isolated from other platform preview caches.

A selects, B returns, BACK opens chat where shown, RB cycles the junk target, and the right stick selects a Focus target. Both physical controller indices remain stable after a disconnect. In offline play, hold BACK+START to return to the shared frontend.

The game uses the existing custom UDP room services, not Xbox LIVE. IPv4 values retain their native network-order memory bytes from parsing through socket calls and raw-address packet fields. No extra PowerPC byte swap is applied. Retail sockets use the same plaintext UDP option as MKart360. Existing account authentication and server public-key pinning are retained.

## Compatibility

Startup, saved-game storage, LAN play, and online connectivity have been checked in Xenia Canary Netplay. Physical-console verification remains pending.

## References

- [Bad Avatar: retail executable requirement](https://github.com/shutterbug2000/ABadAvatar#quick-start)
- [BadUpdate upstream tools](https://github.com/grimdoomer/Xbox360BadUpdate/releases/tag/v1.2)
- [Xbox 360 retail UDP socket example](https://github.com/mojobojo/PublicXboxStuff/blob/master/Applications/Xbox/AllocLogger/PluginDemo.cpp)

## Custom soundtrack

Keep the packaged `music/` folder beside the executable. The original menu and results tracks by sirdankz are licensed separately under `LICENSE-MUSIC.txt`, for use in this port and its forks.
