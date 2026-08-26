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
 * VT52 screen engine.
 *
 * Renders a VT52 byte stream, with the handful of ANSI CSI sequences that are
 * worth having, into the DaisyVideo shadow RAM. This is the display half of
 * what the TERM command does, separated out so that anything producing
 * terminal output can share it. The Z-machine is the first such caller: it
 * emits VT52 and knows nothing about DaisyVideo.
 *
 * What this deliberately does not do is talk to a host. There is no serial
 * port here, no status bar and no menu; a caller that needs those keeps them
 * itself and feeds received bytes in through Vt52Write.
 *
 * Recognised sequences:
 *
 *   ESC A B C D    cursor up, down, right, left
 *   ESC H          home
 *   ESC Y r c      direct cursor address, both biased by 32
 *   ESC I          reverse line feed
 *   ESC J          erase to end of screen
 *   ESC K          erase to end of line
 *   ESC E          clear screen and home
 *   ESC 7 8        save and restore cursor
 *   CSI n A/B/C/D  cursor movement by n
 *   CSI r;c H or f direct cursor address, 1-based
 *   CSI n J        erase to end of screen, or the whole screen when n is 2
 *   CSI K          erase to end of line
 *   CSI t;b r      set the scrolling region, 1-based and inclusive
 *   CSI 0/7 m      normal and reverse video
 *   CSI s u        save and restore cursor
 */

#ifndef INCLUDE_VT52_H_
#define INCLUDE_VT52_H_

#include <Arduino.h>

#define VT52_COLS 40
#define VT52_ROWS 25

/* Clears the screen, resets the scrolling region to the whole display and
 * puts the cursor at the top left. */
void Vt52Init(void);

/* Feeds one byte of terminal output. */
void Vt52Write(uint8_t ch);

void Vt52WriteBuf(const uint8_t* buf, uint16_t len);

/* Pushes any characters still batched in the run buffer to the video board.
 * Output is accumulated so that a run of text becomes one video message
 * rather than one per character, so this has to be called before anything
 * that depends on the screen being up to date, such as waiting for a key. */
void Vt52Flush(void);

/* Blinks the cursor. Call regularly from an idle loop; it paces itself. */
void Vt52Tick(void);

/* Removes the cursor from the display, for a caller about to hand the screen
 * to something else. */
void Vt52HideCursor(void);

/* Current cursor position, zero based. */
uint8_t Vt52GetX(void);
uint8_t Vt52GetY(void);

#endif  // INCLUDE_VT52_H_
