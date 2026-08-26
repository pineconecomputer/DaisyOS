# DaisyOne — Architecture Overview

DaisyOne is a homebrew personal computer built from four cooperating
microcontrollers, each running its own dedicated firmware. The four units
communicate over hardware UARTs using small framed binary protocols. A
fifth component, **DaisyFile**, is a Python TCP server that lives on a
host computer and provides remote file storage over WiFi.

```
                ┌───────────────────────────────────────┐
                │             Daisy Computer            │
                │                                       │
   Kbrd Matrix  │   ┌─────────────┐    ┌─────────────┐  │
   ─────────────┼──▶│  DaisyOS    │ ──▶│  DaisyVideo │──┼───▶ NTSC monitor
                │   │  DaisyBasic │    │  (Mega 2560)│  │
                │   │  (SAM3X)    │    └─────────────┘  │
                │   │             │                     │
                │   │             │    ┌─────────────┐  │
                │   │             │ ──▶│  DaisySound │──┼───▶ Amplifier
                │   │             │    │  (UNO)      │  │
                │   │             │    └─────────────┘  │
                │   │             │                     │
                │   │             │    ┌─────────────┐  │              ┌──────────┐
                │   │             │◀──▶│  ESP 8266   │──┼───▶ WiFi ──▶ │ DaisyFile│
                │   └─────────────┘    │  (ZiModem)  │  │              │  (host)  │
                │                      └─────────────┘  │              └──────────┘
                │                                       │
                └───────────────────────────────────────┘
                                        
```

## The four microcontrollers

| Unit       | MCU         | Role                                                 |
|------------|-------------|------------------------------------------------------|
| DaisyOS    | SAM3X (Due) | Brain. Hosts DaisyBASIC interpreter, screen editor,  |
|            |             | terminal, keyboard scanner, application logic.       |
| DaisyVideo | ATmega2560  | Video controller. Renders 40×25 text + 80×50 pixels  |
|            |             | from a shadow framebuffer; outputs composite/RGB.    |
| DaisySound | ATmega328   | 3-voice audio synthesizer with envelope, PWM, noise, |
|            |             | portamento, and program (sequencer) playback.        |
| ESP 8266   | ESP32       | Connects to WiFi networks and provides I/O over      |
|            |             | TCP/UDP. DaisyFile speaks over WiFi.                 |

Each MCU has its own firmware tree, its own platformio.ini, and its own
git repo, and is flashed independently.

## Communication busses

DaisyOS is the controller. It originates messages on three UARTs:

* **Serial1** ↔ ESP-8266/Zimodem WiFi modem 
  → file I/O frames (`comm_messages`).
* **Serial2** ↔ DaisyVideo
  → video framebuffer updates and graphics ops (`video_messages`).
* **Serial3** ↔ DaisySound
  → tone, sequencer, and PWM commands (`audio_messages`).

All three protocols share a common framing skeleton:

    [SOP=0x5C] [CMD] [PAYLOAD_LEN] [PAYLOAD bytes ...] [CHECKSUM]

The checksum is two's complement of all preceding bytes; receivers reject
frames whose checksum doesn't sum to zero (mod 256). DaisyFile
additionally uses ASCII `print "..."` lines and an `end` sentinel for
free-form responses.

## DaisyOS internal architecture

DaisyOS is divided into two large subsystems plus a small main superloop:

```
                    ┌────────────┐
                    │  main.cpp  │  boot, splash, top-level loop
                    └─────┬──────┘
                          │
              ┌───────────┼────────────┐
              ▼           ▼            ▼
      ┌────────────┐ ┌────────┐ ┌──────────┐
      │ DaisyBASIC │ │ Editor │ │ Terminal │   user-facing front ends
      └─────┬──────┘ └────┬───┘ └────┬─────┘
            └─────────────┼──────────┘
                          ▼
                ┌──────────────────┐
                │ daisyos services │  shadow_ram, cursor, keyboard,
                │                  │  comm_messages, video_messages,
                │                  │  audio_messages, wifi, timer,
                │                  │  buffer, gpio, invaders, conway, daisyterm
                └────────┬─────────┘
                         │
                         ▼
                ┌──────────────────┐
                │  Arduino / SAM3X │  framework, ISRs, hardware
                └──────────────────┘
```

### DaisyBASIC

DaisyBASIC is a multi-line, line-numbered BASIC interpreter implemented as
a tokenized AST-less direct executor. It is split into roughly one
translation unit per concern:

* `basic_state` — global mutable state shared by all modules
* `basic_error` — error code enum, error printing, soft reset
* `basic_tokenizer` — text↔token round-trip, program line storage
* `basic_parse` — low-level scanners (whitespace, names, expressions)
* `basic_expr` — full expression evaluator with built-in functions
* `basic_variables` — scalar/array/string variable storage
* `basic_program` — LIST, DIM, CLR, NEW, LOAD, SAVE, CATALOG, char I/O
* `basic_io` — PRINT, INPUT, GET, LOCATE, NETPRINT, NETGET, NETINPUT, WIFI
* `basic_gfx` — text and pixel-graphics commands
* `basic_audio` — SOUND, SOUNDPGM, SOUNDPWM, ENVELOPE, PLAY, etc.
* `basic_control` — GOTO, GOSUB, RETURN, FOR, IF, WHILE/DO, TIMER, TRAP, DEG/RAD
* `basic_data` — DATA, READ, READMAT, RESTORE
* `basic_file` — FOPEN, FCLOSE, FPRINT, FINPUT, FGET, FPUT, FSEEK, MORE
* `basic_rtc` — real-time clock helpers (TIME, DATE, SETTIME, SETDATE)
* `basic_execute` — top-level dispatcher, RUN/CONT loop, statement decoder
* `basic_demo`, `basic_shapedemo` — built-in DEMO programs

The tokenizer compresses each program line into a compact byte stream
stored in a heap-allocated `tokenPool`. Each `ProgramLine` descriptor
holds `(lineNum, offset, tokenLen)`. The executor walks the program by
line index and dispatches each statement through `ExecuteStatement` →
`ExecuteSingleStatement`, which maps a keyword token (or text prefix in
immediate mode) to a `CmdXxx(args)` function.

Variables, arrays, FOR/GOSUB/WHILE stacks, and user-defined functions are
heap-allocated and grow on demand via `EnsureCapacity`. A unified
`heapBytesUsed` counter feeds the `FRE(0)` builtin.

### daisyos services

OS-side helpers that BASIC and the editor sit on top of:

* `shadow_ram` — local mirror of video framebuffer + character cell
  attributes. All graphics primitives operate on this shadow then sent to 
   DaisyVideo as a frame. 
* `cursor` — text cursor position, blink state, scrolling.
* `keyboard` — TC3-driven 4×13 matrix scanner with debounce, ring buffer
  for keystrokes, modifier handling.
* `buffer` — lock-free SPSC ring buffer used by the keyboard ISR.
* `comm_messages` — frames file I/O commands and sends them with Zimodem, to DaisyFile.
* `video_messages` — frames video updates and sends them to DaisyVideo
  on Serial2.
* `audio_messages` — frames audio commands and sends them to DaisySound
  on Serial3.
* `wifi` — Zimodem AT-command driver: connect, dial, disconnect, line I/O.
* `timer` — countdown timer abstraction used everywhere for timeouts.
* `gpio` — fast SAM3X GPIO read/write helpers used by the keyboard ISR.
* `invaders` — built-in Space Invaders game (`INVADERS` immediate command).
* `conway` — Conway's Game of Life on the attribute grid (`CONWAY`
  immediate command).
* `terminal` — VT52/ANSI terminal emulator (`TERM` immediate command).

### editor

The screen editor is a single translation unit (`editor_service.cpp`)
plus a small public header. It implements line-oriented in-place editing:
the user types over an existing screen, and Enter submits the visible
line back to BASIC for tokenisation and storage.

## Component projects

### DaisyVideo (firmware)

Reads framed messages on Serial1, applies them to its own internal
character + bitmap memory, and outputs video. Includes hand-tuned bitmap
data for the standard ROM character set, a "Daisy" splash logo, and the
block-graphics characters that implement the 80×50 pixel layer.

### DaisySound (firmware)

Three audio voices generated on a Timer1 ISR:

* Voice 0, 1: square / sawtooth / triangle / pulse 
* Voice 2: noise

Supports portamento (pitch glide), PWM, hard sync, and program-driven
sequences. Each voice has independent waveform, frequency, and
PWM state. Commands arrive as framed messages from DaisyOS.

### DaisyFile (host service)

Python TCP server that speaks the framed protocol over a socket. Accepts
multiple concurrent clients; provides per-connection working-directory
state for CHDIR/MKDIR navigation; files live in a sandboxed
`daisyfiles/` subtree with path-escape guards.

## Memory model

DaisyOS uses a soft heap budget (`DAISY_BASIC_HEAP`, default 64 KiB) that
the BASIC interpreter accounts against. The `FRE(0)` builtin reports
remaining free bytes. There are no fixed-size variable, array, or stack
limits — everything reallocs and doubles on demand.

DaisyVideo and DaisySound use static framebuffer / voice tables sized to
fit their MCUs.
