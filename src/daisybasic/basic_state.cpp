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

#include "daisybasic/basic_internal.h"

Variable* variables = NULL;
int variableCount = 0;
int variableCapacity = 0;

ArrayDescriptor* arrayDescriptors = NULL;
int arrayDescriptorCount = 0;
int arrayDescriptorCapacity = 0;

ForLoopEntry* forStack = NULL;
int forStackTop = 0;
int forStackCapacity = 0;

GosubEntry* gosubStack = NULL;
int gosubStackTop = 0;
int gosubStackCapacity = 0;
int compoundStmtIndex = 0;
int outerStmtSkipForGosub = 1;
int thenClauseSkipForGosub = 0;
int pendingThenClauseSkip = 0;

ProgramLine* program = NULL;
int programLineCount = 0;
int programCapacity = 0;
uint8_t* tokenPool = NULL;
uint16_t tokenPoolUsed = 0;
uint16_t tokenPoolCap = 0;

char stringPool[STRING_POOL_SIZE];
uint16_t stringPoolTop = 0;
VarNameEntry varNameTable[MAX_VAR_NAMES];
uint8_t varNameCount = 0;

int currentExecLine = -1;
int continueLineIndex = -1;
bool programRunning = false;
volatile bool breakRequested = false;
bool errorPrinted = false;
int gotoLineNum = -1;

int dataLineIndex = 0;
int dataItemIndex = 0;

UserFunction userFunctions[MAX_USER_FUNCS];
int userFuncCount = 0;

WhileEntry* whileStack = NULL;
int whileStackTop = 0;
int whileStackCapacity = 0;

bool timerEnabled = false;
uint32_t timerIntervalMs = 0;
int timerTargetLine = 0;
uint32_t timerLastFireMs = 0;

size_t heapBytesUsed = 0;

float returnVals[2] = {0.0f, 0.0f};
int returnValCount = 0;

bool trigDegMode = false;

int trapLineNum = -1;
bool trapActive = false;
bool trapTriggered = false;
int trapErrorLineIndex = -1;
BasicError trapErrorCode = ERR_SYNTAX;

// Grows a dynamic array to hold at least `needed` elements, doubling so repeated
// appends stay amortised cheap. Tracks the heap total FRE() reports, and leaves
// the original block intact if the realloc fails.
bool EnsureCapacity(void** ptr, int* capacity, int needed, size_t elemSize,
                    int initialCap, bool zeroNew) {
  if (needed <= *capacity) {
    return true;
  }
  int newCap = (*capacity == 0) ? initialCap : *capacity * 2;
  while (newCap < needed) {
    newCap *= 2;
  }
  size_t oldBytes = (size_t)(*capacity) * elemSize;
  size_t newBytes = (size_t)newCap * elemSize;
  void* np = realloc(*ptr, newBytes);
  if (!np) {
    return false;
  }
  if (zeroNew) {
    memset((uint8_t*)np + oldBytes, 0, newBytes - oldBytes);
  }
  heapBytesUsed += (newBytes - oldBytes);
  *ptr = np;
  *capacity = newCap;
  return true;
}

// Grows the shared token pool that holds every program line's tokens. Capped at
// 65535 because offsets into the pool are stored as 16-bit values.
bool EnsureTokenPoolCapacity(int needed) {
  if (needed <= (int)tokenPoolCap) {
    return true;
  }
  if (needed > 65535) {
    return false;
  }
  int cap = (int)tokenPoolCap;
  int newCap = (cap == 0) ? 128 : cap * 2;
  while (newCap < needed) {
    newCap *= 2;
  }
  if (newCap > 65535) {
    newCap = 65535;
  }
  size_t oldBytes = (size_t)cap;
  size_t newBytes = (size_t)newCap;
  uint8_t* np = (uint8_t*)realloc(tokenPool, newBytes);
  if (!np) {
    return false;
  }
  heapBytesUsed += (newBytes - oldBytes);
  tokenPool = np;
  tokenPoolCap = (uint16_t)newCap;
  return true;
}

// Asks a running program to stop. The execution loop notices between statements.
void BasicRequestBreak(void) { breakRequested = true; }

// True while a program is executing, as opposed to immediate mode.
bool BasicIsRunning(void) { return programRunning; }
