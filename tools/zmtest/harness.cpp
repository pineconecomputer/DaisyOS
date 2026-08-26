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
 * Native test harness for the Z-machine module.
 *
 * Builds the same interpreter sources the firmware does and runs them against
 * a virtual 40x25 VT52 screen, so the port can be exercised without a Due.
 * The virtual screen implements the same escape sequences the DaisyOS
 * terminal does and nothing more, which means a sequence this harness cannot
 * render is one the real terminal would not have rendered either.
 *
 *   ./zmtest STORY.DAT              interactive, reads stdin
 *   ./zmtest STORY.DAT script.txt   feeds a command per line, dumps the screen
 *
 * Exit status is non-zero if the interpreter stopped on a fatal error.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "zmachine/zm_port.h"
#include "zmachine/zm_internal.h"
#include "zmachine/zm_run.h"
#include "zmachine/zm_story.h"

#define SCR_ROWS 25
#define SCR_COLS 40

/* ------------------------------------------------------------------ */
/* Virtual VT52 screen                                                 */
/* ------------------------------------------------------------------ */

static char scr[SCR_ROWS][SCR_COLS];
static unsigned char scr_rev[SCR_ROWS][SCR_COLS];
static int cx, cy;            /* 0-based */
static int reverse_on;
static int scroll_top, scroll_bot;
static int esc_state;
static int esc_y_row;
static int csi_p[4], csi_np;

/* Every line that scrolls off the top is kept so a test can look at output
 * the screen no longer holds. */
static char (*scrollback)[SCR_COLS + 1];
static int scrollback_len, scrollback_cap;

static int unknown_sequences;  /* sequences the DaisyOS terminal would drop */

static void scr_init(void) {
  memset(scr, ' ', sizeof(scr));
  memset(scr_rev, 0, sizeof(scr_rev));
  cx = cy = 0;
  reverse_on = 0;
  scroll_top = 0;
  scroll_bot = SCR_ROWS - 1;
  esc_state = 0;
  csi_np = 0;
  unknown_sequences = 0;
  scrollback_len = 0;
}

static void scrollback_push(const char* row) {
  if (scrollback_len == scrollback_cap) {
    scrollback_cap = scrollback_cap ? scrollback_cap * 2 : 256;
    scrollback = (char(*)[SCR_COLS + 1])realloc(scrollback,
                                                (size_t)scrollback_cap * (SCR_COLS + 1));
  }
  memcpy(scrollback[scrollback_len], row, SCR_COLS);
  scrollback[scrollback_len][SCR_COLS] = '\0';
  scrollback_len++;
}

static void scroll_up(void) {
  int r, c;
  scrollback_push(scr[scroll_top]);
  for (r = scroll_top; r < scroll_bot; r++) {
    memcpy(scr[r], scr[r + 1], SCR_COLS);
    memcpy(scr_rev[r], scr_rev[r + 1], SCR_COLS);
  }
  for (c = 0; c < SCR_COLS; c++) {
    scr[scroll_bot][c] = ' ';
    scr_rev[scroll_bot][c] = 0;
  }
}

static void linefeed(void) {
  if (cy == scroll_bot) {
    scroll_up();
  } else if (cy < SCR_ROWS - 1) {
    cy++;
  }
}

static void erase_eol(void) {
  int c;
  for (c = cx; c < SCR_COLS; c++) {
    scr[cy][c] = ' ';
    scr_rev[cy][c] = 0;
  }
}

static void erase_eos(void) {
  int r, c;
  erase_eol();
  for (r = cy + 1; r < SCR_ROWS; r++) {
    for (c = 0; c < SCR_COLS; c++) {
      scr[r][c] = ' ';
      scr_rev[r][c] = 0;
    }
  }
}

static void clear_all(void) {
  int r, c;
  for (r = 0; r < SCR_ROWS; r++) {
    for (c = 0; c < SCR_COLS; c++) {
      scr[r][c] = ' ';
      scr_rev[r][c] = 0;
    }
  }
}

static void put_printable(unsigned char ch) {
  scr[cy][cx] = (char)ch;
  scr_rev[cy][cx] = (unsigned char)reverse_on;
  if (cx < SCR_COLS - 1) {
    cx++;
  } else {
    cx = 0;
    linefeed();
  }
}

static void handle_csi(unsigned char ch) {
  int p0 = csi_p[0], p1 = csi_p[1];
  esc_state = 0;
  switch (ch) {
    case 'H':
    case 'f':
      cy = p0 ? p0 - 1 : 0;
      cx = (csi_np >= 1 && p1) ? p1 - 1 : 0;
      if (cy >= SCR_ROWS) cy = SCR_ROWS - 1;
      if (cx >= SCR_COLS) cx = SCR_COLS - 1;
      break;
    case 'J':
      if (p0 == 2) {
        clear_all();
        cx = cy = 0;
      } else {
        erase_eos();
      }
      break;
    case 'K':
      erase_eol();
      break;
    case 'r':
      scroll_top = p0 ? p0 - 1 : 0;
      scroll_bot = (csi_np >= 1 && p1) ? p1 - 1 : SCR_ROWS - 1;
      if (scroll_top >= SCR_ROWS) scroll_top = SCR_ROWS - 1;
      if (scroll_bot >= SCR_ROWS) scroll_bot = SCR_ROWS - 1;
      if (scroll_bot < scroll_top) scroll_bot = scroll_top;
      break;
    case 'm':
      if (p0 == 0) {
        reverse_on = 0;
      } else if (p0 == 7) {
        reverse_on = 1;
      }
      break;
    case 'A': if (cy > 0) cy--; break;
    case 'B': if (cy < SCR_ROWS - 1) cy++; break;
    case 'C': if (cx < SCR_COLS - 1) cx++; break;
    case 'D': if (cx > 0) cx--; break;
    case 's': break;
    case 'u': break;
    default:
      unknown_sequences++;
      break;
  }
}

static void scr_put(unsigned char ch) {
  switch (esc_state) {
    case 1: /* after ESC */
      esc_state = 0;
      switch (ch) {
        case 'Y': esc_state = 2; break;
        case '[':
          csi_p[0] = csi_p[1] = csi_p[2] = csi_p[3] = 0;
          csi_np = 0;
          esc_state = 4;
          break;
        case 'H': cx = 0; cy = 0; break;
        case 'J': erase_eos(); break;
        case 'K': erase_eol(); break;
        case 'E': clear_all(); cx = cy = 0; break;
        case 'A': if (cy > 0) cy--; break;
        case 'B': if (cy < SCR_ROWS - 1) cy++; break;
        case 'C': if (cx < SCR_COLS - 1) cx++; break;
        case 'D': if (cx > 0) cx--; break;
        case 'I': if (cy > 0) cy--; break;
        case '7': break;
        case '8': break;
        default: unknown_sequences++; break;
      }
      return;
    case 2: /* ESC Y row */
      esc_y_row = ch - 32;
      esc_state = 3;
      return;
    case 3: /* ESC Y row col */
      cy = esc_y_row;
      cx = ch - 32;
      if (cy < 0) cy = 0;
      if (cx < 0) cx = 0;
      if (cy >= SCR_ROWS) cy = SCR_ROWS - 1;
      if (cx >= SCR_COLS) cx = SCR_COLS - 1;
      esc_state = 0;
      return;
    case 4: /* CSI parameters */
      if (ch >= '0' && ch <= '9') {
        if (csi_np < 4) csi_p[csi_np] = csi_p[csi_np] * 10 + (ch - '0');
      } else if (ch == ';') {
        if (csi_np < 3) csi_np++;
      } else if (ch == '?') {
        /* private parameter introducer */
      } else {
        handle_csi(ch);
      }
      return;
    default:
      break;
  }

  switch (ch) {
    case 27: esc_state = 1; break;
    case 13: cx = 0; break;
    case 10: linefeed(); break;
    case 7:  break; /* bell */
    case 8:
      if (cx > 0) { cx--; scr[cy][cx] = ' '; }
      break;
    case 9:
      cx = (cx & ~7) + 8;
      if (cx >= SCR_COLS) cx = SCR_COLS - 1;
      break;
    default:
      if (ch >= 32 && ch < 127) put_printable(ch);
      break;
  }
}

static void scr_dump(FILE* f, int with_border) {
  int r, c;
  if (with_border) {
    fputs("    +", f);
    for (c = 0; c < SCR_COLS; c++) fputc('-', f);
    fputs("+\n", f);
  }
  for (r = 0; r < SCR_ROWS; r++) {
    if (with_border) fprintf(f, "%3d |", r);
    for (c = 0; c < SCR_COLS; c++) fputc(scr[r][c], f);
    if (with_border) fputc('|', f);
    fputc('\n', f);
  }
  if (with_border) {
    fputs("    +", f);
    for (c = 0; c < SCR_COLS; c++) fputc('-', f);
    fputs("+\n", f);
  }
}

/* ------------------------------------------------------------------ */
/* Host seam                                                           */
/* ------------------------------------------------------------------ */

static FILE* script_fp = NULL;
static char pending[512];
static size_t pending_pos, pending_len;
static int script_exhausted = 0;
static int echo_output = 1;
static int quit_requested = 0;

/* The screen as it stood when the last command was requested. Taken at the
 * prompt rather than at exit, because by exit the interpreter has already
 * cleared the display. */
static char snap[SCR_ROWS][SCR_COLS];
static unsigned char snap_rev[SCR_ROWS][SCR_COLS];
static int snap_taken = 0;

static void take_snapshot(void) {
  memcpy(snap, scr, sizeof(scr));
  memcpy(snap_rev, scr_rev, sizeof(scr_rev));
  snap_taken = 1;
}

void ZmHostPutByte(uint8_t c) { scr_put(c); }
void ZmHostFlush(void) {}
void ZmHostIdle(void) {}

void ZmHostFatal(const char* message) {
  fprintf(stderr, "\n[FATAL] %s\n", message);
  exit(2);
}

/* Feeds the next character of input. In script mode the file supplies one
 * command per line; once it runs out the harness answers "quit" so the run
 * terminates instead of blocking. */
uint8_t ZmHostGetKey(void) {
  /* Once STOP has been pressed the machine has nothing more to give the
   * story, and every further read returns nothing. Checking this first is what
   * makes <STOP> behave like the real keyboard: the harness must not fall
   * through and rescue the run by typing "quit" for it, because that is what
   * hid the "Beg pardon?" loop in the first place. */
  if (quit_requested) {
    return 0;
  }

  if (pending_pos < pending_len) {
    return (uint8_t)pending[pending_pos++];
  }

  take_snapshot();

  if (script_fp != NULL) {
    char buf[480];
    if (fgets(buf, sizeof(buf), script_fp) != NULL) {
      size_t n = strlen(buf);
      while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = '\0';
      if (strcmp(buf, "<STOP>") == 0) {
        /* Stands in for the STOP key on the machine. */
        quit_requested = 1;
        return 0;
      }
      snprintf(pending, sizeof(pending), "%s\r", buf);
      pending_len = strlen(pending);
      pending_pos = 0;
      return (uint8_t)pending[pending_pos++];
    }
    if (!script_exhausted) {
      script_exhausted = 1;
      snprintf(pending, sizeof(pending), "quit\ry\r");
      pending_len = strlen(pending);
      pending_pos = 0;
      return (uint8_t)pending[pending_pos++];
    }
    quit_requested = 1;
    return 0;
  }

  {
    int ch = fgetc(stdin);
    if (ch == EOF) return 0;
    if (ch == '\n') return 13;
    return (uint8_t)ch;
  }
}

uint8_t ZmHostPollKey(void) { return 0; }

int ZmHostQuitRequested(void) { return quit_requested; }

/* Save games go to a file next to the harness. */
struct ZmSaveStream {
  FILE* fp;
};

struct ZmSaveStream* ZmHostSaveOpen(int slot, int writing) {
  char name[64];
  struct ZmSaveStream* s;
  FILE* fp;

  snprintf(name, sizeof(name), "zmtest-save-%d.dat", slot);
  fp = fopen(name, writing ? "wb" : "rb");
  if (fp == NULL) return NULL;

  s = (struct ZmSaveStream*)malloc(sizeof(*s));
  s->fp = fp;
  return s;
}

int ZmHostSaveRead(struct ZmSaveStream* s, void* buf, unsigned len) {
  return fread(buf, 1, len, s->fp) == len;
}

int ZmHostSaveWrite(struct ZmSaveStream* s, const void* buf, unsigned len) {
  return fwrite(buf, 1, len, s->fp) == len;
}

void ZmHostSaveClose(struct ZmSaveStream* s) {
  fclose(s->fp);
  free(s);
}

unsigned long ZmMillis(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (unsigned long)(tv.tv_sec * 1000UL + tv.tv_usec / 1000UL);
}

/* ------------------------------------------------------------------ */

const ZmStory kZmStories[] = {{NULL, NULL, 0}};
const uint8_t kZmStoryCount = 0;

int main(int argc, char** argv) {
  FILE* f;
  long len;
  uint8_t* image;
  ZmStory story;
  int rc;

  if (argc < 2) {
    fprintf(stderr, "usage: %s STORY.DAT [script.txt]\n", argv[0]);
    return 1;
  }

  f = fopen(argv[1], "rb");
  if (f == NULL) {
    perror(argv[1]);
    return 1;
  }
  fseek(f, 0, SEEK_END);
  len = ftell(f);
  fseek(f, 0, SEEK_SET);
  image = (uint8_t*)malloc((size_t)len);
  if (fread(image, 1, (size_t)len, f) != (size_t)len) {
    fprintf(stderr, "short read on %s\n", argv[1]);
    return 1;
  }
  fclose(f);

  if (argc >= 3) {
    script_fp = fopen(argv[2], "r");
    if (script_fp == NULL) {
      perror(argv[2]);
      return 1;
    }
    echo_output = 0;
  }

  scr_init();

  story.title = argv[1];
  story.data = image;
  story.length = (uint32_t)len;

  rc = ZmRunStory(&story);

  printf("\n=== screen at last prompt (40x25) ===\n");
  if (snap_taken) {
    memcpy(scr, snap, sizeof(scr));
    memcpy(scr_rev, snap_rev, sizeof(scr_rev));
  }
  scr_dump(stdout, 1);
  {
    int r, c, rev = 0;
    for (r = 0; r < SCR_ROWS; r++)
      for (c = 0; c < SCR_COLS; c++)
        if (scr_rev[r][c]) rev++;
    printf("=== %d cells in reverse video ===\n", rev);
  }
  printf("=== %d lines scrolled off, %d unrenderable sequences ===\n",
         scrollback_len, unknown_sequences);

  if (getenv("ZM_SCROLLBACK") != NULL) {
    int i;
    printf("\n=== scrollback ===\n");
    for (i = 0; i < scrollback_len; i++) printf("%s\n", scrollback[i]);
  }

  if (rc != 0) {
    printf("\n*** stopped: %s\n", zm_abort_message);
  }
  (void)echo_output;
  return rc;
}
