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

#ifndef INCLUDE_BUFFER_H_
#define INCLUDE_BUFFER_H_
#include <stdint.h>
#include <stddef.h>

#define KB_BUF_SIZE 16
#define KB_BUF_MASK 15

void BufferInit(void);
void BufferAdd(uint8_t b);
bool BufferHasBytes(void);
uint8_t BufferGet(void);
size_t BufferDrain(uint8_t* dst, size_t max);
bool BufferScanAndRemove(uint8_t key);
void BufferClear(void);

extern bool keyClickEnabled;
extern volatile bool keyClickPending;

#endif  // INCLUDE_BUFFER_H_
