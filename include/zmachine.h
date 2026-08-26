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
 * Interactive fiction.
 *
 * Runs Infocom story files on Daisy: a Z-machine interpreter, ported from
 * jzip, drawing through the shared VT52 engine. Stories are linked into the
 * firmware as flash arrays, so no storage hardware is involved.
 *
 * This is the module's whole public surface. Everything else lives under
 * include/zmachine and src/zmachine, in the same way DaisyBASIC keeps its
 * internals to itself.
 */

#ifndef INCLUDE_ZMACHINE_H_
#define INCLUDE_ZMACHINE_H_

#include <Arduino.h>

/* Takes over the screen and keyboard, offers the linked-in stories if there
 * is more than one, and plays the chosen story until it ends or the player
 * presses STOP. Restores the display before returning. */
void RunZMachine(void);

/* Number of stories linked into this build. */
uint8_t ZMachineStoryCount(void);

/* Title of story n, or NULL if n is out of range. */
const char* ZMachineStoryTitle(uint8_t n);

#endif  // INCLUDE_ZMACHINE_H_
