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
 * Odds and ends the interpreter needs from its host.
 *
 * The notable departure from upstream is fatal(). A2Z's version prints to the
 * serial port and then spins forever blinking an LED, which on Daisy would
 * mean a story bug bricks the machine until it is power cycled. Here it
 * unwinds to the run loop instead, so a bad story drops the user back at the
 * BASIC prompt with a message.
 */

#include "zmachine/zm_types.h"
#include "zmachine/zm_internal.h"
#include "zmachine/zm_run.h"

#include <setjmp.h>

/* Set up by ZMachineRun before the interpreter starts. */
jmp_buf zm_abort_env;
int zm_abort_armed = 0;
char zm_abort_message[80];

/*
 * fatal
 *
 * Unwinds to ZMachineRun. Never returns.
 */
void fatal(const char* s) {
  if (s == NULL) {
    s = "unknown error";
  }
  strncpy(zm_abort_message, s, sizeof(zm_abort_message) - 1);
  zm_abort_message[sizeof(zm_abort_message) - 1] = '\0';

  if (zm_abort_armed) {
    zm_abort_armed = 0;
    longjmp(zm_abort_env, 1);
  }

  /* Before the jump target exists there is nothing to unwind to, so fall back
   * on the host, which stops the module without taking DaisyOS down. */
  ZmHostFatal(zm_abort_message);
  for (;;) {
  }
}

#ifdef STRICTZ

static int strictz_error_count[STRICTZ_NUM_ERRORS];

/*
 * report_strictz_error
 *
 * Reports Z-code errors that are technically fatal but which players would
 * rather play through. Each distinct error is reported once so a story that
 * makes the same mistake every turn does not bury the game text.
 */
void report_strictz_error(int errnum, const char* errstr) {
  if (errnum < 0 || errnum >= STRICTZ_NUM_ERRORS) {
    errnum = 0;
  }

  if (strictz_error_count[errnum] == 0) {
    output_string("[");
    output_string(errstr);
    output_string("]");
    output_new_line();
  }
  strictz_error_count[errnum]++;
}

void ZmResetStrictzCounts(void) {
  int i;
  for (i = 0; i < STRICTZ_NUM_ERRORS; i++) {
    strictz_error_count[i] = 0;
  }
}

#endif /* STRICTZ */

/*
 * codes_to_text
 *
 * Converts a ZSCII code above the ASCII range into replacement text. The
 * DaisyVideo character ROM has no accented glyphs, so returning non-zero here
 * says "no translation available" and the caller drops the character.
 */
int codes_to_text(int c, char* s) {
  (void)c;
  (void)s;
  return 1;
}

/*
 * sound
 *
 * DaisySound is driven over its own UART and is not wired to the Z-machine.
 * Sound effect 1 is the bleep every interpreter is expected to have; the rest
 * are ignored.
 */
void sound(int argc, zword_t* argv) {
  if (argc >= 1 && argv[0] == 1) {
    ZmHostPutByte(7); /* BEL, which the terminal turns into a tone */
  }
}

void set_names(const char* storyname) { (void)storyname; }

int get_file_name(char* file_name, char* default_name, int flag) {
  (void)flag;
  if (default_name != NULL && file_name != NULL) {
    strcpy(file_name, default_name);
  }
  return 0;
}

void file_cleanup(const char* file_name, int flag) {
  (void)file_name;
  (void)flag;
}

void process_arguments(int argc, char* argv[]) {
  (void)argc;
  (void)argv;
}

/*
 * Random numbers.
 *
 * Routed through the shim so the firmware and the test harness produce the
 * same sequence from the same seed, which is what makes a reported bug
 * reproducible off the hardware. A 32-bit xorshift is used rather than the
 * platform rand() because newlib's differs from the host's.
 */
static uint32_t rng_state = 1;

void ZmSeedRandom(unsigned long seed) {
  rng_state = (uint32_t)seed;
  if (rng_state == 0) {
    rng_state = 1;
  }
}

long ZmRandom(void) {
  rng_state ^= rng_state << 13;
  rng_state ^= rng_state >> 17;
  rng_state ^= rng_state << 5;
  return (long)(rng_state & 0x7fffffffUL);
}
