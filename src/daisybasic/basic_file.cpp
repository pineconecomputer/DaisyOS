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

static uint8_t openFileChannels = 0;

// Waits for the file server's one-byte acknowledgement.
static bool WaitAck(void) {
  uint8_t resp[1];
  return WifiReadBytes(resp, 1, 5000) == 1 && resp[0] == 0x06;
}

// Closes every open channel, so a program that stops mid-stream does not leak
// file handles on the server. Called by RUN, NEW, CLR and BREAK.
void CloseAllFileChannels(void) {
  if (openFileChannels == 0) {
    return;
  }
  while (Serial1.available()) {
    Serial1.read();
  }
  for (uint8_t ch = 0; ch < 4; ch++) {
    if (openFileChannels & (1u << ch)) {
      CommMsgSendFclose(ch);
    }
  }
  openFileChannels = 0;
}

// Reads one line of file content from the link. The timeout restarts on every
// byte, so a slow transfer is not mistaken for a stall.
static int ReadFileInputLine(char* outBuf, int maxLen, uint32_t timeoutMs) {
  int pos = 0;
  uint32_t deadline = (uint32_t)millis() + timeoutMs;
  while ((uint32_t)millis() < deadline) {
    while (Serial1.available()) {
      char ch = (char)Serial1.read();
      if (ch == '\r') {
        continue;
      }
      if (ch == '\n') {
        outBuf[pos] = '\0';
        return pos;
      }
      if (pos < maxLen - 1) {
        outBuf[pos++] = ch;
      }
      deadline = (uint32_t)millis() + timeoutMs;
    }
  }
  outBuf[pos] = '\0';
  return -1;
}

// FOPEN: opens a server file on a numbered channel for read, write or append.
bool CmdFopen(const char* args) {
  float chanF;
  const char* p = SkipWhitespace(args);
  p = ParseExpression(p, &chanF);
  if (!p) {
    PrintError(ERR_SYNTAX);
    return false;
  }
  int channel = (int)chanF;
  if (channel < 0 || channel > 3) {
    PrintError(ERR_ILLEGAL_QUANTITY);
    return false;
  }

  p = SkipWhitespace(p);
  if (*p != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  p = SkipWhitespace(p + 1);

  char filename[64];
  p = ParseStringExpression(p, filename, sizeof(filename));
  if (!p) {
    PrintError(ERR_SYNTAX);
    return false;
  }

  p = SkipWhitespace(p);
  if (*p != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  p = SkipWhitespace(p + 1);

  char modeStr[16];
  p = ParseStringExpression(p, modeStr, sizeof(modeStr));
  if (!p) {
    PrintError(ERR_SYNTAX);
    return false;
  }

  uint8_t mode = 0;
  if (strcasecmp(modeStr, "W") == 0 || strcasecmp(modeStr, "WRITE") == 0) {
    mode = 1;
  } else if (strcasecmp(modeStr, "A") == 0 ||
             strcasecmp(modeStr, "APPEND") == 0) {
    mode = 2;
  }

  CommMsgSendFopen((uint8_t)channel, mode, filename);
  if (!WaitAck()) {
    PrintError(ERR_FOPEN);
    return false;
  }
  openFileChannels |= (uint8_t)(1u << channel);
  return true;
}

// FCLOSE: flushes and closes a channel.
bool CmdFclose(const char* args) {
  float chanF;
  const char* p = SkipWhitespace(args);
  p = ParseExpression(p, &chanF);
  if (!p) {
    PrintError(ERR_SYNTAX);
    return false;
  }
  int channel = (int)chanF;
  if (channel < 0 || channel > 3) {
    PrintError(ERR_ILLEGAL_QUANTITY);
    return false;
  }

  CommMsgSendFclose((uint8_t)channel);
  openFileChannels &= (uint8_t)~(1u << channel);
  if (!WaitAck()) {
    PrintError(ERR_FCLOSE);
    return false;
  }
  return true;
}

// FPRINT: writes items to a channel. No newline is added, so the program
// controls the file's exact layout.
bool CmdFprint(const char* args) {
  float chanF;
  const char* p = SkipWhitespace(args);
  p = ParseExpression(p, &chanF);
  if (!p) {
    PrintError(ERR_SYNTAX);
    return false;
  }
  int channel = (int)chanF;
  if (channel < 0 || channel > 3) {
    PrintError(ERR_ILLEGAL_QUANTITY);
    return false;
  }

  p = SkipWhitespace(p);
  if (*p != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  p = SkipWhitespace(p + 1);

  char textBuf[MAX_STR_EXPR_BUF];
  int pos = 0;

  while (*p != '\0') {
    p = SkipWhitespace(p);
    if (*p == '\0') {
      break;
    }

    bool itemParsed = false;

    if (*p == '"') {
      char s[MAX_STR_EXPR_BUF];
      const char* after = ParseStringExpression(p, s, sizeof(s));
      if (!after) {
        return false;
      }
      int slen = (int)strlen(s);
      if (pos + slen < (int)sizeof(textBuf) - 1) {
        memcpy(textBuf + pos, s, slen);
        pos += slen;
      }
      p = after;
      itemParsed = true;
    } else {
      if (isalpha(*p)) {
        const char* peek = p;
        while (isalnum(*peek) || *peek == '$') {
          peek++;
        }
        if (peek > p && *(peek - 1) == '$') {
          char s[MAX_STR_EXPR_BUF];
          const char* after = ParseStringExpression(p, s, sizeof(s));
          if (after) {
            int slen = (int)strlen(s);
            if (pos + slen < (int)sizeof(textBuf) - 1) {
              memcpy(textBuf + pos, s, slen);
              pos += slen;
            }
            p = after;
            itemParsed = true;
          }
        }
      }
      if (!itemParsed) {
        float val;
        const char* after = ParseExpression(p, &val);
        if (!after) {
          return false;
        }
        char numBuf[32];
        FormatNumber(val, numBuf, sizeof(numBuf));
        int slen = (int)strlen(numBuf);
        if (pos + slen < (int)sizeof(textBuf) - 1) {
          memcpy(textBuf + pos, numBuf, slen);
          pos += slen;
        }
        p = after;
        itemParsed = true;
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
  textBuf[pos] = '\0';

  uint8_t textLen = (uint8_t)(pos > 254 ? 254 : pos);
  CommMsgSendFprint((uint8_t)channel, textBuf, textLen);
  if (!WaitAck()) {
    PrintError(ERR_FPRINT);
    return false;
  }
  return true;
}

// Stores one line read from a file into a variable or array element, converting
// to the target's type.
static bool AssignFileLine(const char* line, const char* varName, bool isArr,
                           int idx1, int idx2, bool has2nd) {
  if (IsStringVar(varName) || IsStringArrayVar(varName)) {
    if (isArr) {
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
      strncpy(ptr, line, STRING_ELEMENT_LEN - 1);
      ptr[STRING_ELEMENT_LEN - 1] = '\0';
    } else {
      Variable* v = CreateVariable(varName, VAR_STRING);
      if (!v) {
        return false;
      }
      if (!SetStringVar(v, line)) {
        return false;
      }
    }
  } else {
    char* endPtr;
    float val = strtof(line, &endPtr);
    if (endPtr == line) {
      val = 0;
    }
    if (isArr) {
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
  return true;
}

// FINPUT: reads one whole line per variable. Commas in the file are literal
// data here, unlike INPUT, so text containing commas round-trips intact.
bool CmdFinput(const char* args) {
  float chanF;
  const char* p = SkipWhitespace(args);
  p = ParseExpression(p, &chanF);
  if (!p) {
    PrintError(ERR_SYNTAX);
    return false;
  }
  int channel = (int)chanF;
  if (channel < 0 || channel > 3) {
    PrintError(ERR_ILLEGAL_QUANTITY);
    return false;
  }

  p = SkipWhitespace(p);
  if (*p != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  p = SkipWhitespace(p + 1);

  char varNames[4][MAX_VAR_NAME] = {};
  bool isArrayAccess[4] = {};
  int arrayIdx1[4] = {};
  int arrayIdx2[4] = {};
  bool has2ndIdx[4] = {};
  int varCount = 0;

  while (varCount < 4 && *p != '\0') {
    char vname[MAX_VAR_NAME];
    const char* after = ParseVarName(p, vname, MAX_VAR_NAME);
    if (!after) {
      break;
    }
    after = SkipWhitespace(after);
    if (*after == '(') {
      after++;
      float idx1F;
      after = ParseExpression(after, &idx1F);
      if (!after) {
        PrintError(ERR_SYNTAX);
        return false;
      }
      after = SkipWhitespace(after);
      isArrayAccess[varCount] = true;
      arrayIdx1[varCount] = (int)idx1F;
      if (*after == ',') {
        after++;
        float idx2F;
        after = ParseExpression(after, &idx2F);
        if (!after) {
          PrintError(ERR_SYNTAX);
          return false;
        }
        after = SkipWhitespace(after);
        arrayIdx2[varCount] = (int)idx2F;
        has2ndIdx[varCount] = true;
      }
      if (*after != ')') {
        PrintError(ERR_SYNTAX);
        return false;
      }
      after++;
    }
    strncpy(varNames[varCount], vname, MAX_VAR_NAME - 1);
    varNames[varCount][MAX_VAR_NAME - 1] = '\0';
    varCount++;
    p = SkipWhitespace(after);
    if (*p == ',') {
      p++;
      p = SkipWhitespace(p);
    }
  }
  if (varCount == 0) {
    PrintError(ERR_SYNTAX);
    return false;
  }

  char lineBuf[MAX_STR_EXPR_BUF];
  for (int i = 0; i < varCount; i++) {
    CommMsgSendFinput((uint8_t)channel);

    ReadFileInputLine(lineBuf, sizeof(lineBuf), 5000);

    if (lineBuf[0] == '\0') {
      for (int j = i; j < varCount; j++) {
        AssignFileLine("", varNames[j], isArrayAccess[j], arrayIdx1[j],
                       arrayIdx2[j], has2ndIdx[j]);
      }
      return true;
    }

    if (!AssignFileLine(lineBuf, varNames[i], isArrayAccess[i], arrayIdx1[i],
                        arrayIdx2[i], has2ndIdx[i])) {
      return false;
    }
  }
  return true;
}

// FGET: reads a single byte, as a character for string targets and as its code
// for numeric ones.
bool CmdFget(const char* args) {
  float chanF;
  const char* p = SkipWhitespace(args);
  p = ParseExpression(p, &chanF);
  if (!p) {
    PrintError(ERR_SYNTAX);
    return false;
  }
  int channel = (int)chanF;
  if (channel < 0 || channel > 3) {
    PrintError(ERR_ILLEGAL_QUANTITY);
    return false;
  }

  p = SkipWhitespace(p);
  if (*p != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  p = SkipWhitespace(p + 1);

  char varName[MAX_VAR_NAME];
  p = ParseVarName(p, varName, MAX_VAR_NAME);
  if (!p) {
    PrintError(ERR_SYNTAX);
    return false;
  }

  p = SkipWhitespace(p);
  bool isArrayAccess = false;
  int idx1 = 0, idx2 = 0;
  bool has2ndIndex = false;
  if (*p == '(') {
    p++;
    float idx1F;
    p = ParseExpression(p, &idx1F);
    if (!p) {
      PrintError(ERR_SYNTAX);
      return false;
    }
    p = SkipWhitespace(p);
    isArrayAccess = true;
    idx1 = (int)idx1F;
    if (*p == ',') {
      p++;
      float idx2F;
      p = ParseExpression(p, &idx2F);
      if (!p) {
        PrintError(ERR_SYNTAX);
        return false;
      }
      p = SkipWhitespace(p);
      idx2 = (int)idx2F;
      has2ndIndex = true;
    }
    if (*p != ')') {
      PrintError(ERR_SYNTAX);
      return false;
    }
  }

  CommMsgSendFget((uint8_t)channel);

  uint8_t resp[2] = {0, 0};
  WifiReadBytes(resp, 2, 5000);
  uint8_t byteVal = resp[0] ? resp[1] : 0;

  if (IsStringVar(varName) || IsStringArrayVar(varName)) {
    char charStr[2];
    charStr[0] = (byteVal > 0) ? (char)byteVal : '\0';
    charStr[1] = '\0';
    if (isArrayAccess) {
      ArrayDescriptor* arr = FindArray(varName);
      if (!arr) {
        PrintError(ERR_ARRAY_NOT_DIMD);
        return false;
      }
      int li = CalculateArrayIndex(arr, idx1, idx2, has2ndIndex);
      if (li == -2) {
        PrintError(ERR_WRONG_DIMENSIONS);
        return false;
      }
      if (li < 0) {
        PrintError(ERR_BAD_SUBSCRIPT);
        return false;
      }
      char* ptr = (char*)GetArrayElementPtr(arr, li);
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
      int li = CalculateArrayIndex(arr, idx1, idx2, has2ndIndex);
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
        *(int16_t*)ptr = (int16_t)byteVal;
      } else {
        *(float*)ptr = (float)byteVal;
      }
    } else {
      Variable* v = CreateVariable(varName, VAR_INT);
      if (!v) {
        return false;
      }
      v->intVal = byteVal;
    }
  }
  return true;
}

// FPUT: writes a single byte.
bool CmdFput(const char* args) {
  float chanF;
  const char* p = SkipWhitespace(args);
  p = ParseExpression(p, &chanF);
  if (!p) {
    PrintError(ERR_SYNTAX);
    return false;
  }
  int channel = (int)chanF;
  if (channel < 0 || channel > 3) {
    PrintError(ERR_ILLEGAL_QUANTITY);
    return false;
  }

  p = SkipWhitespace(p);
  if (*p != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  p = SkipWhitespace(p + 1);

  float byteF;
  p = ParseExpression(p, &byteF);
  if (!p) {
    PrintError(ERR_SYNTAX);
    return false;
  }

  CommMsgSendFput((uint8_t)channel, (uint8_t)(int)byteF);
  if (!WaitAck()) {
    PrintError(ERR_FPUT);
    return false;
  }
  return true;
}

// FSEEK: moves the file cursor by a signed offset from its current position.
bool CmdFseek(const char* args) {
  float chanF;
  const char* p = SkipWhitespace(args);
  p = ParseExpression(p, &chanF);
  if (!p) {
    PrintError(ERR_SYNTAX);
    return false;
  }
  int channel = (int)chanF;
  if (channel < 0 || channel > 3) {
    PrintError(ERR_ILLEGAL_QUANTITY);
    return false;
  }

  p = SkipWhitespace(p);
  if (*p != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  p = SkipWhitespace(p + 1);

  float offsetF;
  p = ParseExpression(p, &offsetF);
  if (!p) {
    PrintError(ERR_SYNTAX);
    return false;
  }

  CommMsgSendFseek((uint8_t)channel, (uint32_t)(int32_t)offsetF);
  if (!WaitAck()) {
    PrintError(ERR_FSEEK);
    return false;
  }
  return true;
}

// FREWIND: moves the cursor back to the start of the file.
bool CmdFrewind(const char* args) {
  float chanF;
  const char* p = SkipWhitespace(args);
  p = ParseExpression(p, &chanF);
  if (!p) {
    PrintError(ERR_SYNTAX);
    return false;
  }
  int channel = (int)chanF;
  if (channel < 0 || channel > 3) {
    PrintError(ERR_ILLEGAL_QUANTITY);
    return false;
  }
  p = SkipWhitespace(p);
  if (*p != '\0' && *p != ':') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  CommMsgSendFrewind((uint8_t)channel);
  if (!WaitAck()) {
    PrintError(ERR_FSEEK);
    return false;
  }
  return true;
}

#define MORE_CHANNEL 4
#define MORE_PAGE_LINES 24
#define MORE_MAX_PAGES 256

// Bytes remaining on a channel, read as a big-endian count. MORE derives the
// absolute position from this, since the protocol has no way to ask for it.
static uint32_t MoreFbytes(uint8_t chan) {
  CommMsgSendFbytes(chan);
  uint8_t r[4];
  if (WifiReadBytes(r, 4, 5000) != 4) {
    return 0;
  }
  return ((uint32_t)r[0] << 24) | ((uint32_t)r[1] << 16) |
         ((uint32_t)r[2] << 8) | (uint32_t)r[3];
}

// Seeks to an absolute offset, computed as a relative move because the server
// only supports seeking by a delta.
static void MoreSeekAbs(uint8_t chan, uint32_t totalSize, uint32_t target) {
  uint32_t cur = totalSize - MoreFbytes(chan);
  int32_t delta = (int32_t)(target - cur);
  CommMsgSendFseek(chan, (uint32_t)delta);
  WaitAck();
}

// Reads the next line and reports the offset it started at, which is what lets
// MORE record page boundaries it can seek back to.
static int MoreNextLine(uint8_t chan, char* buf, int maxLen, uint32_t totalSize,
                        uint32_t* prePosOut) {
  CommMsgSendFbytes(chan);
  uint8_t r[4];
  if (WifiReadBytes(r, 4, 5000) != 4) {
    buf[0] = '\0';
    return -1;
  }
  uint32_t rem = ((uint32_t)r[0] << 24) | ((uint32_t)r[1] << 16) |
                 ((uint32_t)r[2] << 8) | (uint32_t)r[3];
  if (prePosOut) {
    *prePosOut = totalSize - rem;
  }
  if (rem == 0) {
    buf[0] = '\0';
    return 0;
  }
  CommMsgSendFinput(chan);
  int n = ReadFileInputLine(buf, maxLen, 5000);
  if (n < 0) {
    buf[0] = '\0';
    return -1;
  }
  return 1;
}

// Draws the reverse-video status bar, showing either the page position and key
// hints or a transient message. Padded to the full width so it overwrites
// whatever was there.
static void MoreStatusBar(int page, int pageCount, bool atEof,
                          const char* msg) {
  SetReverseMode(false);
  LocateCursor(0, VID_HEIGHT - 1);
  SetReverseMode(true);
  char s[VID_WIDTH + 1];
  if (msg) {
    snprintf(s, sizeof(s), "%s", msg);
  } else {
    snprintf(s, sizeof(s), "Pg %d/%d%s  Spc:Fwd BS:Bk :Find SA:Quit", page + 1,
             pageCount, atEof ? " END" : "");
  }
  int len = (int)strlen(s);
  if (len > VID_WIDTH) {
    len = VID_WIDTH;
  }
  while (len < VID_WIDTH) {
    s[len++] = ' ';
  }
  s[VID_WIDTH] = '\0';
  PrintStr(s);
  SetReverseMode(false);
}

// Draws one screenful from a given offset and reports where it ended, so the
// next page's start is known. Scroll lock is held on so a full page does not
// push itself off the top.
static bool MoreDisplayPage(uint8_t chan, uint32_t pageOffset,
                            uint32_t totalSize, uint32_t* endPosOut) {
  MoreSeekAbs(chan, totalSize, pageOffset);
  SetReverseMode(false);
  Clrscr(' ');
  LocateCursor(0, 0);
  SetScrollLock(true);

  char line[255];
  bool hitEof = false;
  int rowsLeft = MORE_PAGE_LINES;

  while (rowsLeft > 0) {
    uint32_t prePos;
    int r = MoreNextLine(chan, line, sizeof(line), totalSize, &prePos);
    if (r <= 0) {
      hitEof = true;
      break;
    }

    int lineLen = (int)strlen(line);
    int rowsNeeded = (lineLen == 0) ? 1 : (lineLen + VID_WIDTH - 1) / VID_WIDTH;

    if (rowsNeeded > rowsLeft) {
      MoreSeekAbs(chan, totalSize, prePos);
      break;
    }

    if (lineLen == 0) {
      Newline();
      rowsLeft--;
    } else {
      int off = 0;
      while (off < lineLen) {
        int chunk = lineLen - off;
        if (chunk > VID_WIDTH) {
          chunk = VID_WIDTH;
        }
        char tmp[VID_WIDTH + 1];
        memcpy(tmp, line + off, chunk);
        tmp[chunk] = '\0';
        PrintStr(tmp);
        if (GetCursorX() != 0) {
          Newline();
        }
        off += chunk;
        rowsLeft--;
      }
    }
  }
  while (rowsLeft > 0) {
    Newline();
    rowsLeft--;
  }

  uint32_t rem = MoreFbytes(chan);
  uint32_t endPos = totalSize - rem;
  if (endPosOut) {
    *endPosOut = endPos;
  }
  if (rem == 0) {
    hitEof = true;
  }
  return hitEof;
}

// Searches forward from a line for matching text, returning the line it was
// found on. The caller retries from the top so a search wraps.
static int MoreSearch(uint8_t chan, uint32_t totalSize, uint32_t* pageOffsets,
                      int* pageCount, int startLine, const char* str) {
  int sLen = (int)strlen(str);
  if (sLen == 0) {
    return -1;
  }

  int startPage = startLine / MORE_PAGE_LINES;
  if (startPage >= *pageCount) {
    startPage = 0;
    startLine = 0;
  }

  MoreSeekAbs(chan, totalSize, pageOffsets[startPage]);

  char buf[255];
  int lineNum = startPage * MORE_PAGE_LINES;

  while (lineNum < startLine) {
    if (MoreNextLine(chan, buf, sizeof(buf), totalSize, NULL) <= 0) {
      return -1;
    }
    lineNum++;
  }

  while (true) {
    uint8_t k = BufferGet();
    if (k == STOP_KEY || k == CTRL_C_KEY) {
      return -2;
    }

    uint32_t prePos;
    int r = MoreNextLine(chan, buf, sizeof(buf), totalSize, &prePos);
    if (r <= 0) {
      return -1;
    }

    if (lineNum > 0 && (lineNum % MORE_PAGE_LINES) == 0) {
      int pIdx = lineNum / MORE_PAGE_LINES;
      if (pIdx >= *pageCount && pIdx < MORE_MAX_PAGES) {
        pageOffsets[pIdx] = prePos;
        *pageCount = pIdx + 1;
      }
    }

    int bLen = (int)strlen(buf);
    for (int bi = 0; bi <= bLen - sLen; bi++) {
      if (strncasecmp(buf + bi, str, (size_t)sLen) == 0) {
        return lineNum;
      }
    }
    lineNum++;
  }
}

// MORE: pages through a server file. Records each page's byte offset as it goes
// so backwards paging and search can seek directly instead of re-reading from
// the start.
bool CmdMore(const char* args) {
  args = SkipWhitespace(args);
  if (!args || *args == '\0') {
    PrintError(ERR_SYNTAX);
    return false;
  }

  char filename[64];
  const char* after = ParseStringExpression(args, filename, sizeof(filename));
  if (!after || filename[0] == '\0') {
    strncpy(filename, args, sizeof(filename) - 1);
    filename[sizeof(filename) - 1] = '\0';
    for (int i = (int)strlen(filename) - 1; i >= 0 && filename[i] == ' '; i--) {
      filename[i] = '\0';
    }
  }
  if (filename[0] == '\0') {
    PrintError(ERR_SYNTAX);
    return false;
  }

  CommMsgSendFopen(MORE_CHANNEL, 0, filename);
  if (!WaitAck()) {
    PrintError(ERR_FOPEN);
    return false;
  }

  uint32_t totalSize = MoreFbytes(MORE_CHANNEL);
  if (totalSize == 0) {
    CommMsgSendFclose(MORE_CHANNEL);
    WaitAck();
    PrintStr((char*)"?EMPTY FILE");
    Newline();
    return true;
  }

  uint32_t* pageOffsets = (uint32_t*)malloc(MORE_MAX_PAGES * sizeof(uint32_t));
  if (!pageOffsets) {
    CommMsgSendFclose(MORE_CHANNEL);
    WaitAck();
    PrintStr((char*)"?OUT OF MEMORY");
    Newline();
    return false;
  }
  pageOffsets[0] = 0;
  int pageCount = 1;

  int currentPage = 0;
  uint32_t endPos = 0;
  bool atEof = MoreDisplayPage(MORE_CHANNEL, 0, totalSize, &endPos);
  if (!atEof && pageCount < MORE_MAX_PAGES) {
    pageOffsets[pageCount++] = endPos;
  }
  MoreStatusBar(currentPage, pageCount, atEof, NULL);

  char searchStr[41] = "";
  int searchStartLine = 0;

  while (true) {
    CursorHandler();
    uint8_t key = BufferGet();
    if (!key) {
      continue;
    }

    if (key == STOP_KEY || key == CTRL_C_KEY) {
      break;
    } else if (key == ' ') {
      if (atEof) {
        continue;
      }
      int nextPage = currentPage + 1;
      if (nextPage >= MORE_MAX_PAGES) {
        continue;
      }
      uint32_t newEnd = 0;
      atEof = MoreDisplayPage(MORE_CHANNEL, pageOffsets[nextPage], totalSize,
                              &newEnd);
      currentPage = nextPage;
      if (!atEof && currentPage + 1 >= pageCount &&
          pageCount < MORE_MAX_PAGES) {
        pageOffsets[pageCount++] = newEnd;
      }
      MoreStatusBar(currentPage, pageCount, atEof, NULL);
    } else if (key == BS_KEY) {
      if (currentPage == 0) {
        continue;
      }
      currentPage--;
      uint32_t newEnd = 0;
      atEof = MoreDisplayPage(MORE_CHANNEL, pageOffsets[currentPage], totalSize,
                              &newEnd);
      MoreStatusBar(currentPage, pageCount, atEof, NULL);
    } else if (key == ':') {
      SetReverseMode(false);
      LocateCursor(0, VID_HEIGHT - 1);
      SetReverseMode(true);
      char prompt[VID_WIDTH + 1];
      int pl = snprintf(prompt, sizeof(prompt), "/");
      while (pl < VID_WIDTH) {
        prompt[pl++] = ' ';
      }
      prompt[VID_WIDTH] = '\0';
      PrintStr(prompt);
      LocateCursor(1, VID_HEIGHT - 1);
      SetReverseMode(false);

      char inputBuf[41] = "";
      BasicReadLine(inputBuf, sizeof(inputBuf));

      if (inputBuf[0] != '\0') {
        strncpy(searchStr, inputBuf, sizeof(searchStr) - 1);
        searchStr[sizeof(searchStr) - 1] = '\0';
        searchStartLine = currentPage * MORE_PAGE_LINES;
      }

      if (searchStr[0] == '\0') {
        MoreStatusBar(currentPage, pageCount, atEof, NULL);
        continue;
      }

      char notice[VID_WIDTH + 1];
      snprintf(notice, sizeof(notice), "Searching: %s", searchStr);
      MoreStatusBar(currentPage, pageCount, atEof, notice);

      int found = MoreSearch(MORE_CHANNEL, totalSize, pageOffsets, &pageCount,
                             searchStartLine, searchStr);

      if (found == -1 && searchStartLine > 0) {
        found = MoreSearch(MORE_CHANNEL, totalSize, pageOffsets, &pageCount, 0,
                           searchStr);
      }

      if (found == -2) {
        MoreStatusBar(currentPage, pageCount, atEof, NULL);
        continue;
      }

      if (found < 0) {
        char nfMsg[VID_WIDTH + 1];
        snprintf(nfMsg, sizeof(nfMsg), "Not found: %s", searchStr);
        MoreStatusBar(currentPage, pageCount, atEof, nfMsg);
        searchStr[0] = '\0';
        searchStartLine = 0;
        continue;
      }

      searchStartLine = found + 1;
      int foundPage = found / MORE_PAGE_LINES;
      if (foundPage < pageCount) {
        currentPage = foundPage;
        uint32_t newEnd = 0;
        atEof = MoreDisplayPage(MORE_CHANNEL, pageOffsets[foundPage], totalSize,
                                &newEnd);
        if (!atEof && currentPage + 1 >= pageCount &&
            pageCount < MORE_MAX_PAGES) {
          pageOffsets[pageCount++] = newEnd;
        }
        MoreStatusBar(currentPage, pageCount, atEof, NULL);
      }
    }
  }

  SetScrollLock(false);
  SetReverseMode(false);
  CommMsgSendFclose(MORE_CHANNEL);
  WaitAck();
  free(pageOffsets);
  Clrscr(' ');
  LocateCursor(0, 0);
  return true;
}
