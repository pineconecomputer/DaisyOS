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
 * Portability shim for the Z-machine module.
 *
 * The interpreter sources are built twice: once as DaisyOS firmware for the
 * SAM3X, and once as a native binary by test/zmachine/harness.cpp so the
 * interpreter can be exercised on a development machine without hardware.
 * Everything that differs between those two targets is confined to this file.
 *
 * ZM_HOST_BUILD is defined by the harness. Firmware builds leave it undefined
 * and pick up Arduino.h.
 */

#ifndef INCLUDE_ZMACHINE_ZM_PORT_H_
#define INCLUDE_ZMACHINE_ZM_PORT_H_

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#ifdef ZM_HOST_BUILD

#include <stdio.h>

/* The SAM3X maps its flash into the normal address space, so a story file
 * held in flash is read with a plain array subscript. The harness keeps the
 * story in ordinary heap memory, which behaves the same way. Nothing in the
 * interpreter needs to know which it is. */
#define ZM_FLASH

unsigned long ZmMillis(void);

#else /* firmware build */

#include <Arduino.h>
#define ZM_FLASH

inline unsigned long ZmMillis(void) { return millis(); }

#endif /* ZM_HOST_BUILD */

/* jzip calls RANDOM_FUNC/SRANDOM_FUNC; route them through the shim so the
 * host build and the firmware build produce the same sequence for a given
 * seed. That matters for reproducing a reported bug from a save file. */
void ZmSeedRandom(unsigned long seed);
long ZmRandom(void);

#define RANDOM_FUNC ZmRandom
#define SRANDOM_FUNC ZmSeedRandom

#endif  // INCLUDE_ZMACHINE_ZM_PORT_H_
