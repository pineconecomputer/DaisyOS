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

#ifndef INCLUDE_CURSOR_H_
#define INCLUDE_CURSOR_H_

#include <stdint.h>

#define CURSOR_BLINK_NORMAL 300
#define CURSOR_BLINK_INSERT 180

void CursorHandler(void);
void InitCursor(void);
void SetCursorBlinkRate(uint64_t ms);

#endif  // INCLUDE_CURSOR_H_
