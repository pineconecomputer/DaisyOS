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

#include "keyboard.h"
#include "gpio.h"
#include "buffer.h"
#include "joyport.h"

#define kDebounceThreshold 3
#define kDebounceReset 0

static KeyboadMode mode = kNormalMode;

static uint16_t keyboard_matrix[kNumCols];
KeyState key_states[kNumCols][kNumRows];
static const uint8_t kColPinStartsAt = 13;

// Maps a matrix line number to the Arduino pin it is wired to. The table exists
// because the physical wiring is not contiguous.
uint8_t PinMatrixToArduino(uint8_t strobe) {
  static const uint8_t row_map[] = {22, 23, 24, 25, 26, 27, 28, 29, 30,
                                    31, 32, 33, 34, 35, 36, 37, 38};
  return row_map[strobe];
}

// Sets every matrix pin to pulled-up input, the idle state.
void PinConfigurePins(void) {
  uint8_t cbm_pin = 0;
  for (int pin = 0; pin < 17; pin++) {
    cbm_pin = PinMatrixToArduino(pin);
    pinMode(cbm_pin, INPUT_PULLUP);
  }
}

// Strobes one column low and reads all rows as a bitmap. The column is released
// back to floating afterwards so it cannot fight the next strobe.
static void PinReadRowsInColumn(uint8_t colnum, uint16_t* row_bitmap) {
  *row_bitmap = 0;
  uint8_t matrix_pin = PinMatrixToArduino(colnum + kColPinStartsAt);
  PinSetLow(matrix_pin);
  for (uint8_t rownum = 0; rownum < kNumRows; rownum++) {
    *row_bitmap |= (digitalRead(PinMatrixToArduino(rownum)) == LOW) << rownum;
  }
  PinSetFloat(matrix_pin);
}

// Clears all per-key debounce and press state.
void InitKeyStates(void) { memset(key_states, 0, sizeof(key_states)); }

// Scans the whole matrix one column at a time into the raw bitmap.
static void ReadKeys(void) {
  for (uint8_t col_num = 0; col_num < kNumCols; col_num++) {
    PinReadRowsInColumn(col_num, &keyboard_matrix[col_num]);
  }
}

// Debounces one key: a reading must repeat for several consecutive scans before
// it is accepted, and any disagreement restarts the count.
static void DebounceKey(uint8_t col_num, uint8_t row_num) {
  bool key_reading = false;
  KeyState* ptr_key_state;

  key_reading = keyboard_matrix[col_num] & (1 << row_num);
  ptr_key_state = &key_states[col_num][row_num];

  if (key_reading == ptr_key_state->last_read_state) {
    ptr_key_state->debounce_counter++;
    if (ptr_key_state->debounce_counter >= kDebounceThreshold) {
      ptr_key_state->current_state = key_reading;
      ptr_key_state->debounce_counter = kDebounceThreshold;
    }
  } else {
    ptr_key_state->debounce_counter = kDebounceReset;
  }
  ptr_key_state->last_read_state = key_reading;
}

// Debounces every key in the matrix.
static void DebounceKeys(void) {
  for (uint8_t col_num = 0; col_num < kNumCols; col_num++) {
    for (uint8_t row_num = 0; row_num < kNumRows; row_num++) {
      DebounceKey(col_num, row_num);
    }
  }
}

// True for the three keys that select a keymap rather than emit a character.
static inline bool IsModifierKey(uint8_t key) {
  return key == SHIFT_KEY || key == CTRL_KEY || key == AC_KEY;
}

// Returns the keymap column matching the modifier currently held down.
static inline const uint8_t* GetActiveKeymap(uint8_t col_num) {
  switch (mode) {
    case kShiftMode:
      return shifted_keymap[col_num];
    case kCtrlMode:
      return ctrl_keymap[col_num];
    case kACMode:
      return ac_keymap[col_num];
    default:
      return current_keymap[col_num];
  }
}

// Turns a debounced press or release into a queued keystroke. Modifiers switch
// the active keymap instead of emitting anything, and the mode is always read
// from the unshifted map so a modifier is recognised whichever map is active.
static void ReportKeyStateChange(uint8_t col_num, uint8_t row_num) {
  KeyState* ptr_key_state = &key_states[col_num][row_num];

  if (ptr_key_state->current_state == ptr_key_state->last_state) {
    return;
  }

  uint8_t base_key = current_keymap[col_num][row_num];

  if (ptr_key_state->current_state) {
    if (mode == kNormalMode && IsModifierKey(base_key)) {
      if (base_key == SHIFT_KEY) {
        mode = kShiftMode;
      } else if (base_key == CTRL_KEY) {
        mode = kCtrlMode;
      } else {
        mode = kACMode;
      }
    } else {
      uint8_t mapped = GetActiveKeymap(col_num)[row_num];
      if (mapped) {
        BufferAdd(mapped);
      }
    }
  } else {
    if (IsModifierKey(base_key)) {
      mode = kNormalMode;
    }
  }

  ptr_key_state->last_state = ptr_key_state->current_state;
}

// Emits keystrokes for every key whose debounced state changed this scan.
static void HandleKeyStateChanges(void) {
  for (uint8_t col_num = 0; col_num < kNumCols; col_num++) {
    for (uint8_t row_num = 0; row_num < kNumRows; row_num++) {
      ReportKeyStateChange(col_num, row_num);
    }
  }
}

// Live debounced state of one matrix position, bypassing the keystroke queue.
bool IsKeyColRowPressed(uint8_t col, uint8_t row) {
  if (col < kNumCols && row < kNumRows) {
    return key_states[col][row].current_state;
  }
  return false;
}

// Starts the ~1 kHz timer interrupt that scans the keyboard and joystick.
void KeyboardTimerInit(void) {
  pmc_set_writeprotect(false);
  pmc_enable_periph_clk(TC3_IRQn);

  TC_Configure(TC1, 0,
               TC_CMR_WAVE | TC_CMR_WAVSEL_UP_RC | TC_CMR_TCCLKS_TIMER_CLOCK4);
  TC_SetRC(TC1, 0, 656);
  TC1->TC_CHANNEL[0].TC_IER = TC_IER_CPCS;
  TC1->TC_CHANNEL[0].TC_IDR = ~TC_IER_CPCS;

  NVIC_ClearPendingIRQ(TC3_IRQn);
  NVIC_SetPriority(TC3_IRQn, 4);
  NVIC_EnableIRQ(TC3_IRQn);

  TC_Start(TC1, 0);
}

// Reboots the machine when the four-key escape combination is held, a way out
// of a wedged program that does not need the power switch.
void HandleHardReset(void) {
  bool reset = IsKeyColRowPressed(1, 9) && IsKeyColRowPressed(3, 4) &&
               IsKeyColRowPressed(3, 1) && IsKeyColRowPressed(2, 0);
  if (reset) {
    digitalWrite(52, HIGH);
    RSTC->RSTC_CR = RSTC_CR_KEY(0xA5) | RSTC_CR_PROCRST | RSTC_CR_PERRST;
  }
}

// The 1 kHz scan interrupt: read the matrix, debounce it, queue any keystrokes,
// check the reset combination, and sample the joystick.
extern "C" void TC3_Handler(void) {
  TC_GetStatus(TC1, 0);
  ReadKeys();
  DebounceKeys();
  HandleKeyStateChanges();
  HandleHardReset();
  DoCheckJoyport();
}
