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

#include "joyport.h"
#include <Arduino.h>

enum {
  kJoypinUp = 3,
  kJoypinDown = 4,
  kJoypinLeft = 5,
  kJoypinRight = 6,
  kJoypinTrigger = 10
};

uint8_t joy_buttons[kNumJoyButtons] = {kJoypinUp, kJoypinDown, kJoypinLeft,
                                       kJoypinRight, kJoypinTrigger};

volatile uint8_t joy_flags;

#define kJoyPressThreshold 3
#define kJoyReleaseThreshold 3

typedef struct {
  bool raw;
  bool stable;
  uint8_t count;
} JoyState;

static JoyState joy_states[kNumJoyButtons];

// Configures the five joystick pins as pulled-up inputs and clears debounce
// state. The stick pulls a line low when pressed.
void InitJoyport(void) {
  for (uint8_t pin = 0; pin < kNumJoyButtons; pin++) {
    pinMode(joy_buttons[pin], INPUT_PULLUP);
  }
  memset(joy_states, 0, sizeof(joy_states));
  joy_flags = 0;
}

// Samples all five buttons, called from the 1 kHz keyboard scan. A reading has
// to persist for several consecutive samples before it is accepted, so contact
// bounce never reaches the program.
void DoCheckJoyport(void) {
  for (uint8_t pin = 0; pin < kNumJoyButtons; pin++) {
    bool reading = (digitalRead(joy_buttons[pin]) == LOW);
    JoyState* ptr_joy_state = &joy_states[pin];

    if (reading != ptr_joy_state->raw) {
      ptr_joy_state->raw = reading;
      ptr_joy_state->count = 1;
    } else if (ptr_joy_state->count < 0xFF) {
      ptr_joy_state->count++;
    }

    uint8_t threshold = reading ? kJoyPressThreshold : kJoyReleaseThreshold;
    if (reading != ptr_joy_state->stable && ptr_joy_state->count >= threshold) {
      ptr_joy_state->stable = reading;
    }

    if (ptr_joy_state->stable) {
      joy_flags |= (1 << pin);
    } else {
      joy_flags &= ~(1 << pin);
    }
  }
}

// Reads the debounced stick. n == 0 returns all five bits at once so a program
// can test diagonals in one call; 1..5 return a single button.
uint8_t JoyRead(uint8_t n) {
  uint8_t flags = joy_flags;

  if (n == 0) {
    return flags;
  }
  if (n > kNumJoyButtons) {
    return 0;
  }
  return (flags >> (n - 1)) & 1;
}
