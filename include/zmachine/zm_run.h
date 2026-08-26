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

#ifndef INCLUDE_ZMACHINE_ZM_RUN_H_
#define INCLUDE_ZMACHINE_ZM_RUN_H_

#include <setjmp.h>
#include "zmachine/zm_port.h"
#include "zmachine/zm_story.h"

/* fatal() unwinds to the setjmp in ZmRunStory rather than hanging. */
extern jmp_buf zm_abort_env;
extern int zm_abort_armed;
extern char zm_abort_message[80];

void ZmResetStrictzCounts(void);

/* Runs one story to completion. Returns 0 on a normal quit, non-zero if the
 * story was stopped by a fatal error, in which case zm_abort_message says
 * what happened. Safe to call repeatedly. */
int ZmRunStory(const ZmStory* story);

#endif  // INCLUDE_ZMACHINE_ZM_RUN_H_
