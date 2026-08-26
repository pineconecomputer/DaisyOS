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

// Reads a line from the keyboard, echoing as it goes. Blocks, but keeps the
// cursor blinking and honours Ctrl-C, which returns false to abort the caller.
bool BasicReadLine(char* buf, size_t maxLen) {
  size_t pos = 0;
  buf[0] = '\0';

  while (pos < maxLen - 1) {
    CursorHandler();
    uint8_t keys[16];
    size_t keyLen = BufferDrain(keys, sizeof(keys));
    if (keyLen > 0) {
      ClearAttribute();
      for (size_t i = 0; i < keyLen && pos < maxLen - 1; i++) {
        uint8_t key = keys[i];
        if (key == CTRL_C_KEY) {
          buf[pos] = '\0';
          return false;
        } else if (key == RETURN_KEY) {
          buf[pos] = '\0';
          return true;
        } else if (key == BS_KEY || key == 8) {
          if (pos > 0) {
            pos--;
            MoveCursor(kCursorLeft);
            Chrout(' ');
            MoveCursor(kCursorLeft);
          }
        } else if (key == CTRL_I) {
          uint8_t spaces = 4 - (GetCursorX() % 4);
          for (uint8_t s = 0; s < spaces && pos < maxLen - 1; s++) {
            buf[pos++] = ' ';
            Chrout(' ');
          }
        } else if (key >= 32 && key < 127) {
          buf[pos++] = (char)key;
          Chrout(key);
        }
      }
    }
  }
  buf[pos] = '\0';
  return true;
}

// Takes the next keystroke without waiting, or 0 if none is pending.
uint8_t BasicGetChar(void) { return BufferGet(); }

// LET, and bare assignment: evaluates the right-hand side and stores it into a
// scalar or array element of the matching type. Also handles the comma-separated
// bulk form that fills successive array elements.
bool CmdLet(const char* args) {
  char varName[MAX_VAR_NAME];
  char strVal[MAX_STR_EXPR_BUF];

  args = ParseVarName(args, varName, sizeof(varName));
  if (!args) {
    return false;
  }

  args = SkipWhitespace(args);

  if (*args == '(') {
    args++;

    float idx1F;
    const char* afterIndex = ParseExpression(args, &idx1F);
    if (!afterIndex) {
      return false;
    }
    afterIndex = SkipWhitespace(afterIndex);

    int idx1 = (int)idx1F;
    int idx2 = 0;
    bool has2ndIndex = false;

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

    afterIndex = SkipWhitespace(afterIndex);
    if (*afterIndex != '=') {
      return false;
    }
    afterIndex++;
    afterIndex = SkipWhitespace(afterIndex);

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

    uint32_t totalElements = (arr->dim2Size == 0)
                                 ? (uint32_t)arr->dim1Size
                                 : (uint32_t)arr->dim1Size * arr->dim2Size;

    while (true) {
      if ((uint32_t)linearIdx >= totalElements) {
        PrintError(ERR_BAD_SUBSCRIPT);
        return false;
      }
      void* ptr = GetArrayElementPtr(arr, linearIdx);

      if (arr->type == ARRAY_TYPE_STRING) {
        const char* afterStr =
            ParseStringExpression(afterIndex, strVal, sizeof(strVal));
        if (!afterStr) {
          return false;
        }
        strncpy((char*)ptr, strVal, STRING_ELEMENT_LEN - 1);
        ((char*)ptr)[STRING_ELEMENT_LEN - 1] = '\0';
        afterIndex = afterStr;
      } else if (arr->type == ARRAY_TYPE_INT) {
        float result;
        const char* afterExpr = ParseExpression(afterIndex, &result);
        if (!afterExpr) {
          return false;
        }
        *(int16_t*)ptr = (int16_t)(int)result;
        afterIndex = afterExpr;
      } else {
        float result;
        const char* afterExpr = ParseExpression(afterIndex, &result);
        if (!afterExpr) {
          return false;
        }
        *(float*)ptr = result;
        afterIndex = afterExpr;
      }

      afterIndex = SkipWhitespace(afterIndex);
      if (*afterIndex != ',') {
        break;
      }
      afterIndex++;
      afterIndex = SkipWhitespace(afterIndex);
      linearIdx++;
    }
    return true;
  }

  if (*args != '=') {
    return false;
  }
  args++;
  args = SkipWhitespace(args);

  if (IsStringVar(varName)) {
    args = ParseStringExpression(args, strVal, sizeof(strVal));
    if (!args) {
      return false;
    }
    Variable* v = CreateVariable(varName, VAR_STRING);
    if (!v) {
      return false;
    }
    if (!SetStringVar(v, strVal)) {
      return false;
    }
  } else {
    float result;
    if (!EvalExpression(args, &result)) {
      return false;
    }
    if (result == (int)result && result >= -32768 && result <= 32767) {
      Variable* v = CreateVariable(varName, VAR_INT);
      if (!v) {
        return false;
      }
      v->intVal = (int)result;
    } else {
      Variable* v = CreateVariable(varName, VAR_FLOAT);
      if (!v) {
        return false;
      }
      v->floatVal = result;
    }
  }
  return true;
}

// PRINT: evaluates and displays each item. A semicolon joins items directly, a
// comma advances to the next tab stop, and a trailing separator suppresses the
// newline. Also handles the AT and TAB cursor modifiers.
bool CmdPrint(const char* args) {
  char str[256];
  bool suppressNewline = false;
  const char* p = SkipWhitespace(args);

  if (*p == '\0') {
    Newline();
    return true;
  }

  while (*p != '\0') {
    p = SkipWhitespace(p);
    if (*p == '\0') {
      break;
    }

    bool itemParsed = false;

    if (strncasecmp(p, "TAB", 3) == 0 && !isalnum(p[3]) && p[3] != '$') {
      p += 3;
      p = SkipWhitespace(p);
      if (*p != '(') {
        return false;
      }
      p++;
      float col;
      const char* afterExpr = ParseExpression(p, &col);
      if (!afterExpr) {
        return false;
      }
      p = SkipWhitespace(afterExpr);
      if (*p != ')') {
        return false;
      }
      p++;
      int targetCol = (int)col;
      if (targetCol >= 0 && targetCol < VID_WIDTH) {
        LocateCursor(targetCol, GetCursorY());
      }
      itemParsed = true;
    } else if (strncasecmp(p, "AT", 2) == 0 && !isalnum(p[2]) && p[2] != '$') {
      p += 2;
      p = SkipWhitespace(p);
      if (*p != '(') {
        return false;
      }
      p++;
      float col;
      const char* afterCol = ParseExpression(p, &col);
      if (!afterCol) {
        return false;
      }
      p = SkipWhitespace(afterCol);
      if (*p != ',') {
        return false;
      }
      p++;
      float row;
      const char* afterRow = ParseExpression(p, &row);
      if (!afterRow) {
        return false;
      }
      p = SkipWhitespace(afterRow);
      if (*p != ')') {
        return false;
      }
      p++;
      int targetRow = (int)row;
      int targetCol = (int)col;
      if (targetCol >= 0 && targetCol < VID_WIDTH && targetRow >= 0 &&
          targetRow < VID_HEIGHT) {
        LocateCursor(targetCol, targetRow);
      }
      itemParsed = true;
    } else if (*p == '"') {
      const char* afterStr = ParseStringExpression(p, str, sizeof(str));
      if (!afterStr) {
        return false;
      }
      PrintStr(str);
      p = afterStr;
      itemParsed = true;
    } else {
      if (isalpha(*p)) {
        const char* peek = p;
        while (isalnum(*peek) || *peek == '$') {
          peek++;
        }
        bool looksLikeString = (peek > p && *(peek - 1) == '$');
        if (looksLikeString) {
          const char* afterStr = ParseStringExpression(p, str, sizeof(str));
          if (afterStr) {
            PrintStr(str);
            p = afterStr;
            itemParsed = true;
          } else {
            return false;
          }
        }
      }

      if (!itemParsed) {
        float result;
        const char* afterExpr = ParseExpression(p, &result);
        if (afterExpr) {
          FormatNumber(result, str, sizeof(str));
          PrintStr(str);
          p = afterExpr;
          itemParsed = true;
        }
      }
    }

    if (!itemParsed) {
      return false;
    }

    p = SkipWhitespace(p);
    if (*p == ';') {
      p++;
      p = SkipWhitespace(p);
      if (*p == '\0') {
        suppressNewline = true;
      }
    } else if (*p == ',') {
      int curCol = GetCursorX();
      int nextTab = ((curCol / 10) + 1) * 10;
      if (nextTab < VID_WIDTH) {
        LocateCursor(nextTab, GetCursorY());
      }
      p++;
      p = SkipWhitespace(p);
      if (*p == '\0') {
        suppressNewline = true;
      }
    } else if (*p != '\0') {
      return false;
    }
  }

  if (!suppressNewline) {
    Newline();
  }
  return true;
}

// INPUT: prompts, reads a line, and splits it on commas into the listed
// variables, converting each to its target type.
bool CmdInput(const char* args) {
  char varNames[4][MAX_VAR_NAME];
  bool isArrayAccess[4] = {false, false, false, false};
  int arrayIdx1[4] = {0, 0, 0, 0};
  int arrayIdx2[4] = {0, 0, 0, 0};
  bool has2ndIdx[4] = {false, false, false, false};
  int varCount = 0;
  bool suppressNewline = false;

  args = SkipWhitespace(args);

  while (varCount < 4 && *args && *args != ';') {
    const char* next = ParseVarName(args, varNames[varCount], MAX_VAR_NAME);
    if (!next) {
      break;
    }
    next = SkipWhitespace(next);

    if (*next == '(') {
      next++;
      float idx1F;
      const char* afterIndex = ParseExpression(next, &idx1F);
      if (!afterIndex) {
        return false;
      }
      afterIndex = SkipWhitespace(afterIndex);
      isArrayAccess[varCount] = true;
      arrayIdx1[varCount] = (int)idx1F;

      if (*afterIndex == ',') {
        afterIndex++;
        float idx2F;
        const char* afterIdx2 = ParseExpression(afterIndex, &idx2F);
        if (!afterIdx2) {
          return false;
        }
        afterIndex = SkipWhitespace(afterIdx2);
        arrayIdx2[varCount] = (int)idx2F;
        has2ndIdx[varCount] = true;
      }

      if (*afterIndex != ')') {
        return false;
      }
      afterIndex++;
      next = afterIndex;
    }

    varCount++;
    args = SkipWhitespace(next);
    if (*args == ',') {
      args++;
      args = SkipWhitespace(args);
    }
  }

  if (varCount == 0) {
    return false;
  }

  args = SkipWhitespace(args);
  if (*args == ';') {
    suppressNewline = true;
  }

  char inputBuf[128];
  if (!BasicReadLine(inputBuf, sizeof(inputBuf))) {
    breakRequested = true;
    Newline();
    return false;
  }

  const char* p = inputBuf;
  for (int i = 0; i < varCount; i++) {
    while (*p && isspace(*p)) {
      p++;
    }

    if (IsStringVar(varNames[i]) || IsStringArrayVar(varNames[i])) {
      char strVal[MAX_STR_EXPR_BUF];
      size_t j = 0;
      while (*p && *p != ',' && j < (size_t)(MAX_STR_EXPR_BUF - 1)) {
        strVal[j++] = *p++;
      }
      while (j > 0 && isspace((unsigned char)strVal[j - 1])) {
        j--;
      }
      strVal[j] = '\0';

      if (isArrayAccess[i]) {
        ArrayDescriptor* arr = FindArray(varNames[i]);
        if (!arr) {
          PrintError(ERR_ARRAY_NOT_DIMD);
          return false;
        }
        int linearIdx =
            CalculateArrayIndex(arr, arrayIdx1[i], arrayIdx2[i], has2ndIdx[i]);
        if (linearIdx == -2) {
          PrintError(ERR_WRONG_DIMENSIONS);
          return false;
        }
        if (linearIdx < 0) {
          PrintError(ERR_BAD_SUBSCRIPT);
          return false;
        }
        char* ptr = (char*)GetArrayElementPtr(arr, linearIdx);
        strncpy(ptr, strVal, STRING_ELEMENT_LEN - 1);
        ptr[STRING_ELEMENT_LEN - 1] = '\0';
      } else {
        Variable* v = CreateVariable(varNames[i], VAR_STRING);
        if (!v) {
          return false;
        }
        if (!SetStringVar(v, strVal)) {
          return false;
        }
      }
    } else {
      char* endPtr;
      float val = strtof(p, &endPtr);
      if (endPtr == p ||
          (*endPtr != ',' && *endPtr != '\0' && !isspace(*endPtr))) {
        val = -1;
        while (*p && *p != ',') {
          p++;
        }
      } else {
        p = endPtr;
      }

      if (isArrayAccess[i]) {
        ArrayDescriptor* arr = FindArray(varNames[i]);
        if (!arr) {
          PrintError(ERR_ARRAY_NOT_DIMD);
          return false;
        }
        int linearIdx =
            CalculateArrayIndex(arr, arrayIdx1[i], arrayIdx2[i], has2ndIdx[i]);
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
          Variable* v = CreateVariable(varNames[i], VAR_INT);
          if (!v) {
            return false;
          }
          v->intVal = (int)val;
        } else {
          Variable* v = CreateVariable(varNames[i], VAR_FLOAT);
          if (!v) {
            return false;
          }
          v->floatVal = val;
        }
      }
    }

    while (*p && isspace(*p)) {
      p++;
    }
    if (*p == ',') {
      p++;
    }
  }

  if (!suppressNewline) {
    Newline();
  }
  return true;
}

// GET: takes one keystroke without waiting, assigning 0 or "" when none is
// pending so a polling loop never blocks.
bool CmdGet(const char* args) {
  char varName[MAX_VAR_NAME];
  bool isArrayAccess = false;
  int idx1 = 0, idx2 = 0;
  bool has2ndIndex = false;

  args = SkipWhitespace(args);
  args = ParseVarName(args, varName, sizeof(varName));
  if (!args) {
    return false;
  }

  args = SkipWhitespace(args);
  if (*args == '(') {
    args++;
    float idx1F;
    const char* afterIndex = ParseExpression(args, &idx1F);
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
  }

  uint8_t key = BasicGetChar();

  if (IsStringVar(varName) || IsStringArrayVar(varName)) {
    char charStr[2];
    charStr[0] = (key > 0) ? (char)key : '\0';
    charStr[1] = '\0';

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
      strncpy(ptr, charStr, STRING_ELEMENT_LEN - 1);
      ptr[STRING_ELEMENT_LEN - 1] = '\0';
    } else {
      Variable* v = CreateVariable(varName, VAR_STRING);
      if (!v) {
        return false;
      }
      if (!SetStringVar(v, charStr)) {
        return false;
      }
    }
  } else {
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
        *(int16_t*)ptr = (int16_t)key;
      } else {
        *(float*)ptr = (float)key;
      }
    } else {
      Variable* v = CreateVariable(varName, VAR_INT);
      if (!v) {
        return false;
      }
      v->intVal = key;
    }
  }
  return true;
}

// Parses a comma-separated list of assignment targets, recording array
// subscripts alongside each name. Shared by the network input statements.
static int ParseVarList(const char* args, char varNames[][MAX_VAR_NAME],
                        bool isArrayAccess[], int arrayIdx1[], int arrayIdx2[],
                        bool has2ndIdx[], int maxVars, const char** argsOut) {
  int varCount = 0;
  args = SkipWhitespace(args);

  while (varCount < maxVars && *args && *args != ';') {
    const char* next = ParseVarName(args, varNames[varCount], MAX_VAR_NAME);
    if (!next) {
      break;
    }
    next = SkipWhitespace(next);

    if (*next == '(') {
      next++;
      float idx1F;
      const char* afterIndex = ParseExpression(next, &idx1F);
      if (!afterIndex) {
        *argsOut = args;
        return -1;
      }
      afterIndex = SkipWhitespace(afterIndex);
      isArrayAccess[varCount] = true;
      arrayIdx1[varCount] = (int)idx1F;

      if (*afterIndex == ',') {
        afterIndex++;
        float idx2F;
        const char* afterIdx2 = ParseExpression(afterIndex, &idx2F);
        if (!afterIdx2) {
          *argsOut = args;
          return -1;
        }
        afterIndex = SkipWhitespace(afterIdx2);
        arrayIdx2[varCount] = (int)idx2F;
        has2ndIdx[varCount] = true;
      }

      if (*afterIndex != ')') {
        *argsOut = args;
        return -1;
      }
      afterIndex++;
      next = afterIndex;
    }

    varCount++;
    args = SkipWhitespace(next);
    if (*args == ',') {
      args++;
      args = SkipWhitespace(args);
    }
  }

  *argsOut = args;
  return varCount;
}

// Assigns the empty default -- 0 or "" -- used when a non-blocking network read
// finds nothing waiting.
static void AssignBtDefault(const char* varName, bool arrAccess, int idx1,
                            int idx2, bool has2nd) {
  if (IsStringVar(varName) || IsStringArrayVar(varName)) {
    if (arrAccess) {
      ArrayDescriptor* arr = FindArray(varName);
      if (!arr) {
        return;
      }
      int li = CalculateArrayIndex(arr, idx1, idx2, has2nd);
      if (li < 0) {
        return;
      }
      char* ptr = (char*)GetArrayElementPtr(arr, li);
      ptr[0] = '\0';
    } else {
      Variable* v = CreateVariable(varName, VAR_STRING);
      if (v) {
        SetStringVar(v, "");
      }
    }
  } else {
    if (arrAccess) {
      ArrayDescriptor* arr = FindArray(varName);
      if (!arr) {
        return;
      }
      int li = CalculateArrayIndex(arr, idx1, idx2, has2nd);
      if (li < 0) {
        return;
      }
      void* ptr = GetArrayElementPtr(arr, li);
      if (arr->type == ARRAY_TYPE_INT) {
        *(int16_t*)ptr = 0;
      } else {
        *(float*)ptr = 0.0f;
      }
    } else {
      Variable* v = CreateVariable(varName, VAR_INT);
      if (v) {
        v->intVal = 0;
      }
    }
  }
}

// Assigns one received byte, as a character for string targets and as its code
// for numeric ones.
static bool AssignBtByte(const char* varName, uint8_t ch, bool arrAccess,
                         int idx1, int idx2, bool has2nd) {
  if (IsStringVar(varName) || IsStringArrayVar(varName)) {
    char cs[2] = {(ch > 0) ? (char)ch : '\0', '\0'};
    if (arrAccess) {
      ArrayDescriptor* arr = FindArray(varName);
      if (!arr) {
        PrintError(ERR_ARRAY_NOT_DIMD);
        return false;
      }
      int li = CalculateArrayIndex(arr, idx1, idx2, has2nd);
      if (li == -2) {
        PrintError(ERR_WRONG_DIMENSIONS);
        return false;
      }
      if (li < 0) {
        PrintError(ERR_BAD_SUBSCRIPT);
        return false;
      }
      char* ptr = (char*)GetArrayElementPtr(arr, li);
      strncpy(ptr, cs, STRING_ELEMENT_LEN - 1);
      ptr[STRING_ELEMENT_LEN - 1] = '\0';
    } else {
      Variable* v = CreateVariable(varName, VAR_STRING);
      if (!v) {
        return false;
      }
      if (!SetStringVar(v, cs)) {
        return false;
      }
    }
  } else {
    int16_t val = (int16_t)ch;
    if (arrAccess) {
      ArrayDescriptor* arr = FindArray(varName);
      if (!arr) {
        PrintError(ERR_ARRAY_NOT_DIMD);
        return false;
      }
      int li = CalculateArrayIndex(arr, idx1, idx2, has2nd);
      if (li == -2) {
        PrintError(ERR_WRONG_DIMENSIONS);
        return false;
      }
      if (li < 0) {
        PrintError(ERR_BAD_SUBSCRIPT);
        return false;
      }
      void* ptr = GetArrayElementPtr(arr, li);
      if (arr->type == ARRAY_TYPE_INT) {
        *(int16_t*)ptr = val;
      } else {
        *(float*)ptr = (float)val;
      }
    } else {
      Variable* v = CreateVariable(varName, VAR_INT);
      if (!v) {
        return false;
      }
      v->intVal = val;
    }
  }
  return true;
}

// Assigns one comma-separated field from a received line, converting to the
// target's type.
static bool AssignBtToken(const char* varName, bool arrAccess, int idx1,
                          int idx2, bool has2nd, const char** p) {
  while (**p && isspace((unsigned char)**p)) {
    (*p)++;
  }

  if (IsStringVar(varName) || IsStringArrayVar(varName)) {
    char strVal[MAX_STR_EXPR_BUF];
    size_t j = 0;
    while (**p && **p != ',' && j < (size_t)(MAX_STR_EXPR_BUF - 1)) {
      strVal[j++] = *(*p)++;
    }
    while (j > 0 && isspace((unsigned char)strVal[j - 1])) {
      j--;
    }
    strVal[j] = '\0';

    if (arrAccess) {
      ArrayDescriptor* arr = FindArray(varName);
      if (!arr) {
        PrintError(ERR_ARRAY_NOT_DIMD);
        return false;
      }
      int li = CalculateArrayIndex(arr, idx1, idx2, has2nd);
      if (li == -2) {
        PrintError(ERR_WRONG_DIMENSIONS);
        return false;
      }
      if (li < 0) {
        PrintError(ERR_BAD_SUBSCRIPT);
        return false;
      }
      char* ptr = (char*)GetArrayElementPtr(arr, li);
      strncpy(ptr, strVal, STRING_ELEMENT_LEN - 1);
      ptr[STRING_ELEMENT_LEN - 1] = '\0';
    } else {
      Variable* v = CreateVariable(varName, VAR_STRING);
      if (!v) {
        return false;
      }
      if (!SetStringVar(v, strVal)) {
        return false;
      }
    }
  } else {
    char* endPtr;
    float val = strtof(*p, &endPtr);
    if (endPtr == *p || (*endPtr != ',' && *endPtr != '\0' &&
                         !isspace((unsigned char)*endPtr))) {
      val = 0.0f;
      while (**p && **p != ',') {
        (*p)++;
      }
    } else {
      *p = endPtr;
    }

    if (arrAccess) {
      ArrayDescriptor* arr = FindArray(varName);
      if (!arr) {
        PrintError(ERR_ARRAY_NOT_DIMD);
        return false;
      }
      int li = CalculateArrayIndex(arr, idx1, idx2, has2nd);
      if (li == -2) {
        PrintError(ERR_WRONG_DIMENSIONS);
        return false;
      }
      if (li < 0) {
        PrintError(ERR_BAD_SUBSCRIPT);
        return false;
      }
      void* ptr = GetArrayElementPtr(arr, li);
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

  while (**p && isspace((unsigned char)**p)) {
    (*p)++;
  }
  if (**p == ',') {
    (*p)++;
  }
  return true;
}

// NETPRINT: sends items to the network stream. No newline is added, since the
// receiving protocol decides its own framing.
bool CmdNetPrint(const char* args) {
  char str[256];
  const char* p = SkipWhitespace(args);

  while (*p != '\0') {
    p = SkipWhitespace(p);
    if (*p == '\0') {
      break;
    }

    bool itemParsed = false;

    if (*p == '"') {
      size_t sendLen = 0;
      const char* afterStr =
          ParseStringExpressionLen(p, str, sizeof(str), &sendLen);
      if (!afterStr) {
        return false;
      }
      WifiSendBytes((const uint8_t*)str, (uint8_t)sendLen);
      p = afterStr;
      itemParsed = true;
    } else {
      if (isalpha(*p)) {
        const char* peek = p;
        while (isalnum(*peek) || *peek == '$') {
          peek++;
        }
        if (peek > p && *(peek - 1) == '$') {
          size_t sendLen = 0;
          const char* afterStr =
              ParseStringExpressionLen(p, str, sizeof(str), &sendLen);
          if (afterStr) {
            WifiSendBytes((const uint8_t*)str, (uint8_t)sendLen);
            p = afterStr;
            itemParsed = true;
          }
        }
      }

      if (!itemParsed) {
        float result;
        const char* afterExpr = ParseExpression(p, &result);
        if (afterExpr) {
          FormatNumber(result, str, sizeof(str));
          WifiSend(str);
          p = afterExpr;
          itemParsed = true;
        }
      }
    }

    if (!itemParsed) {
      return false;
    }

    p = SkipWhitespace(p);
    if (*p == ';' || *p == ',') {
      p++;
    } else if (*p != '\0') {
      return false;
    }
  }

  return true;
}

// NETGET: reads one byte per variable. Non-blocking unless a trailing semicolon
// asks it to wait, and waiting stays interruptible with BREAK.
bool CmdNetGet(const char* args) {
  char varNames[4][MAX_VAR_NAME] = {};
  bool isArrayAccess[4] = {};
  int arrayIdx1[4] = {};
  int arrayIdx2[4] = {};
  bool has2ndIdx[4] = {};
  const char* argsAfter;

  int varCount = ParseVarList(args, varNames, isArrayAccess, arrayIdx1,
                              arrayIdx2, has2ndIdx, 4, &argsAfter);
  if (varCount <= 0) {
    return false;
  }

  argsAfter = SkipWhitespace(argsAfter);
  bool blocking = (*argsAfter == ';');

  for (int i = 0; i < varCount; i++) {
    uint8_t ch = 0;
    if (blocking) {
      while (!Serial1.available()) {
        if (BufferScanAndRemove(CTRL_C_KEY)) {
          breakRequested = true;
          return false;
        }
      }
      ch = (uint8_t)Serial1.read();
    } else {
      if (Serial1.available()) {
        ch = (uint8_t)Serial1.read();
      }
    }

    if (!AssignBtByte(varNames[i], ch, isArrayAccess[i], arrayIdx1[i],
                      arrayIdx2[i], has2ndIdx[i])) {
      return false;
    }
  }
  return true;
}

// NETINPUT: reads one line and splits it on commas into the listed variables.
// Non-blocking unless a trailing semicolon asks it to wait.
bool CmdNetInput(const char* args) {
  char varNames[4][MAX_VAR_NAME] = {};
  bool isArrayAccess[4] = {};
  int arrayIdx1[4] = {};
  int arrayIdx2[4] = {};
  bool has2ndIdx[4] = {};
  const char* argsAfter;

  int varCount = ParseVarList(args, varNames, isArrayAccess, arrayIdx1,
                              arrayIdx2, has2ndIdx, 4, &argsAfter);
  if (varCount <= 0) {
    return false;
  }

  argsAfter = SkipWhitespace(argsAfter);
  bool blocking = (*argsAfter == ';');

  char inputBuf[128];
  int btResult;

  if (blocking) {
    btResult = 0;
    while (btResult == 0) {
      btResult = WifiReadLine(inputBuf, sizeof(inputBuf));
      if (BufferScanAndRemove(CTRL_C_KEY)) {
        breakRequested = true;
        return false;
      }
    }
  } else {
    btResult = WifiReadLine(inputBuf, sizeof(inputBuf));
  }

  if (btResult != 1) {
    for (int i = 0; i < varCount; i++) {
      AssignBtDefault(varNames[i], isArrayAccess[i], arrayIdx1[i], arrayIdx2[i],
                      has2ndIdx[i]);
    }
    return true;
  }

  const char* p = inputBuf;
  for (int i = 0; i < varCount; i++) {
    if (!AssignBtToken(varNames[i], isArrayAccess[i], arrayIdx1[i],
                       arrayIdx2[i], has2ndIdx[i], &p)) {
      return false;
    }
  }
  return true;
}

// WIFI: joins a network by SSID and password.
bool CmdWifi(const char* args) {
  char ssid[64], password[64];
  args = SkipWhitespace(args);
  const char* after = ParseString(args, ssid, sizeof(ssid));
  if (!after) {
    after = ParseStringExpression(args, ssid, sizeof(ssid));
    if (!after) {
      return false;
    }
  }
  after = SkipWhitespace(after);
  if (*after == ',') {
    after++;
  }
  after = SkipWhitespace(after);
  const char* after2 = ParseString(after, password, sizeof(password));
  if (!after2) {
    after2 = ParseStringExpression(after, password, sizeof(password));
    if (!after2) {
      return false;
    }
  }
  if (ServerConnected()) {
    ServerDisconnect();
  }
  if (!WifiConnect(ssid, password)) {
    PrintError(ERR_WIFI_CONNECT);
    return false;
  }
  return true;
}

// NETCONNECT: opens a TCP session to a host and port.
bool CmdNetConnect(const char* args) {
  char host[64];
  float portF;
  args = SkipWhitespace(args);
  const char* after = ParseString(args, host, sizeof(host));
  if (!after) {
    after = ParseStringExpression(args, host, sizeof(host));
    if (!after) {
      return false;
    }
  }
  after = SkipWhitespace(after);
  if (*after == ',') {
    after++;
  }
  after = ParseExpression(after, &portF);
  if (!after) {
    return false;
  }
  if (!ServerConnect(host, (uint16_t)(int)portF)) {
    PrintError(ERR_NETCONNECT);
    return false;
  }
  return true;
}

// NETDISCONNECT: closes the TCP session.
bool CmdNetDisconnect(const char* args) {
  (void)args;
  ServerDisconnect();
  return true;
}

// LOCATE: moves the cursor without printing.
bool CmdLocate(const char* args) {
  float xf, yf;

  args = ParseExpression(args, &xf);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args == ',') {
    args++;
  }
  args = ParseExpression(args, &yf);
  if (!args) {
    return false;
  }

  int x = (int)xf, y = (int)yf;
  if (x >= 0 && x < VID_WIDTH && y >= 0 && y < VID_HEIGHT) {
    LocateCursor(x, y);
    return true;
  }
  PrintError(ERR_ILLEGAL_QUANTITY);
  return false;
}
