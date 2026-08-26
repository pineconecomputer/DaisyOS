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
 * VT52 screen engine. See vt52.h for the sequences understood.
 *
 * The parser is a small state machine because bytes arrive one at a time and
 * an escape sequence can be split across arrivals. Printable characters are
 * gathered into a run buffer and written as a single PutStringAt plus one
 * attribute message; sending a video message per character is too slow to
 * keep up with a program printing a screenful of text.
 *
 * Scrolling is done on the video board with a block move rather than by
 * redrawing, which is why the shadow RAM helpers are used instead of writing
 * cells directly.
 */

#include "vt52.h"
#include "shadow_ram.h"
#include "video_messages.h"
#include "audio_messages.h"

#define CURSOR_BLINK_MS 500

static uint8_t cx, cy;
static uint8_t saved_cx, saved_cy;
static uint8_t scroll_top, scroll_bot;
static bool reverse_on;
static bool cursor_on;
static uint32_t cursor_timer;

/* Parser state: 0 idle, 1 after ESC, 2 and 3 collecting ESC Y coordinates,
 * 4 collecting CSI parameters. */
static uint8_t es;
static uint8_t esc_y_row;
static uint8_t csi_p[4];
static uint8_t csi_np;

/* Pending run of printable characters. */
static uint8_t txbuf[VT52_COLS];
static uint8_t txlen;
static uint8_t txcol, txrow;

static inline uint16_t cell_of(uint8_t x, uint8_t y) {
  return (uint16_t)y * VT52_COLS + x;
}

static void cursor_show(void) {
  if (!cursor_on) {
    uint8_t attr = 0xFF;
    VideoMsgSendPutAttribsAtCell(cell_of(cx, cy), (char*)&attr, 1);
    cursor_on = true;
  }
}

static void cursor_hide_internal(void) {
  if (cursor_on) {
    uint8_t attr = reverse_on ? 0xFF : 0x00;
    VideoMsgSendPutAttribsAtCell(cell_of(cx, cy), (char*)&attr, 1);
    cursor_on = false;
  }
}

void Vt52HideCursor(void) { cursor_hide_internal(); }

void Vt52Tick(void) {
  uint32_t now = millis();
  if (now - cursor_timer >= CURSOR_BLINK_MS) {
    cursor_timer = now;
    if (cursor_on) {
      cursor_hide_internal();
    } else {
      cursor_show();
    }
  }
}

/* Writes the pending run and its attributes as one pair of video messages. */
void Vt52Flush(void) {
  if (txlen == 0) {
    return;
  }
  {
    uint16_t cell = cell_of(txcol, txrow);
    uint8_t attrs[VT52_COLS];
    uint8_t attr = reverse_on ? 0xFF : 0x00;

    PutStringAt(cell, (char*)txbuf, txlen);
    memset(attrs, attr, txlen);
    VideoMsgSendPutAttribsAtCell(cell, (char*)attrs, txlen);
  }
  txlen = 0;
}

static void scroll_up(void) {
  Vt52Flush();
  if (scroll_bot > scroll_top) {
    VideoMsgSendMoveBlock(0, scroll_top + 1, 0, scroll_top, VT52_COLS,
                          scroll_bot - scroll_top);
  }
  FillCells(0, scroll_bot, ' ', VT52_COLS);
}

static void scroll_down(void) {
  Vt52Flush();
  if (scroll_bot > scroll_top) {
    VideoMsgSendMoveBlock(0, scroll_top, 0, scroll_top + 1, VT52_COLS,
                          scroll_bot - scroll_top);
  }
  FillCells(0, scroll_top, ' ', VT52_COLS);
}

static void linefeed(void) {
  if (cy == scroll_bot) {
    scroll_up();
  } else if (cy < VT52_ROWS - 1) {
    cy++;
  }
}

static void erase_eol(void) {
  Vt52Flush();
  if (cx < VT52_COLS) {
    FillCells(cx, cy, ' ', VT52_COLS - cx);
  }
}

static void erase_eos(void) {
  uint8_t row;
  erase_eol();
  for (row = cy + 1; row < VT52_ROWS; row++) {
    FillCells(0, row, ' ', VT52_COLS);
  }
}

static void clear_all(void) {
  Vt52Flush();
  Clrscr(' ');
  cursor_on = false;
}

static void handle_csi(uint8_t ch) {
  uint8_t p0 = csi_p[0];
  uint8_t p1 = csi_p[1];
  uint8_t n = p0 ? p0 : 1;

  es = 0;
  switch (ch) {
    case 'A':
      while (n-- && cy > 0) cy--;
      break;
    case 'B':
      while (n-- && cy < VT52_ROWS - 1) cy++;
      break;
    case 'C':
      while (n-- && cx < VT52_COLS - 1) cx++;
      break;
    case 'D':
      while (n-- && cx > 0) cx--;
      break;
    case 'H':
    case 'f':
      cy = p0 ? p0 - 1 : 0;
      cx = (csi_np >= 1 && p1) ? p1 - 1 : 0;
      if (cy >= VT52_ROWS) cy = VT52_ROWS - 1;
      if (cx >= VT52_COLS) cx = VT52_COLS - 1;
      break;
    case 'J':
      if (p0 == 2) {
        clear_all();
        cx = 0;
        cy = 0;
      } else {
        erase_eos();
      }
      break;
    case 'K':
      erase_eol();
      break;
    case 'r':
      scroll_top = p0 ? p0 - 1 : 0;
      scroll_bot = (csi_np >= 1 && p1) ? p1 - 1 : VT52_ROWS - 1;
      if (scroll_top >= VT52_ROWS) scroll_top = VT52_ROWS - 1;
      if (scroll_bot >= VT52_ROWS) scroll_bot = VT52_ROWS - 1;
      if (scroll_bot < scroll_top) scroll_bot = scroll_top;
      break;
    case 'm':
      if (p0 == 0) {
        reverse_on = false;
      } else if (p0 == 7) {
        reverse_on = true;
      }
      break;
    case 's':
      saved_cx = cx;
      saved_cy = cy;
      break;
    case 'u':
      cx = saved_cx;
      cy = saved_cy;
      break;
    default:
      break;
  }
}

static void handle_esc(uint8_t ch) {
  es = 0;
  switch (ch) {
    case 'A': if (cy > 0) cy--; break;
    case 'B': if (cy < VT52_ROWS - 1) cy++; break;
    case 'C': if (cx < VT52_COLS - 1) cx++; break;
    case 'D': if (cx > 0) cx--; break;
    case 'H': cx = 0; cy = 0; break;
    case 'E': clear_all(); cx = 0; cy = 0; break;
    case 'I':
      if (cy > scroll_top) {
        cy--;
      } else {
        scroll_down();
      }
      break;
    case 'J': erase_eos(); break;
    case 'K': erase_eol(); break;
    case 'Y': es = 2; break;
    case '7': saved_cx = cx; saved_cy = cy; break;
    case '8':
      cx = saved_cx;
      cy = saved_cy;
      if (cx >= VT52_COLS) cx = VT52_COLS - 1;
      if (cy >= VT52_ROWS) cy = VT52_ROWS - 1;
      break;
    case '[':
      csi_p[0] = csi_p[1] = csi_p[2] = csi_p[3] = 0;
      csi_np = 0;
      es = 4;
      break;
    default:
      break;
  }
}

void Vt52Write(uint8_t ch) {
  /* Anything that is not another printable character in the same run ends the
   * run, so the batch is flushed before the cursor moves. */
  switch (es) {
    case 1:
      Vt52Flush();
      cursor_hide_internal();
      handle_esc(ch);
      return;
    case 2:
      esc_y_row = (uint8_t)(ch - 32);
      es = 3;
      return;
    case 3:
      cy = esc_y_row;
      cx = (uint8_t)(ch - 32);
      if (cy >= VT52_ROWS) cy = VT52_ROWS - 1;
      if (cx >= VT52_COLS) cx = VT52_COLS - 1;
      es = 0;
      return;
    case 4:
      if (ch >= '0' && ch <= '9') {
        if (csi_np < 4) csi_p[csi_np] = csi_p[csi_np] * 10 + (ch - '0');
      } else if (ch == ';') {
        if (csi_np < 3) csi_np++;
      } else if (ch == '?') {
        /* private parameter introducer, accepted and ignored */
      } else {
        Vt52Flush();
        handle_csi(ch);
      }
      return;
    default:
      break;
  }

  switch (ch) {
    case 27:
      Vt52Flush();
      es = 1;
      break;
    case 13:
      Vt52Flush();
      cursor_hide_internal();
      cx = 0;
      break;
    case 10:
    case 11:
      Vt52Flush();
      cursor_hide_internal();
      linefeed();
      break;
    case 12:
      clear_all();
      cx = 0;
      cy = 0;
      break;
    case 7:
      AudioMsgSendToneOn(0, 1047, 75);
      break;
    case 8:
    case 127:
      Vt52Flush();
      cursor_hide_internal();
      if (cx > 0) {
        cx--;
        PlotChar(cx, cy, ' ');
      }
      break;
    case 9:
      Vt52Flush();
      cx = (uint8_t)((cx & ~7u) + 8);
      if (cx >= VT52_COLS) cx = VT52_COLS - 1;
      break;
    default:
      if (ch < 32) {
        break;
      }
      cursor_hide_internal();
      if (txlen == 0) {
        txcol = cx;
        txrow = cy;
      }
      txbuf[txlen++] = ch;
      cx++;
      if (cx >= VT52_COLS) {
        Vt52Flush();
        cx = 0;
        linefeed();
      }
      break;
  }
}

void Vt52WriteBuf(const uint8_t* buf, uint16_t len) {
  while (len-- > 0) {
    Vt52Write(*buf++);
  }
}

void Vt52Init(void) {
  cx = cy = 0;
  saved_cx = saved_cy = 0;
  scroll_top = 0;
  scroll_bot = VT52_ROWS - 1;
  reverse_on = false;
  cursor_on = false;
  cursor_timer = millis();
  es = 0;
  csi_np = 0;
  txlen = 0;
  Clrscr(' ');
}

uint8_t Vt52GetX(void) { return cx; }
uint8_t Vt52GetY(void) { return cy; }
