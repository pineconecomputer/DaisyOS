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

#include <Arduino.h>
#include "gpio.h"

// Releases a pin to input with pull-up, the idle state for the keyboard matrix
// so an unselected line does not drive the bus.
void PinSetFloat(uint32_t pin) { pinMode(pin, INPUT_PULLUP); }

// Drives a pin low, used to select a keyboard matrix column.
void PinSetLow(uint32_t pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
}

// Drives a pin high.
void PinSetHigh(uint32_t pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH);
}
