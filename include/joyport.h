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

#ifndef INCLUDE_JOYPORT_H_
#define INCLUDE_JOYPORT_H_
#include <stdint.h>

#define kNumJoyButtons 5

extern volatile uint8_t joy_flags;

void InitJoyport(void);
void DoCheckJoyport(void);
uint8_t JoyRead(uint8_t n);

#endif  // INCLUDE_JOYPORT_H_
