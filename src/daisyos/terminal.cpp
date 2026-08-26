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

#include "terminal.h"
#include "shadow_ram.h"
#include "video_messages.h"
#include "audio_messages.h"
#include "keyboard.h"
#include "buffer.h"
#include "daisybasic.h"
#include "wifi.h"
#include <Arduino.h>
#include <string.h>
#include <stdio.h>

#define T_COLS 40
#define T_ROWS 25
#define CURSOR_BLINK_MS 500

#define BOX_H 196
#define BOX_V 179
#define BOX_TL 218
#define BOX_TR 191
#define BOX_BL 192
#define BOX_BR 217
#define BOX_LT 195
#define BOX_RT 180

#define CP437_BASE 128
#define CP437_COUNT 128

static const uint8_t cp437_bitmaps[CP437_COUNT * 8] = {
    0x7C, 0xC6, 0xC0, 0xC0, 0xC6, 0x7C, 0x18, 0x70, 0xCC, 0x00, 0xCC, 0xCC,
    0xCC, 0xCC, 0x76, 0x00, 0x0C, 0x18, 0x7C, 0xC6, 0xFE, 0xC0, 0x7C, 0x00,
    0x10, 0x38, 0x6C, 0x78, 0x0C, 0x7C, 0xCC, 0x76, 0xCC, 0x00, 0x78, 0x0C,
    0x7C, 0xCC, 0x76, 0x00, 0x60, 0x30, 0x78, 0x0C, 0x7C, 0xCC, 0x76, 0x00,
    0x38, 0x6C, 0x38, 0x0C, 0x7C, 0xCC, 0x76, 0x00, 0x00, 0x00, 0x7C, 0xC6,
    0xC0, 0xC6, 0x7C, 0x18, 0x10, 0x38, 0x6C, 0x7C, 0xC6, 0xFE, 0xC0, 0x7C,
    0xCC, 0x00, 0x7C, 0xC6, 0xFE, 0xC0, 0x7C, 0x00, 0x60, 0x30, 0x7C, 0xC6,
    0xFE, 0xC0, 0x7C, 0x00, 0x66, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3C, 0x00,
    0x18, 0x3C, 0x66, 0x00, 0x38, 0x18, 0x18, 0x3C, 0x60, 0x30, 0x00, 0x38,
    0x18, 0x18, 0x3C, 0x00, 0xC6, 0x38, 0x6C, 0xC6, 0xFE, 0xC6, 0xC6, 0x00,
    0x38, 0x6C, 0x38, 0x6C, 0xC6, 0xFE, 0xC6, 0x00, 0x18, 0x30, 0xFE, 0xC0,
    0xF8, 0xC0, 0xFE, 0x00, 0x00, 0x00, 0x6C, 0x12, 0x7E, 0xD0, 0x7E, 0x00,
    0x3E, 0x6C, 0xCC, 0xFE, 0xCC, 0xCC, 0xCE, 0x00, 0x10, 0x38, 0x6C, 0x7C,
    0xC6, 0xC6, 0x7C, 0x00, 0xC6, 0x00, 0x7C, 0xC6, 0xC6, 0xC6, 0x7C, 0x00,
    0x60, 0x30, 0x7C, 0xC6, 0xC6, 0xC6, 0x7C, 0x00, 0x18, 0x3C, 0x66, 0x00,
    0x66, 0x66, 0x3E, 0x00, 0x60, 0x30, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x00,
    0x66, 0x00, 0x66, 0x66, 0x66, 0x3E, 0x06, 0x7C, 0xC6, 0x38, 0x6C, 0xC6,
    0xC6, 0x6C, 0x38, 0x00, 0xC6, 0x00, 0xC6, 0xC6, 0xC6, 0xC6, 0x7C, 0x00,
    0x18, 0x18, 0x7E, 0xC0, 0xC0, 0x7E, 0x18, 0x18, 0x38, 0x6C, 0x60, 0xF0,
    0x60, 0x66, 0xFC, 0x00, 0xCC, 0x78, 0x30, 0xFC, 0x30, 0xFC, 0x30, 0x00,
    0xF0, 0xD8, 0xF0, 0xD8, 0xDE, 0xCC, 0xCE, 0xC0, 0x0E, 0x1B, 0x18, 0x7E,
    0x18, 0xD8, 0x70, 0x00, 0x18, 0x30, 0x78, 0x0C, 0x7C, 0xCC, 0x76, 0x00,
    0x18, 0x30, 0x00, 0x38, 0x18, 0x18, 0x3C, 0x00, 0x18, 0x30, 0x7C, 0xC6,
    0xC6, 0xC6, 0x7C, 0x00, 0x18, 0x30, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x00,
    0x76, 0xDC, 0x00, 0xDC, 0x66, 0x66, 0x66, 0x00, 0x76, 0xDC, 0x00, 0xC6,
    0xE6, 0xF6, 0xDE, 0xCE, 0x3C, 0x6C, 0x6C, 0x3E, 0x00, 0x7E, 0x00, 0x00,
    0x38, 0x6C, 0x6C, 0x38, 0x00, 0x7C, 0x00, 0x00, 0x18, 0x00, 0x18, 0x30,
    0x60, 0x66, 0x3C, 0x00, 0x00, 0x00, 0x00, 0xFE, 0xC0, 0xC0, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xFE, 0x06, 0x06, 0x00, 0x00, 0xC0, 0xC8, 0xD0, 0xFE,
    0x46, 0x8C, 0x1E, 0x02, 0xC0, 0xC8, 0xD0, 0xEC, 0x5C, 0xBE, 0x0C, 0x1E,
    0x18, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x33, 0x66, 0xCC,
    0x66, 0x33, 0x00, 0x00, 0x00, 0xCC, 0x66, 0x33, 0x66, 0xCC, 0x00, 0x00,
    0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x22, 0x88, 0x55, 0xAA, 0x55, 0xAA,
    0x55, 0xAA, 0x55, 0xAA, 0xDD, 0x77, 0xDD, 0x77, 0xDD, 0x77, 0xDD, 0x77,
    0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0xF8,
    0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0xF8, 0x18, 0xF8, 0x18, 0x18, 0x18,
    0x36, 0x36, 0x36, 0x36, 0xF6, 0x36, 0x36, 0x36, 0x00, 0x00, 0x00, 0x00,
    0xFE, 0x36, 0x36, 0x36, 0x00, 0x00, 0xF8, 0x18, 0xF8, 0x18, 0x18, 0x18,
    0x36, 0x36, 0xF6, 0x06, 0xF6, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0x00, 0x00, 0xFE, 0x06, 0xF6, 0x36, 0x36, 0x36,
    0x36, 0x36, 0xF6, 0x06, 0xFE, 0x00, 0x00, 0x00, 0x36, 0x36, 0x36, 0x36,
    0xFE, 0x00, 0x00, 0x00, 0x18, 0x18, 0xF8, 0x18, 0xF8, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xF8, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
    0x1F, 0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0xFF, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
    0x1F, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00,
    0x18, 0x18, 0x18, 0x18, 0xFF, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1F, 0x18,
    0x1F, 0x18, 0x18, 0x18, 0x36, 0x36, 0x36, 0x36, 0x37, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x37, 0x30, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x30,
    0x37, 0x36, 0x36, 0x36, 0x36, 0x36, 0xF7, 0x00, 0xFF, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xFF, 0x00, 0xF7, 0x36, 0x36, 0x36, 0x36, 0x36, 0x37, 0x30,
    0x37, 0x36, 0x36, 0x36, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00,
    0x36, 0x36, 0xF7, 0x00, 0xF7, 0x36, 0x36, 0x36, 0x18, 0x18, 0xFF, 0x00,
    0xFF, 0x00, 0x00, 0x00, 0x36, 0x36, 0x36, 0x36, 0xFF, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xFF, 0x00, 0xFF, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x3F, 0x00, 0x00, 0x00,
    0x18, 0x18, 0x1F, 0x18, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x18,
    0x1F, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x36, 0x36, 0x36,
    0x36, 0x36, 0x36, 0x36, 0xFF, 0x36, 0x36, 0x36, 0x18, 0x18, 0xFF, 0x18,
    0xFF, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0xF8, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x1F, 0x18, 0x18, 0x18, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F,
    0x0F, 0x0F, 0x0F, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x76, 0xDC, 0xC8, 0xDC, 0x76, 0x00, 0x00, 0x78, 0xCC, 0xF8,
    0xCC, 0xF8, 0xC0, 0xC0, 0x00, 0xFC, 0xCC, 0xC0, 0xC0, 0xC0, 0xC0, 0x00,
    0x00, 0x00, 0xFE, 0x6C, 0x6C, 0x6C, 0x6C, 0x00, 0xFC, 0xCC, 0x60, 0x30,
    0x60, 0xCC, 0xFC, 0x00, 0x00, 0x00, 0x7E, 0xD8, 0xD8, 0xD8, 0x70, 0x00,
    0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x7C, 0xC0, 0x00, 0x76, 0xDC, 0x18,
    0x18, 0x18, 0x18, 0x00, 0xFC, 0x30, 0x78, 0xCC, 0xCC, 0x78, 0x30, 0xFC,
    0x38, 0x6C, 0xC6, 0xFE, 0xC6, 0x6C, 0x38, 0x00, 0x38, 0x6C, 0xC6, 0xC6,
    0x6C, 0x6C, 0xEE, 0x00, 0x1C, 0x30, 0x18, 0x7C, 0xCC, 0xCC, 0x78, 0x00,
    0x00, 0x00, 0x7E, 0xDB, 0xDB, 0x7E, 0x00, 0x00, 0x06, 0x0C, 0x7E, 0xDB,
    0xDB, 0x7E, 0x60, 0xC0, 0x38, 0x60, 0xC0, 0xF8, 0xC0, 0x60, 0x38, 0x00,
    0x78, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0x00, 0x00, 0xFC, 0x00, 0xFC,
    0x00, 0xFC, 0x00, 0x00, 0x30, 0x30, 0xFC, 0x30, 0x30, 0x00, 0xFC, 0x00,
    0x60, 0x30, 0x18, 0x30, 0x60, 0x00, 0xFC, 0x00, 0x18, 0x30, 0x60, 0x30,
    0x18, 0x00, 0xFC, 0x00, 0x0E, 0x1B, 0x1B, 0x18, 0x18, 0x18, 0x18, 0x18,
    0x18, 0x18, 0x18, 0x18, 0x18, 0xD8, 0xD8, 0x70, 0x30, 0x30, 0x00, 0xFC,
    0x00, 0x30, 0x30, 0x00, 0x00, 0x76, 0xDC, 0x00, 0x76, 0xDC, 0x00, 0x00,
    0x38, 0x6C, 0x6C, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18,
    0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00,
    0x0F, 0x0C, 0x0C, 0x0C, 0xEC, 0x6C, 0x3C, 0x1C, 0x6C, 0x36, 0x36, 0x36,
    0x36, 0x00, 0x00, 0x00, 0x78, 0x0C, 0x38, 0x60, 0x7C, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x3C, 0x3C, 0x3C, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
};

static const uint8_t vt52_gfx_to_cp437[32] = {
    ' ', 4,   178, 26,  12,  27,  25,  248, 241, 25,  25,
    217, 191, 218, 192, 197, 196, 196, 196, 196, 196, 195,
    180, 193, 194, 179, 243, 242, 227, 247, 156, 250,
};

static bool cfg_echo = false;
static uint8_t cfg_return = 0;
static uint8_t cfg_bell = 0;
static bool cfg_screen = false;
static bool cfg_statusbar = true;
static bool reset_wifi = true;

#define STATUS_UPDATE_MS 1000

#define DOT_CHAR 4
static const uint8_t logo_bitmap[8] = {
    0x00, 0x18, 0x3C, 0x7E, 0x3C, 0x18, 0x00, 0x00,
};

#define RX_BUF_SIZE 512
static uint8_t rx_buf[RX_BUF_SIZE];
static volatile uint16_t rx_head, rx_tail;

// Moves everything waiting on the serial port into the terminal's own ring
// buffer. Called often, including mid-render, so the host is not throttled
// while the screen is being painted.
static void rx_drain(void) {
  while (Serial1.available()) {
    rx_buf[rx_head & (RX_BUF_SIZE - 1)] = (uint8_t)Serial1.read();
    rx_head++;
  }
}

// Bytes currently buffered.
static inline uint16_t rx_count(void) { return rx_head - rx_tail; }

// Pops the next buffered byte. Head and tail are free-running counters masked
// at use, so the size must stay a power of two.
static inline uint8_t rx_get(void) {
  return rx_buf[rx_tail++ & (RX_BUF_SIZE - 1)];
}

static bool gfx_mode;
static uint8_t cx, cy;
static uint8_t es;
static uint8_t yr;
static uint8_t csi_p[4];
static uint8_t csi_np;
static bool csi_priv;
static uint8_t saved_cx, saved_cy;
static uint8_t scroll_top, scroll_bot;
static bool app_cursor;
static bool sgr_reverse;
static bool cursor_on;
static uint32_t cursor_timer;
static uint32_t status_timer;
static uint8_t top_row;
static char status_prev[T_COLS];

// First row usable for text, one lower when the status bar is shown.
static inline uint8_t content_top(void) { return cfg_statusbar ? 1 : 0; }

// Renders the status bar into a buffer: title, date and time, and connection
// state.
static void status_build(char* buf) {
  memset(buf, ' ', T_COLS);

  buf[1] = DOT_CHAR;
  memcpy(&buf[3], "DaisyTerm", 9);

  int mo = RtcGetMonth();
  int dy = RtcGetDay();
  int yr = RtcGetYear() % 100;
  int h = RtcGetHours();
  int m = RtcGetMinutes();
  int s = RtcGetSeconds();
  char datetime[20];
  snprintf(datetime, sizeof(datetime), "%02d/%02d/%02d %02d:%02d:%02d", mo, dy,
           yr, h, m, s);
  int dlen = (int)strlen(datetime);
  int pos = T_COLS - dlen - 1;
  if (pos > 0) {
    memcpy(&buf[pos], datetime, dlen);
  }
}

// Paints the status bar in reverse video.
static void status_draw(void) {
  if (!cfg_statusbar) {
    return;
  }

  status_build(status_prev);

  PutStringAt(0, status_prev, T_COLS);
  uint8_t attr = cfg_screen ? 0x00 : 0xFF;
  uint8_t attrs[T_COLS];
  memset(attrs, attr, T_COLS);
  VideoMsgSendPutAttribsAtCell(0, (char*)attrs, T_COLS);
}

// Redraws the status bar only when its text actually changed, so an unchanged
// clock does not cost a video message every tick.
static void status_update(void) {
  if (!cfg_statusbar) {
    return;
  }

  char bar[T_COLS];
  status_build(bar);

  uint8_t attr = cfg_screen ? 0x00 : 0xFF;

  for (uint8_t i = 0; i < T_COLS; i++) {
    if (bar[i] != status_prev[i]) {
      uint8_t start = i;
      while (i < T_COLS && bar[i] != status_prev[i]) {
        i++;
      }
      uint8_t len = i - start;

      VideoMsgSendPutStringAtCell(start, &bar[start], len);
      uint8_t attrs[T_COLS];
      memset(attrs, attr, len);
      VideoMsgSendPutAttribsAtCell(start, (char*)attrs, len);

      memcpy(&status_prev[start], &bar[start], len);
    }
  }
}

// Blanks the status row when the bar is switched off.
static void status_clear(void) {
  FillCells(0, 0, ' ', T_COLS);
  uint8_t attr = cfg_screen ? 0xFF : 0x00;
  uint8_t attrs[T_COLS];
  memset(attrs, attr, T_COLS);
  VideoMsgSendPutAttribsAtCell(0, (char*)attrs, T_COLS);
  memset(status_prev, 0, T_COLS);
}

// Refreshes the status bar on its own slow interval.
static void status_tick(void) {
  if (!cfg_statusbar) {
    return;
  }
  uint32_t now = millis();
  if (now - status_timer >= STATUS_UPDATE_MS) {
    status_timer = now;
    status_update();
  }
}

// Linear cell index of the terminal cursor.
static inline uint16_t cursor_cell(void) { return (uint16_t)cy * T_COLS + cx; }

// Makes the cursor visible by inverting its cell.
static void cursor_show(void) {
  if (!cursor_on) {
    uint8_t attr = cfg_screen ? 0x00 : 0xFF;
    VideoMsgSendPutAttribsAtCell(cursor_cell(), (char*)&attr, 1);
    cursor_on = true;
  }
}

// Restores the cell under the cursor. Called before any drawing so the cursor
// is not left painted into the text.
static void cursor_hide(void) {
  if (cursor_on) {
    uint8_t attr =
        sgr_reverse ? (cfg_screen ? 0x00 : 0xFF) : (cfg_screen ? 0xFF : 0x00);
    VideoMsgSendPutAttribsAtCell(cursor_cell(), (char*)&attr, 1);
    cursor_on = false;
  }
}

// Blinks the cursor on its own interval.
static void cursor_tick(void) {
  uint32_t now = millis();
  if (now - cursor_timer >= CURSOR_BLINK_MS) {
    cursor_timer = now;
    if (cursor_on) {
      cursor_hide();
    } else {
      cursor_show();
    }
  }
}

// Clears the text area and homes the cursor, preserving the status bar.
static void clear_screen(void) {
  Clrscr(' ');
  cursor_on = false;
  if (cfg_statusbar) {
    status_draw();
  }
}

// Scrolls the region between the scroll margins up one line.
static void scroll_up(void) {
  VideoMsgSendMoveBlock(0, scroll_top + 1, 0, scroll_top, T_COLS,
                        scroll_bot - scroll_top);
  FillCells(0, scroll_bot, ' ', T_COLS);
}

// Scrolls the scrolling region down one line.
static void scroll_down(void) {
  VideoMsgSendMoveBlock(0, scroll_top, 0, scroll_top + 1, T_COLS,
                        scroll_bot - scroll_top);
  FillCells(0, scroll_top, ' ', T_COLS);
}

// Moves down one line, scrolling at the bottom margin.
static void linefeed(void) {
  if (cy == scroll_bot) {
    scroll_up();
  } else if (cy < T_ROWS - 1) {
    cy++;
  }
}

// Erases from the cursor to the end of the screen.
static void erase_eos(void) {
  if (cx < T_COLS) {
    FillCells(cx, cy, ' ', T_COLS - cx);
  }
  for (uint8_t row = cy + 1; row < T_ROWS; row++) {
    FillCells(0, row, ' ', T_COLS);
  }
}

// Erases from the cursor to the end of the line.
static void erase_eol(void) {
  if (cx < T_COLS) {
    FillCells(cx, cy, ' ', T_COLS - cx);
  }
}

// Handles BEL, either as a tone or a visual flash depending on configuration.
static void do_bell(void) {
  if (cfg_bell == 0 || cfg_bell == 2) {
    AudioMsgSendToneOn(0, 1047, 75);
  }
  if (cfg_bell == 1 || cfg_bell == 2) {
    SetReverseScreen(!cfg_screen);
    uint32_t t = millis();
    while (millis() - t < 80) {
    }
    SetReverseScreen(cfg_screen);
    cursor_on = false;
    if (cfg_statusbar) {
      status_draw();
    }
  }
}

// Accumulates and executes an ANSI CSI sequence: cursor movement, erasing,
// scroll margins and attributes. Parameters arrive a byte at a time, so this is
// driven as a state machine.
static void handle_csi(uint8_t ch) {
  es = 0;
  uint8_t p0 = csi_p[0];
  uint8_t p1 = csi_p[1];
  uint8_t n = p0 ? p0 : 1;
  switch (ch) {
    case 'A':
      cursor_hide();
      {
        uint8_t i = n;
        while (i-- && cy > top_row) {
          cy--;
        }
      }
      break;
    case 'B':
      cursor_hide();
      {
        uint8_t i = n;
        while (i-- && cy < T_ROWS - 1) {
          cy++;
        }
      }
      break;
    case 'C':
      cursor_hide();
      {
        uint8_t i = n;
        while (i-- && cx < T_COLS - 1) {
          cx++;
        }
      }
      break;
    case 'D':
      cursor_hide();
      {
        uint8_t i = n;
        while (i-- && cx > 0) {
          cx--;
        }
      }
      break;
    case 'H':
    case 'f':
      cursor_hide();
      cy = p0 ? p0 - 1 + top_row : top_row;
      cx = (csi_np >= 1 && p1) ? p1 - 1 : 0;
      if (cy >= T_ROWS) {
        cy = T_ROWS - 1;
      }
      if (cx >= T_COLS) {
        cx = T_COLS - 1;
      }
      break;
    case 'J':
      cursor_hide();
      if (p0 == 2) {
        clear_screen();
        cx = 0;
        cy = top_row;
      } else {
        erase_eos();
      }
      break;
    case 'K':
      cursor_hide();
      erase_eol();
      break;
    case 's':
      saved_cx = cx;
      saved_cy = cy;
      break;
    case 'u':
      cursor_hide();
      cx = saved_cx;
      cy = saved_cy;
      if (cx >= T_COLS) {
        cx = T_COLS - 1;
      }
      if (cy >= T_ROWS) {
        cy = T_ROWS - 1;
      }
      break;
    case 'n':
      break;
    case 'r': {
      uint8_t st = p0 ? p0 - 1 + top_row : top_row;
      uint8_t sb = (csi_np >= 1 && p1) ? p1 - 1 + top_row : T_ROWS - 1;
      if (st < T_ROWS && sb < T_ROWS && st < sb) {
        scroll_top = st;
        scroll_bot = sb;
      } else {
        scroll_top = top_row;
        scroll_bot = T_ROWS - 1;
      }
      cursor_hide();
      cx = 0;
      cy = top_row;
      break;
    }
    case 'L': {
      cursor_hide();
      if (cy >= scroll_top && cy <= scroll_bot) {
        uint8_t avail = scroll_bot - cy + 1;
        uint8_t ins = (n < avail) ? n : avail;
        uint8_t to_move = avail - ins;
        if (to_move > 0) {
          VideoMsgSendMoveBlock(0, cy, 0, cy + ins, T_COLS, to_move);
        }
        for (uint8_t i = 0; i < ins; i++) {
          FillCells(0, cy + i, ' ', T_COLS);
        }
      }
      break;
    }
    case 'M': {
      cursor_hide();
      if (cy >= scroll_top && cy <= scroll_bot) {
        uint8_t avail = scroll_bot - cy + 1;
        uint8_t del = (n < avail) ? n : avail;
        uint8_t to_move = avail - del;
        if (to_move > 0) {
          VideoMsgSendMoveBlock(0, cy + del, 0, cy, T_COLS, to_move);
        }
        for (uint8_t i = 0; i < del; i++) {
          FillCells(0, scroll_bot - i, ' ', T_COLS);
        }
      }
      break;
    }
    case '@': {
      cursor_hide();
      if (cx < T_COLS) {
        uint8_t room = T_COLS - cx;
        uint8_t ins = (n < room) ? n : room;
        uint8_t to_move = room - ins;
        if (to_move > 0) {
          VideoMsgSendMoveBlock(cx, cy, cx + ins, cy, to_move, 1);
        }
        FillCells(cx, cy, ' ', ins);
      }
      break;
    }
    case 'P': {
      cursor_hide();
      if (cx < T_COLS) {
        uint8_t room = T_COLS - cx;
        uint8_t del = (n < room) ? n : room;
        uint8_t to_move = room - del;
        if (to_move > 0) {
          VideoMsgSendMoveBlock(cx + del, cy, cx, cy, to_move, 1);
        }
        FillCells(T_COLS - del, cy, ' ', del);
      }
      break;
    }
    case 'm':
      for (uint8_t i = 0; i <= csi_np; i++) {
        if (csi_p[i] == 0) {
          sgr_reverse = false;
        } else if (csi_p[i] == 7) {
          sgr_reverse = true;
        }
      }
      break;
    case 'h':
      if (csi_priv && p0 == 1) {
        app_cursor = true;
      }
      break;
    case 'l':
      if (csi_priv && p0 == 1) {
        app_cursor = false;
      }
      break;
    default:
      break;
  }
}

// Handles VT52 escape sequences and hands ANSI ones off to the CSI parser.
static void handle_esc(uint8_t ch) {
  cursor_hide();
  es = 0;
  switch (ch) {
    case 'A':
      if (cy > top_row) {
        cy--;
      }
      break;
    case 'B':
      if (cy < T_ROWS - 1) {
        cy++;
      }
      break;
    case 'C':
      if (cx < T_COLS - 1) {
        cx++;
      }
      break;
    case 'D':
      if (cx > 0) {
        cx--;
      }
      break;
    case 'E':
      clear_screen();
      cx = 0;
      cy = top_row;
      break;
    case 'F':
      gfx_mode = true;
      break;
    case 'G':
      gfx_mode = false;
      break;
    case 'H':
      cx = 0;
      cy = top_row;
      break;
    case '7':
      saved_cx = cx;
      saved_cy = cy;
      break;
    case '8':
      cx = saved_cx;
      cy = saved_cy;
      if (cx >= T_COLS) {
        cx = T_COLS - 1;
      }
      if (cy >= T_ROWS) {
        cy = T_ROWS - 1;
      }
      break;
    case 'I':
      if (cy > top_row) {
        cy--;
      } else {
        scroll_down();
      }
      break;
    case 'J':
      erase_eos();
      break;
    case 'K':
      erase_eol();
      break;
    case 'Y':
      es = 2;
      break;
    case 'Z':
      Serial1.write((uint8_t)27);
      Serial1.write('/');
      Serial1.write('K');
      break;
    case '[':
      csi_p[0] = csi_p[1] = csi_p[2] = csi_p[3] = 0;
      csi_np = 0;
      csi_priv = false;
      es = 4;
      break;
    default:
      break;
  }
}

static uint8_t txbuf[T_COLS];
static uint8_t txlen;
static uint8_t txcol, txrow;

// Writes the pending run of characters and their attributes in one pair of
// video messages. Batching runs rather than sending per character is what keeps
// the terminal fast enough to follow a host at full speed.
static void flush_txbuf(void) {
  if (txlen == 0) {
    return;
  }
  uint16_t cell = (uint16_t)txrow * T_COLS + txcol;
  PutStringAt(cell, (char*)txbuf, txlen);
  {
    uint8_t attr = (sgr_reverse != cfg_screen) ? 0xFF : 0x00;
    uint8_t attrs[T_COLS];
    memset(attrs, attr, txlen);
    VideoMsgSendPutAttribsAtCell(cell, (char*)attrs, txlen);
  }
  txlen = 0;
  rx_drain();
}

// Processes one received byte: control codes act immediately, printable
// characters accumulate into the pending run, and escapes enter the parser.
static void handle_char(uint8_t ch) {
  switch (ch) {
    case 27:
      flush_txbuf();
      es = 1;
      break;
    case 7:
      flush_txbuf();
      do_bell();
      break;
    case 13:
      flush_txbuf();
      cx = 0;
      break;
    case 10:
      flush_txbuf();
      linefeed();
      break;
    case 11:
      flush_txbuf();
      linefeed();
      break;
    case 12:
      flush_txbuf();
      clear_screen();
      cx = 0;
      cy = top_row;
      break;
    case 8:
      flush_txbuf();
      if (cx > 0) {
        cx--;
        PlotChar(cx, cy, ' ');
      }
      break;
    case 9:
      flush_txbuf();
      cx = (cx & ~7u) + 8;
      if (cx >= T_COLS) {
        cx = T_COLS - 1;
      }
      break;
    case 127:
      flush_txbuf();
      if (cx > 0) {
        cx--;
        PlotChar(cx, cy, ' ');
      }
      break;
    default:
      if (ch < 32) {
        break;
      }
      if (gfx_mode && ch >= 0x5F && ch <= 0x7E) {
        ch = vt52_gfx_to_cp437[ch - 0x5F];
      }
      if (txlen == 0) {
        txcol = cx;
        txrow = cy;
      }
      txbuf[txlen++] = ch;
      cx++;
      if (cx >= T_COLS) {
        flush_txbuf();
        cx = 0;
        linefeed();
      }
      break;
  }
}

// Sends a byte to the host, echoing it locally when local echo is enabled.
static void send_byte(uint8_t b) {
  Serial1.write(b);
  if (cfg_echo) {
    cursor_hide();
    handle_char(b);
    flush_txbuf();
  }
}

// Translates a keypress into what the host expects, emitting arrow keys as the
// escape sequences the current cursor mode calls for.
static void send_key(uint8_t k) {
  switch (k) {
    case UP_KEY:
      Serial1.write((uint8_t)27);
      Serial1.write('A');
      break;
    case DOWN_KEY:
      Serial1.write((uint8_t)27);
      Serial1.write('B');
      break;
    case RIGHT_KEY:
      Serial1.write((uint8_t)27);
      Serial1.write('C');
      break;
    case LEFT_KEY:
      Serial1.write((uint8_t)27);
      Serial1.write('D');
      break;
    case CTRL_C_INTERNAL:
      send_byte(0x03);
      break;
    case STOP_KEY:
      send_byte(0x18);
      break;
    case CTRL_L_KEY:
      send_byte(0x0C);
      break;
    case RETURN_KEY:
      if (cfg_return == 0) {
        send_byte('\r');
      } else if (cfg_return == 1) {
        send_byte('\n');
      } else {
        send_byte('\r');
        send_byte('\n');
      }
      break;
    default:
      send_byte(k);
      break;
  }
}

#define MENU_X 4
#define MENU_Y 5
#define MENU_W 32
#define MENU_H 14
#define MENU_INNER 30

static uint8_t menu_bg[MENU_W * MENU_H];

// Saves the screen area the configuration menu will cover.
static void menu_save(void) {
  for (int r = 0; r < MENU_H; r++) {
    for (int c = 0; c < MENU_W; c++) {
      menu_bg[r * MENU_W + c] = GetCharAt(MENU_X + c, MENU_Y + r);
    }
  }
}

// Puts back the screen contents the menu covered, so leaving it does not
// disturb the session.
static void menu_restore(void) {
  uint8_t attrs[MENU_W];
  uint8_t bg_attr = cfg_screen ? 0xFF : 0x00;
  memset(attrs, bg_attr, MENU_W);
  for (int r = 0; r < MENU_H; r++) {
    uint16_t cell = (uint16_t)(MENU_Y + r) * T_COLS + MENU_X;
    PutStringAt(cell, (char*)&menu_bg[r * MENU_W], MENU_W);
    VideoMsgSendPutAttribsAtCell(cell, (char*)attrs, MENU_W);
  }
}

// Draws the menu's border.
static void menu_draw_box(void) {
  FillCells(MENU_X + 1, MENU_Y, BOX_H, MENU_INNER);
  FillCells(MENU_X + 1, MENU_Y + 2, BOX_H, MENU_INNER);
  FillCells(MENU_X + 1, MENU_Y + MENU_H - 1, BOX_H, MENU_INNER);
  for (uint8_t r = 1; r < MENU_H - 1; r++) {
    PlotChar(MENU_X, MENU_Y + r, BOX_V);
    PlotChar(MENU_X + MENU_W - 1, MENU_Y + r, BOX_V);
  }
  PlotChar(MENU_X, MENU_Y, BOX_TL);
  PlotChar(MENU_X + MENU_W - 1, MENU_Y, BOX_TR);
  PlotChar(MENU_X, MENU_Y + MENU_H - 1, BOX_BL);
  PlotChar(MENU_X + MENU_W - 1, MENU_Y + MENU_H - 1, BOX_BR);
  PlotChar(MENU_X, MENU_Y + 2, BOX_LT);
  PlotChar(MENU_X + MENU_W - 1, MENU_Y + 2, BOX_RT);
}

// Writes one menu row, highlighted when selected.
static void menu_put_row(uint8_t row, const char* content, bool highlight) {
  uint16_t cell = (uint16_t)(MENU_Y + row) * T_COLS + MENU_X + 1;
  VideoMsgSendPutStringAtCell(cell, (char*)content, MENU_INNER);
  uint8_t content_attr = (highlight != cfg_screen) ? 0xFF : 0x00;
  uint8_t attrs[MENU_INNER];
  memset(attrs, content_attr, MENU_INNER);
  VideoMsgSendPutAttribsAtCell(cell, (char*)attrs, MENU_INNER);
}

// Screen row for a menu item index.
static inline uint8_t item_row(uint8_t item) {
  if (item < 6) {
    return 3 + item;
  }
  return 10 + (item - 6);
}

// Builds a menu line: the setting's label and its current value.
static void menu_item_content(uint8_t item, char* buf30) {
  static const char* const ret_str[] = {"[CR]", "[LF]", "[CR+LF]"};
  static const char* const bell_str[] = {"[AUDIBLE]", "[VISUAL]", "[BOTH]"};
  switch (item) {
    case 0:
      snprintf(buf30, MENU_INNER + 1, "  %-12s %-15s", "ECHO",
               cfg_echo ? "[ON]" : "[OFF]");
      break;
    case 1:
      snprintf(buf30, MENU_INNER + 1, "  %-12s %-15s", "RETURN",
               ret_str[cfg_return]);
      break;
    case 2:
      snprintf(buf30, MENU_INNER + 1, "  %-12s %-15s", "BELL",
               bell_str[cfg_bell]);
      break;
    case 3:
      snprintf(buf30, MENU_INNER + 1, "  %-12s %-15s", "SCREEN",
               cfg_screen ? "[REVERSE]" : "[NORMAL]");
      break;
    case 4:
      snprintf(buf30, MENU_INNER + 1, "  %-12s %-15s", "STATUS BAR",
               cfg_statusbar ? "[ON]" : "[OFF]");
      break;
    case 5:
      snprintf(buf30, MENU_INNER + 1, "  %-12s %-15s", "WIFI",
               reset_wifi ? "[NORMAL]" : "[RESET]");
      break;
    case 6:
      snprintf(buf30, MENU_INNER + 1, "  %-28s", "DISCONNECT SERVER");
      break;
    case 7:
      snprintf(buf30, MENU_INNER + 1, "  %-28s", "OK  (Ctrl-Z)");
      break;
    case 8:
      snprintf(buf30, MENU_INNER + 1, "  %-28s", "EXIT TO BASIC");
      break;
  }
}

// Redraws a single menu item, used when the selection or a value changes.
static void menu_draw_item(uint8_t item, bool highlight) {
  char buf30[MENU_INNER + 1];
  menu_item_content(item, buf30);
  menu_put_row(item_row(item), buf30, highlight);
}

// Draws the whole menu with one item selected.
static void menu_draw_full(uint8_t sel) {
  char spaces[MENU_INNER + 1];
  memset(spaces, ' ', MENU_INNER);
  spaces[MENU_INNER] = '\0';

  menu_draw_box();

  menu_put_row(1, "    VT52 TERMINAL SETTINGS    ", !cfg_screen);

  for (uint8_t i = 0; i < 6; i++) {
    menu_draw_item(i, sel == i);
  }

  menu_put_row(9, spaces, false);

  menu_draw_item(6, sel == 6);
  menu_draw_item(7, sel == 7);
  menu_draw_item(8, sel == 8);
}

// Applies a change to the status-bar setting, reflowing the text area since the
// usable height changes with it.
static void apply_statusbar(void) {
  top_row = content_top();
  scroll_top = top_row;
  scroll_bot = T_ROWS - 1;
  if (cfg_statusbar) {
    status_draw();
    status_timer = millis();
  } else {
    status_clear();
  }
  if (cy < top_row) {
    cy = top_row;
  }
}

// Runs the Ctrl-Z configuration menu until dismissed. Returns whether the user
// chose to leave the terminal.
static bool run_menu(void) {
  cursor_hide();
  menu_save();
  uint8_t sel = 0;
  menu_draw_full(sel);

  for (;;) {
    uint8_t k = BufferGet();
    if (!k) {
      continue;
    }

    uint8_t old_sel = sel;
    bool val_changed = false;

    switch (k) {
      case ',':
        if (sel > 0) {
          sel--;
        }
        break;
      case '.':
        if (sel < 8) {
          sel++;
        }
        break;
      case LEFT_KEY:
        if (sel == 0) {
          cfg_echo = !cfg_echo;
          val_changed = true;
        } else if (sel == 1) {
          cfg_return = (cfg_return + 2) % 3;
          val_changed = true;
        } else if (sel == 2) {
          cfg_bell = (cfg_bell + 2) % 3;
          val_changed = true;
        } else if (sel == 3) {
          cfg_screen = !cfg_screen;
          SetReverseScreen(cfg_screen);
          status_draw();
          menu_draw_full(sel);
        } else if (sel == 4) {
          cfg_statusbar = !cfg_statusbar;
          val_changed = true;
        } else if (sel == 5) {
          reset_wifi = !reset_wifi;
          digitalWrite(8, reset_wifi ? HIGH : LOW);
          val_changed = true;
        }
        break;
      case RIGHT_KEY:
        if (sel == 0) {
          cfg_echo = !cfg_echo;
          val_changed = true;
        } else if (sel == 1) {
          cfg_return = (cfg_return + 1) % 3;
          val_changed = true;
        } else if (sel == 2) {
          cfg_bell = (cfg_bell + 1) % 3;
          val_changed = true;
        } else if (sel == 3) {
          cfg_screen = !cfg_screen;
          SetReverseScreen(cfg_screen);
          status_draw();
          menu_draw_full(sel);
        } else if (sel == 4) {
          cfg_statusbar = !cfg_statusbar;
          val_changed = true;
        } else if (sel == 5) {
          reset_wifi = !reset_wifi;
          digitalWrite(8, reset_wifi ? HIGH : LOW);
          val_changed = true;
        }
        break;
      case RETURN_KEY:
        if (sel == 6) {
          ServerDisconnect();
          if (cfg_statusbar) {
            status_draw();
          }
          break;
        }
        if (sel == 7) {
          menu_restore();
          apply_statusbar();
          return false;
        }
        if (sel == 8) {
          menu_restore();
          apply_statusbar();
          return true;
        }
        if (sel == 0) {
          cfg_echo = !cfg_echo;
          val_changed = true;
        } else if (sel == 1) {
          cfg_return = (cfg_return + 1) % 3;
          val_changed = true;
        } else if (sel == 2) {
          cfg_bell = (cfg_bell + 1) % 3;
          val_changed = true;
        } else if (sel == 3) {
          cfg_screen = !cfg_screen;
          SetReverseScreen(cfg_screen);
          status_draw();
          menu_draw_full(sel);
        } else if (sel == 4) {
          cfg_statusbar = !cfg_statusbar;
          val_changed = true;
        } else if (sel == 5) {
          reset_wifi = !reset_wifi;
          digitalWrite(8, reset_wifi ? HIGH : LOW);
          val_changed = true;
        }
        break;
      case CTRL_Z:
      case 27:
      case STOP_KEY:
        menu_restore();
        apply_statusbar();
        return false;
      default:
        break;
    }

    if (sel != old_sel) {
      menu_draw_item(old_sel, false);
      menu_draw_item(sel, true);
    } else if (val_changed) {
      menu_draw_item(sel, true);
    }
  }
}

// TERM: the VT52/ANSI terminal. Owns the machine until the user exits, looping
// over received bytes and keystrokes while keeping the cursor and status bar
// ticking.
void RunTerminal(void) {
  {
    const uint8_t CHUNK = 28;
    for (uint8_t off = 0; off < CP437_COUNT; off += CHUNK) {
      uint8_t n =
          (uint8_t)((CP437_COUNT - off) < CHUNK ? (CP437_COUNT - off) : CHUNK);
      VideoMsgSendDefineCharBulk((uint8_t)(CP437_BASE + off),
                                 cp437_bitmaps + (size_t)off * 8, n);
    }
  }
  VideoMsgSendDefineCharBulk(DOT_CHAR, logo_bitmap, 1);

  top_row = content_top();
  clear_screen();
  if (cfg_screen) {
    SetReverseScreen(true);
  }
  if (cfg_statusbar) {
    status_draw();
    status_timer = millis();
  }
  cx = 0;
  cy = top_row;
  es = 0;
  yr = 0;
  gfx_mode = false;
  txlen = 0;
  csi_np = 0;
  csi_priv = false;
  saved_cx = 0;
  saved_cy = top_row;
  scroll_top = top_row;
  scroll_bot = T_ROWS - 1;
  app_cursor = false;
  sgr_reverse = false;
  rx_head = rx_tail = 0;
  cursor_on = false;
  cursor_timer = millis();
  BufferClear();

  delay(500);
  while (Serial1.available()) {
    Serial1.read();
  }
  rx_head = rx_tail = 0;

  for (;;) {
    uint8_t k = BufferGet();
    if (k) {
      if (k == CTRL_Z) {
        if (run_menu()) {
          break;
        }
        cursor_timer = millis();
      } else {
        send_key(k);
      }
    }

    rx_drain();
    if (rx_count()) {
      cursor_hide();
      txlen = 0;
      while (rx_count()) {
        uint8_t ch = rx_get();
        switch (es) {
          case 0:
            handle_char(ch);
            break;
          case 1:
            flush_txbuf();
            handle_esc(ch);
            break;
          case 2:
            flush_txbuf();
            yr = ch - 32;
            es = 3;
            break;
          case 3:
            flush_txbuf();
            cx = ch - 32;
            cy = yr + top_row;
            es = 0;
            if (cx >= T_COLS) {
              cx = T_COLS - 1;
            }
            if (cy >= T_ROWS) {
              cy = T_ROWS - 1;
            }
            break;
          case 4:
            if (ch == '?') {
              csi_priv = true;
            } else if (ch >= '0' && ch <= '9') {
              if (csi_np < 4) {
                csi_p[csi_np] = csi_p[csi_np] * 10 + (ch - '0');
              }
            } else if (ch == ';') {
              if (csi_np < 3) {
                csi_np++;
              }
            } else {
              flush_txbuf();
              handle_csi(ch);
            }
            break;
        }
        rx_drain();
      }
      flush_txbuf();
    }

    cursor_tick();
    status_tick();
  }

  Serial1.write((uint8_t)0x03);

  if (cfg_screen) {
    SetReverseScreen(false);
  }
  for (uint16_t c = CP437_BASE; c < CP437_BASE + CP437_COUNT; c++) {
    VideoMsgSendResetChar((uint8_t)c);
  }
  VideoMsgSendResetChar(DOT_CHAR);
  cursor_hide();
  Clrscr(' ');
}
