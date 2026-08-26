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
 * Starting and stopping a story.
 *
 * Replaces the parts of jzip.c and A2Z's sketch that read the header and run
 * the interpreter. Everything is re-initialised on entry rather than relying
 * on static initialisers, because unlike a command line interpreter this one
 * can be started more than once in a session: quit Zork, start it again, and
 * the second run has to behave like the first.
 */

#include "zmachine/zm_types.h"
#include "zmachine/zm_internal.h"
#include "zmachine/zm_run.h"

/*
 * configure
 *
 * Reads the story header.
 *
 * Upstream does this by pointing datap at a stack buffer holding page 0 so
 * that get_byte/get_word work before dynamic memory exists, then setting it
 * back to NULL. That trick is unnecessary here: zm_dyn_size is still zero at
 * this point, so the accessors already fall through to the flash image. The
 * ordering matters though, and load_cache must not have run yet.
 */
static int configure(zbyte_t min_version, zbyte_t max_version) {
  h_type = get_byte(H_TYPE);
  GLOBALVER = h_type;

  if (h_type < min_version || h_type > max_version) {
    fatal("Unsupported Z-machine version");
    return 1;
  }
  if (get_byte(H_CONFIG) & CONFIG_BYTE_SWAPPED) {
    fatal("Byte swapped story files are not supported");
    return 1;
  }

  if (h_type < V4) {
    story_scaler = 2;
    story_shift = 1;
    property_mask = P3_MAX_PROPERTIES - 1;
    property_size_mask = 0xe0;
  } else if (h_type < V8) {
    story_scaler = 4;
    story_shift = 2;
    property_mask = P4_MAX_PROPERTIES - 1;
    property_size_mask = 0x3f;
  } else {
    story_scaler = 8;
    story_shift = 3;
    property_mask = P4_MAX_PROPERTIES - 1;
    property_size_mask = 0x3f;
  }

  h_config = get_byte(H_CONFIG);
  h_version = get_word(H_VERSION);
  h_data_size = get_word(H_DATA_SIZE);
  h_start_pc = get_word(H_START_PC);
  h_words_offset = get_word(H_WORDS_OFFSET);
  h_objects_offset = get_word(H_OBJECTS_OFFSET);
  h_globals_offset = get_word(H_GLOBALS_OFFSET);
  h_restart_size = get_word(H_RESTART_SIZE);
  h_flags = get_word(H_FLAGS);
  h_synonyms_offset = get_word(H_SYNONYMS_OFFSET);
  h_file_size = get_word(H_FILE_SIZE);
  if (h_file_size == 0) {
    h_file_size = get_story_size();
  }
  h_checksum = get_word(H_CHECKSUM);
  h_alternate_alphabet_offset = get_word(H_ALTERNATE_ALPHABET_OFFSET);

  if (h_type >= V5) {
    h_unicode_table = get_word(H_UNICODE_TABLE);
  }

  return 0;
}

/*
 * reset_globals
 *
 * jzip keeps the machine's state in file-scope variables initialised at load
 * time, which is fine for a process that runs one game and exits. This module
 * can be entered repeatedly, so everything that would otherwise carry over
 * from the previous story is put back to its starting value here.
 */
static void reset_globals(void) {
  sp = STACK_SIZE;
  fp = STACK_SIZE - 1;
  frame_count = 0;
  pc = 0;
  interpreter_state = RUN;
  interpreter_status = 0;

  formatting = ON;
  outputting = ON;
  redirecting = OFF;
  redirect_depth = 0;
  scripting = OFF;
  scripting_disable = OFF;
  recording = OFF;
  replaying = OFF;
  font = TEXT_FONT;

  screen_window = TEXT_WINDOW;
  interp_initialized = 0;
  status_active = 0;
  status_size = 0;
  lines_written = 0;
  status_pos = 0;

  right_margin = DEFAULT_RIGHT_MARGIN;
  top_margin = DEFAULT_TOP_MARGIN;

  h_unicode_table = 0;
  h_alternate_alphabet_offset = 0;

  data_size = 0;
  datap = NULL;
  undo_datap = NULL;
  line = NULL;
  status_line = NULL;

#ifdef STRICTZ
  ZmResetStrictzCounts();
#endif
}

/*
 * ZmRunStory
 *
 * Brings up one story, runs it, and tears everything down again.
 */
int ZmRunStory(const ZmStory* story) {
  int failed = 0;

  if (story == NULL || story->data == NULL || story->length < 64) {
    return 1;
  }

  zm_abort_message[0] = '\0';
  reset_globals();
  ZmStoryMount(story);

  /* Seed from the clock so two runs of the same story differ. */
  SRANDOM_FUNC(ZmMillis());

  /* fatal() from here on unwinds back to this point rather than hanging, so
   * a malformed story returns the user to the prompt. */
  if (setjmp(zm_abort_env) != 0) {
    failed = 1;
  } else {
    zm_abort_armed = 1;

    initialize_screen();

    if (configure(V1, V8) == 0) {
      load_cache();
      /* z_restart is what sets pc to h_start_pc and puts the screen and
       * interpreter into their starting state. Without it the interpreter
       * begins executing from address zero. */
      z_restart();
      interpret();
    }
  }

  zm_abort_armed = 0;

  /* unload_cache calls z_new_line, which needs a working output path, so the
   * screen is only reset afterwards. Guarded because the abort may have come
   * from inside load_cache with buffers half allocated. */
  if (datap != NULL || line != NULL) {
    unload_cache();
  }
  reset_screen();

  return failed;
}
