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
 * Story access, save and restore.
 *
 * Replaces jzip's fileio.c. There is no file system underneath the story: it
 * is a const array in flash, so opening it is a matter of pointing at it and
 * reading its header.
 *
 * Saved games use jzip's own pre-Quetzal layout rather than Quetzal itself.
 * Quetzal exists so that a save can move between interpreters, which is not
 * something a save living on this machine can do, and skipping it leaves out
 * 619 lines of IFF chunking and run-length encoding. The layout is:
 *
 *   PC, FP and the version word are pushed onto the Z-machine stack and the
 *   resulting stack pointer is stored in stack[0]; the whole stack array is
 *   written out, followed by the h_restart_size bytes of dynamic memory.
 *
 * Words go out most significant byte first so a save is readable regardless of
 * which machine wrote it.
 */

#include "zmachine/zm_types.h"
#include "zmachine/zm_story.h"
#include "zmachine/zm_internal.h"

/* Undo state. Only allocated for stories that can ask for it, which means V5
 * and later; reserving 2 KB of stack copy plus another copy of dynamic memory
 * for a V3 story would be most of a 96 KB machine's spare RAM. */
static zword_t* undo_stack = NULL;
static int undo_valid = FALSE;

const uint8_t* zm_story_data = NULL;
uint32_t zm_story_length = 0;

void ZmStoryMount(const ZmStory* story) {
  zm_story_data = story->data;
  zm_story_length = story->length;
}

/*
 * open_story
 *
 * Nothing to open. Kept so the startup sequence in zm_run.cpp reads the same
 * way as upstream's.
 */
void open_story(const char* storyname) { (void)storyname; }

void close_story(void) {}

unsigned int get_story_size(void) { return (unsigned int)zm_story_length; }

/*
 * z_verify
 *
 * Sums every byte of the story after the 64 byte header and compares the
 * result with the checksum the header records. Zork I's "verify" command
 * reports the outcome.
 */
void z_verify(void) {
  unsigned long i;
  zword_t sum = 0;
  unsigned long len = (unsigned long)h_file_size * story_scaler;

  if (len == 0 || len > zm_story_length) {
    len = zm_story_length;
  }

  for (i = 0x40; i < len; i++) {
    sum = (zword_t)(sum + zm_story_data[i]);
  }

  conditional_jump(sum == h_checksum);
}

/*
 * ZmAllocUndo
 *
 * Called once dynamic memory is sized, for stories that support undo.
 */
static void alloc_undo_if_needed(void) {
  if (h_type >= V5 && undo_stack == NULL) {
    undo_stack = (zword_t*)malloc(sizeof(zword_t) * STACK_SIZE);
  }
}

/*
 * push_machine_state / pop_machine_state
 *
 * The Z-machine's registers do not live in the stack array, so they are
 * pushed onto it before it is written and taken back off afterwards. stack[0]
 * carries the stack pointer, since sp itself is not part of the array.
 */
static void push_machine_state(void) {
  stack[--sp] = (zword_t)(pc / PAGE_SIZE);
  stack[--sp] = (zword_t)(pc % PAGE_SIZE);
  stack[--sp] = fp;
  stack[--sp] = h_version;
  stack[0] = sp;
}

static int pop_machine_state(void) {
  sp = stack[0];

  if (sp >= STACK_SIZE) {
    return 1;
  }
  if (stack[sp++] != h_version) {
    return 1;
  }
  fp = stack[sp++];
  pc = stack[sp++];
  pc += (unsigned long)stack[sp++] * PAGE_SIZE;
  return 0;
}

/* Word-at-a-time stream helpers, most significant byte first. */
static int write_word(struct ZmSaveStream* s, zword_t w) {
  uint8_t b[2];
  b[0] = (uint8_t)(w >> 8);
  b[1] = (uint8_t)(w & 0xff);
  return ZmHostSaveWrite(s, b, 2);
}

static int read_word(struct ZmSaveStream* s, zword_t* w) {
  uint8_t b[2];
  if (!ZmHostSaveRead(s, b, 2)) {
    return 0;
  }
  *w = (zword_t)(((zword_t)b[0] << 8) | (zword_t)b[1]);
  return 1;
}

/*
 * z_save
 *
 * Writes the current game state to a save slot. V5's auxiliary-file form of
 * SAVE, which writes an arbitrary table rather than machine state, is not
 * supported and reports failure.
 */
int z_save(int argc, zword_t table, zword_t bytes, zword_t name) {
  struct ZmSaveStream* s;
  int status = 0;
  int i;
  zword_t scripting_flag;

  (void)table;
  (void)bytes;
  (void)name;

  if (argc == 3) {
    output_line("Auxiliary save files are not supported.");
    status = 1;
    goto finished;
  }

  s = ZmHostSaveOpen(0, 1);
  if (s == NULL) {
    output_line("Cannot open save file.");
    status = 1;
    goto finished;
  }

  /* Scripting state is per-session, not part of the saved game. */
  scripting_flag = get_word(H_FLAGS) & SCRIPTING_FLAG;
  set_word(H_FLAGS, get_word(H_FLAGS) & (~SCRIPTING_FLAG));

  push_machine_state();

  for (i = 0; i < STACK_SIZE && status == 0; i++) {
    if (!write_word(s, stack[i])) {
      status = 1;
    }
  }

  if (status == 0 && !ZmHostSaveWrite(s, datap, h_restart_size)) {
    status = 1;
  }

  pop_machine_state();
  set_word(H_FLAGS, get_word(H_FLAGS) | scripting_flag);

  ZmHostSaveClose(s);

finished:
  /* Report the result to the story.
   *
   * In V3 SAVE is a branch instruction, so the branch data that follows it in
   * the instruction stream has to be consumed here. Returning without doing
   * so leaves those bytes to be decoded as the next opcode, which is what
   * "failing opcode: 0" turns out to mean. V4 and later store a result
   * instead. */
  if (h_type < V4) {
    conditional_jump(status == 0);
  } else {
    store_operand((zword_t)((status == 0) ? 1 : 0));
  }

  return status;
}

/*
 * z_restore
 *
 * Reads a save back. On failure the machine is left exactly as it was, which
 * matters because a half-read stack would otherwise leave the interpreter
 * executing from a meaningless PC. The stack is read into a scratch copy and
 * only committed once the whole file has been accepted.
 */
int z_restore(int argc, zword_t table, zword_t bytes, zword_t name) {
  struct ZmSaveStream* s;
  zword_t* scratch;
  zword_t saved_sp, saved_fp;
  unsigned long saved_pc;
  int status = 0;
  int i;

  (void)table;
  (void)bytes;
  (void)name;

  if (argc == 3) {
    output_line("Auxiliary save files are not supported.");
    status = 1;
    goto finished;
  }

  scratch = (zword_t*)malloc(sizeof(zword_t) * STACK_SIZE);
  if (scratch == NULL) {
    output_line("Not enough memory to restore.");
    status = 1;
    goto finished;
  }

  s = ZmHostSaveOpen(0, 0);
  if (s == NULL) {
    free(scratch);
    output_line("Cannot open save file.");
    status = 1;
    goto finished;
  }

  for (i = 0; i < STACK_SIZE && status == 0; i++) {
    if (!read_word(s, &scratch[i])) {
      status = 1;
    }
  }

  saved_sp = sp;
  saved_fp = fp;
  saved_pc = pc;

  if (status == 0) {
    memcpy(stack, scratch, sizeof(zword_t) * STACK_SIZE);
    if (pop_machine_state() != 0) {
      status = 1;
    }
  }

  if (status == 0 && !ZmHostSaveRead(s, datap, h_restart_size)) {
    status = 1;
  }

  if (status != 0) {
    /* Roll back. Dynamic memory may be partly overwritten at this point, so
     * reload it from the story rather than leaving a mixture of two games. */
    ZmReloadDynamic();
    sp = saved_sp;
    fp = saved_fp;
    pc = saved_pc;
    output_line("Save file is damaged or from another game.");
  }

  ZmHostSaveClose(s);
  free(scratch);

finished:
  /* As for SAVE, V3 branches and later versions store.
   *
   * On a successful restore the PC has already been replaced by the one the
   * save recorded, which points at the branch data of the SAVE that wrote the
   * file. Taking the branch here is therefore what makes execution resume
   * just after that SAVE, and is why the same call appears on both paths. */
  if (h_type < V4) {
    conditional_jump(status == 0);
  } else {
    store_operand((zword_t)((status == 0) ? 2 : 0));
  }

  return status;
}

/*
 * z_save_undo / z_restore_undo
 *
 * A single level of undo held in RAM. Reports "not available" for stories
 * older than V5, which is what the specification asks for.
 */
void z_save_undo(void) {
  alloc_undo_if_needed();

  if (undo_datap == NULL || undo_stack == NULL) {
    store_operand((zword_t)-1);
    return;
  }

  push_machine_state();
  memcpy(undo_stack, stack, sizeof(zword_t) * STACK_SIZE);
  memcpy(undo_datap, datap, h_restart_size);
  pop_machine_state();

  undo_valid = TRUE;
  store_operand(1);
}

void z_restore_undo(void) {
  if (undo_datap == NULL || undo_stack == NULL || !undo_valid) {
    store_operand(0);
    return;
  }

  memcpy(stack, undo_stack, sizeof(zword_t) * STACK_SIZE);
  memcpy(datap, undo_datap, h_restart_size);

  if (pop_machine_state() != 0) {
    store_operand(0);
    return;
  }
  store_operand(2);
}

void swap_bytes(zword_t* ptr, int len) {
  int i;
  for (i = 0; i < len / 2; i++) {
    ptr[i] = (zword_t)((ptr[i] >> 8) | (ptr[i] << 8));
  }
}

/*
 * Transcription and command recording.
 *
 * Daisy has no printer and no scratch file system, so the script and record
 * streams are accepted and discarded. Games check the scripting flag and
 * behave correctly when it stays clear.
 */
void open_script(void) {}
void close_script(void) {}
void flush_script(void) {}
void script_char(int c) { (void)c; }
void script_string(const char* s) { (void)s; }
void script_line(const char* s) { (void)s; }
void script_new_line(void) {}

void open_record(void) {}
void close_record(void) {}
void record_line(const char* s) { (void)s; }
void record_key(int c) { (void)c; }

void z_open_playback(int arg) { (void)arg; }

void z_input_stream(int arg) { (void)arg; }

/*
 * playback_line
 *
 * Returns -1 to say there is no recorded input, which sends get_line to the
 * keyboard.
 */
int playback_line(int buflen, char* buffer, int* read_size) {
  (void)buflen;
  (void)buffer;
  (void)read_size;
  return -1;
}

/*
 * playback_key
 *
 * Counterpart to playback_line for single keypresses. Returns -1 because
 * there is no recorded input to replay.
 */
int playback_key(void) { return -1; }
