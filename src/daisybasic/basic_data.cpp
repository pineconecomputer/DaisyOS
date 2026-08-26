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

// Advances the READ cursor to the next line starting with DATA, skipping
// everything else. Lines are detokenised to check, since DATA content is kept
// as source text.
static bool FindNextDataLine(void) {
  while (dataLineIndex < programLineCount) {
    char lineBuf[256];
    DetokenizeLine(GetLineTokens(dataLineIndex),
                   program[dataLineIndex].tokenLen, lineBuf, sizeof(lineBuf));
    const char* p = SkipWhitespace(lineBuf);
    if (strncasecmp(p, "DATA", 4) == 0 && (p[4] == ' ' || p[4] == '\0')) {
      return true;
    }
    dataLineIndex++;
    dataItemIndex = 0;
  }
  return false;
}

// Pulls the next value from the DATA stream, moving to the following DATA line
// when the current one runs out. Quoted items may contain commas, so the walk
// tracks quoting rather than splitting naively.
static const char* GetNextDataItem(char* itemBuf, size_t bufLen) {
  if (!FindNextDataLine()) {
    return NULL;
  }

  char lineBuf[256];
  DetokenizeLine(GetLineTokens(dataLineIndex), program[dataLineIndex].tokenLen,
                 lineBuf, sizeof(lineBuf));

  const char* p = SkipWhitespace(lineBuf);
  p += 4;
  p = SkipWhitespace(p);

  int itemNum = 0;
  while (itemNum < dataItemIndex && *p) {
    if (*p == '"') {
      p++;
      while (*p && *p != '"') {
        p++;
      }
      if (*p == '"') {
        p++;
      }
    } else {
      while (*p && *p != ',') {
        p++;
      }
    }
    if (*p == ',') {
      p++;
    }
    p = SkipWhitespace(p);
    itemNum++;
  }

  if (*p == '\0') {
    dataLineIndex++;
    dataItemIndex = 0;
    if (!FindNextDataLine()) {
      return NULL;
    }
    return GetNextDataItem(itemBuf, bufLen);
  }

  size_t len = 0;
  if (*p == '"') {
    p++;
    while (*p && *p != '"' && len < bufLen - 1) {
      itemBuf[len++] = *p++;
    }
    if (*p == '"') {
      p++;
    }
  } else {
    while (*p && *p != ',' && len < bufLen - 1) {
      itemBuf[len++] = *p++;
    }
    while (len > 0 && itemBuf[len - 1] == ' ') {
      len--;
    }
  }
  itemBuf[len] = '\0';
  dataItemIndex++;
  return itemBuf;
}

// DATA declares constants and does nothing when executed; READ consumes them.
bool CmdData(const char* args) {
  (void)args;
  return true;
}

// READ: assigns successive DATA values into the listed variables or array
// elements, converting to each target's type.
bool CmdRead(const char* args) {
  char varName[MAX_VAR_NAME];
  char itemBuf[MAX_STR_EXPR_BUF];

  args = SkipWhitespace(args);

  while (*args) {
    const char* next = ParseVarName(args, varName, sizeof(varName));
    if (!next) {
      return false;
    }

    bool isArrayAccess = false;
    int idx1 = 0, idx2 = 0;
    bool has2ndIndex = false;
    next = SkipWhitespace(next);

    if (*next == '(') {
      next++;
      float idx1F;
      const char* afterIndex = ParseExpression(next, &idx1F);
      if (!afterIndex) {
        return false;
      }
      afterIndex = SkipWhitespace(afterIndex);
      isArrayAccess = true;
      idx1 = (int)idx1F;

      if (*afterIndex == ',') {
        afterIndex++;
        float idx2F;
        const char* afterIdx2 = ParseExpression(afterIndex, &idx2F);
        if (!afterIdx2) {
          return false;
        }
        afterIndex = SkipWhitespace(afterIdx2);
        idx2 = (int)idx2F;
        has2ndIndex = true;
      }

      if (*afterIndex != ')') {
        return false;
      }
      afterIndex++;
      next = afterIndex;
    }

    if (!GetNextDataItem(itemBuf, sizeof(itemBuf))) {
      PrintError(ERR_OUT_OF_DATA);
      return false;
    }

    if (IsStringVar(varName) || IsStringArrayVar(varName)) {
      if (isArrayAccess) {
        ArrayDescriptor* arr = FindArray(varName);
        if (!arr) {
          PrintError(ERR_ARRAY_NOT_DIMD);
          return false;
        }
        int linearIdx = CalculateArrayIndex(arr, idx1, idx2, has2ndIndex);
        if (linearIdx == -2) {
          PrintError(ERR_WRONG_DIMENSIONS);
          return false;
        }
        if (linearIdx < 0) {
          PrintError(ERR_BAD_SUBSCRIPT);
          return false;
        }
        char* ptr = (char*)GetArrayElementPtr(arr, linearIdx);
        strncpy(ptr, itemBuf, STRING_ELEMENT_LEN - 1);
        ptr[STRING_ELEMENT_LEN - 1] = '\0';
      } else {
        Variable* v = CreateVariable(varName, VAR_STRING);
        if (!v) {
          return false;
        }
        if (!SetStringVar(v, itemBuf)) {
          return false;
        }
      }
    } else {
      char* end;
      float val = strtof(itemBuf, &end);
      if (end == itemBuf) {
        return false;
      }

      if (isArrayAccess) {
        ArrayDescriptor* arr = FindArray(varName);
        if (!arr) {
          PrintError(ERR_ARRAY_NOT_DIMD);
          return false;
        }
        int linearIdx = CalculateArrayIndex(arr, idx1, idx2, has2ndIndex);
        if (linearIdx == -2) {
          PrintError(ERR_WRONG_DIMENSIONS);
          return false;
        }
        if (linearIdx < 0) {
          PrintError(ERR_BAD_SUBSCRIPT);
          return false;
        }
        void* ptr = GetArrayElementPtr(arr, linearIdx);
        if (arr->type == ARRAY_TYPE_INT) {
          *(int16_t*)ptr = (int16_t)val;
        } else {
          *(float*)ptr = val;
        }
      } else {
        if (val == (int)val && val >= -32768 && val <= 32767) {
          Variable* v = CreateVariable(varName, VAR_INT);
          if (!v) {
            return false;
          }
          v->intVal = (int)val;
        } else {
          Variable* v = CreateVariable(varName, VAR_FLOAT);
          if (!v) {
            return false;
          }
          v->floatVal = val;
        }
      }
    }

    args = SkipWhitespace(next);
    if (*args == ',') {
      args++;
      args = SkipWhitespace(args);
    }
  }
  return true;
}

// READMAT: fills an entire array from DATA in row-major order, reading exactly
// as many values as the array holds.
bool CmdReadMat(const char* args) {
  char varName[MAX_VAR_NAME];
  char itemBuf[MAX_STR_EXPR_BUF];

  args = SkipWhitespace(args);
  const char* next = ParseVarName(args, varName, sizeof(varName));
  if (!next) {
    PrintError(ERR_SYNTAX);
    return false;
  }

  ArrayDescriptor* arr = FindArray(varName);
  if (!arr) {
    PrintError(ERR_ARRAY_NOT_DIMD);
    return false;
  }

  uint32_t totalElements = (arr->dim2Size == 0)
                               ? (uint32_t)arr->dim1Size
                               : (uint32_t)arr->dim1Size * arr->dim2Size;

  for (uint32_t idx = 0; idx < totalElements; idx++) {
    if (!GetNextDataItem(itemBuf, sizeof(itemBuf))) {
      PrintError(ERR_OUT_OF_DATA);
      return false;
    }
    void* ptr = GetArrayElementPtr(arr, (int)idx);
    if (arr->type == ARRAY_TYPE_STRING) {
      strncpy((char*)ptr, itemBuf, STRING_ELEMENT_LEN - 1);
      ((char*)ptr)[STRING_ELEMENT_LEN - 1] = '\0';
    } else {
      char* endptr;
      float val = strtof(itemBuf, &endptr);
      if (endptr == itemBuf) {
        val = 0.0f;
      }
      if (arr->type == ARRAY_TYPE_INT) {
        *(int16_t*)ptr = (int16_t)(int)val;
      } else {
        *(float*)ptr = val;
      }
    }
  }
  return true;
}

// RESTORE: rewinds the DATA cursor to the start, or to the first DATA line at
// or after a given line number so a program can select a block.
bool CmdRestore(const char* args) {
  args = SkipWhitespace(args);
  if (*args == '\0' || *args == ':') {
    dataLineIndex = 0;
    dataItemIndex = 0;
    return true;
  }
  float lineNumF;
  if (!ParseExpression(args, &lineNumF)) {
    PrintError(ERR_SYNTAX);
    return false;
  }
  int target = (int)lineNumF;
  if (target < 1 || target > 65535) {
    PrintError(ERR_ILLEGAL_LINE_NUMBER);
    return false;
  }
  int idx = 0;
  while (idx < programLineCount && program[idx].lineNum < (uint16_t)target) {
    idx++;
  }
  if (idx >= programLineCount) {
    PrintError(ERR_UNDEF_STATEMENT);
    return false;
  }
  dataLineIndex = idx;
  dataItemIndex = 0;
  return true;
}
