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

// Conway's Game of Life, ported from conway.bas. A live cell is a reversed
// video attribute, so the whole 40x25 text screen is the board and no glyphs
// are redefined.

#include "conway.h"
#include "shadow_ram.h"
#include "video_messages.h"
#include "keyboard.h"
#include "buffer.h"
#include <Arduino.h>
#include <string.h>

// Milliseconds between generations. The BASIC version ran flat out and was
// slow enough to watch; in C the pacing has to be deliberate.
#define GEN_MS 80

// Attribute byte for a cell, matching what SetAttribAt writes.
#define ATTR_ALIVE 0xFF
#define ATTR_DEAD 0x00

static uint8_t grid[VID_HEIGHT][VID_WIDTH];
static uint8_t next[VID_HEIGHT][VID_WIDTH];

static uint32_t rng;

// Xorshift pseudo-random generator, same as the one invaders uses -- the soup
// only needs one bit per cell, so the library RNG is not worth pulling in.
static inline uint32_t xrng(void) {
  rng ^= rng << 13;
  rng ^= rng >> 17;
  rng ^= rng << 5;
  return rng;
}

// Draws the whole board, one message per row rather than one per cell. This
// leaves the shadow RAM's attribute mirror stale, as invaders does for its HUD
// rows; the Clrscr on exit puts both sides back in step.
static void draw(void) {
  char row[VID_WIDTH];
  for (uint8_t y = 0; y < VID_HEIGHT; y++) {
    for (uint8_t x = 0; x < VID_WIDTH; x++) {
      row[x] = grid[y][x] ? (char)ATTR_ALIVE : (char)ATTR_DEAD;
    }
    VideoMsgSendPutAttribsAtCell((uint16_t)y * VID_WIDTH, row, VID_WIDTH);
  }
}

// Fills the board with a random 50/50 soup, as line 25 of the BASIC does.
static void seed(void) {
  for (uint8_t y = 0; y < VID_HEIGHT; y++) {
    for (uint8_t x = 0; x < VID_WIDTH; x++) {
      grid[y][x] = (uint8_t)(xrng() & 1);
    }
  }
}

// Counts the eight neighbours of a cell. Cells off the edge count as dead,
// which is what the BASIC got from ATTRIBAT's out-of-range check -- the board
// does not wrap.
static uint8_t neighbours(int8_t x, int8_t y) {
  uint8_t count = 0;
  for (int8_t dy = -1; dy <= 1; dy++) {
    int8_t ny = y + dy;
    if (ny < 0 || ny >= VID_HEIGHT) {
      continue;
    }
    for (int8_t dx = -1; dx <= 1; dx++) {
      if (dx == 0 && dy == 0) {
        continue;
      }
      int8_t nx = x + dx;
      if (nx < 0 || nx >= VID_WIDTH) {
        continue;
      }
      count += grid[ny][nx];
    }
  }
  return count;
}

// Advances one generation. Every cell is judged against the current board and
// written to a second one, so the update is synchronous -- the BASIC did the
// same by reading ATTRIBAT and deferring the SETATTRIB pass to line 150.
static void step(void) {
  for (uint8_t y = 0; y < VID_HEIGHT; y++) {
    for (uint8_t x = 0; x < VID_WIDTH; x++) {
      uint8_t n = neighbours((int8_t)x, (int8_t)y);
      if (grid[y][x]) {
        next[y][x] = (n == 2 || n == 3) ? 1 : 0;
      } else {
        next[y][x] = (n == 3) ? 1 : 0;
      }
    }
  }
  memcpy(grid, next, sizeof(grid));
}

// Waits between generations while staying responsive, returning the key that
// interrupted the wait, or 0 if the full interval elapsed.
static uint8_t waitKey(uint32_t ms) {
  uint32_t end = millis() + ms;
  while (millis() < end) {
    uint8_t k = BufferGet();
    if (k) {
      return k;
    }
  }
  return 0;
}

// Runs until the player presses BREAK. R reseeds the board, which is useful
// once a soup has settled into still lifes and blinkers.
void RunConway(void) {
  rng = millis() | 1;

  Clrscr(' ');
  LocateCursor(0, 0);
  seed();
  draw();

  while (1) {
    uint8_t k = waitKey(GEN_MS);
    if (k == CTRL_C_KEY) {
      break;
    }
    if (k == 'r' || k == 'R') {
      seed();
      draw();
      continue;
    }
    step();
    draw();
  }

  Clrscr(' ');
  LocateCursor(0, 0);
}
