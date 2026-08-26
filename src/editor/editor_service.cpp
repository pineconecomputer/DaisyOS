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

#include "editor/editor_internal.h"

static char line_entered[255];
static bool insertMode = false;

// Strips trailing spaces, which screen lines are padded with.
static void TrimSpace(char* str) {
  uint8_t len = strlen(str);
  while (len > 0 && str[len - 1] == ' ') {
    len--;
  }
  str[len] = '\0';
}

// Reads the logical line under the cursor, joining it with its neighbour when
// a long line has wrapped, so the editor sees what the user sees.
static void EditorGrabFullLine(char* line) {
  char other_line[2 * VID_WIDTH + 1];
  uint8_t current_line = GetCurrentLine();
  GetVideoRamLine(current_line, line);

  if (current_line != 0 && HasContinue(current_line - 1)) {
    GetVideoRamLine(current_line - 1, other_line);
    strncat(other_line, line, VID_WIDTH);
    strcpy(line, other_line);
  } else if (HasContinue(current_line)) {
    GetVideoRamLine(current_line + 1, other_line);
    strncat(line, other_line, VID_WIDTH);
  }

  TrimSpace(line);
}

// One pass of the full-screen editor: blink the cursor, then handle whatever
// keystrokes have queued up. Return submits the current line to BASIC. Called
// from the main loop, so it must not block.
bool EditorService(void) {
  bool processed_keys = false;
  uint8_t keys_pressed[16];
  size_t key_byte_len;

  CursorHandler();

  key_byte_len = BufferDrain(keys_pressed, sizeof(keys_pressed));
  if (key_byte_len > 0) {
    processed_keys = true;
    ClearAttribute();
    for (uint8_t i = 0; i < key_byte_len; i++) {
      uint8_t key_pressed = keys_pressed[i];
      switch (key_pressed) {
        case (RETURN_KEY): {
          EditorGrabFullLine(line_entered);
          Newline();
          BasicExecute(line_entered);
          break;
        }
        case (BS_KEY): {
          if (GetCursorX() > 0) {
            MoveCursor(kCursorLeft);
            DeleteCharAtCursor();
          }
          break;
        }
        case (UP_KEY):
        case (RIGHT_KEY):
        case (DOWN_KEY):
        case (LEFT_KEY): {
          MoveCursor((CursorDirection)key_pressed);
          break;
        }
        case (SCROLL_LOCK_KEY): {
          SetScrollLock(!GetScrollLock());
          break;
        }
        case (CTRL_RETURN_KEY): {
          ClearContinue(GetCurrentLine());
          break;
        }
        case (CTRL_G_KEY): {
          AudioMsgSendToneOn(0, 1047, 75);
          break;
        }
        case (CTRL_L_KEY): {
          Clrscr(' ');
          LocateCursor(0, 0);
          break;
        }
        case (CTRL_C_KEY): {
          if (BasicIsRunning()) {
            BasicRequestBreak();
          }
          break;
        }
        case (CTRL_SPACE_KEY): {
          uint8_t sx = GetCursorX(), sy = GetCursorY();
          InsertCharAtCursor(' ');
          LocateCursor(sx, sy);
          break;
        }
        case (DEL_KEY): {
          DeleteCharAtCursor();
          break;
        }
        case (AC_SPACE_KEY): {
          insertMode = !insertMode;
          SetCursorBlinkRate(insertMode ? CURSOR_BLINK_INSERT
                                        : CURSOR_BLINK_NORMAL);
          break;
        }
        case (CTRL_I): {
          uint8_t spaces = 4 - (GetCursorX() % 4);
          for (uint8_t s = 0; s < spaces; s++) {
            if (insertMode) {
              InsertCharAtCursor(' ');
              MoveCursor(kCursorRight);
            } else {
              Chrout(' ');
            }
          }
          break;
        }
        default: {
          if (key_pressed != NULL) {
            if (insertMode) {
              InsertCharAtCursor(key_pressed);
            } else {
              Chrout(key_pressed);
            }
          }
          break;
        }
      }
    }
  }
  return processed_keys;
}
