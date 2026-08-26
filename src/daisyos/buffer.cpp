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

#include "buffer.h"

static volatile uint8_t rb_buf[KB_BUF_SIZE];
static volatile uint8_t rb_head;
static volatile uint8_t rb_tail;

bool keyClickEnabled = false;
volatile bool keyClickPending = false;

// Empties the keystroke ring buffer.
void BufferInit(void) {
  rb_head = 0;
  rb_tail = 0;
}

// Pushes one keystroke from the scanner ISR. Drops the key if the buffer is
// full rather than overwriting, so the oldest input survives a burst. Also
// flags a pending key click for the main loop to sound.
void BufferAdd(uint8_t b) {
  uint8_t next = (rb_head + 1) & KB_BUF_MASK;
  if (next == rb_tail) {
    return;
  }
  rb_buf[rb_head] = b;
  rb_head = next;
  if (keyClickEnabled) {
    keyClickPending = true;
  }
}

// True if any keystroke is waiting.
bool BufferHasBytes(void) { return rb_head != rb_tail; }

// Pops the oldest keystroke, or 0 if none is waiting.
uint8_t BufferGet(void) {
  if (rb_head == rb_tail) {
    return 0;
  }
  uint8_t b = rb_buf[rb_tail];
  rb_tail = (rb_tail + 1) & KB_BUF_MASK;
  return b;
}

// Moves up to max waiting keystrokes into a caller buffer, returning the count.
// Lets the main loop take a whole burst in one pass.
size_t BufferDrain(uint8_t* dst, size_t max) {
  size_t n = 0;
  while (n < max && rb_head != rb_tail) {
    dst[n++] = rb_buf[rb_tail];
    rb_tail = (rb_tail + 1) & KB_BUF_MASK;
  }
  return n;
}

// Discards all pending keystrokes.
void BufferClear(void) { rb_tail = rb_head; }

// Removes the first occurrence of a key from the queue, shuffling everything
// ahead of it forward. Used to pull out keys the system handles itself, such
// as BREAK, without disturbing the order of the rest.
bool BufferScanAndRemove(uint8_t key) {
  uint8_t h = rb_head;
  uint8_t t = rb_tail;

  bool found = false;
  uint8_t pos = t;
  while (pos != h) {
    if (rb_buf[pos] == key) {
      found = true;
      break;
    }
    pos = (pos + 1) & KB_BUF_MASK;
  }
  if (!found) {
    return false;
  }

  uint8_t dst = pos;
  while (dst != t) {
    uint8_t src = (dst - 1) & KB_BUF_MASK;
    rb_buf[dst] = rb_buf[src];
    dst = src;
  }
  rb_tail = (t + 1) & KB_BUF_MASK;
  return true;
}
