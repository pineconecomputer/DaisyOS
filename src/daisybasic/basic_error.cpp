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

const char* const errorMessages[] = {"?TYPE MISMATCH ERROR",
                                     "?ARRAY NOT DIM'D ERROR",
                                     "?WRONG NUMBER OF DIMENSIONS ERROR",
                                     "?BAD SUBSCRIPT ERROR",
                                     "?DIVISION BY ZERO ERROR",
                                     "?ILLEGAL QUANTITY ERROR",
                                     "?SYNTAX ERROR",
                                     "?OUT OF MEMORY ERROR",
                                     "?RETURN WITHOUT GOSUB ERROR",
                                     "?OUT OF DATA ERROR",
                                     "?NEXT WITHOUT FOR ERROR",
                                     "?LOAD ERROR",
                                     "?MISSING FILE NAME ERROR",
                                     "?CATALOG ERROR",
                                     "?UNDEF'D STATEMENT ERROR",
                                     "?ILLEGAL LINE NUMBER",
                                     "?ILLEGAL DIRECT ERROR",
                                     "?WEND WITHOUT WHILE ERROR",
                                     "?WHILE WITHOUT WEND ERROR",
                                     "?UNTIL WITHOUT DO ERROR",
                                     "?DO WITHOUT UNTIL ERROR",
                                     "?EXIT NOT IN LOOP ERROR",
                                     "?RESULT WITHOUT RETURN ERROR",
                                     "?FOPEN ERROR",
                                     "?FCLOSE ERROR",
                                     "?FPRINT ERROR",
                                     "?FPUT ERROR",
                                     "?FSEEK ERROR",
                                     "?WIFI CONNECT FAILED",
                                     "?NETCONNECT FAILED",
                                     "?RESUME WITHOUT TRAP ERROR",
                                     "?LINE TOO LONG ERROR",
                                     "?FORMULA TOO COMPLEX ERROR"};

// Message text for an error code, or a fallback for an out-of-range code so a
// bad value still prints something rather than reading past the table.
const char* GetErrorMessage(BasicError err) {
  if (err >= 0 &&
      (size_t)err < sizeof(errorMessages) / sizeof(errorMessages[0])) {
    return errorMessages[err];
  }
  return "?UNKNOWN ERROR";
}

// Reports a runtime error. If a TRAP handler is armed and not already running,
// the error is recorded for the handler instead of printed -- the re-entry
// check stops an error inside the handler from looping back into it. Otherwise
// it prints the message, with the offending line number when running.
void PrintError(BasicError err) {
  if (trapLineNum >= 0 && !trapActive && programRunning) {
    trapErrorCode = err;
    trapErrorLineIndex = currentExecLine;
    trapTriggered = true;
    errorPrinted = true;
    return;
  }

  Newline();
  LocateCursor(0, GetCursorY());
  PrintStr((char*)GetErrorMessage(err));
  if (programRunning && currentExecLine >= 0 &&
      currentExecLine < programLineCount) {
    char lineBuf[20];
    snprintf(lineBuf, sizeof(lineBuf), " IN %d",
             program[currentExecLine].lineNum);
    PrintStr(lineBuf);
  }
  Newline();
  errorPrinted = true;
}

// Hard-resets the processor.
void DoReset() {
  digitalWrite(52, HIGH);
  RSTC->RSTC_CR = RSTC_CR_KEY(0xA5) | RSTC_CR_PROCRST | RSTC_CR_PERRST;
}
