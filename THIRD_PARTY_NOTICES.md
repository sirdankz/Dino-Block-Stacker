# Third-party notices

Dino Block Stacker includes third-party software and uses external build tools. Original copyright notices and license terms remain applicable to the respective components.

## Included source

**LakeSnes** — angelo_wf and contributors  
License: MIT  
License text: [`third_party/lakesnes/LICENSE.txt`](third_party/lakesnes/LICENSE.txt) (also reproduced in [`LICENSES-LAKESNES-MIT.txt`](LICENSES-LAKESNES-MIT.txt)).

**Monocypher** — Loup Vaillant and contributors  
License: BSD-2-Clause **or** CC0-1.0  
The original copyright, BSD license notice, and CC0 dedication are retained in [`third_party/monocypher/monocypher.c`](third_party/monocypher/monocypher.c) and [`third_party/monocypher/monocypher.h`](third_party/monocypher/monocypher.h).

## External build tools and libraries

**Team Resurgent RXDK** — Used to compile and link the Original Xbox executable. RXDK and its runtime libraries are installed separately; they are not included in this source archive. Relevant licenses and notices must be observed when distributing builds linked against them.  
Project: https://github.com/Team-Resurgent/RXDK-Libs

**Android SDK / NDK** — The Android application is built with the Android build toolchain and links against Android platform libraries. The Android SDK and NDK are not included in this repository.

**Xbox 360 XDK** — External PowerPC compiler, graphics, audio, controller and platform libraries for the Xbox 360 target. The SDK is installed separately and is not included in this source archive.

**XexTool** — External homebrew executable conversion tool. The Xbox 360 build downloads a checksum-pinned copy from the upstream BadUpdate tools release into an ignored local cache. Neither XexTool nor the upstream tools archive is redistributed in this source package.

## Game content

No original *Tetris Attack* ROM, ROM-derived startup cache, Nintendo artwork, music, sound effects, or game manual is distributed. Compatible game content must be provided separately and is loaded locally by the application. Neither this project nor its licenses grants permission to obtain, reproduce, or redistribute third-party game content.
