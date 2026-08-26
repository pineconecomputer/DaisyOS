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

#include "cursor.h"

#include "shadow_ram.h"
#include "timer.h"

static Timer cursorTimer;
static uint64_t currentBlinkRate = CURSOR_BLINK_NORMAL;

// Starts the cursor blinking at the default rate.
void InitCursor(void) { TimerCreate(&cursorTimer, CURSOR_BLINK_NORMAL); }

// Changes the blink period and restarts the phase, so the change is visible
// immediately rather than after the current half-cycle.
void SetCursorBlinkRate(uint64_t ms) {
  currentBlinkRate = ms;
  TimerCreate(&cursorTimer, ms);
}

// Polled from the main loop: flips the cursor cell's attribute each time the
// blink interval elapses. Drawing the cursor as an attribute means it needs no
// saved character underneath.
void CursorHandler(void) {
  if (TimerIsDone(&cursorTimer)) {
    ToggleAttribute();
    TimerReset(&cursorTimer);
  }
}
