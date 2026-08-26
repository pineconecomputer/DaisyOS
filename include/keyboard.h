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

#ifndef INCLUDE_KEYBOARD_H_
#define INCLUDE_KEYBOARD_H_

#include <Arduino.h>

const uint8_t kNumCols = 4;
const uint8_t kNumRows = 13;

typedef struct {
  volatile bool current_state;
  bool last_state;
  bool last_read_state;
  uint8_t debounce_counter;
} KeyState;

extern KeyState key_states[kNumCols][kNumRows];

typedef enum { kNormalMode, kShiftMode, kCtrlMode, kACMode } KeyboadMode;

const uint8_t SHIFT_KEY = 15;
const uint8_t CTRL_KEY = 17;
const uint8_t STOP_KEY = 24;
const uint8_t BS_KEY = 8;
const uint8_t DEL_KEY = 127;
const uint8_t INS_KEY = 45;
const uint8_t PI_KEY = 227;
const uint8_t DOT_KEY = 244;

const uint8_t UP_KEY = 1;
const uint8_t RIGHT_KEY = 2;
const uint8_t DOWN_KEY = 3;
const uint8_t LEFT_KEY = 4;

const uint8_t AC_KEY = 5;

const uint8_t SCROLL_LOCK_KEY = 16;
const uint8_t CTRL_RETURN_KEY = 18;
const uint8_t CTRL_G_KEY = 7;
const uint8_t CTRL_L_KEY = 12;
const uint8_t CTRL_C_KEY = 24;
const uint8_t CTRL_SPACE_KEY = 30;
const uint8_t AC_SPACE_KEY = 27;
const uint8_t ESC_KEY = 27;
const uint8_t PROG_KEY = 0;
const uint8_t CAROT_KEY = 94;
const uint8_t RETURN_KEY = 13;
const uint8_t SOP_KEY = 0x5c;

const uint8_t AT_KEY = 0x40;
const uint8_t TILDE_KEY = 0x7e;

const uint8_t CTRL_A = 1;
const uint8_t CTRL_B = 2;
const uint8_t CTRL_C = 3;
const uint8_t CTRL_D = 4;
const uint8_t CTRL_E = 5;
const uint8_t CTRL_F = 6;
const uint8_t CTRL_G = 7;
const uint8_t CTRL_H = 8;
const uint8_t CTRL_I = 9;
const uint8_t CTRL_J = 10;
const uint8_t CTRL_K = 11;
const uint8_t CTRL_L = 12;
const uint8_t CTRL_M = 13;
const uint8_t CTRL_N = 14;
const uint8_t CTRL_O = 15;
const uint8_t CTRL_P = 16;
const uint8_t CTRL_Q = 17;
const uint8_t CTRL_R = 18;
const uint8_t CTRL_S = 19;
const uint8_t CTRL_T = 20;
const uint8_t CTRL_U = 21;
const uint8_t CTRL_V = 22;
const uint8_t CTRL_W = 23;
const uint8_t CTRL_X = 24;
const uint8_t CTRL_Y = 25;
const uint8_t CTRL_Z = 26;

const uint8_t CTRL_NINE_CH = 41;
const uint8_t CTRL_COMMA_CH = 149;
const uint8_t CTRL_DOT_CH = 150;

const uint8_t CTRL_C_INTERNAL = 252;

const uint8_t current_keymap[kNumCols][kNumRows] = {
    {'-', '0', '8', '9', NULL, '2', '7', '6', RIGHT_KEY, '1', '5', '4', '3'},
    {'p', 'o', 'u', 'i', NULL, 'q', 'y', 't', LEFT_KEY, AC_KEY, 'r', 'e', 'w'},
    {RETURN_KEY, ';', 'k', 'l', CTRL_KEY, 's', 'j', 'h', PROG_KEY, 'a', 'g',
     'f', 'd'},
    {':', ' ', ',', '.', SHIFT_KEY, 'x', 'm', 'n', AC_KEY, 'z', 'b', 'v', 'c'},
};

const uint8_t shifted_keymap[kNumCols][kNumRows] = {
    {'=', '?', '(', ')', NULL, '"', '\'', '&', RIGHT_KEY, '!', '%', '$', '#'},
    {'P', 'O', 'U', 'I', NULL, 'Q', 'Y', 'T', LEFT_KEY, STOP_KEY, 'R', 'E',
     'W'},
    {RETURN_KEY, '[', 'K', 'L', '#', 'S', 'J', 'H', '#', 'A', 'G', 'F', 'D'},
    {']', BS_KEY, '<', '>', '#', 'X', 'M', 'N', STOP_KEY, 'Z', 'B', 'V', 'C'},
};

#if 0
const uint8_t ctrl_keymap[kNumCols][kNumRows] = {
    {CTRL_L_KEY, SOP_KEY, 2, 41, NULL, 3, 4, 5, 6, 7, 9, 10, 11},
    {'+', 'o', STOP_KEY, 9, NULL, 140, 147, 143, SCROLL_LOCK_KEY, '`', 141, 131,
     145},
    {CTRL_RETURN_KEY, '{', 11, 'l', 22, 142, 135, 133, '#', 128, CTRL_G_KEY,
     132, 130},
    {'}', CTRL_SPACE_KEY, 149, 150, '#', 146, 136, 137, '#', 148, 129, 144,
     CTRL_C_KEY},
};
#endif
const uint8_t ctrl_keymap[kNumCols][kNumRows] = {
    {CTRL_L_KEY, SOP_KEY, CTRL_B, CTRL_NINE_CH, NULL, CTRL_C, CTRL_D, CTRL_E,
     CTRL_F, CTRL_G, CTRL_I, CTRL_J, CTRL_K},
    {CTRL_P, CTRL_O, CTRL_U, CTRL_I, NULL, CTRL_Q, CTRL_Y, CTRL_T,
     SCROLL_LOCK_KEY, '`', CTRL_R, CTRL_E, CTRL_W},
    {CTRL_RETURN_KEY, CTRL_SPACE_KEY, CTRL_K, CTRL_L, CTRL_V, CTRL_S, CTRL_J,
     CTRL_H, '#', CTRL_A, CTRL_G, CTRL_F, CTRL_D},
    {DEL_KEY, CTRL_SPACE_KEY, CTRL_COMMA_CH, CTRL_DOT_CH, '#', CTRL_X, CTRL_M,
     CTRL_N, '#', CTRL_Z, CTRL_B, CTRL_V, CTRL_C_INTERNAL},
};

const uint8_t ac_keymap[kNumCols][kNumRows] = {
    {TILDE_KEY, '0', '8', '9', NULL, '2', '7', CAROT_KEY, DOWN_KEY, '|', '5',
     '4', '3'},
    {'+', '*', 'u', '/', NULL, 'q', UP_KEY, CTRL_I, UP_KEY, '`', 'r', ESC_KEY,
     'w'},
    {SCROLL_LOCK_KEY, '{', PI_KEY, DOT_KEY, CTRL_KEY, 's', RIGHT_KEY, DOWN_KEY,
     '#', AT_KEY, LEFT_KEY, 'f', 'd'},
    {'}', AC_SPACE_KEY, CAROT_KEY, CAROT_KEY, SHIFT_KEY, 'x', 'm', 'n', '#',
     'z', 'b', 'v', 'c'},
};

void InitKeyStates(void);
void PinConfigurePins(void);
bool IsKeyColRowPressed(uint8_t col, uint8_t row);
void KeyboardTimerInit(void);

#endif  // INCLUDE_KEYBOARD_H_
