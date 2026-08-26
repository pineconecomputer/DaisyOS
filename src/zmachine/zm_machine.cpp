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
 * The DaisyOS-facing end of the Z-machine module: story selection, and
 * handing the screen over and back.
 *
 * Excluded from the native test build, which drives ZmRunStory directly.
 */

#ifndef ZM_HOST_BUILD

#include "zmachine.h"
#include "zmachine/zm_run.h"
#include "zmachine/zm_story.h"
#include "zmachine/zm_internal.h"

#include "vt52.h"
#include "buffer.h"
#include "keyboard.h"
#include "shadow_ram.h"

void ZmHostResetQuit(void);

uint8_t ZMachineStoryCount(void) { return kZmStoryCount; }

const char* ZMachineStoryTitle(uint8_t n) {
  if (n >= kZmStoryCount) {
    return NULL;
  }
  return kZmStories[n].title;
}

static void put_str(const char* s) {
  while (*s != '\0') {
    Vt52Write((uint8_t)*s++);
  }
}

static void put_line(const char* s) {
  put_str(s);
  Vt52Write('\r');
  Vt52Write('\n');
}

/*
 * pick_story
 *
 * Offers the linked-in stories and waits for a choice. Returns the index, or
 * -1 if the player pressed STOP. Skipped entirely when only one story is
 * built in, which is the usual case given how much flash a story takes.
 */
static int pick_story(void) {
  uint8_t i;

  if (kZmStoryCount == 0) {
    return -1;
  }
  if (kZmStoryCount == 1) {
    return 0;
  }

  Vt52Init();
  put_line("");
  put_line("  INTERACTIVE FICTION");
  put_line("");

  for (i = 0; i < kZmStoryCount; i++) {
    char num[8]; /* "  10. " and a terminator, should the list ever grow */
    snprintf(num, sizeof(num), "  %u. ", (unsigned)(i + 1));
    put_str(num);
    put_line(kZmStories[i].title);
  }

  put_line("");
  put_str("  Choose, or STOP to go back: ");
  Vt52Flush();

  for (;;) {
    uint8_t k = BufferGet();

    if (k == STOP_KEY || k == CTRL_C_INTERNAL) {
      return -1;
    }
    if (k >= '1' && (uint8_t)(k - '1') < kZmStoryCount) {
      return (int)(k - '1');
    }
    Vt52Tick();
  }
}

/*
 * RunZMachine
 *
 * Entry point for the ZORK immediate command.
 */
void RunZMachine(void) {
  int choice;

  if (kZmStoryCount == 0) {
    /* A build with no story linked in still has the interpreter, so say so
     * plainly rather than appearing to do nothing. */
    Vt52Init();
    put_line("");
    put_line("  No stories are built into this");
    put_line("  firmware. See tools/mkstory.py.");
    Vt52Flush();
    delay(2500);
    Vt52HideCursor();
    Clrscr(' ');
    return;
  }

  choice = pick_story();
  if (choice < 0) {
    Vt52HideCursor();
    Clrscr(' ');
    return;
  }

  ZmHostResetQuit();
  Vt52Init();

  if (ZmRunStory(&kZmStories[choice]) != 0) {
    /* The story stopped on an error. Show it before giving the screen back,
     * otherwise the machine appears to have quit for no reason. */
    Vt52Init();
    put_line("");
    put_line("  The story stopped:");
    put_str("  ");
    put_line(zm_abort_message);
    Vt52Flush();
    delay(3000);
  }

  Vt52HideCursor();
  Clrscr(' ');
}

#endif /* !ZM_HOST_BUILD */
