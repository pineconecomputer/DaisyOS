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
 * Story file access.
 *
 * A Z-machine story divides into two regions at the address in header word
 * 0x0E. Below it is dynamic memory, which the game writes to and which must
 * therefore live in RAM. At and above it is static memory, which the game
 * only ever reads.
 *
 * jzip was written for machines that read the story from a disk, so it pages
 * the whole file through an LRU cache and, to keep dictionary lookups from
 * thrashing that cache, copies everything up to the end of the dictionary
 * into resident memory. For Zork I that inflates the resident footprint from
 * 11,859 bytes to 20,480.
 *
 * Daisy keeps the story in flash, which the SAM3X maps into the address
 * space, so static memory is already directly readable and none of that
 * machinery is needed. Only the dynamic region is copied into RAM. See
 * zm_memory.cpp.
 */

#ifndef INCLUDE_ZMACHINE_ZM_STORY_H_
#define INCLUDE_ZMACHINE_ZM_STORY_H_

#include "zmachine/zm_port.h"

typedef struct {
  const char* title;      /* shown in the story picker */
  const uint8_t* data;    /* flash-resident story image */
  uint32_t length;        /* bytes in data */
} ZmStory;

/* Stories linked into the firmware, terminated by a NULL title. */
extern const ZmStory kZmStories[];
extern const uint8_t kZmStoryCount;

/* The story currently mounted. Set by ZmStoryMount before load_cache runs. */
extern const uint8_t* zm_story_data;
extern uint32_t zm_story_length;

void ZmStoryMount(const ZmStory* story);

/* Read one byte of the story image. Callers must keep addr < zm_story_length;
 * ZmStoryByteChecked is the guarded form used on paths that derive an address
 * from game data. */
static inline uint8_t ZmStoryByte(uint32_t addr) { return zm_story_data[addr]; }

uint8_t ZmStoryByteChecked(uint32_t addr);

#endif  // INCLUDE_ZMACHINE_ZM_STORY_H_
