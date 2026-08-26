# THE DAISY Z-MACHINE

### A User's Guide to the Interactive Fiction Interpreter

---

**CONTENTS**

    Introduction
    What the Z-Machine Is
    Why Not Emulate a Z80
    Where the Interpreter Came From
    Why A2Z Could Not Be Used As It Stood
    The Other Reference Port
    What Was Kept, Replaced and Thrown Away
    Arduino Code That Had To Go
    How a Story File Is Divided
    What JZIP Does
    What Daisy Does Instead
    A Trap in the Header Field Names
    Measured Cost
    Corrections to the Interpreter
    The Display
    DaisyOS System Calls
    Saving and Restoring
    Adding a Story
    The Test Harness
    Story File Header Fields
    Memory Map, Zork I
    Files of the Module
    Limitations

---

## Introduction

This guide describes the Z-machine interpreter supplied with DaisyOS. The
interpreter lets Daisy run the interactive fiction Infocom published in the
1980s: ZORK, DEADLINE, THE HITCHHIKER'S GUIDE TO THE GALAXY, and some thirty
more. It runs them directly, without emulating any other computer.

To begin, type:

    ZORK

With more than one story built in, a menu appears first. The interpreter then
takes over the display and keyboard, plays the story, and returns you to the
BASIC prompt when the story ends or when you press STOP.

This build has all three Zork games in it.

> **NOTE:** Saving and restoring a game are not available. See Saving and
> Restoring, below.

## What the Z-Machine Is

Infocom did not write their games for any particular computer. They wrote them
for an imaginary one, the Z-machine, and then wrote a small interpreter for
each real machine they wanted to sell to. A story file such as `ZORK1.DAT` holds
the compiled program for that imaginary computer.

This is why a ZORK that runs on the Apple II also runs on the Commodore 64, the
TRS-80 and the IBM PC. Only the interpreter changes.

Daisy is now one more machine on that list.

## Why Not Emulate a Z80

You might ask why Daisy does not just emulate a Z80 and run the CP/M release of
ZORK, since that software already exists.

The answer is that `ZORK1.COM` is *itself* a Z-machine interpreter, compiled for
CP/M-80. To run it, Daisy would emulate a Z80 in order to reach the same virtual
machine one layer further down. That costs a 64K address space in RAM and a disk
drive to hold the story file.

Running the story directly costs 11,941 bytes of RAM and no storage hardware at
all.

> **NOTE:** The Z80 route was worked out in full before this port was started.
> It needs 87,524 bytes of a 98,304 byte machine, which leaves DaisyBASIC no
> room to run.

## Where the Interpreter Came From

The interpreter is not original work. It descends as follows:

    ZIP 2.0          Mark Howell
      |
    JZIP 2.1         John Holder             2000
      |
    A2Z_Machine      Dan Cogliano            2018
      |              (ZContent)
      |
    Daisy Z-machine  this port               2026

JZIP is a portable C interpreter covering Z-machine versions 1 through 8. It
carries a BSD licence, so it may be used here. The files written for this port
carry the GNU General Public Licence version 3, like the rest of DaisyOS.

A2Z_Machine is JZIP moved to an Adafruit ItsyBitsy M4, an ARM Cortex-M4 board.
It was taken as the starting point because it had already settled the
differences between JZIP's assumptions and those of the Arduino framework.

## Why A2Z Could Not Be Used As It Stood

A2Z runs on a board with 192K of RAM and 2 megabytes of SPI flash holding a file
system. It loads the whole story image into RAM and reads it from files on a
mass storage device.

Daisy has 98,304 bytes of RAM, of which DaisyOS itself uses 28,080, and no mass
storage of any kind. `ZORK1.DAT` is 84,992 bytes. The A2Z method therefore needs
113,072 bytes on a machine that has 98,304.

A2Z also uses four libraries Daisy does not carry (`SdFat`,
`Adafruit_SPIFlash`, `TinyUSB` and `mcurses`), and its display code is written
against `mcurses` throughout.

## The Other Reference Port

A second port was studied: Zorkduino, by rossum, which runs ZORK on an ATmega328
with **2K of RAM**. It manages this by putting every stack and memory access
through a 160-byte cache and a 512-byte disk buffer, backed by a one-megabyte
page file on an SD card.

Daisy needs neither method. It has forty-eight times the RAM of an ATmega328,
and, more important, its processor maps flash into the normal address space.

## What Was Kept, Replaced and Thrown Away

A2Z_Machine holds 23 source files. Eleven were kept, four were replaced, one was
adapted, and seven were thrown away.

**Kept.** `interpre`, `control`, `math`, `object`, `operand`, `property`, `text`,
`variable`, `input`, `screen` and `extern`. These are the interpreter proper.
They are unchanged apart from the corrections listed later in this guide.

**Adapted.** `ztypes.h`, which holds the types and the data access macros. See A
Trap in the Header Field Names.

**Replaced.**

| File | Lines | Reason |
|---|---:|---|
| `fileio.cpp` | 1432 | Assumes a file system. |
| `osdepend.cpp` | 769 | MS-DOS and VMS conditionals throughout. |
| `acursesio.cpp` | 578 | Written against `mcurses`. |
| `memory.cpp` | 463 | The paging cache. See What Daisy Does Instead. |

**Thrown away.**

| File | Lines | Reason |
|---|---:|---|
| `quetzal.cpp` | 619 | Portable save format. Saving is not available. |
| `a2z_machine.ino` | 618 | Sketch. USB mass storage and story picker. |
| `jzip.cpp` | 171 | Command-line entry point. |
| `getopt.cpp` | 89 | Command-line argument parsing. |
| `jzexe.h` | 86 | Stories bound into MS-DOS executables. |
| `license.cpp` | 48 | Prints a licence banner. |
| `jzip.h` | 37 | Version banner. |

## Arduino Code That Had To Go

These were in the A2Z sources and had to be removed or replaced. They are listed
because each one fails to compile with a message that does not point at the
cause.

| Item | What was done |
|---|---|
| `yield()` | Replaced by `ZmHostIdle()`. |
| `min()` | An Arduino macro. Replaced with a plain comparison. |
| `byte` | An Arduino type. The line using it was removed. |
| `Blink()` | LED helper. Removed along with `fatal()`. |
| `srandom()` | **Clashes with newlib.** See below. |
| `a2zrandom()` | Removed. `ZmRandom()` replaces it. |
| `randomSeed()` | Removed. `ZmSeedRandom()` replaces it. |

> **IMPORTANT:** A2Z writes its own `srandom()` because the AVR core has none.
> The ARM toolchain's newlib *does* have one, and the two clash at link time
> with the message `multiple definition of 'srandom'`. Both A2Z replacements
> were removed, and `RANDOM_FUNC` and `SRANDOM_FUNC` now go through `zm_port.h`
> to a single generator. That also means the firmware and the test harness
> produce the same numbers from the same seed.

## How a Story File Is Divided

The next few sections describe the main difference between this port and the
ones it came from. Read them before changing anything in the module.

Every Z-machine story splits into three regions. The boundaries are held in the
header:

    +---------------------------+  $0000
    |                           |
    |   DYNAMIC MEMORY          |  The game reads and writes this.
    |   globals, objects,       |  Must be in RAM.
    |   the parse buffer        |
    |                           |
    +---------------------------+  $2E53   header word $0E
    |                           |
    |   STATIC MEMORY           |  The game only reads this.
    |   the dictionary,         |
    |   abbreviations           |
    |                           |
    +---------------------------+  $4E37   header word $04
    |                           |
    |   HIGH MEMORY             |  Routines and strings.
    |   executable code,        |  Read only.
    |   packed text             |
    |                           |
    +---------------------------+  $14C00

The addresses shown are those of ZORK I.

## What JZIP Does

JZIP was written for machines that read the story off a disk one page at a time.
It keeps a chain of 512-byte pages in a least-recently-used cache.

A dictionary lookup is a binary search across the whole dictionary. If the
dictionary were paged, every lookup would throw out the page the interpreter is
running from, and the interpreter would spend its life reading the disk. So JZIP
makes its **resident** area reach all the way up to the base of high memory,
keeping dynamic *and* static memory in RAM at once.

For ZORK I that is 20,480 bytes, or forty pages, of which only 11,859 are ever
written.

## What Daisy Does Instead

The SAM3X maps its flash into the normal address space. A story compiled into
the firmware as a `const` array can therefore be read with a plain subscript, at
full speed, with no cache and no copying.

The cache, the page chain and the dictionary padding were all removed. Only the
writable region goes in RAM:

    JZIP resident area, ZORK I .................. 20,480 bytes
    Daisy resident area, ZORK I ................. 11,859 bytes
                                                  -------------
    Saved ........................................ 8,621 bytes

The other 73,133 bytes are read from flash where they sit.

## A Trap in the Header Field Names

> **WARNING:** Two of JZIP's header field names do not mean what they look like
> they mean. Get them backwards and the port fails in ways that are hard to
> trace.

| JZIP name | Header offset | What it really is |
|---|---|---|
| `h_data_size` | `$04` | Base of **HIGH** memory (20,023 in ZORK I) |
| `h_restart_size` | `$0E` | Base of **STATIC** memory (11,859 in ZORK I) |

The second one is the size of the writable region, and is the figure this port
allocates.

The proof is simple. `z_restart` reloads `h_restart_size` bytes, and the save
routine writes exactly `h_restart_size` bytes of `datap`. A save that left out
part of dynamic memory would not restore.

## Measured Cost

The figures below are read from the linked ELF file. Note that PlatformIO's
reported RAM figure counts only `.bss` and leaves out `.relocate`, which on the
Due is also in RAM. Both are counted here.

| Build | Static RAM | Flash |
|---|---:|---:|
| DaisyOS alone | 28,080 | 159,272 |
| With the interpreter, no story | 30,848 | 178,888 |
| With ZORK I | 30,848 | 263,808 |
| With all three ZORK games | 30,848 | 436,984 |

The interpreter itself costs 19,616 bytes of flash and 2,768 bytes of RAM, and
that figure does not change with the number of stories. Everything above it is
story image: 84,992 bytes for ZORK I, 258,048 for the three together.

Static RAM does not move at all when stories are added, because the images stay
in flash. The three-story build leaves 87,304 bytes of flash free.

While a story is running, another 11,941 bytes are allocated. They are given
back when it ends:

    Dynamic memory (datap) ...................... 11,859
    Output line buffer ............................... 41
    Status line buffer ............................... 41
                                                  -------
                                                   11,941

    Peak RAM in use ............................. 42,789 of 98,304  (43.5%)
    Free at peak ................................ 55,515

> **NOTE:** The Z-machine stack, 1024 words or 2,048 bytes, is a static array.
> It is counted in the 30,848 above, not in the runtime figure.

DaisyBASIC's heap is left alone. The two never run at the same time, and the
interpreter gives its memory back before the BASIC prompt returns.

## Corrections to the Interpreter

Seven faults had to be fixed. Four follow from the smaller
resident area described above and would not show up in a port that kept JZIP's
paging. Two are old faults that a 40-column display brings out.

They are written down in full here, because anyone merging a later JZIP release
will have to apply them again.

### 1. `z_restart` Runs Past the End of Dynamic Memory

**Symptom:** memory corruption after the RESTART command.

`z_restart` reloads dynamic memory in whole pages:

    restart_size = ( h_restart_size / PAGE_SIZE ) + 1;
    for ( i = 0; i < restart_size; i++ )
        read_page( i, &datap[i * PAGE_SIZE] );

For ZORK I that is 24 pages, or 12,288 bytes, written into a buffer of 11,859.
The overrun is **429 bytes**. JZIP gets away with it only because its `datap` is
20,480 bytes long.

**Fix:** the region is copied from the flash image by `ZmReloadDynamic()`.

### 2. The Data Accessors Do Not Reach the Dictionary

**Symptom:** the parser recognises no words at all, and reads past the end of
the allocation.

JZIP defines its accessors as plain subscripts:

    #define get_byte(offset) ((zbyte_t) datap[offset])

In ZORK I the dictionary sits at `$3B21` to `$4E37`, that is 15,137 to 20,023,
which is **above** the 11,859-byte dynamic region. Every dictionary lookup would
read past the end of `datap`.

**Fix:** `get_byte`, `get_word`, `set_byte` and `set_word` are now bounds-aware
inline functions. Reads at or above `zm_dyn_size` are answered from the flash
story image.

Writes at or above that line break the Z-machine specification. JZIP soaks them
up silently into its resident copy of read-only data. Flash cannot be written at
all, so such a store is now dropped and reported through `ZmWriteGuard()`.

### 3. Unchecked Pointers Into Dynamic Memory

**Symptom:** none seen in normal play. A bad story file could read outside the
allocation.

Three routines take a raw C pointer into `datap` from an address the story
supplies: `tokenise_line`, `z_sread_aread` and `z_encode`.

**Fix:** each is now guarded by `ZmDynamicRange()`.

### 4. SAVE Does Not Consume Its Branch Data

**Symptom:** `interpret(): failing opcode: 0` right after typing SAVE.

This one needs explaining. In Z-machine versions 1 to 3, SAVE is a **branch**
instruction. The bytes after the opcode are branch data, and the interpreter has
to read them and act on them. From version 4 on, SAVE stores a result instead.

The rewritten `z_save` returned a status code to its caller but did neither. The
branch data was left in the instruction stream and decoded as the next opcode.

**Fix:** every exit from `z_save` and `z_restore`, including the early refusals,
now goes through one reporting point:

    if ( h_type < V4 )
        conditional_jump( status == 0 );
    else
        store_operand( status == 0 ? 1 : 0 );   /* 2 for restore */

This matters even though saving is unavailable, because refusing a save has to
leave the interpreter running.

### 5. The Status Line Assumes Eighty Columns

**Symptom:** on a 40-column display, ZORK I prints

    North of Score  0        Moves: 2

where it should print the room name, the score and the move count.

`z_show_status` puts its fields at `screen_cols - 31` and `screen_cols - 15`. On
an 80-column screen those are columns 49 and 65. On Daisy's 40-column screen
they are columns 9 and 25.

The room name "North of House" is fourteen characters and has already gone past
column 9. `pad_line` sets `status_pos = column` no matter what, which *rewinds*
the buffer instead of advancing it, and the score field lands on top of the room
name.

**Fix:** displays narrower than 72 columns use short labels and field positions
of `screen_cols - 14` and `screen_cols - 8`, which leaves twenty-five columns
for the room name:

     West of House             S:0   M:4

> **NOTE:** `pad_line`'s rewinding was left in on purpose. It is what cuts an
> over-long room name short, and it is also what holds the status buffer to
> `screen_cols + 1` bytes. Taking it out would cause an overflow.

### 6. The [MORE] Prompt Is Never Erased

**Symptom:** a stray `[MORE]` sits in the middle of the text.

JZIP prints the prompt, waits for a key, then counts on the story's next output
to write over it. When that output happens to be a newline, the prompt survives
and scrolls up into the story text. ZORK III shows this on its opening screen.

**Fix:** the cursor goes back to the start of the prompt and the line is erased
once the key is pressed.

### 7. STOP Does Not Stop the Story

**Symptom:** pressing STOP or CTRL-C puts the game in a loop printing
"Beg pardon?".

Ending the current read is not enough. `input_line` returned an empty line, the
story's parser printed its complaint and asked again, and the next read returned
nothing straight away, so the game sat in that loop until the power went off.

What actually ends a story is `interpreter_state`, which `interpret()` tests at
the top of every instruction. Nothing was setting it.

There was a second fault behind the first. `ZmHostPollKey()` returns 0 both for
"no key waiting" and for "STOP pressed", so the timed reads could not see a stop
request at all and simply waited out their timeout.

**Fix:** `ZmHostQuitRequested()` was added to the host seam, and all six places
that wait for a key now set `interpreter_state` to `STOP`. The interpreter
leaves its loop as soon as the current opcode finishes, before the parser ever
sees the empty line.

> **NOTE:** This was found on the machine, not by the test harness, which had
> been quietly rescuing itself. When a script ran out the harness typed `quit`
> for the story, so the abandon path was never taken. `<STOP>` in a script now
> stands in for the key and is terminal, as it is on the machine.

### A Fatal Error Stops the Machine

Not a JZIP fault, but an A2Z one, and worth fixing. Its `fatal()` prints to the
serial port and then loops forever flashing an LED. On Daisy that would mean a
fault in a story file makes the computer unusable until you switch it off.

**Fix:** `fatal()` now unwinds to `ZmRunStory()` using `setjmp` and `longjmp`. A
bad story puts the user back at the BASIC prompt with a message. A test that
feeds the interpreter four kilobytes of random data checks this.

## The Display

The screen driver, `zm_vt52.cpp`, implements JZIP's screen interface by sending
VT52 escape sequences **and nothing else**. It mentions DaisyVideo nowhere, nor
shadow RAM, nor any other part of DaisyOS.

That has two results. On the machine, the byte stream goes to the VT52 engine
described below, which draws it. Under test, the same byte stream goes to a
simulated screen that a test program can read back.

The driver sticks to sequences the DaisyOS terminal implements:

| Sequence | Effect |
|---|---|
| `ESC Y r c` | Position cursor. Row and column each biased by 32. |
| `ESC J` | Erase from cursor to end of screen. |
| `ESC K` | Erase from cursor to end of line. |
| `ESC [ 2 J` | Clear the whole screen. |
| `ESC [ t ; b r` | Set scrolling region to rows t through b, 1-based. |
| `ESC [ 7 m` | Reverse video on. |
| `ESC [ 0 m` | Reverse video off. |

> **NOTE:** JZIP counts rows and columns from 1. VT52 counts from 0 and adds 32.
> Every coordinate is converted on the way out.

The scrolling region is what holds the status line still while the story text
scrolls underneath it. It is reset whenever the story changes the window split.

`src/daisyos/vt52.cpp` draws a VT52 stream into the DaisyVideo shadow RAM. It
was taken out of the display half of `terminal.cpp` so that anything producing
terminal output can share it.

It does not talk to a host. There is no serial port in it, no status bar and no
menu. A caller that needs those keeps them and feeds bytes in through
`Vt52Write()`.

Printable characters are collected into a run buffer and written with a single
`PutStringAt` and one attribute message. One video message per character is too
slow to keep up with a program printing a full screen of text.

## DaisyOS System Calls

The interpreter reaches DaisyOS through eleven functions and no others. They are
declared in `include/zmachine/zm_internal.h` and written in
`src/zmachine/zm_host_daisy.cpp`. The test harness supplies its own versions of
the same eight.

| Function | Purpose |
|---|---|
| `ZmHostPutByte(c)` | One byte of terminal output. |
| `ZmHostFlush()` | Push batched output to the display. |
| `ZmHostGetKey()` | Wait for a keystroke. Returns 0 to drop the story. |
| `ZmHostPollKey()` | Return a keystroke if one is waiting, else 0. |
| `ZmHostQuitRequested()` | True once STOP has been pressed. |
| `ZmHostIdle()` | Yield. Called once per instruction. |
| `ZmHostFatal(s)` | Report an error there is no recovering from. |
| `ZmHostSaveOpen(slot, writing)` | Open a saved game. Always fails. |
| `ZmHostSaveRead/Write/Close` | Transfer a saved game. |

Here is every DaisyOS entry point the module uses.

From `src/daisyos/vt52.cpp`, for the display:

| Call | Header | Purpose |
|---|---|---|
| `Clrscr(ch)` | `shadow_ram.h` | Fill the screen with a character. |
| `PlotChar(x, y, ch)` | `shadow_ram.h` | Write one character cell. |
| `FillCells(x, y, ch, n)` | `shadow_ram.h` | Fill a run of cells. |
| `PutStringAt(cell, s, len)` | `shadow_ram.h` | Write a run of characters. |
| `VideoMsgSendPutAttribsAtCell(cell, a, n)` | `video_messages.h` | Set inverse video. |
| `VideoMsgSendMoveBlock(sx, sy, dx, dy, w, h)` | `video_messages.h` | Scroll by block move. |
| `AudioMsgSendToneOn(v, hz, ms)` | `audio_messages.h` | Sound the bell on BEL. |
| `millis()` | Arduino | Cursor blink timing. |

From `src/zmachine/zm_host_daisy.cpp`, for the keyboard:

| Call | Header | Purpose |
|---|---|---|
| `BufferGet()` | `buffer.h` | Take a key from the ring. Returns 0 when empty. |
| `STOP_KEY` | `keyboard.h` | Drop the story. |
| `CTRL_C_INTERNAL` | `keyboard.h` | The same. |

> **NOTE:** Timer TC3 scans the keyboard matrix on an interrupt and fills a
> lock-free ring buffer on its own. The interpreter can therefore sit in
> `ZmHostGetKey()` without losing keystrokes. It must still call `Vt52Tick()`
> while it waits, or the cursor stops blinking and the machine looks dead.

`ZmHostPollKey()` returns 0 both when no key is waiting and when STOP has been
pressed, so a caller polling in a loop must ask `ZmHostQuitRequested()` to tell
the two apart. Ending the read is not on its own enough to stop a story. See
correction 7.

Two changes were made outside the module. Both are small, and both follow the
pattern already set by INVADERS, CONWAY and TERM.

In `include/daisybasic/basic_internal.h`:

    #include "zmachine.h"

In `src/daisybasic/basic_execute.cpp`, in the immediate-command dispatcher:

    if ((rest = MatchCommand(line, "zork")) != NULL) {
      RunZMachine();
      Newline();
      if (showReady) {
        PrintReady();
      }
      return true;
    }

`RunZMachine()` takes over the display, offers the built-in stories if there is
more than one, plays the one you pick, and puts the display back before it
returns.

## Saving and Restoring

**Saving and restoring are not available.** SAVE and RESTORE reply that the file
could not be opened, and play carries on.

The reason is that Daisy has no writable storage of its own. The only writable
medium is DaisyFile, reached over the WiFi modem.

Writing would work. `CommMsgSendFprint` carries up to 255 bytes in a frame, and
a saved game is 13,907 bytes, or fifty-five frames.

Reading would not. `CommMsgSendFget` moves **one byte per round trip**, each one
a request and an acknowledgement, at 38,400 baud. Reading 13,907 bytes that way
would take several minutes.

To switch the feature on, add a block read to the DaisyFile protocol. Only the
four `ZmHostSave*` functions need to change. The code above them is finished and
is tested by the harness, which uses ordinary files.

## Adding a Story

Story files are not supplied. To install one:

    tools/mkstory.py ZORK1.DAT > src/zmachine/zm_stories.cpp
    pio run -t upload

The tool says what it has done:

    ZORK1          v3   84992 bytes flash, 11859 bytes RAM when running

You can give it several stories at once. With more than one built in, the ZORK
command offers a menu, and the name shown there is the file name unless you
give a better one:

    tools/mkstory.py "Zork I=ZORK1.DAT" \
                     "Zork II=ZORK2.DAT" \
                     "Zork III=ZORK3.DAT" > src/zmachine/zm_stories.cpp

    Zork I         v3   84992 bytes flash, 11859 bytes RAM when running
    Zork II        v3   90112 bytes flash, 11189 bytes RAM when running
    Zork III       v3   82944 bytes flash, 11656 bytes RAM when running
                       258048 bytes of flash total

Keep the names short. The menu is 40 columns wide and indents them by four.

Mind the flash budget. Each story takes 80 to 90 kilobytes of the Due's 512, of
which DaisyOS and the interpreter use about 179. Three Infocom-sized stories is
close to the limit; a fourth would not fit.

## The Test Harness

The interpreter can be run without hardware. `tools/zmtest/` compiles the *same*
source files natively and runs them against a simulated 40 by 25 VT52 screen.

    tools/zmtest/build.sh
    tools/zmtest/run-tests.sh /path/to/stories
    tools/zmtest/zmtest ZORK1.DAT

The simulated screen implements exactly the sequences `daisyos/vt52.cpp`
implements and no others. So a sequence it cannot draw is one the real terminal
would not have drawn either.

The suite makes thirty-one checks, each on what actually reaches the screen
rather than on the interpreter's internal state:

    boot and parser ...................... 5 checks
    movement and world model ............. 3
    objects .............................. 3
    status line fits 40 columns .......... 3
    save and restore ..................... 5
    STOP abandons the story .............. 5
    restart .............................. 2
    [MORE] paging ........................ 2
    other stories ........................ 2
    corrupt story rejected ............... 1
                                          --
                                          31

The third argument to `zmtest` is a file of commands, one to a line. Set
`ZM_SCROLLBACK` in the environment to print every line that has scrolled off the
top of the display.

## Story File Header Fields

Offsets are from the start of the story file. All values are words, high byte
first, unless noted.

| Offset | JZIP name | Contents |
|---|---|---|
| `$00` | `h_type` | Z-machine version, 1 to 8. One byte. |
| `$01` | `h_config` | Configuration flags. One byte. |
| `$02` | `h_version` | Release number. |
| `$04` | `h_data_size` | Base of high memory. See the warning above. |
| `$06` | `h_start_pc` | Initial program counter. |
| `$08` | `h_words_offset` | Dictionary. |
| `$0A` | `h_objects_offset` | Object table. |
| `$0C` | `h_globals_offset` | Global variables. |
| `$0E` | `h_restart_size` | Base of static memory. See the warning above. |
| `$10` | `h_flags` | Interpreter capability flags. |
| `$18` | `h_synonyms_offset` | Abbreviations table. |
| `$1A` | `h_file_size` | File length, divided by the version scaler. |
| `$1C` | `h_checksum` | Sum of all bytes from `$40` to the end. |

## Memory Map, Zork I

    Story file length ........................... 84,992   $014C00

    Abbreviations table .............................. 496   $01F0   dynamic
    Object table ..................................... 688   $02B0   dynamic
    Global variables ............................... 8,817   $2271   dynamic
    ----------------------------------------------------------------
    Base of static memory ($0E) ................... 11,859   $2E53
    ----------------------------------------------------------------
    Dictionary .................................... 15,137   $3B21   static
    ----------------------------------------------------------------
    Base of high memory ($04) ..................... 20,023   $4E37
    ----------------------------------------------------------------
    Initial program counter ($06) ................. 20,229   $4F05   high

    In RAM while playing .......................... 11,859 bytes
    Read from flash in place ...................... 73,133 bytes

## Files of the Module

    include/zmachine.h              Public interface. Three functions.
    include/zmachine/
      zm_port.h                     Firmware and host-test differences.
      zm_types.h                    JZIP's ztypes.h, adapted.
      zm_story.h                    Story descriptors, flash access.
      zm_internal.h                 The host seam.
      zm_run.h                      Story startup, abort handling.

    src/zmachine/
      zm_interpre.cpp               JZIP, kept
      zm_control.cpp                        "
      zm_math.cpp                           "
      zm_object.cpp                         "
      zm_operand.cpp                        "
      zm_property.cpp                       "
      zm_text.cpp                           "
      zm_variable.cpp                       "
      zm_input.cpp                          "
      zm_screen.cpp                         "
      zm_extern.cpp                         "
      zm_memory.cpp                 REPLACED. The memory model.
      zm_fileio.cpp                 REPLACED. Story access.
      zm_osdep.cpp                  REPLACED. Errors, random numbers.
      zm_vt52.cpp                   REPLACED. Screen driver.
      zm_run.cpp                    NEW. Startup and teardown.
      zm_machine.cpp                NEW. Story picker, ZORK command.
      zm_host_daisy.cpp             NEW. The only file touching DaisyOS.
      zm_stories.cpp                GENERATED by tools/mkstory.py.

    include/vt52.h                  Shared VT52 engine.
    src/daisyos/vt52.cpp                    "

    tools/mkstory.py                Story file to flash array.
    tools/zmtest/                   Native test harness.

## Limitations

**Saving and restoring are not available.** See Saving and Restoring, above.

**`terminal.cpp` still holds its own copy of the VT52 parser.** The shared engine
in `daisyos/vt52.cpp` was taken out of it and is what the Z-machine uses, but the
TERM command has not been moved over to it. Doing so would get rid of the
duplication and is the obvious next job. It was left alone here because it means
changing working code, which needs testing on real hardware.

**Accented characters are dropped.** The Z-machine's extended character set has
the accented letters of Western European languages. The DaisyVideo character
generator has no shapes for them.

**Flash holds about three stories.** DaisyOS and the interpreter take 178,888
bytes of the Due's 524,288, and an Infocom story is 80 to 90 kilobytes. The
three ZORK games fit with 87,304 bytes to spare. A fourth of that size would
not.

**Version 6 is not supported.** Its graphics and sound have nothing to match
them on this machine. Versions 1 through 5 and 7 through 8 should run. Only
version 3 has been tested, that being the ZORK trilogy.

**The display is forty columns.** Infocom set their text for eighty. Prose reads
perfectly well at forty, and the interpreter re-wraps it, but tables and diagrams
inside a story, such as the maze plans in ZORK III, were laid out for a wider
screen.

---

*Daisy is a homebrew computer by Joe Cassara.*
*This document, and the port it describes, are issued under the GNU General*
*Public Licence, version 3.*
