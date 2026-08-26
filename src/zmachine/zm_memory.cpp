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
 * Story memory.
 *
 * This file replaces jzip's memory.c. Upstream assumes the story lives on a
 * disk, so it reads the file through an LRU chain of 512-byte pages and keeps
 * a resident copy of everything up to the end of the dictionary to stop
 * dictionary lookups from evicting the page the interpreter is executing
 * from. On Daisy the story is a const array in flash, which the SAM3X maps
 * into the address space, so every static byte is already one subscript away.
 * The cache, the page chain and the dictionary padding all go.
 *
 * What remains in RAM is the dynamic region alone: header word 0x0E bytes,
 * being 11,859 for Zork I against the 20,480 upstream would have reserved.
 *
 *   0                zm_dyn_size                        zm_story_length
 *   |  dynamic memory   |        static + high memory          |
 *   |  datap (RAM)      |        flash, read directly          |
 *
 * z_restart reloads datap from flash, which is why the pristine image has to
 * stay addressable for the whole session rather than being copied and
 * discarded.
 */

#include "zmachine/zm_types.h"
#include "zmachine/zm_story.h"
#include "zmachine/zm_internal.h"

/* Size of the writable region. Everything at or above this comes from flash. */
unsigned long zm_dyn_size = 0;

/*
 * ZmWriteGuard
 *
 * Called when a game writes at or above the static memory boundary. The
 * Z-machine specification forbids this, and upstream would silently scribble
 * on its resident copy of read-only data. Flash cannot be written at all, so
 * the store is dropped and reported rather than being allowed to look like it
 * worked.
 */
void ZmWriteGuard(unsigned long offset) {
#ifdef STRICTZ
  char buf[48];
  snprintf(buf, sizeof(buf), "write to static memory at %lu", offset);
  report_strictz_error(STRZERR_NO_ERROR, buf);
#else
  (void)offset;
#endif
}

/*
 * ZmStoryByteChecked
 *
 * Guarded story read for paths that derive an address from game data, where a
 * corrupt table could otherwise index past the end of the image. Returns 0
 * outside the story, which is what an unmapped read would have produced on the
 * machines these games were written for.
 */
uint8_t ZmStoryByteChecked(uint32_t addr) {
  if (addr >= zm_story_length) {
    return 0;
  }
  return zm_story_data[addr];
}

/*
 * ZmDynamicRange
 *
 * True when [off, off+len) lies wholly inside dynamic memory. Used where the
 * interpreter takes a raw pointer into datap from an address supplied by the
 * story, which upstream does without checking because its datap is larger.
 */
int ZmDynamicRange(unsigned long off, unsigned long len) {
  if (datap == NULL) {
    return 0;
  }
  if (off >= zm_dyn_size) {
    return 0;
  }
  if (len > zm_dyn_size - off) {
    return 0;
  }
  return 1;
}

/*
 * load_cache
 *
 * Allocates dynamic memory and copies it out of the story image. Also
 * allocates the output and status-line buffers that upstream sizes here.
 */
void load_cache(void) {
  line = (char*)malloc(screen_cols + 1);
  if (line == NULL) {
    fatal("load_cache(): out of memory for output buffer");
    return;
  }
  status_line = (char*)malloc(screen_cols + 1);
  if (status_line == NULL) {
    fatal("load_cache(): out of memory for status buffer");
    return;
  }

  /* Mind the upstream names, which do not say what they appear to:
   *
   *   h_data_size    is header word 0x04, the base of HIGH memory (20023 in
   *                  Zork I). Upstream makes its resident area this large so
   *                  that static memory, chiefly the dictionary and the
   *                  abbreviation table, never has to be paged.
   *   h_restart_size is header word 0x0E, the base of STATIC memory (11859 in
   *                  Zork I), and so the true size of the writable region.
   *
   * z_restart reloads h_restart_size bytes, and save/restore serialises
   * exactly h_restart_size bytes of datap (see quetzal.cpp), which is what
   * confirms the second of these is the dynamic area. Reading static memory
   * from flash costs nothing here, so only that region is made resident:
   * 11,859 bytes rather than 20,023. */
  zm_dyn_size = h_restart_size;
  if (zm_dyn_size == 0 || zm_dyn_size > zm_story_length) {
    fatal("load_cache(): bad static memory base in story header");
    return;
  }
  data_size = (unsigned int)zm_dyn_size;

  datap = (zbyte_t*)malloc(zm_dyn_size);
  if (datap == NULL) {
    fatal("load_cache(): out of memory for dynamic area");
    return;
  }
  memcpy(datap, zm_story_data, zm_dyn_size);

  /* SAVE_UNDO/RESTORE_UNDO are v5 and later. Allocating a second copy of
   * dynamic memory for a v3 story would waste 11K of a 96K machine, so the
   * undo buffer is only taken when the story can actually ask for it. */
  if (h_type >= V5) {
    undo_datap = (zbyte_t*)malloc(zm_dyn_size);
  } else {
    undo_datap = NULL;
  }
}

/*
 * unload_cache
 *
 * Releases everything load_cache took. Called between games so a second story
 * can be started without a reboot.
 */
void unload_cache(void) {
  z_new_line();

  free(line);
  line = NULL;
  free(status_line);
  status_line = NULL;
  free(datap);
  datap = NULL;
  free(undo_datap);
  undo_datap = NULL;
  zm_dyn_size = 0;
  data_size = 0;
}

/*
 * ZmReloadDynamic
 *
 * Restores dynamic memory to its as-shipped contents. Used by z_restart, which
 * upstream implements by re-reading pages from the story file.
 */
void ZmReloadDynamic(void) {
  if (datap != NULL) {
    memcpy(datap, zm_story_data, zm_dyn_size);
  }
}

/*
 * read_page
 *
 * Upstream's page reader, kept because z_verify walks the whole story through
 * it to compute the checksum. Reads straight out of flash.
 */
void read_page(int page, void* buffer) {
  unsigned long offset = (unsigned long)page * PAGE_SIZE;
  unsigned long count = PAGE_SIZE;

  if (offset >= zm_story_length) {
    memset(buffer, 0, PAGE_SIZE);
    return;
  }
  if (offset + count > zm_story_length) {
    count = zm_story_length - offset;
    memset((zbyte_t*)buffer + count, 0, PAGE_SIZE - count);
  }
  memcpy(buffer, zm_story_data + offset, count);
}

/*
 * read_code_byte
 *
 * Fetches the next instruction byte and advances the PC. Routines live in high
 * memory for every published story, so this almost always reads flash, but the
 * dynamic case is handled too rather than assumed away.
 */
zbyte_t read_code_byte(void) {
  zbyte_t value;

  if (pc < zm_dyn_size) {
    value = datap[pc];
  } else if (pc < zm_story_length) {
    value = zm_story_data[pc];
  } else {
    fatal("read_code_byte(): PC past end of story");
    return 0;
  }
  pc++;
  return value;
}

zword_t read_code_word(void) {
  zword_t w;

  w = (zword_t)read_code_byte() << 8;
  w |= (zword_t)read_code_byte();
  return w;
}

/*
 * read_data_byte
 *
 * Reads a byte at *addr and advances it. Used for table walks, which routinely
 * cross the dynamic/static boundary, so the split is checked per byte.
 */
zbyte_t read_data_byte(unsigned long* addr) {
  zbyte_t value;

  if (*addr < zm_dyn_size) {
    value = datap[*addr];
  } else if (*addr < zm_story_length) {
    value = zm_story_data[*addr];
  } else {
    value = 0;
  }
  (*addr)++;
  return value;
}

zword_t read_data_word(unsigned long* addr) {
  zword_t w;

  w = (zword_t)read_data_byte(addr) << 8;
  w |= (zword_t)read_data_byte(addr);
  return w;
}
