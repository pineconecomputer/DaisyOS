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

#ifndef INCLUDE_DAISYBASIC_H_
#define INCLUDE_DAISYBASIC_H_

#include <Arduino.h>

bool BasicExecute(const char* line);

void BasicRequestBreak(void);

bool BasicIsRunning(void);

void RtcInit(void);

int RtcGetHours(void);
int RtcGetMinutes(void);
int RtcGetSeconds(void);
int RtcGetDay(void);
int RtcGetMonth(void);
int RtcGetYear(void);

#endif  // INCLUDE_DAISYBASIC_H_
