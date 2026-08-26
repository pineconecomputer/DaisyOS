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
#include "comm_messages.h"

// Grows the array descriptor table.
static bool ArrayDescEnsureCapacity(int needed) {
  return EnsureCapacity((void**)&arrayDescriptors, &arrayDescriptorCapacity,
                        needed, sizeof(ArrayDescriptor), 4, true);
}

// LIST: prints the program, or a line or range of them, detokenising as it
// goes. Interruptible with BREAK so a long listing can be stopped.
bool CmdList(const char* args) {
  char lineBuf[128];
  int startLine = 0, endLine = 65535;

  args = SkipWhitespace(args);

  if (*args && isdigit(*args)) {
    startLine = 0;
    while (isdigit(*args)) {
      startLine = startLine * 10 + (*args - '0');
      args++;
    }
    endLine = startLine;

    args = SkipWhitespace(args);
    if (*args == '-') {
      args++;
      args = SkipWhitespace(args);
      if (isdigit(*args)) {
        endLine = 0;
        while (isdigit(*args)) {
          endLine = endLine * 10 + (*args - '0');
          args++;
        }
      } else {
        endLine = 65535;
      }
    }
  }

  for (int i = 0; i < programLineCount; i++) {
    uint8_t keys[16];
    size_t keyLen = BufferDrain(keys, sizeof(keys));
    for (size_t j = 0; j < keyLen; j++) {
      if (keys[j] == CTRL_C_KEY) {
        Newline();
        PrintStr("BREAK");
        Newline();
        return true;
      }
      if (keys[j] == SCROLL_LOCK_KEY) {
        uint8_t k;
        do {
          k = BufferGet();
        } while (k != SCROLL_LOCK_KEY && k != CTRL_C_KEY);
        if (k == CTRL_C_KEY) {
          Newline();
          PrintStr("BREAK");
          Newline();
          return true;
        }
      }
    }

    uint16_t lineNum = program[i].lineNum;
    if (lineNum >= (uint16_t)startLine && lineNum <= (uint16_t)endLine) {
      DetokenizeLine(GetLineTokens(i), program[i].tokenLen, lineBuf,
                     sizeof(lineBuf));
      char outBuf[140];
      snprintf(outBuf, sizeof(outBuf), "%d %s", lineNum, lineBuf);
      PrintStr(outBuf);
      Newline();
    }
  }
  return true;
}

// DIM: declares one or more arrays, allocating and zeroing their storage.
bool CmdDim(const char* args) {
  for (;;) {
    char varName[MAX_VAR_NAME];

    args = ParseVarName(args, varName, sizeof(varName));
    if (!args) {
      PrintError(ERR_SYNTAX);
      return false;
    }

    args = SkipWhitespace(args);
    if (*args != '(') {
      PrintError(ERR_SYNTAX);
      return false;
    }
    args++;

    float dim1F;
    const char* afterDim1 = ParseExpression(args, &dim1F);
    if (!afterDim1) {
      PrintError(ERR_SYNTAX);
      return false;
    }
    args = SkipWhitespace(afterDim1);

    uint16_t dim1 = (uint16_t)(int)dim1F;
    uint16_t dim2 = 0;

    if (*args == ',') {
      args++;
      float dim2F;
      const char* afterDim2 = ParseExpression(args, &dim2F);
      if (!afterDim2) {
        PrintError(ERR_SYNTAX);
        return false;
      }
      args = SkipWhitespace(afterDim2);
      dim2 = (uint16_t)(int)dim2F;
      if (dim2 < 1) {
        PrintError(ERR_ILLEGAL_QUANTITY);
        return false;
      }
    }

    if (*args != ')') {
      PrintError(ERR_SYNTAX);
      return false;
    }
    if (dim1 < 1) {
      PrintError(ERR_ILLEGAL_QUANTITY);
      return false;
    }

    ArrayDescriptor* existing = FindArray(varName);
    if (existing) {
      uint32_t newTotalElements = (dim2 == 0) ? dim1 : (uint32_t)dim1 * dim2;
      uint32_t newTotalBytes =
          newTotalElements * GetElementSize(existing->type);
      uint32_t oldBytes = existing->totalBytes;
      heapBytesUsed -= oldBytes;
      free(existing->data);
      existing->data = (uint8_t*)malloc(newTotalBytes);
      if (!existing->data) {
        existing->isDimmed = false;
        existing->totalBytes = 0;
        PrintError(ERR_OUT_OF_MEMORY);
        return false;
      }
      memset(existing->data, 0, newTotalBytes);
      heapBytesUsed += newTotalBytes;
      existing->dim1Size = dim1;
      existing->dim2Size = dim2;
      existing->totalBytes = newTotalBytes;
    } else {
      int slot = -1;
      for (int i = 0; i < arrayDescriptorCount; i++) {
        if (!arrayDescriptors[i].isDimmed) {
          slot = i;
          break;
        }
      }
      if (slot < 0) {
        if (!ArrayDescEnsureCapacity(arrayDescriptorCount + 1)) {
          PrintError(ERR_OUT_OF_MEMORY);
          return false;
        }
        slot = arrayDescriptorCount++;
      }

      ArrayDescriptor* desc = &arrayDescriptors[slot];
      strncpy(desc->name, varName, MAX_VAR_NAME - 1);
      desc->name[MAX_VAR_NAME - 1] = '\0';
      desc->type = GetArrayTypeFromName(varName);

      if (!AllocateArray(desc, dim1, dim2)) {
        PrintError(ERR_OUT_OF_MEMORY);
        return false;
      }
    }

    args++;
    args = SkipWhitespace(args);
    if (*args != ',') {
      break;
    }
    args++;
    args = SkipWhitespace(args);
  }
  return true;
}

// CLR: frees variables, arrays, loop and call stacks, and user functions, but
// keeps the program text.
bool CmdClr(const char* args) {
  (void)args;

  if (variables) {
    for (int i = 0; i < variableCount; i++) {
      if (variables[i].type == VAR_STRING && variables[i].strVal) {
        heapBytesUsed -= strlen(variables[i].strVal) + 1;
        free(variables[i].strVal);
      }
    }
    heapBytesUsed -= variableCapacity * sizeof(Variable);
    free(variables);
    variables = NULL;
  }
  variableCount = 0;
  variableCapacity = 0;

  ClearAllArrays();

  if (arrayDescriptors) {
    heapBytesUsed -= arrayDescriptorCapacity * sizeof(ArrayDescriptor);
    free(arrayDescriptors);
    arrayDescriptors = NULL;
  }
  arrayDescriptorCount = 0;
  arrayDescriptorCapacity = 0;

  if (forStack) {
    heapBytesUsed -= forStackCapacity * sizeof(ForLoopEntry);
    free(forStack);
    forStack = NULL;
  }
  forStackTop = 0;
  forStackCapacity = 0;

  if (gosubStack) {
    heapBytesUsed -= gosubStackCapacity * sizeof(GosubEntry);
    free(gosubStack);
    gosubStack = NULL;
  }
  gosubStackTop = 0;
  gosubStackCapacity = 0;

  if (whileStack) {
    heapBytesUsed -= whileStackCapacity * sizeof(WhileEntry);
    free(whileStack);
    whileStack = NULL;
  }
  whileStackTop = 0;
  whileStackCapacity = 0;

  dataLineIndex = 0;
  dataItemIndex = 0;
  userFuncCount = 0;
  timerEnabled = false;
  returnValCount = 0;
  returnVals[0] = 0.0f;
  returnVals[1] = 0.0f;

  trapLineNum = -1;
  trapActive = false;
  trapTriggered = false;
  trapErrorLineIndex = -1;
  trigDegMode = false;

  CloseAllFileChannels();

  return true;
}

// Parses an array name with an optional starting subscript, as used by CLEARARR
// and SWAPARR.
static const char* ParseArrayRef(const char* args, char* name, size_t nameMax,
                                 int* startIdx) {
  args = SkipWhitespace(args);
  args = ParseVarName(args, name, nameMax);
  if (!args) {
    return NULL;
  }
  args = SkipWhitespace(args);
  *startIdx = 0;
  if (*args == '(') {
    args++;
    float idxF;
    const char* after = ParseExpression(args, &idxF);
    if (!after) {
      return NULL;
    }
    args = SkipWhitespace(after);
    if (*args != ')') {
      return NULL;
    }
    args++;
    *startIdx = (int)idxF;
    if (*startIdx < 0) {
      *startIdx = 0;
    }
  }
  return args;
}

// Total element count across both dimensions.
static inline uint32_t ArrTotal(ArrayDescriptor* a) {
  return (a->dim2Size == 0) ? a->dim1Size : (uint32_t)a->dim1Size * a->dim2Size;
}

// Zeroes an array from an index to the end.
static void ZeroElements(ArrayDescriptor* a, int startIdx) {
  int total = (int)ArrTotal(a);
  if (startIdx >= total) {
    return;
  }
  uint16_t sz = GetElementSize(a->type);
  if (a->type == ARRAY_TYPE_STRING) {
    for (int i = startIdx; i < total; i++) {
      ((char*)GetArrayElementPtr(a, i))[0] = '\0';
    }
  } else {
    memset((uint8_t*)a->data + startIdx * sz, 0, (total - startIdx) * sz);
  }
}

// Exchanges one element between two arrays.
static void SwapElems(ArrayDescriptor* a, int ai, ArrayDescriptor* b, int bi) {
  uint8_t tmp[STRING_ELEMENT_LEN];
  void* pA = GetArrayElementPtr(a, ai);
  void* pB = GetArrayElementPtr(b, bi);
  uint16_t szA = GetElementSize(a->type);
  uint16_t szB = GetElementSize(b->type);
  memcpy(tmp, pA, szA);
  uint16_t cpAB = szB < szA ? szB : szA;
  memcpy(pA, pB, cpAB);
  if (szB < szA) {
    memset((uint8_t*)pA + szB, 0, szA - szB);
  }
  uint16_t cpBA = szA < szB ? szA : szB;
  memcpy(pB, tmp, cpBA);
  if (szA < szB) {
    memset((uint8_t*)pB + szA, 0, szB - szA);
  }
}

// Rotates one element through three arrays in a single pass.
static void RotateElems(ArrayDescriptor* a, int ai, ArrayDescriptor* b, int bi,
                        ArrayDescriptor* c, int ci) {
  uint8_t tmp[STRING_ELEMENT_LEN];
  void* pA = GetArrayElementPtr(a, ai);
  void* pB = GetArrayElementPtr(b, bi);
  void* pC = GetArrayElementPtr(c, ci);
  uint16_t szA = GetElementSize(a->type);
  uint16_t szB = GetElementSize(b->type);
  uint16_t szC = GetElementSize(c->type);
  memcpy(tmp, pA, szA);
  uint16_t n;
  n = szB < szA ? szB : szA;
  memcpy(pA, pB, n);
  if (szB < szA) {
    memset((uint8_t*)pA + n, 0, szA - n);
  }
  n = szC < szB ? szC : szB;
  memcpy(pB, pC, n);
  if (szC < szB) {
    memset((uint8_t*)pB + n, 0, szB - n);
  }
  n = szA < szC ? szA : szC;
  memcpy(pC, tmp, n);
  if (szA < szC) {
    memset((uint8_t*)pC + n, 0, szC - n);
  }
}

// CLEARARR: zeroes arrays from an optional starting index. Arrays that were
// never dimensioned are skipped rather than raising an error.
bool CmdClearArr(const char* args) {
  for (;;) {
    char name[MAX_VAR_NAME];
    int startIdx;
    const char* after = ParseArrayRef(args, name, sizeof(name), &startIdx);
    if (!after) {
      PrintError(ERR_SYNTAX);
      return false;
    }
    args = SkipWhitespace(after);

    ArrayDescriptor* arr = FindArray(name);
    if (arr && arr->isDimmed && arr->data) {
      ZeroElements(arr, startIdx);
    }

    if (*args != ',') {
      break;
    }
    args++;
  }
  return true;
}

// SWAPARR: exchanges two arrays element by element, or rotates three. Mismatched
// lengths leave the excess zero-filled.
bool CmdSwapArr(const char* args) {
  char nameA[MAX_VAR_NAME], nameB[MAX_VAR_NAME], nameC[MAX_VAR_NAME];
  int startA = 0, startB = 0, startC = 0;
  bool has3 = false;

  const char* after = ParseArrayRef(args, nameA, sizeof(nameA), &startA);
  if (!after) {
    PrintError(ERR_SYNTAX);
    return false;
  }
  after = SkipWhitespace(after);
  if (*after != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  after++;

  after = ParseArrayRef(after, nameB, sizeof(nameB), &startB);
  if (!after) {
    PrintError(ERR_SYNTAX);
    return false;
  }
  after = SkipWhitespace(after);

  if (*after == ',') {
    after++;
    after = ParseArrayRef(after, nameC, sizeof(nameC), &startC);
    if (!after) {
      PrintError(ERR_SYNTAX);
      return false;
    }
    has3 = true;
  }

  ArrayDescriptor* arrA = FindArray(nameA);
  ArrayDescriptor* arrB = FindArray(nameB);
  ArrayDescriptor* arrC = has3 ? FindArray(nameC) : NULL;

  if (!arrA || !arrA->isDimmed) {
    return true;
  }
  if (!arrB || !arrB->isDimmed) {
    return true;
  }
  if (has3 && (!arrC || !arrC->isDimmed)) {
    return true;
  }

  int nA = (int)ArrTotal(arrA) - startA;
  if (nA < 0) {
    nA = 0;
  }
  int nB = (int)ArrTotal(arrB) - startB;
  if (nB < 0) {
    nB = 0;
  }

  if (!has3) {
    int n = nA < nB ? nA : nB;
    for (int i = 0; i < n; i++) {
      SwapElems(arrA, startA + i, arrB, startB + i);
    }
    if (nA > n) {
      ZeroElements(arrA, startA + n);
    }
    if (nB > n) {
      ZeroElements(arrB, startB + n);
    }
  } else {
    int nC = (int)ArrTotal(arrC) - startC;
    if (nC < 0) {
      nC = 0;
    }
    int n = nA < nB ? nA : nB;
    if (nC < n) {
      n = nC;
    }
    for (int i = 0; i < n; i++) {
      RotateElems(arrA, startA + i, arrB, startB + i, arrC, startC + i);
    }
    if (nA > n) {
      ZeroElements(arrA, startA + n);
    }
    if (nB > n) {
      ZeroElements(arrB, startB + n);
    }
    if (nC > n) {
      ZeroElements(arrC, startC + n);
    }
  }
  return true;
}

// NEW: erases the program along with all variables and arrays.
bool CmdNew(const char* args) {
  ClearProgram();
  CmdClr(args);
  heapBytesUsed = 0;
  return true;
}

// Sends a filename to the file server, accepting it quoted or bare.
static void BtSendFilename(const char* args) {
  char buf[256];
  if (*args == '"') {
    args++;
    const char* end = strchr(args, '"');
    if (end) {
      int len = end - args;
      if (len > 0 && len < (int)sizeof(buf)) {
        strncpy(buf, args, len);
        buf[len] = '\0';
        WifiSendLine(buf);
      }
    }
  } else if (*args != '\0') {
    int len = 0;
    while (args[len] && !isspace(args[len]) && len < (int)sizeof(buf) - 1) {
      buf[len] = args[len];
      len++;
    }
    buf[len] = '\0';
    if (len > 0) {
      WifiSendLine(buf);
    }
  }
}

static bool IsEndLine(const char* line);

// Reads a filename argument, quoted or bare.
static const char* ParseFilename(const char* args, char* out, int outSize) {
  args = SkipWhitespace(args);
  int i = 0;
  if (*args == '"') {
    args++;
    while (*args && *args != '"' && i < outSize - 1) {
      out[i++] = *args++;
    }
    if (*args == '"') {
      args++;
    }
  } else {
    while (*args && *args != ',' && !isspace((unsigned char)*args) &&
           i < outSize - 1) {
      out[i++] = *args++;
    }
  }
  out[i] = '\0';
  return args;
}

// Waits for the server's reply, executing each line it sends. The server
// answers in BASIC, so an error arrives as a PRINT the interpreter simply runs.
// The timeout restarts on partial input so a slow transfer is not cut off.
static bool WaitFileResponse(void) {
  char lineBuf[256];
  Timer timeout;
  TimerCreate(&timeout, 10000);
  while (true) {
    int r = WifiReadLine(lineBuf, sizeof(lineBuf));
    if (r == 1) {
      if (IsEndLine(lineBuf)) {
        return true;
      }
      _BasicExecute(lineBuf, false);
      return false;
    } else if (r == -1) {
      TimerReset(&timeout);
    }
    if (TimerIsDone(&timeout)) {
      PrintError(ERR_LOAD);
      return false;
    }
  }
}

// Recognises the unnumbered END that terminates a server transfer, as opposed
// to a numbered END that is part of the program.
static bool IsEndLine(const char* line) {
  while (*line && isspace(*line)) {
    line++;
  }
  if (!isdigit(*line) && strncasecmp(line, "end", 3) == 0) {
    line += 3;
    while (*line && isspace(*line)) {
      line++;
    }
    return *line == '\0';
  }
  return false;
}

// LOAD: receives a program from the file server, entering each line as it
// arrives.
bool CmdLoad(const char* args) {
  char lineBuf[256];
  char filename[64];
  Timer timeout;

  ParseFilename(args, filename, sizeof(filename));
  if (filename[0] == '\0') {
    PrintError(ERR_MISSING_FILE_NAME);
    return false;
  }

  CommMsgSendLoad(filename);
  TimerCreate(&timeout, 10000);

  while (true) {
    int result = WifiReadLine(lineBuf, sizeof(lineBuf));
    if (result == 1) {
      TimerReset(&timeout);
      if (IsEndLine(lineBuf)) {
        return true;
      }
      if (!_BasicExecute(lineBuf, false)) {
        PrintStr(lineBuf);
        Newline();
      }
      WifiSendByte(0x06);
      break;
    } else if (result == -1) {
      TimerReset(&timeout);
    }
    if (TimerIsDone(&timeout)) {
      PrintError(ERR_LOAD);
      return false;
    }
  }

  const int LOAD_BATCH = 8;
  int batchCount = 0;
  while (true) {
    int result = WifiReadLine(lineBuf, sizeof(lineBuf));
    if (result == 1) {
      TimerReset(&timeout);
      if (IsEndLine(lineBuf)) {
        return true;
      }
      if (!_BasicExecute(lineBuf, false)) {
        PrintStr(lineBuf);
        Newline();
      }
      if (++batchCount % LOAD_BATCH == 0) {
        WifiSendByte(0x06);
      }
    } else if (result == -1) {
      TimerReset(&timeout);
    }
    if (TimerIsDone(&timeout)) {
      PrintError(ERR_LOAD);
      return false;
    }
  }
}

// SAVE: sends the program to the file server as plain text. A leading dash on
// the name overwrites without confirmation.
bool CmdSave(const char* args) {
  char lineBuf[128];
  char outBuf[140];
  char filename[66];
  int fnIdx = 0;

  args = SkipWhitespace(args);
  if (*args == '\0') {
    PrintError(ERR_MISSING_FILE_NAME);
    return false;
  }

  if (*args == '-') {
    filename[fnIdx++] = *args++;
  }
  ParseFilename(args, &filename[fnIdx], (int)sizeof(filename) - fnIdx);
  if (filename[0] == '\0' || (filename[0] == '-' && filename[1] == '\0')) {
    PrintError(ERR_MISSING_FILE_NAME);
    return false;
  }

  CommMsgSendSave(filename);

  {
    char readyBuf[32];
    while (WifiReadLine(readyBuf, sizeof(readyBuf)) == 0) {
    }
    if (readyBuf[0] != '.') {
      _BasicExecute(readyBuf, false);
      return false;
    }
  }

  for (int i = 0; i < programLineCount; i++) {
    uint16_t lineNum = program[i].lineNum;
    DetokenizeLine(GetLineTokens(i), program[i].tokenLen, lineBuf,
                   sizeof(lineBuf));
    snprintf(outBuf, sizeof(outBuf), "%d %s", lineNum, lineBuf);
    WifiSendLine(outBuf);
    char ackBuf[8];
    while (WifiReadLine(ackBuf, sizeof(ackBuf)) == 0) {
    }
  }
  WifiSendLine("END");

  while (WifiReadLine(lineBuf, sizeof(lineBuf)) == 0) {
  }
  _BasicExecute(lineBuf, false);

  return true;
}

// CAT: requests and displays the server's directory listing.
bool CmdCatalog(const char* args) {
  char lineBuf[256];
  Timer timeout;
  Newline();
  CommMsgSendCatalog();
  TimerCreate(&timeout, 10000);

  uint8_t col = 0;

  while (true) {
    int result = WifiReadLine(lineBuf, sizeof(lineBuf));
    if (result == 1) {
      TimerReset(&timeout);
      if (IsEndLine(lineBuf)) {
        if (col == 1) {
          Newline();
        }
        return true;
      }
      if ((uint8_t)lineBuf[0] == 0x16) {
        if (col == 1) {
          Newline();
        }
        PrintStr(lineBuf + 1);
        Newline();
        col = 0;
      } else {
        LocateCursor(15 * col, GetCursorY());
        if (col == 0 &&
            ((uint8_t)lineBuf[0] == 0x17 || (uint8_t)lineBuf[0] == 0x12)) {
          PrintStr(lineBuf + 1);
        } else {
          PrintStr(lineBuf);
        }
        col = (col + 1) % 2;
        if (col == 0) {
          Newline();
        }
      }
      WifiSendByte(0x06);
    } else if (result == -1) {
      TimerReset(&timeout);
    }
    if (TimerIsDone(&timeout)) {
      PrintError(ERR_CATALOG);
      return false;
    }
  }
}

// CHDIR: changes the server's working directory.
bool CmdChdir(const char* args) {
  char path[64];
  ParseFilename(args, path, sizeof(path));
  if (path[0] == '\0') {
    PrintError(ERR_MISSING_FILE_NAME);
    return false;
  }
  CommMsgSendChdir(path);
  bool ok = WaitFileResponse();
  Newline();
  return ok;
}

// MKDIR: creates a directory on the server.
bool CmdMkdir(const char* args) {
  char path[64];
  ParseFilename(args, path, sizeof(path));
  if (path[0] == '\0') {
    PrintError(ERR_MISSING_FILE_NAME);
    return false;
  }
  CommMsgSendMkdir(path);
  bool ok = WaitFileResponse();
  Newline();
  return ok;
}

// DEL: deletes a file on the server.
bool CmdDel(const char* args) {
  char filename[64];
  ParseFilename(args, filename, sizeof(filename));
  if (filename[0] == '\0') {
    PrintError(ERR_MISSING_FILE_NAME);
    return false;
  }
  CommMsgSendDel(filename);
  bool ok = WaitFileResponse();
  Newline();
  return ok;
}

// REN: renames a file on the server.
bool CmdRen(const char* args) {
  char oldname[64], newname[64];
  args = ParseFilename(args, oldname, sizeof(oldname));
  if (oldname[0] == '\0') {
    PrintError(ERR_MISSING_FILE_NAME);
    return false;
  }
  args = SkipWhitespace(args);
  if (*args == ',') {
    args++;
  }
  ParseFilename(args, newname, sizeof(newname));
  if (newname[0] == '\0') {
    PrintError(ERR_MISSING_FILE_NAME);
    return false;
  }
  CommMsgSendRen(oldname, newname);
  bool ok = WaitFileResponse();
  Newline();
  return ok;
}

// COPY: copies a file on the server.
bool CmdCopy(const char* args) {
  char src[64], dst[64];
  args = ParseFilename(args, src, sizeof(src));
  if (src[0] == '\0') {
    PrintError(ERR_MISSING_FILE_NAME);
    return false;
  }
  args = SkipWhitespace(args);
  if (*args == ',') {
    args++;
  }
  ParseFilename(args, dst, sizeof(dst));
  if (dst[0] == '\0') {
    PrintError(ERR_MISSING_FILE_NAME);
    return false;
  }
  CommMsgSendCopy(src, dst);
  bool ok = WaitFileResponse();
  Newline();
  return ok;
}

// LOADC: receives raw glyph bitmaps from the server into character or graphics
// RAM, sending them on to DaisyVideo in bulk.
bool CmdLoadchar(const char* args) {
  args = SkipWhitespace(args);
  if (*args == '\0') {
    PrintError(ERR_MISSING_FILE_NAME);
    return false;
  }

  char filename[64];
  int fnLen = 0;
  if (*args == '"') {
    args++;
    while (*args && *args != '"' && fnLen < (int)sizeof(filename) - 1) {
      filename[fnLen++] = *args++;
    }
    if (*args == '"') {
      args++;
    }
  } else {
    while (*args && *args != ',' && !isspace((unsigned char)*args) &&
           fnLen < (int)sizeof(filename) - 1) {
      filename[fnLen++] = *args++;
    }
  }
  filename[fnLen] = '\0';

  bool isGfx = false;
  args = SkipWhitespace(args);
  if (*args == ',') {
    args++;
    args = SkipWhitespace(args);
    if (strncasecmp(args, "gfx", 3) == 0 &&
        (args[3] == ',' || args[3] == '\0' ||
         isspace((unsigned char)args[3]))) {
      isGfx = true;
      args += 3;
    } else if (strncasecmp(args, "char", 4) == 0 &&
               (args[4] == ',' || args[4] == '\0' ||
                isspace((unsigned char)args[4]))) {
      args += 4;
    }
  }

  uint8_t startIdx = 0;
  args = SkipWhitespace(args);
  if (*args == ',') {
    args++;
    float val;
    const char* next = ParseExpression(args, &val);
    if (next) {
      startIdx = (uint8_t)(int)val;
      args = next;
    }
  }

  CommMsgSendLoadChar(filename, isGfx, startIdx);

#define BTLC_BATCH 28
  uint8_t batchBuf[BTLC_BATCH * 8];
  uint8_t batchCount = 0;
  uint8_t batchStart = startIdx;
  uint16_t charIdx = startIdx;
  Timer timeout;
  TimerCreate(&timeout, 10000);

  while (charIdx <= 255) {
    uint8_t bitrows[8];
    int got = WifiReadBytes(bitrows, 8, 2000);
    if (got < 8) {
      break;
    }
    TimerReset(&timeout);

    memcpy(&batchBuf[batchCount * 8], bitrows, 8);
    batchCount++;

    if (batchCount == BTLC_BATCH || charIdx == 255) {
      if (isGfx) {
        VideoMsgSendDefineGfxBulk(batchStart, batchBuf, batchCount);
      } else {
        VideoMsgSendDefineCharBulk(batchStart, batchBuf, batchCount);
      }
      for (uint8_t n = 0; n < batchCount; n++) {
        if (isGfx) {
          SetGfxDef(batchStart + n, &batchBuf[n * 8]);
        } else {
          SetCharDef(batchStart + n, &batchBuf[n * 8]);
        }
      }
      batchStart = (uint8_t)(charIdx + 1);
      batchCount = 0;
    }

    WifiSendByte(0x06);
    charIdx++;
    if (TimerIsDone(&timeout)) {
      PrintError(ERR_LOAD);
      return false;
    }
  }

  if (batchCount > 0) {
    if (isGfx) {
      VideoMsgSendDefineGfxBulk(batchStart, batchBuf, batchCount);
    } else {
      VideoMsgSendDefineCharBulk(batchStart, batchBuf, batchCount);
    }
    for (uint8_t n = 0; n < batchCount; n++) {
      if (isGfx) {
        SetGfxDef(batchStart + n, &batchBuf[n * 8]);
      } else {
        SetCharDef(batchStart + n, &batchBuf[n * 8]);
      }
    }
  }
#undef BTLC_BATCH
  return true;
}

// SAVEC: sends a character set to the server as raw bitmaps.
bool CmdSavechar(const char* args) {
  args = SkipWhitespace(args);
  if (*args == '\0') {
    PrintError(ERR_MISSING_FILE_NAME);
    return false;
  }

  char filename[64];
  int fnLen = 0;
  if (*args == '"') {
    args++;
    while (*args && *args != '"' && fnLen < (int)sizeof(filename) - 1) {
      filename[fnLen++] = *args++;
    }
    if (*args == '"') {
      args++;
    }
  } else {
    while (*args && *args != ',' && !isspace((unsigned char)*args) &&
           fnLen < (int)sizeof(filename) - 1) {
      filename[fnLen++] = *args++;
    }
  }
  filename[fnLen] = '\0';

  bool isGfx = false;
  args = SkipWhitespace(args);
  if (*args == ',') {
    args++;
    args = SkipWhitespace(args);
    if (strncasecmp(args, "gfx", 3) == 0 &&
        (args[3] == ',' || args[3] == '\0' ||
         isspace((unsigned char)args[3]))) {
      isGfx = true;
      args += 3;
    } else if (strncasecmp(args, "char", 4) == 0 &&
               (args[4] == ',' || args[4] == '\0' ||
                isspace((unsigned char)args[4]))) {
      args += 4;
    }
  }

  uint8_t startIdx = 0;
  args = SkipWhitespace(args);
  if (*args == ',') {
    args++;
    float val;
    const char* next = ParseExpression(args, &val);
    if (next) {
      startIdx = (uint8_t)(int)val;
      args = next;
    }
  }

  CommMsgSendSaveChar(filename, isGfx, startIdx);

  {
    char readyBuf[32];
    while (WifiReadLine(readyBuf, sizeof(readyBuf)) == 0) {
    }
    if (readyBuf[0] != '.') {
      _BasicExecute(readyBuf, false);
      return false;
    }
  }

  for (uint16_t ch = startIdx; ch <= 255; ch++) {
    uint8_t bitrows[8];
    if (isGfx) {
      GetGfxDef((uint8_t)ch, bitrows);
    } else {
      GetCharDef((uint8_t)ch, bitrows);
    }
    WifiSendBytes(bitrows, 8);
    uint8_t ack;
    WifiReadBytes(&ack, 1, 5000);
  }

  char lineBuf[128];
  while (WifiReadLine(lineBuf, sizeof(lineBuf)) == 0) {
  }
  _BasicExecute(lineBuf, false);
  return true;
}

// Rewrites line-number references inside a line for RENUMBER. Walks the text as
// a small state machine so it only rewrites numbers in positions that really
// are line references -- after GOTO, THEN, ON lists, RESTORE and TIMER -- and
// leaves ordinary numeric constants alone.
static void RenumberRefsInText(char* buf, int maxLen, const uint16_t* oldNums,
                               const uint16_t* newNums, int count) {
  char out[256];
  int outPos = 0;
  const char* p = buf;

  typedef enum {
    RS_NORMAL,
    RS_GOTO,
    RS_AFTER_LN,
    RS_SKIP_COMMA,
    RS_TIMER_LN
  } RenumState;

  RenumState state = RS_NORMAL;
  int depth = 0;

  while (*p && outPos < (int)sizeof(out) - 8) {
    unsigned char c = (unsigned char)*p;

    if (c == '"') {
      out[outPos++] = c;
      p++;
      while (*p && outPos < (int)sizeof(out) - 8) {
        unsigned char sc = (unsigned char)*p;
        out[outPos++] = sc;
        p++;
        if (sc == '"') {
          break;
        }
      }
      continue;
    }

    if (c == '(') {
      depth++;
      out[outPos++] = c;
      p++;
      continue;
    }
    if (c == ')') {
      if (depth > 0) {
        depth--;
      }
      out[outPos++] = c;
      p++;
      continue;
    }

    if (c == ',' && depth == 0) {
      if (state == RS_AFTER_LN) {
        state = RS_GOTO;
      } else if (state == RS_SKIP_COMMA) {
        state = RS_TIMER_LN;
      }
      out[outPos++] = ',';
      p++;
      continue;
    }

    if (isspace(c)) {
      out[outPos++] = c;
      p++;
      continue;
    }

    if (state == RS_AFTER_LN) {
      state = RS_NORMAL;
    }

    if (isalpha(c)) {
      char word[32];
      int wlen = 0;
      while ((isalnum((unsigned char)*p) || IsTypeSuffixAt(p)) && wlen < 31) {
        word[wlen++] = *p++;
      }
      word[wlen] = '\0';

      if (strcasecmp(word, "GOTO") == 0 || strcasecmp(word, "GOSUB") == 0 ||
          strcasecmp(word, "RESTORE") == 0) {
        state = RS_GOTO;
      } else if (strcasecmp(word, "THEN") == 0) {
        state = RS_GOTO;
      } else if (strcasecmp(word, "TIMER") == 0) {
        state = RS_SKIP_COMMA;
      } else if (strcasecmp(word, "REM") == 0) {
        for (int j = 0; j < wlen && outPos < (int)sizeof(out) - 8; j++) {
          out[outPos++] = word[j];
        }
        while (*p && outPos < (int)sizeof(out) - 8) {
          out[outPos++] = *p++;
        }
        break;
      } else {
        if (state == RS_GOTO) {
          state = RS_NORMAL;
        }
      }

      for (int j = 0; j < wlen && outPos < (int)sizeof(out) - 8; j++) {
        out[outPos++] = word[j];
      }
      continue;
    }

    if (isdigit(c)) {
      unsigned int num = 0;
      while (isdigit((unsigned char)*p)) {
        num = num * 10 + ((unsigned char)*p - '0');
        p++;
      }
      if (num > 65535) {
        num = 65535;
      }

      bool isRef = (depth == 0) && (state == RS_GOTO || state == RS_TIMER_LN);

      if (isRef) {
        unsigned int newNum = num;
        for (int j = 0; j < count; j++) {
          if ((unsigned int)oldNums[j] == num) {
            newNum = newNums[j];
            break;
          }
        }
        outPos += snprintf(&out[outPos], sizeof(out) - outPos, "%u", newNum);
        state = (state == RS_GOTO) ? RS_AFTER_LN : RS_NORMAL;
      } else {
        outPos += snprintf(&out[outPos], sizeof(out) - outPos, "%u", num);
      }
      continue;
    }

    out[outPos++] = c;
    p++;
  }

  out[outPos] = '\0';
  strncpy(buf, out, (size_t)(maxLen - 1));
  buf[maxLen - 1] = '\0';
}

// RENUMBER: reassigns line numbers at a fixed interval and updates every
// reference to them. Refuses the whole operation if any new number would
// overflow, rather than renumbering part of the program.
bool CmdRenumber(const char* args) {
  if (programRunning) {
    PrintError(ERR_ILLEGAL_DIRECT);
    return false;
  }

  args = SkipWhitespace(args);
  if (!*args || !isdigit((unsigned char)*args)) {
    PrintError(ERR_SYNTAX);
    return false;
  }

  float startF;
  const char* p = ParseExpression(args, &startF);
  if (!p) {
    PrintError(ERR_SYNTAX);
    return false;
  }
  int startNum = (int)startF;
  if (startNum < 1 || startNum > 65535) {
    PrintError(ERR_ILLEGAL_LINE_NUMBER);
    return false;
  }

  int step = 5;
  p = SkipWhitespace(p);
  if (*p == ',') {
    p++;
    float stepF;
    p = ParseExpression(p, &stepF);
    if (!p) {
      PrintError(ERR_SYNTAX);
      return false;
    }
    step = (int)stepF;
    if (step < 1) {
      PrintError(ERR_ILLEGAL_QUANTITY);
      return false;
    }
  }

  if (programLineCount == 0) {
    return true;
  }

  int n = programLineCount;

  uint16_t* oldNums = (uint16_t*)malloc((size_t)n * sizeof(uint16_t));
  uint16_t* newNums = (uint16_t*)malloc((size_t)n * sizeof(uint16_t));
  if (!oldNums || !newNums) {
    free(oldNums);
    free(newNums);
    PrintError(ERR_OUT_OF_MEMORY);
    return false;
  }

  bool overflow = false;
  for (int i = 0; i < n; i++) {
    oldNums[i] = program[i].lineNum;
    int32_t newNum = (int32_t)startNum + (int32_t)i * step;
    if (newNum > 65535) {
      overflow = true;
      break;
    }
    newNums[i] = (uint16_t)newNum;
  }

  if (overflow) {
    free(oldNums);
    free(newNums);
    PrintStr("?RENUMBERING OVERFLOW");
    Newline();
    return false;
  }

  uint8_t newToks[MAX_LINE_TOKENS];

  for (int i = 0; i < n; i++) {
    char lineBuf[256];
    DetokenizeLine(GetLineTokens(i), program[i].tokenLen, lineBuf,
                   sizeof(lineBuf));

    RenumberRefsInText(lineBuf, sizeof(lineBuf), oldNums, newNums, n);

    int newLen = TokenizeLine(lineBuf, newToks, MAX_LINE_TOKENS);
    if (newLen <= 0) {
      newLen = 1;
      newToks[0] = TOK_EOL;
    }

    uint16_t old_off = program[i].offset;
    uint8_t old_len = program[i].tokenLen;
    if (old_len > 0) {
      memmove(tokenPool + old_off, tokenPool + old_off + old_len,
              tokenPoolUsed - old_off - old_len);
      tokenPoolUsed -= old_len;
      heapBytesUsed -= old_len;
      for (int k = 0; k < n; k++) {
        if (program[k].offset > old_off) {
          program[k].offset -= old_len;
        }
      }
    }

    if (!EnsureTokenPoolCapacity(tokenPoolUsed + newLen)) {
      free(oldNums);
      free(newNums);
      PrintError(ERR_OUT_OF_MEMORY);
      return false;
    }
    memcpy(tokenPool + tokenPoolUsed, newToks, (size_t)newLen);
    heapBytesUsed += newLen;
    program[i].offset = tokenPoolUsed;
    program[i].tokenLen = (uint8_t)newLen;
    tokenPoolUsed += (uint16_t)newLen;
  }

  for (int i = 0; i < n; i++) {
    program[i].lineNum = newNums[i];
  }

  free(oldNums);
  free(newNums);
  return true;
}
