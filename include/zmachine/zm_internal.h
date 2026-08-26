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
 * Interfaces internal to the Z-machine module, and the seam the module is
 * tested through.
 *
 * The interpreter never touches DaisyOS directly. Everything it needs from the
 * outside world is one of the ZmHost* calls below. The firmware implements
 * them in zm_osdep.cpp against the keyboard buffer and the VT52 engine; the
 * test harness implements them against a virtual screen it can assert on.
 */

#ifndef INCLUDE_ZMACHINE_ZM_INTERNAL_H_
#define INCLUDE_ZMACHINE_ZM_INTERNAL_H_

#include "zmachine/zm_port.h"

/* ---- provided by zm_memory.cpp ---- */

void ZmReloadDynamic(void);
void load_cache(void);
void unload_cache(void);

/* True when off is a legal index into dynamic memory. Used to keep a story
 * that hands the interpreter a bad buffer address from walking off datap. */
int ZmDynamicRange(unsigned long off, unsigned long len);

/* ---- the host seam ---- */

/* One byte of Z-machine output. The firmware feeds this to the VT52 engine;
 * everything above emits VT52 escape sequences and nothing else, so no part of
 * the interpreter knows what a DaisyVideo message looks like. */
void ZmHostPutByte(uint8_t c);

/* Flush any batched output so the screen matches what the game has printed.
 * Called before the interpreter waits for input. */
void ZmHostFlush(void);

/* Blocking read of one key. Returns 0 only if the host wants to abort, which
 * the interpreter treats as a quit request. */
uint8_t ZmHostGetKey(void);

/* Non-blocking poll. Returns 0 when nothing is waiting. Note that this is
 * also what a stop request looks like, so a caller polling in a loop has to
 * ask ZmHostQuitRequested to tell the two apart. */
uint8_t ZmHostPollKey(void);

/* True once the user has asked to abandon the story, by pressing STOP on the
 * machine or by the harness running out of input. Stays true until the next
 * story starts. */
int ZmHostQuitRequested(void);

/* Called while the interpreter is idle so the host can blink a cursor or
 * service background work. */
void ZmHostIdle(void);

/* Abort with a message the user can read. Does not return. */
void ZmHostFatal(const char* message);

/* Persistent storage for save games, keyed by slot. Returning 0 from Begin
 * means the host declined, and the interpreter reports a failed save. */
struct ZmSaveStream;
struct ZmSaveStream* ZmHostSaveOpen(int slot, int writing);
int ZmHostSaveRead(struct ZmSaveStream* s, void* buf, unsigned len);
int ZmHostSaveWrite(struct ZmSaveStream* s, const void* buf, unsigned len);
void ZmHostSaveClose(struct ZmSaveStream* s);

#endif  // INCLUDE_ZMACHINE_ZM_INTERNAL_H_
