# DaisyOS

Main firmware for **Daisy**, a homebrew personal computer built from
several cooperating microcontrollers. DaisyOS runs on an Arduino Due
(Atmel SAM3X) and is the brain of the machine: it hosts the DaisyBASIC
interpreter, the full-screen editor, the terminal, and the keyboard
matrix scanner, and it drives the video and audio coprocessors over
hardware UARTs.

See [ARCHITECTURE.md](ARCHITECTURE.md) for the system-level design and
the wire protocols, and [DAISYBASIC_REFERENCE.md](DAISYBASIC_REFERENCE.md)
for the BASIC dialect.

## Companion firmware

DaisyOS is one of several independently flashed units. The others are
not required in order to build it:

| Unit       | MCU         | Role                                        |
|------------|-------------|---------------------------------------------|
| DaisyOS    | SAM3X (Due) | This repo. BASIC, editor, terminal, keyboard |
| [DaisyVideo](https://github.com/pineconecomputer/DaisyVideo) | ATmega2560 | 40×25 text + graphics, composite video out |
| [DaisySound](https://github.com/pineconecomputer/DaisySound) | ATmega328 | 3-voice synthesizer with envelopes and noise |
| ESP8266    | —           | WiFi modem (stock Zimodem firmware)         |

## Building

Requires [PlatformIO](https://platformio.org/install) and an Arduino Due.
PlatformIO fetches the toolchain and the one library dependency
(`MarkusLange/RTCDue`) on first build.

```sh
git clone https://github.com/pineconecomputer/DaisyOS.git
cd DaisyOS
pio run                  # build
pio run -t upload        # build and flash over the native USB port
```

Flash the Due through the **native USB** port (`env:dueUSB`).

## Serial links

| Port    | Baud   | Connects to                          |
|---------|--------|--------------------------------------|
| Serial  | 115200 | Host console / debug                 |
| Serial1 | 38400  | ESP8266 WiFi modem (`comm_messages`) |
| Serial2 | 115200 | DaisyVideo (`video_messages`)        |
| Serial3 | 115200 | DaisySound (`audio_messages`)        |

## Layout

```
include/           public headers, one per subsystem
  daisybasic/      BASIC interpreter internals
  daisyos/         core OS internals
  editor/          screen editor internals
src/
  daisybasic/      tokenizer, parser, expression eval, execution, graphics
  daisyos/         keyboard, terminal, cursor, shadow RAM, message codecs
  editor/          full-screen line editor
  main.cpp         setup() / loop()
tools/             character-ROM and graphics-test generators (Python)
git_branch.py      PlatformIO pre-script; stamps the git branch into the build
```

`git_branch.py` embeds the current branch name as the `GIT_BRANCH` macro.
Outside a git checkout it resolves to `"unknown"`.

## Code style

Formatted to the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
via the checked-in `.clang-format`:

```sh
clang-format -i $(find include src -name '*.cpp' -o -name '*.h')
```

Include sorting is disabled, since include order matters in Arduino
sources.

## License

Licensed under the **GNU General Public License, version 3**. See
[LICENSE](LICENSE) for the full text.

    Copyright (C) 2026 Joe Cassara
