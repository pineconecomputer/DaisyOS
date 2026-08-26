/*
 * DaisyOS - main firmware for the Daisy computer.
 * Copyright (C) 2026 Joe Cassara
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/*
 * The Daisy end of the Z-machine's host seam.
 *
 * Output goes to the shared VT52 engine, which renders into the DaisyVideo
 * shadow RAM. Input comes from the keyboard ring the matrix scanner fills on
 * its timer interrupt. Neither the interpreter nor the screen driver above it
 * refers to DaisyOS at all; this file is the only place the two meet.
 *
 * Excluded from the native test build, which supplies its own implementations
 * of the same functions in tools/zmtest/harness.cpp.
 */

#ifndef ZM_HOST_BUILD

#include "zmachine/zm_internal.h"
#include "zmachine/zm_run.h"

#include "vt52.h"
#include "buffer.h"
#include "keyboard.h"
#include "shadow_ram.h"

/* Set while a story is running so the key reader can unwind on request. */
static bool zm_quit_requested = false;

void ZmQuitRequest(void) { zm_quit_requested = true; }

void ZmHostPutByte(uint8_t c) { Vt52Write(c); }

void ZmHostFlush(void) { Vt52Flush(); }

void ZmHostIdle(void) { Vt52Tick(); }

/*
 * ZmHostGetKey
 *
 * Blocks until a key arrives. The scanner runs on TC3 and fills the ring
 * independently, so spinning here does not lose keystrokes, but the cursor
 * still has to be blinked or the machine looks hung.
 *
 * STOP unwinds the interpreter: it returns 0, which input_line reports as an
 * empty line and ZMachineRun turns into a return to the caller. Without it a
 * story stuck waiting for input could only be escaped by a power cycle.
 */
uint8_t ZmHostGetKey(void) {
  Vt52Flush();

  for (;;) {
    uint8_t k = BufferGet();

    if (k != 0) {
      if (k == STOP_KEY || k == CTRL_C_INTERNAL) {
        zm_quit_requested = true;
        return 0;
      }
      return k;
    }

    if (zm_quit_requested) {
      return 0;
    }
    Vt52Tick();
  }
}

uint8_t ZmHostPollKey(void) {
  uint8_t k = BufferGet();

  if (k == STOP_KEY || k == CTRL_C_INTERNAL) {
    zm_quit_requested = true;
    return 0;
  }
  return k;
}

int ZmHostQuitRequested(void) { return zm_quit_requested ? 1 : 0; }

void ZmHostResetQuit(void) { zm_quit_requested = false; }

/*
 * ZmHostFatal
 *
 * Only reached if the interpreter aborts before ZMachineRun has armed its
 * jump target, which in practice cannot happen. Leaves a message rather than
 * hanging the machine.
 */
void ZmHostFatal(const char* message) {
  Vt52Init();
  Vt52Write('\r');
  Vt52Write('\n');
  while (*message != '\0') {
    Vt52Write((uint8_t)*message++);
  }
  Vt52Flush();
}

/*
 * Save games.
 *
 * Not available in this build, and the interpreter is told so rather than
 * being left to fail in some less obvious way: z_save reports "Cannot open
 * save file" and, importantly, still performs the branch the story is waiting
 * on, so refusing a save is not the same as crashing.
 *
 * The only writable storage Daisy has is DaisyFile, reached over the WiFi
 * modem through comm_messages. Writing would be workable, since FPRINT
 * carries up to 255 bytes per frame and a save is about 14 KB, but reading is
 * one byte per request-and-acknowledge round trip at 38400 baud. Restoring
 * 13,907 bytes that way would take minutes. Enabling save and restore means
 * adding a block read to the DaisyFile protocol first; the stream interface
 * below is already shaped for it, so only these four functions change.
 *
 * UNDO is unaffected, since it is held in RAM, though V3 stories such as Zork
 * do not use it.
 */
struct ZmSaveStream* ZmHostSaveOpen(int slot, int writing) {
  (void)slot;
  (void)writing;
  return NULL;
}

int ZmHostSaveRead(struct ZmSaveStream* s, void* buf, unsigned len) {
  (void)s;
  (void)buf;
  (void)len;
  return 0;
}

int ZmHostSaveWrite(struct ZmSaveStream* s, const void* buf, unsigned len) {
  (void)s;
  (void)buf;
  (void)len;
  return 0;
}

void ZmHostSaveClose(struct ZmSaveStream* s) { (void)s; }

#endif /* !ZM_HOST_BUILD */
