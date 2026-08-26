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

const KeywordEntry keywords[] = {{"PRINT", TOK_KW_PRINT},
                                 {"LET", TOK_KW_LET},
                                 {"LOCATE", TOK_KW_LOCATE},
                                 {"LINE", TOK_KW_LINE},
                                 {"BEEP", TOK_KW_BEEP},
                                 {"CLS", TOK_KW_CLS},
                                 {"GOTO", TOK_KW_GOTO},
                                 {"LIST", TOK_KW_LIST},
                                 {"RUN", TOK_KW_RUN},
                                 {"NEW", TOK_KW_NEW},
                                 {"END", TOK_KW_END},
                                 {"REM", TOK_KW_REM},
                                 {"FOR", TOK_KW_FOR},
                                 {"TO", TOK_KW_TO},
                                 {"STEP", TOK_KW_STEP},
                                 {"NEXT", TOK_KW_NEXT},
                                 {"IF", TOK_KW_IF},
                                 {"THEN", TOK_KW_THEN},
                                 {"INPUT", TOK_KW_INPUT},
                                 {"GET", TOK_KW_GET},
                                 {"CLR", TOK_KW_CLR},
                                 {"GOSUB", TOK_KW_GOSUB},
                                 {"RETURN", TOK_KW_RETURN},
                                 {"DATA", TOK_KW_DATA},
                                 {"READ", TOK_KW_READ},
                                 {"RESTORE", TOK_KW_RESTORE},
                                 {"DEFCHAR", TOK_KW_DEFCHAR},
                                 {"RESETCHAR", TOK_KW_RESETCHAR},
                                 {"DEFGFX", TOK_KW_DEFGFX},
                                 {"RESETGFX", TOK_KW_RESETGFX},
                                 {"CHARMODE", TOK_KW_CHARMODE},
                                 {"SOUND", TOK_KW_TONEON},
                                 {"SHUSH", TOK_KW_TONEOFF},
                                 {"SLEEP", TOK_KW_SLEEP},
                                 {"AND", TOK_KW_AND},
                                 {"OR", TOK_KW_OR},
                                 {"XOR", TOK_KW_XOR},
                                 {"NOT", TOK_KW_NOT},
                                 {"DIM", TOK_KW_DIM},
                                 {"SOUNDPGM", TOK_KW_SOUNDPGM},
                                 {"REVERSE", TOK_KW_REVERSE},
                                 {"NORMAL", TOK_KW_NORMAL},
                                 {"REBOOT", TOK_KW_REBOOT},
                                 {"CAT", TOK_KW_CATALOG},
                                 {"CATALOG", TOK_KW_CATALOG},
                                 {"CONT", TOK_KW_CONT},
                                 {"PLOTCHAR", TOK_KW_PLOTCHAR},
                                 {"FILLCELLS", TOK_KW_FILLCELLS},
                                 {"HLINE", TOK_KW_HLINE},
                                 {"VLINE", TOK_KW_VLINE},
                                 {"WAITMS", TOK_KW_WAITMS},
                                 {"READMAT", TOK_KW_READMAT},
                                 {"ON", TOK_KW_ON},
                                 {"DEF", TOK_KW_DEF},
                                 {"TIMER", TOK_KW_TIMER},
                                 {"MOVEBLOCK", TOK_KW_MOVEBLOCK},
                                 {"PPLOT", TOK_KW_PPLOT},
                                 {"SCROLL", TOK_KW_SCROLL},
                                 {"SOUNDPWM", TOK_KW_SOUNDPWM},
                                 {"FILLBLOCK", TOK_KW_FILLBLOCK},
                                 {"PLINE", TOK_KW_PLINE},
                                 {"PCIRCLE", TOK_KW_PCIRCLE},
                                 {"PFILL", TOK_KW_PFILL},
                                 {"LOADC", TOK_KW_LOADCHAR},
                                 {"SAVEC", TOK_KW_SAVECHAR},
                                 {"COPYCHAR", TOK_KW_COPYCHAR},
                                 {"PLAY", TOK_KW_PLAY},
                                 {"SOUNDPRT", TOK_KW_SOUNDPRT},
                                 {"SCROLLX", TOK_KW_SCROLLX},
                                 {"NETGET", TOK_KW_NETGET},
                                 {"NETINPUT", TOK_KW_NETINPUT},
                                 {"NETPRINT", TOK_KW_NETPRINT},
                                 {"WHILE", TOK_KW_WHILE},
                                 {"WEND", TOK_KW_WEND},
                                 {"DO", TOK_KW_DO},
                                 {"UNTIL", TOK_KW_UNTIL},
                                 {"EXIT", TOK_KW_EXIT},
                                 {"VERSION", TOK_KW_VERSION},
                                 {"CHUNK", TOK_KW_CHUNK},
                                 {"SETTIME", TOK_KW_SETTIME},
                                 {"SETDATE", TOK_KW_SETDATE},
                                 {"PPOLY", TOK_KW_PPOLY},
                                 {"BOX", TOK_KW_BOX},
                                 {"SETATTRIB", TOK_KW_SETATTRIB},
                                 {"CLEARARR", TOK_KW_CLEARARR},
                                 {"SWAPARR", TOK_KW_SWAPARR},
                                 {"RENUMBER", TOK_KW_RENUMBER},
                                 {"LOAD", TOK_KW_LOAD},
                                 {"SAVE", TOK_KW_SAVE},
                                 {"WIFI", TOK_KW_WIFI},
                                 {"NETCONNECT", TOK_KW_NETCONNECT},
                                 {"NETDISCONNECT", TOK_KW_NETDISCONNECT},
                                 {"FOPEN", TOK_KW_FOPEN},
                                 {"FCLOSE", TOK_KW_FCLOSE},
                                 {"FPRINT", TOK_KW_FPRINT},
                                 {"FINPUT", TOK_KW_FINPUT},
                                 {"FGET", TOK_KW_FGET},
                                 {"FPUT", TOK_KW_FPUT},
                                 {"FSEEK", TOK_KW_FSEEK},
                                 {"TRAP", TOK_KW_TRAP},
                                 {"RESUME", TOK_KW_RESUME},
                                 {"MORE", TOK_KW_MORE},
                                 {"DEG", TOK_KW_DEG},
                                 {"RAD", TOK_KW_RAD},
                                 {"DEL", TOK_KW_DEL},
                                 {"DELETE", TOK_KW_DEL},
                                 {"REN", TOK_KW_REN},
                                 {"RENAME", TOK_KW_REN},
                                 {"COPY", TOK_KW_COPY},
                                 {"CHDIR", TOK_KW_CHDIR},
                                 {"MKDIR", TOK_KW_MKDIR},
                                 {NULL, 0}};

// Looks up a keyword's token byte, or 0 if the text is not a keyword.
static uint8_t FindKeywordToken(const char* str, int len) {
  for (int i = 0; keywords[i].keyword != NULL; i++) {
    if (strncasecmp(str, keywords[i].keyword, len) == 0 &&
        strlen(keywords[i].keyword) == (size_t)len) {
      return keywords[i].token;
    }
  }
  return 0;
}

// Reverse lookup used when detokenising a line back to readable text.
static const char* FindKeywordString(uint8_t token) {
  for (int i = 0; keywords[i].keyword != NULL; i++) {
    if (keywords[i].token == token) {
      return keywords[i].keyword;
    }
  }
  return NULL;
}

// Searches the string pool for an identical literal. Entries are
// length-prefixed, so the walk steps entry to entry.
int FindStringInPool(const char* str, int len) {
  uint16_t offset = 0;
  while (offset < stringPoolTop) {
    uint8_t storedLen = (uint8_t)stringPool[offset];
    if (storedLen == len && memcmp(&stringPool[offset + 1], str, len) == 0) {
      return offset;
    }
    offset += 1 + storedLen;
  }
  return -1;
}

// Interns a string literal, returning the offset of an existing copy when there
// is one. Sharing duplicates matters because the pool is fixed size.
int AddStringToPool(const char* str, int len) {
  int existing = FindStringInPool(str, len);
  if (existing >= 0) {
    return existing;
  }

  if (stringPoolTop + 1 + len > STRING_POOL_SIZE) {
    return -1;
  }

  int offset = stringPoolTop;
  stringPool[stringPoolTop++] = (uint8_t)len;
  memcpy(&stringPool[stringPoolTop], str, len);
  stringPoolTop += len;
  return offset;
}

// Retrieves an interned literal and its length by offset.
const char* GetStringFromPool(uint16_t offset, uint8_t* outLen) {
  if (offset >= stringPoolTop) {
    return NULL;
  }
  *outLen = (uint8_t)stringPool[offset];
  return &stringPool[offset + 1];
}

// Looks up a variable name in the intern table, case-insensitively.
int FindVarNameInTable(const char* name, int len) {
  for (int i = 0; i < varNameCount; i++) {
    if (strncasecmp(varNameTable[i].name, name, len) == 0 &&
        varNameTable[i].name[len] == '\0') {
      return i;
    }
  }
  return -1;
}

// Interns a variable name and returns its index. Tokenised lines store the
// one-byte index rather than the text, which is what keeps programs compact.
// Names are folded to upper case so they compare case-insensitively.
int AddVarNameToTable(const char* name, int len) {
  int existing = FindVarNameInTable(name, len);
  if (existing >= 0) {
    return existing;
  }

  if (varNameCount >= MAX_VAR_NAMES) {
    return -1;
  }

  int index = varNameCount++;
  int copyLen = (len < MAX_VAR_NAME_LEN - 1) ? len : MAX_VAR_NAME_LEN - 1;
  for (int i = 0; i < copyLen; i++) {
    varNameTable[index].name[i] = toupper(name[i]);
  }
  varNameTable[index].name[copyLen] = '\0';
  return index;
}

// Recovers a variable's name from its interned index.
const char* GetVarNameFromTable(uint8_t index) {
  if (index >= varNameCount) {
    return NULL;
  }
  return varNameTable[index].name;
}

// Pointer to a line's tokens inside the shared pool.
uint8_t* GetLineTokens(int lineIndex) {
  if (lineIndex < 0 || lineIndex >= programLineCount) {
    return NULL;
  }
  return tokenPool + program[lineIndex].offset;
}

// Compresses a source line into tokens: keywords become single bytes, and
// literals and variable names become indexes into their pools. This is what
// lets a useful program fit in the available RAM.
int TokenizeLine(const char* line, uint8_t* tokens, int maxTokens) {
  int pos = 0;
  const char* p = line;

  while (*p && pos < maxTokens - 1) {
    while (*p && isspace(*p)) {
      p++;
    }
    if (!*p) {
      break;
    }

    if (isalpha(*p)) {
      const char* start = p;
      while (isalnum(*p)) {
        p++;
      }
      int len = p - start;

      uint8_t kwToken = FindKeywordToken(start, len);
      if (kwToken && !IsTypeSuffixAt(p)) {
        tokens[pos++] = kwToken;

        if (kwToken == TOK_KW_REM) {
          while (*p && isspace(*p)) {
            p++;
          }
          int remLen = strlen(p);
          int strOffset = AddStringToPool(p, remLen);
          if (strOffset < 0) {
            PrintError(ERR_OUT_OF_MEMORY);
            return -1;
          }
          if (pos + 3 >= maxTokens) {
            PrintError(ERR_LINE_TOO_LONG);
            return -1;
          }
          tokens[pos++] = TOK_STRING_REF;
          tokens[pos++] = (strOffset >> 8) & 0xFF;
          tokens[pos++] = strOffset & 0xFF;
          p += remLen;
          break;
        }
      } else {
        if (IsTypeSuffixAt(p)) {
          p++;
        }
        int varLen = p - start;
        int varIndex = AddVarNameToTable(start, varLen);
        if (varIndex < 0 || varIndex >= MAX_VAR_NAMES) {
          PrintError(ERR_OUT_OF_MEMORY);
          return -1;
        }
        if (pos + 2 >= maxTokens) {
          PrintError(ERR_LINE_TOO_LONG);
          return -1;
        }
        tokens[pos++] = (uint8_t)TOK_VAR_EXT;
        tokens[pos++] = (uint8_t)varIndex;
      }
    } else if (*p == '"') {
      p++;
      const char* start = p;
      while (*p && *p != '"') {
        p++;
      }
      int len = p - start;
      if (*p == '"') {
        p++;
      }

      int strOffset = AddStringToPool(start, len);
      if (strOffset < 0) {
        PrintError(ERR_OUT_OF_MEMORY);
        return -1;
      }
      if (pos + 3 >= maxTokens) {
        PrintError(ERR_LINE_TOO_LONG);
        return -1;
      }
      tokens[pos++] = TOK_STRING_REF;
      tokens[pos++] = (strOffset >> 8) & 0xFF;
      tokens[pos++] = strOffset & 0xFF;
    } else if (isdigit(*p) || (*p == '-' && isdigit(*(p + 1))) ||
               (*p == '.' && isdigit(*(p + 1)))) {
      char* end;
      float val = strtof(p, &end);
      p = end;

      if (val == (float)(int)val && val >= 0 && val <= 11) {
        if (pos + 1 >= maxTokens) {
          PrintError(ERR_LINE_TOO_LONG);
          return -1;
        }
        tokens[pos++] = (uint8_t)(TOK_SMALL_INT_BASE + (int)val);
      } else if (val == (int)val && val >= -32768 && val <= 32767) {
        if (pos + 3 >= maxTokens) {
          PrintError(ERR_LINE_TOO_LONG);
          return -1;
        }
        tokens[pos++] = TOK_NUMBER_INT;
        int16_t ival = (int16_t)val;
        tokens[pos++] = (ival >> 8) & 0xFF;
        tokens[pos++] = ival & 0xFF;
      } else {
        if (pos + 5 >= maxTokens) {
          PrintError(ERR_LINE_TOO_LONG);
          return -1;
        }
        tokens[pos++] = TOK_NUMBER_FLOAT;
        uint8_t* fp = (uint8_t*)&val;
        for (int i = 0; i < 4; i++) {
          tokens[pos++] = fp[i];
        }
      }
    } else if (strchr("+-*/^%()=,;:<>.", *p)) {
      tokens[pos++] = *p++;
    } else {
      p++;
    }
  }

  if (*p) {
    PrintError(ERR_LINE_TOO_LONG);
    return -1;
  }

  tokens[pos++] = TOK_EOL;
  return pos;
}

// Expands a tokenised line back to source text for LIST and for the statements
// that re-read their own source, such as DATA.
int DetokenizeLine(const uint8_t* tokens, int tokenLen, char* out, int maxOut) {
  int outPos = 0;
  int i = 0;
  bool afterRem = false;

  while (i < tokenLen && tokens[i] != TOK_EOL && outPos < maxOut - 1) {
    uint8_t tok = tokens[i++];

    if (tok >= (uint8_t)TOK_KW_PRINT && tok <= 235) {
      const char* kw = FindKeywordString(tok);
      afterRem = (tok == (uint8_t)TOK_KW_REM);
      if (kw) {
        int len = strlen(kw);
        if (outPos > 0 && out[outPos - 1] != ' ') {
          if (outPos + len + 2 >= maxOut) {
            break;
          }
          out[outPos++] = ' ';
        } else {
          if (outPos + len + 1 >= maxOut) {
            break;
          }
        }
        strcpy(&out[outPos], kw);
        outPos += len;
        out[outPos++] = ' ';
      }
    } else if (tok == (uint8_t)TOK_VAR_EXT) {
      if (i >= tokenLen) {
        break;
      }
      uint8_t varIndex = tokens[i++];
      const char* varName = GetVarNameFromTable(varIndex);
      if (varName) {
        int len = strlen(varName);
        if (outPos > 0 && isalnum((unsigned char)out[outPos - 1])) {
          if (outPos + len + 1 < maxOut) {
            out[outPos++] = ' ';
          }
        }
        if (outPos + len < maxOut) {
          memcpy(&out[outPos], varName, len);
          outPos += len;
        }
      }
    } else if (tok == TOK_STRING_LIT) {
      if (i >= tokenLen) {
        break;
      }
      int len = tokens[i++];
      if (outPos + len + 3 >= maxOut) {
        break;
      }
      out[outPos++] = '"';
      memcpy(&out[outPos], &tokens[i], len);
      outPos += len;
      i += len;
      out[outPos++] = '"';
    } else if (tok == TOK_NUMBER_INT) {
      if (i + 1 >= tokenLen) {
        break;
      }
      int16_t val = (int16_t)((tokens[i] << 8) | tokens[i + 1]);
      i += 2;
      if (outPos > 0 && isalnum((unsigned char)out[outPos - 1]) &&
          outPos + 1 < maxOut) {
        out[outPos++] = ' ';
      }
      outPos += snprintf(&out[outPos], maxOut - outPos, "%d", val);
    } else if (tok == TOK_NUMBER_FLOAT) {
      if (i + 3 >= tokenLen) {
        break;
      }
      float val;
      memcpy(&val, &tokens[i], 4);
      i += 4;
      if (outPos > 0 && isalnum((unsigned char)out[outPos - 1]) &&
          outPos + 1 < maxOut) {
        out[outPos++] = ' ';
      }
      outPos += snprintf(&out[outPos], maxOut - outPos, "%.6g", val);
    } else if (tok == TOK_VARNAME) {
      if (i >= tokenLen) {
        break;
      }
      int len = tokens[i++];
      if (outPos + len >= maxOut) {
        break;
      }
      if (outPos > 0 && isalnum((unsigned char)out[outPos - 1]) &&
          outPos + len + 1 < maxOut) {
        out[outPos++] = ' ';
      }
      memcpy(&out[outPos], &tokens[i], len);
      outPos += len;
      i += len;
    } else if (tok == TOK_LINENUM) {
      if (i + 1 >= tokenLen) {
        break;
      }
      uint16_t lineNum = (uint16_t)((tokens[i] << 8) | tokens[i + 1]);
      i += 2;
      if (outPos > 0 && isalnum((unsigned char)out[outPos - 1]) &&
          outPos + 1 < maxOut) {
        out[outPos++] = ' ';
      }
      outPos += snprintf(&out[outPos], maxOut - outPos, "%u", lineNum);
    } else if (tok == TOK_STRING_REF) {
      if (i + 1 >= tokenLen) {
        break;
      }
      uint16_t offset = (uint16_t)((tokens[i] << 8) | tokens[i + 1]);
      i += 2;
      uint8_t strLen;
      const char* str = GetStringFromPool(offset, &strLen);
      if (afterRem) {
        if (str && outPos + strLen + 1 < maxOut) {
          memcpy(&out[outPos], str, strLen);
          outPos += strLen;
        }
        afterRem = false;
      } else {
        if (str && outPos + strLen + 3 < maxOut) {
          out[outPos++] = '"';
          memcpy(&out[outPos], str, strLen);
          outPos += strLen;
          out[outPos++] = '"';
        }
      }
    } else if (tok >= (uint8_t)TOK_SMALL_INT_BASE &&
               tok <= (uint8_t)TOK_SMALL_INT_MAX) {
      if (outPos > 0 && isalnum((unsigned char)out[outPos - 1]) &&
          outPos + 1 < maxOut) {
        out[outPos++] = ' ';
      }
      outPos += snprintf(&out[outPos], maxOut - outPos, "%d",
                         tok - (uint8_t)TOK_SMALL_INT_BASE);
    } else if (tok < 128) {
      if (tok == ':' && outPos > 0 && out[outPos - 1] == ' ') {
        outPos--;
      }
      out[outPos++] = tok;
    }
  }

  out[outPos] = '\0';
  return outPos;
}

// Binary-searches for a line by number. Lines are kept sorted, so lookup during
// GOTO and GOSUB stays fast as programs grow.
int FindProgramLine(uint16_t lineNum) {
  if (programLineCount == 0) {
    return -1;
  }

  int low = 0;
  int high = programLineCount - 1;

  while (low <= high) {
    int mid = low + ((high - low) >> 1);
    uint16_t midLine = program[mid].lineNum;

    if (midLine == lineNum) {
      return mid;
    } else if (midLine < lineNum) {
      low = mid + 1;
    } else {
      high = mid - 1;
    }
  }
  return -1;
}

// Binary-searches for where a new line number belongs, keeping the program in
// ascending order.
static int FindInsertionPoint(uint16_t lineNum) {
  if (programLineCount == 0) {
    return 0;
  }

  int low = 0;
  int high = programLineCount;

  while (low < high) {
    int mid = low + ((high - low) >> 1);
    if (program[mid].lineNum < lineNum) {
      low = mid + 1;
    } else {
      high = mid;
    }
  }
  return low;
}

// Removes a line's tokens from the pool and closes the gap, fixing up every
// later line's offset. Compacting keeps the fixed-size pool from fragmenting.
static void RemoveFromPool(int pos) {
  uint16_t old_off = program[pos].offset;
  uint8_t old_len = program[pos].tokenLen;
  if (old_len == 0) {
    return;
  }
  memmove(tokenPool + old_off, tokenPool + old_off + old_len,
          tokenPoolUsed - old_off - old_len);
  tokenPoolUsed -= old_len;
  heapBytesUsed -= old_len;
  for (int k = 0; k < programLineCount; k++) {
    if (program[k].offset > old_off) {
      program[k].offset -= old_len;
    }
  }
}

// Inserts, replaces, or deletes a program line. Entering a bare line number
// tokenises to nothing and deletes the line, which is how the editor removes
// lines.
bool InsertProgramLine(uint16_t lineNum, const uint8_t* tokens, int tokenLen) {
  int pos = FindInsertionPoint(lineNum);

  if (pos < programLineCount && program[pos].lineNum == lineNum) {
    RemoveFromPool(pos);

    if (tokenLen == 1 && tokens[0] == TOK_EOL) {
      if (pos < programLineCount - 1) {
        memmove(&program[pos], &program[pos + 1],
                (programLineCount - pos - 1) * sizeof(ProgramLine));
      }
      programLineCount--;
      return true;
    }

    if (tokenPoolUsed + tokenLen > 65535) {
      return false;
    }
    if (!EnsureTokenPoolCapacity(tokenPoolUsed + tokenLen)) {
      return false;
    }
    uint16_t new_off = tokenPoolUsed;
    memcpy(tokenPool + new_off, tokens, tokenLen);
    tokenPoolUsed += (uint16_t)tokenLen;
    heapBytesUsed += tokenLen;
    program[pos].offset = new_off;
    program[pos].tokenLen = (uint8_t)tokenLen;
    return true;
  }

  if (tokenLen == 1 && tokens[0] == TOK_EOL) {
    return true;
  }

  if (tokenPoolUsed + tokenLen > 65535) {
    return false;
  }
  if (!EnsureCapacity((void**)&program, &programCapacity, programLineCount + 1,
                      sizeof(ProgramLine), 32, true)) {
    return false;
  }
  if (!EnsureTokenPoolCapacity(tokenPoolUsed + tokenLen)) {
    return false;
  }

  uint16_t new_off = tokenPoolUsed;
  memcpy(tokenPool + new_off, tokens, tokenLen);
  tokenPoolUsed += (uint16_t)tokenLen;
  heapBytesUsed += tokenLen;

  if (pos < programLineCount) {
    memmove(&program[pos + 1], &program[pos],
            (programLineCount - pos) * sizeof(ProgramLine));
  }
  program[pos].lineNum = lineNum;
  program[pos].offset = new_off;
  program[pos].tokenLen = (uint8_t)tokenLen;
  programLineCount++;
  return true;
}

// Discards the program and both intern pools, used by NEW.
void ClearProgram(void) {
  if (tokenPool) {
    heapBytesUsed -= tokenPoolCap;
    free(tokenPool);
    tokenPool = NULL;
  }
  tokenPoolUsed = 0;
  tokenPoolCap = 0;
  if (program) {
    heapBytesUsed -= programCapacity * sizeof(ProgramLine);
    free(program);
    program = NULL;
  }
  programLineCount = 0;
  programCapacity = 0;
  stringPoolTop = 0;
  varNameCount = 0;
  currentExecLine = -1;
  continueLineIndex = -1;
  programRunning = false;
}
