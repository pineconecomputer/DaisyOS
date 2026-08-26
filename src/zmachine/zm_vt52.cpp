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
 * Screen driver.
 *
 * This replaces jzip's curses-based screenio. Every call here turns into VT52
 * escape sequences written a byte at a time to ZmHostPutByte, and nothing
 * else. The interpreter therefore has no idea what it is drawing on: the
 * firmware points ZmHostPutByte at the terminal engine that already renders
 * VT52 into the DaisyVideo shadow RAM, and the test harness points it at a
 * virtual screen it can make assertions about.
 *
 * Sequences used, all of which the DaisyOS terminal implements:
 *
 *   ESC Y r c    direct cursor address, row and column biased by 32
 *   ESC J        erase to end of screen
 *   ESC K        erase to end of line
 *   ESC [ 2 J    clear the whole screen
 *   ESC [ t;b r  set the scrolling region to rows t..b
 *   ESC [ 7 m    reverse video, ESC [ 0 m normal
 *
 * jzip addresses the screen with 1-based row and column; VT52 is 0-based, so
 * every coordinate is converted on the way out.
 *
 * The scrolling region is what keeps the status line still while the story
 * text scrolls under it. It is reset whenever the split changes.
 */

#include "zmachine/zm_types.h"
#include "zmachine/zm_internal.h"

#define ZM_ESC 27

/* Where the cursor is, in jzip's 1-based coordinates. jzip asks for this back
 * through get_cursor_position, and the interpreter also needs it to decide
 * when a word will not fit on the current line, so it is tracked here rather
 * than queried from the terminal. */
static int cur_row = 1;
static int cur_col = 1;
static int saved_row = 1;
static int saved_col = 1;

/* Rows 1..status_rows are the status window; the rest is the text window. */
static int status_rows = 0;
static int in_status_window = 0;
static int cur_attribute = NORMAL;

/*
 * stop_story
 *
 * Called when the user presses STOP. Ending the current read is not enough on
 * its own: input_line would hand the story an empty line, the story's parser
 * would print "Beg pardon?" and ask again, and the next read would return
 * nothing straight away, so the game would sit in that loop for ever. What
 * actually ends a story is interpreter_state, which interpret() tests at the
 * top of every instruction. Setting it here means the interpreter leaves its
 * loop as soon as the current opcode finishes, before the parser ever sees the
 * empty line.
 */
static void stop_story(void) { interpreter_state = STOP; }

static void emit(uint8_t c) { ZmHostPutByte(c); }

static void emit_str(const char* s) {
  while (*s != '\0') {
    emit((uint8_t)*s++);
  }
}

/* Writes a decimal number, used for the CSI parameters. */
static void emit_num(int n) {
  char buf[8];
  int i = 0;

  if (n <= 0) {
    emit('0');
    return;
  }
  while (n > 0 && i < (int)sizeof(buf)) {
    buf[i++] = (char)('0' + (n % 10));
    n /= 10;
  }
  while (i-- > 0) {
    emit((uint8_t)buf[i]);
  }
}

/*
 * set_scroll_region
 *
 * Confines scrolling to the text window so that printing at the bottom of the
 * screen leaves the status line untouched. Rows are 1-based and inclusive.
 */
static void set_scroll_region(int top, int bottom) {
  emit(ZM_ESC);
  emit('[');
  emit_num(top);
  emit(';');
  emit_num(bottom);
  emit('r');
}

/*
 * move_cursor
 *
 * jzip's coordinates are 1-based. VT52 biases both by 32, so row 1 column 1
 * becomes ESC Y space space.
 */
void move_cursor(int row, int col) {
  if (row < 1) {
    row = 1;
  }
  if (col < 1) {
    col = 1;
  }
  if (row > screen_rows) {
    row = screen_rows;
  }
  if (col > screen_cols) {
    col = screen_cols;
  }

  emit(ZM_ESC);
  emit('Y');
  emit((uint8_t)(31 + row));
  emit((uint8_t)(31 + col));

  cur_row = row;
  cur_col = col;
}

void get_cursor_position(int* row, int* col) {
  *row = cur_row;
  *col = cur_col;
}

void save_cursor_position(void) {
  saved_row = cur_row;
  saved_col = cur_col;
}

void restore_cursor_position(void) { move_cursor(saved_row, saved_col); }

/*
 * set_attribute
 *
 * Only NORMAL and REVERSE are meaningful on DaisyVideo, which stores one
 * inverse bit per character cell. Bold, emphasis and fixed-pitch all collapse
 * to normal; the status line is the only thing that actually needs reverse.
 */
void set_attribute(int attribute) {
  if (attribute == NORMAL) {
    emit_str("\033[0m");
    cur_attribute = NORMAL;
    return;
  }
  if (attribute & REVERSE) {
    emit_str("\033[7m");
    cur_attribute = REVERSE;
  }
}

/* DaisyVideo is monochrome, so colour requests are accepted and ignored. A
 * story that sets colours still renders correctly in black and white. */
void set_colours(zword_t foreground, zword_t background) {
  (void)foreground;
  (void)background;
}

void set_font(int font_type) { (void)font_type; }

/*
 * display_char
 *
 * Writes one character and advances the tracked cursor. Wrapping is left to
 * the terminal, which moves to column 1 of the next line and scrolls at the
 * bottom of the scrolling region, so the model here has to do the same.
 */
void display_char(int c) {
  if (c == '\n') {
    emit('\r');
    emit('\n');
    cur_col = 1;
    if (cur_row < screen_rows) {
      cur_row++;
    }
    return;
  }

  /* The Z-machine emits ZSCII, which agrees with ASCII over the printable
   * range. Anything outside it would be an accented character from the
   * extended set; the DaisyVideo character ROM has no glyphs for those, so
   * they are dropped rather than drawn as garbage. */
  if (c < 32 || c > 126) {
    return;
  }

  emit((uint8_t)c);
  if (cur_col < screen_cols) {
    cur_col++;
  } else {
    cur_col = 1;
    if (cur_row < screen_rows) {
      cur_row++;
    }
  }
}

/*
 * scroll_line
 *
 * Moves the text window up one line. Emitting a line feed while the cursor
 * sits on the last row of the scrolling region is what makes the terminal
 * scroll, so the cursor is parked there first.
 */
void scroll_line(void) {
  move_cursor(screen_rows, 1);
  emit('\n');
  cur_col = 1;
}

void clear_screen(void) {
  emit_str("\033[2J");
  move_cursor(1, 1);
}

void clear_line(void) { emit_str("\033K"); }

/*
 * clear_text_window
 *
 * Erases only the region below the status line, leaving the status line as it
 * was. Done by homing to the first text row and erasing to the end of screen.
 */
void clear_text_window(void) {
  int save_r = cur_row;
  int save_c = cur_col;

  move_cursor(status_rows + 1, 1);
  emit_str("\033J");
  move_cursor(save_r, save_c);
}

void clear_status_window(void) {
  int save_r = cur_row;
  int save_c = cur_col;
  int row;

  for (row = 1; row <= status_rows; row++) {
    move_cursor(row, 1);
    emit_str("\033K");
  }
  move_cursor(save_r, save_c);
}

/*
 * create_status_window / delete_status_window
 *
 * status_size is maintained by z_split_window in zm_screen.cpp; these are
 * called after it changes so the scrolling region can be moved to match.
 */
void create_status_window(void) {
  status_rows = status_size;
  if (status_rows < 0) {
    status_rows = 0;
  }
  if (status_rows >= screen_rows) {
    status_rows = screen_rows - 1;
  }
  set_scroll_region(status_rows + 1, screen_rows);
}

void delete_status_window(void) {
  status_rows = 0;
  set_scroll_region(1, screen_rows);
}

/*
 * select_status_window
 *
 * The status window does not scroll and is drawn in reverse video, which is
 * how these games have always distinguished it.
 */
void select_status_window(void) {
  save_cursor_position();
  in_status_window = 1;
  set_attribute(REVERSE);
}

void select_text_window(void) {
  in_status_window = 0;
  set_attribute(NORMAL);
  restore_cursor_position();
}

/*
 * initialize_screen
 *
 * Establishes the geometry the rest of the interpreter works from and puts
 * the terminal into a known state.
 */
void initialize_screen(void) {
  screen_rows = DEFAULT_ROWS;
  screen_cols = DEFAULT_COLS;

  status_rows = 0;
  in_status_window = 0;
  cur_attribute = NORMAL;

  set_attribute(NORMAL);
  set_scroll_region(1, screen_rows);
  clear_screen();
  move_cursor(screen_rows, 1);
}

void reset_screen(void) {
  set_attribute(NORMAL);
  delete_status_window();
  clear_screen();
  ZmHostFlush();
}

void restart_screen(void) {
  status_rows = 0;
  set_attribute(NORMAL);
  set_scroll_region(1, screen_rows);
}

/*
 * input_character
 *
 * Reads a single keypress for READ_CHAR. A timeout of zero means wait
 * indefinitely; jzip passes tenths of a second otherwise and expects -1 when
 * the time runs out.
 */
int input_character(int timeout) {
  ZmHostFlush();

  if (timeout > 0) {
    unsigned long deadline = ZmMillis() + (unsigned long)timeout * 100UL;
    for (;;) {
      uint8_t k = ZmHostPollKey();
      if (k != 0) {
        return (int)k;
      }
      if (ZmHostQuitRequested()) {
        stop_story();
        return 13;
      }
      if ((long)(ZmMillis() - deadline) >= 0) {
        return -1;
      }
      ZmHostIdle();
    }
  }

  {
    uint8_t k = ZmHostGetKey();
    if (k == 0) {
      stop_story();
      return 13;
    }
    return (int)k;
  }
}

/*
 * input_line
 *
 * Reads an edited line. Returns the terminating character, or -1 if a timeout
 * expired, which get_line uses to drive the story's interrupt routine.
 *
 * read_size is in-out: V5 stories can pre-load the buffer with text they have
 * already printed, and editing has to continue from the end of it rather than
 * from an empty line.
 */
int input_line(int buflen, char* buffer, int timeout, int* read_size) {
  int pos = *read_size;
  unsigned long deadline = 0;

  if (pos < 0) {
    pos = 0;
  }
  if (pos > buflen) {
    pos = buflen;
  }

  if (timeout > 0) {
    deadline = ZmMillis() + (unsigned long)timeout * 100UL;
  }

  ZmHostFlush();

  for (;;) {
    uint8_t k;

    if (timeout > 0) {
      k = ZmHostPollKey();
      if (k == 0) {
        if (ZmHostQuitRequested()) {
          stop_story();
          *read_size = 0;
          return 13;
        }
        if ((long)(ZmMillis() - deadline) >= 0) {
          *read_size = pos;
          return -1;
        }
        ZmHostIdle();
        continue;
      }
    } else {
      k = ZmHostGetKey();
      if (k == 0) {
        stop_story();
        *read_size = 0;
        return 13;
      }
    }

    if (k == 13 || k == 10) {
      display_char('\n');
      ZmHostFlush();
      *read_size = pos;
      return 13;
    }

    if (k == 8 || k == 127) {
      if (pos > 0) {
        pos--;
        /* Erase the character on screen as well as in the buffer. */
        if (cur_col > 1) {
          cur_col--;
          move_cursor(cur_row, cur_col);
          emit(' ');
          move_cursor(cur_row, cur_col);
        }
      }
      ZmHostFlush();
      continue;
    }

    /* Printable ASCII only. The keyboard can produce control codes and the
     * cursor keys, none of which the parser has any use for. */
    if (k < 32 || k > 126) {
      continue;
    }

    if (pos >= buflen) {
      continue;
    }

    buffer[pos++] = (char)k;
    display_char((int)k);
    ZmHostFlush();
  }
}

/*
 * fit_line
 *
 * True when another word of the given length still fits on the current line.
 * The right margin keeps a column free so a full line does not trigger the
 * terminal's own wrap, which would scroll at a moment the interpreter is not
 * expecting.
 */
int fit_line(const char* line_buffer, int pos, int max) {
  (void)line_buffer;
  return (pos < (max - right_margin));
}

/*
 * print_status
 *
 * V4 and later draw their own status line by writing into the status window,
 * so this is only reached for stories that ask the interpreter to do it.
 * Returning FALSE tells the caller to fall back on the generic version in
 * zm_screen.cpp, which is the right layout for a 40 column screen.
 */
int print_status(int argc, char* argv[]) {
  (void)argc;
  (void)argv;
  return FALSE;
}

/*
 * inc
 *
 * Waits for a keypress. Used for the [MORE] prompt, where the interpreter has
 * filled the screen and pauses before scrolling further. A timeout in tenths
 * of a second returns -1 if nothing is pressed; echo is suppressed at the
 * [MORE] prompt so the key does not land in the story text.
 */
int inc(uint32_t timeout, bool echo) {
  int c;

  ZmHostFlush();

  if (timeout > 0) {
    unsigned long deadline = ZmMillis() + (unsigned long)timeout * 100UL;
    for (;;) {
      uint8_t k = ZmHostPollKey();
      if (k != 0) {
        c = (int)k;
        break;
      }
      if (ZmHostQuitRequested()) {
        stop_story();
        return 13;
      }
      if ((long)(ZmMillis() - deadline) >= 0) {
        return -1;
      }
      ZmHostIdle();
    }
  } else {
    c = (int)ZmHostGetKey();
    if (c == 0) {
      /* STOP at a [MORE] prompt, which is where a story that is printing a
       * long passage spends most of its time. */
      stop_story();
      return 13;
    }
  }

  if (echo && c >= 32 && c < 127) {
    display_char(c);
    ZmHostFlush();
  }
  return c;
}
