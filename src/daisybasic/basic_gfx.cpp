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

// LINE: draws a character-cell line between two points, accepting the fill
// character as a code or a quoted literal.
bool CmdLine(const char* args) {
  float x1f, y1f, x2f, y2f;
  char ch;

  args = ParseExpression(args, &x1f);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;

  args = ParseExpression(args, &y1f);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;

  args = ParseExpression(args, &x2f);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;

  args = ParseExpression(args, &y2f);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;

  args = SkipWhitespace(args);
  if (*args == '"') {
    args++;
    if (*args == '"' || *args == '\0') {
      PrintError(ERR_SYNTAX);
      return false;
    }
    ch = *args;
  } else {
    float chf;
    args = ParseExpression(args, &chf);
    if (!args) {
      return false;
    }
    ch = (char)(int)chf;
  }

  int x1 = (int)x1f, y1 = (int)y1f, x2 = (int)x2f, y2 = (int)y2f;
  uint8_t uch = (uint8_t)ch;

  int dx = x2 - x1, dy = y2 - y1;
  int sx = (dx > 0) ? 1 : -1;
  int sy = (dy > 0) ? 1 : -1;
  if (dx < 0) {
    dx = -dx;
  }
  if (dy < 0) {
    dy = -dy;
  }

  int err = dx - dy;
  int x = x1, y = y1;

  while (1) {
    if (x >= 0 && y >= 0) {
      PlotChar((uint8_t)x, (uint8_t)y, uch);
    }
    if (x == x2 && y == y2) {
      break;
    }
    int e2 = err << 1;
    if (e2 > -dy) {
      err -= dy;
      x += sx;
    }
    if (e2 < dx) {
      err += dx;
      y += sy;
    }
  }
  return true;
}

// Shared body of DEFCHAR and DEFGFX. Omitted rows keep their current value, so
// a single row can be changed without restating the whole glyph.
static bool DefBitmapCmd(const char* args, bool isGfx) {
  float charNumF;
  float vals[8];
  bool hasValue[8] = {false, false, false, false, false, false, false, false};

  args = ParseExpression(args, &charNumF);
  if (!args) {
    return false;
  }

  for (int i = 0; i < 8; i++) {
    args = SkipWhitespace(args);
    if (*args == ',') {
      args++;
    }
    args = SkipWhitespace(args);
    if (*args == ',' || *args == '\0' || *args == ':') {
      hasValue[i] = false;
    } else {
      const char* result = ParseExpression(args, &vals[i]);
      if (!result) {
        return false;
      }
      args = result;
      hasValue[i] = true;
    }
  }

  uint8_t charNum = (uint8_t)(int)charNumF;
  uint8_t bitrows[8];
  if (isGfx) {
    GetGfxDef(charNum, bitrows);
  } else {
    GetCharDef(charNum, bitrows);
  }
  for (int i = 0; i < 8; i++) {
    if (hasValue[i]) {
      bitrows[i] = (uint8_t)(int)vals[i];
    }
  }
  if (isGfx) {
    SetGfxDef(charNum, bitrows);
    VideoMsgSendDefineGfx(charNum, bitrows);
  } else {
    SetCharDef(charNum, bitrows);
    VideoMsgSendDefineChar(charNum, bitrows);
  }
  return true;
}

// DEFCHAR: redefines a glyph in the text character set.
bool CmdDefChar(const char* args) { return DefBitmapCmd(args, false); }
// DEFGFX: redefines a glyph in the graphics character set.
bool CmdDefGfx(const char* args) { return DefBitmapCmd(args, true); }

// Shared body of RESETCHAR and RESETGFX: restores one glyph from ROM, or the
// whole set when given no argument.
static bool ResetBitmapCmd(const char* args, bool isGfx) {
  args = SkipWhitespace(args);
  if (*args == '\0' || *args == ':') {
    for (int ch = 0; ch < 256; ch++) {
      if (isGfx) {
        ResetGfxDef((uint8_t)ch);
        VideoMsgSendResetGfx((uint8_t)ch);
      } else {
        ResetCharDef((uint8_t)ch);
        VideoMsgSendResetChar((uint8_t)ch);
      }
    }
    return true;
  }
  float charNumF;
  args = ParseExpression(args, &charNumF);
  if (!args) {
    return false;
  }
  uint8_t charNum = (uint8_t)(int)charNumF;
  if (isGfx) {
    ResetGfxDef(charNum);
    VideoMsgSendResetGfx(charNum);
  } else {
    ResetCharDef(charNum);
    VideoMsgSendResetChar(charNum);
  }
  return true;
}

// RESETCHAR: restores one text glyph from ROM, or all of them.
bool CmdResetChar(const char* args) { return ResetBitmapCmd(args, false); }
// RESETGFX: restores one graphics glyph from ROM, or all of them.
bool CmdResetGfx(const char* args) { return ResetBitmapCmd(args, true); }

// CHARMODE: selects the text or graphics character set for a range of rows.
bool CmdCharMode(const char* args) {
  args = SkipWhitespace(args);

  bool isGfx;
  if (strncasecmp(args, "GFX", 3) == 0 && !isalnum(args[3])) {
    isGfx = true;
    args += 3;
  } else if (strncasecmp(args, "CHAR", 4) == 0 && !isalnum(args[4])) {
    isGfx = false;
    args += 4;
  } else {
    return false;
  }

  args = SkipWhitespace(args);
  if (*args != ',') {
    return false;
  }
  args++;

  float firstF;
  args = ParseExpression(args, &firstF);
  if (!args) {
    return false;
  }
  int firstLine = (int)firstF;
  if (firstLine < 0 || firstLine >= VID_HEIGHT) {
    return false;
  }

  int lastLine = firstLine;
  args = SkipWhitespace(args);
  if (*args == ',') {
    args++;
    float lastF;
    const char* next = ParseExpression(args, &lastF);
    if (next) {
      lastLine = (int)lastF;
      if (lastLine < firstLine) {
        lastLine = firstLine;
      }
      if (lastLine >= VID_HEIGHT) {
        lastLine = VID_HEIGHT - 1;
      }
      args = next;
    }
  }

  for (int line = firstLine; line <= lastLine; line++) {
    SetLineGfxMode((uint8_t)line, isGfx);
  }
  return true;
}

// CLS: clears the screen and homes the cursor.
bool CmdCls(const char* args) {
  (void)args;
  Clrscr(' ');
  LocateCursor(0, 0);
  return true;
}

// REVERSE: turns on reverse video for later text, or inverts the whole screen
// with the SCREEN keyword.
bool CmdReverse(const char* args) {
  args = SkipWhitespace(args);
  if (strncasecmp(args, "SCREEN", 6) == 0 && !isalnum(args[6])) {
    SetReverseScreen(true);
    return true;
  }
  SetReverseMode(true);
  return true;
}

// NORMAL: undoes REVERSE, for later text or for the whole screen.
bool CmdNormal(const char* args) {
  args = SkipWhitespace(args);
  if (strncasecmp(args, "SCREEN", 6) == 0 && !isalnum(args[6])) {
    SetReverseScreen(false);
    return true;
  }
  SetReverseMode(false);
  return true;
}

// PLOTCHAR: writes a character to a cell without moving the cursor or changing
// the cell's attribute.
bool CmdPlotChar(const char* args) {
  float cf, rf, chf;
  args = ParseExpression(args, &cf);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &rf);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &chf);
  if (!args) {
    return false;
  }
  PlotChar((uint8_t)(int)cf, (uint8_t)(int)rf, (uint8_t)(int)chf);
  return true;
}

// FILLCELLS: fills a run of cells, wrapping onto following rows.
bool CmdFillCells(const char* args) {
  float cf, rf, chf, countf;
  args = ParseExpression(args, &cf);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &rf);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &chf);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &countf);
  if (!args) {
    return false;
  }
  FillCells((uint8_t)(int)cf, (uint8_t)(int)rf, (uint8_t)(int)chf,
            (uint8_t)(int)countf);
  return true;
}

// HLINE: fills a horizontal run, swapping reversed endpoints and clipping.
bool CmdHLine(const char* args) {
  float x1f, x2f, yf, chf;
  args = ParseExpression(args, &x1f);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &x2f);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &yf);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &chf);
  if (!args) {
    return false;
  }

  int x1 = (int)x1f, x2 = (int)x2f, y = (int)yf;
  if (x1 > x2) {
    int t = x1;
    x1 = x2;
    x2 = t;
  }
  if (x1 < 0) {
    x1 = 0;
  }
  if (x2 >= VID_WIDTH) {
    x2 = VID_WIDTH - 1;
  }
  if (y < 0 || y >= VID_HEIGHT) {
    return true;
  }
  FillCells((uint8_t)x1, (uint8_t)y, (uint8_t)(int)chf, (uint8_t)(x2 - x1 + 1));
  return true;
}

// VLINE: fills a vertical run, swapping reversed endpoints and clipping.
bool CmdVLine(const char* args) {
  float xf, y1f, y2f, chf;
  args = ParseExpression(args, &xf);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &y1f);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &y2f);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &chf);
  if (!args) {
    return false;
  }

  int x = (int)xf, y1 = (int)y1f, y2 = (int)y2f;
  if (y1 > y2) {
    int t = y1;
    y1 = y2;
    y2 = t;
  }
  if (x < 0 || x >= VID_WIDTH) {
    return true;
  }
  if (y1 < 0) {
    y1 = 0;
  }
  if (y2 >= VID_HEIGHT) {
    y2 = VID_HEIGHT - 1;
  }
  uint8_t uch = (uint8_t)(int)chf;
  for (int y = y1; y <= y2; y++) {
    PlotChar((uint8_t)x, (uint8_t)y, uch);
  }
  return true;
}

// SETATTRIB: sets one cell to normal or reverse video, leaving its character
// alone.
bool CmdSetAttrib(const char* args) {
  float fx, fy, fz;
  args = ParseExpression(args, &fx);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &fy);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &fz);
  if (!args) {
    return false;
  }
  SetAttribAt((uint8_t)(int)fx, (uint8_t)(int)fy, (int)fz != 0 ? 1 : 0);
  return true;
}

// BOX: draws a rectangular border from the box-drawing glyphs, or erases it
// with spaces. Corners are swapped and clipped as needed.
bool CmdBox(const char* args) {
  float xf, yf, wf, lf, pf;
  args = ParseExpression(args, &xf);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &yf);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &wf);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &lf);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &pf);
  if (!args) {
    return false;
  }

  int x1 = (int)xf, y1 = (int)yf, x2 = (int)wf, y2 = (int)lf;
  bool draw = ((int)pf != 0);
  uint8_t sp = 32;

  if (x1 > x2) {
    int t = x1;
    x1 = x2;
    x2 = t;
  }
  if (y1 > y2) {
    int t = y1;
    y1 = y2;
    y2 = t;
  }

  if (x1 < 0) {
    x1 = 0;
  }
  if (y1 < 0) {
    y1 = 0;
  }
  if (x2 >= VID_WIDTH) {
    x2 = VID_WIDTH - 1;
  }
  if (y2 >= VID_HEIGHT) {
    y2 = VID_HEIGHT - 1;
  }
  if (x1 > x2 || y1 > y2) {
    return true;
  }

  PlotChar((uint8_t)x1, (uint8_t)y1, draw ? 22 : sp);
  for (int x = x1 + 1; x < x2; x++) {
    PlotChar((uint8_t)x, (uint8_t)y1, draw ? 26 : sp);
  }
  if (x2 > x1) {
    PlotChar((uint8_t)x2, (uint8_t)y1, draw ? 28 : sp);
  }

  for (int y = y1 + 1; y < y2; y++) {
    PlotChar((uint8_t)x1, (uint8_t)y, draw ? 21 : sp);
    if (x2 > x1) {
      PlotChar((uint8_t)x2, (uint8_t)y, draw ? 21 : sp);
    }
  }

  if (y2 > y1) {
    PlotChar((uint8_t)x1, (uint8_t)y2, draw ? 19 : sp);
    for (int x = x1 + 1; x < x2; x++) {
      PlotChar((uint8_t)x, (uint8_t)y2, draw ? 26 : sp);
    }
    if (x2 > x1) {
      PlotChar((uint8_t)x2, (uint8_t)y2, draw ? 25 : sp);
    }
  }

  return true;
}

// MOVEBLOCK: copies a rectangle of cells, optionally filling the source
// afterwards. The video board handles overlap correctly.
bool CmdMoveBlock(const char* args) {
  float fx1, fy1, fx2, fy2, fw, fh;

  args = ParseExpression(args, &fx1);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &fy1);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &fx2);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &fy2);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &fw);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &fh);
  if (!args) {
    return false;
  }

  args = SkipWhitespace(args);
  bool hasFill = (*args == ',');
  uint8_t fillChar = 0;
  if (hasFill) {
    args++;
    float ff;
    args = ParseExpression(args, &ff);
    if (!args) {
      return false;
    }
    fillChar = (uint8_t)(int)ff;
  }

  VideoMsgSendMoveBlock((uint8_t)(int)fx1, (uint8_t)(int)fy1, (uint8_t)(int)fx2,
                        (uint8_t)(int)fy2, (uint8_t)(int)fw, (uint8_t)(int)fh,
                        hasFill, fillChar);
  return true;
}

// FILLBLOCK: fills a rectangle with one character, a repeating range, or an
// ascending sequence, depending on how the arguments are given.
bool CmdFillBlock(const char* args) {
  float fx, fy, fw, fh, fc;

  args = ParseExpression(args, &fx);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &fy);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &fw);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &fh);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &fc);
  if (!args) {
    return false;
  }

  uint8_t startChar = (uint8_t)(int)fc;
  uint8_t endChar = startChar;

  args = SkipWhitespace(args);
  if (*args == ',') {
    args++;
    float fe;
    const char* next = ParseExpression(args, &fe);
    if (next) {
      endChar = (uint8_t)(int)fe;
      args = next;
    } else {
      endChar = (uint8_t)(startChar - 1);
    }
  }

  uint8_t bx = (uint8_t)(int)fx, by = (uint8_t)(int)fy;
  uint8_t bw = (uint8_t)(int)fw, bh = (uint8_t)(int)fh;
  FillBlockShadow(bx, by, bw, bh, startChar, endChar);
  VideoMsgSendFillBlock(bx, by, bw, bh, startChar, endChar);
  return true;
}

// PPLOT: sets one pixel off, solid, or dithered.
bool CmdPPlot(const char* args) {
  float xf, yf, pf;

  args = ParseExpression(args, &xf);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &yf);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &pf);
  if (!args) {
    return false;
  }

  PlotPixel((uint8_t)(int)xf, (uint8_t)(int)yf, (uint8_t)(int)pf);
  return true;
}

// Bresenham line drawn into the local mirror only. Used when the video board is
// being sent one batched command for the whole shape.
static void DrawLineInShadow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1,
                             uint8_t on) {
  int dx = abs((int)x1 - (int)x0);
  int dy = -abs((int)y1 - (int)y0);
  int sx = x0 < x1 ? 1 : -1;
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  int cx = x0, cy = y0;
  for (;;) {
    UpdateShadowPixel((uint8_t)cx, (uint8_t)cy, on);
    if (cx == x1 && cy == y1) {
      break;
    }
    int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      cx += sx;
    }
    if (e2 <= dx) {
      err += dx;
      cy += sy;
    }
  }
}

// PLINE: draws a pixel line, or a rectangle with the B and BF suffixes.
bool CmdPLine(const char* args) {
  float x0f, y0f, x1f, y1f, pf;

  args = ParseExpression(args, &x0f);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &y0f);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &x1f);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &y1f);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &pf);
  if (!args) {
    return false;
  }

  uint8_t x0 = (uint8_t)(int)x0f, y0 = (uint8_t)(int)y0f;
  uint8_t x1 = (uint8_t)(int)x1f, y1 = (uint8_t)(int)y1f;
  uint8_t on = (uint8_t)(int)pf;

  args = SkipWhitespace(args);
  bool boxMode = false, fillMode = false;
  if (*args == ',') {
    const char* p = SkipWhitespace(args + 1);
    if (*p == 'B' || *p == 'b') {
      const char* q = p + 1;
      if (*q == 'F' || *q == 'f') {
        fillMode = true;
      } else {
        boxMode = true;
      }
    }
  }

  if (x0 > x1) {
    uint8_t t = x0;
    x0 = x1;
    x1 = t;
  }
  if (y0 > y1) {
    uint8_t t = y0;
    y0 = y1;
    y1 = t;
  }

  if (fillMode) {
    for (uint8_t y = y0; y <= y1; y++) {
      DrawLineInShadow(x0, y, x1, y, on);
      VideoMsgSendDrawLine(x0, y, x1, y, on);
    }
  } else if (boxMode) {
    DrawLineInShadow(x0, y0, x1, y0, on);
    VideoMsgSendDrawLine(x0, y0, x1, y0, on);
    DrawLineInShadow(x0, y1, x1, y1, on);
    VideoMsgSendDrawLine(x0, y1, x1, y1, on);
    DrawLineInShadow(x0, y0, x0, y1, on);
    VideoMsgSendDrawLine(x0, y0, x0, y1, on);
    DrawLineInShadow(x1, y0, x1, y1, on);
    VideoMsgSendDrawLine(x1, y0, x1, y1, on);
  } else {
    DrawLineInShadow(x0, y0, x1, y1, on);
    VideoMsgSendDrawLine(x0, y0, x1, y1, on);
  }
  return true;
}

static bool DoFloodFill(int sx, int sy, uint8_t fill_val);

// Tests whether a point's angle falls inside an arc. Handles ranges that wrap
// past zero, and negates dy because screen y grows downward.
static bool pix_in_arc(int dx, int dy, int s_deg, int e_deg) {
  float ang = atan2f(-(float)dy, (float)dx) * (180.0f / (float)M_PI);
  if (ang < 0.0f) {
    ang += 360.0f;
  }
  int a = (int)ang;
  if (s_deg <= e_deg) {
    return (a >= s_deg && a <= e_deg);
  }
  return (a >= s_deg || a <= e_deg);
}

// Plots one ellipse pixel if it lies within the arc and on screen.
static void ecell(int px, int py, int dx, int dy, uint8_t on, bool full,
                  int s_deg, int e_deg) {
  if (!full && !pix_in_arc(dx, dy, s_deg, e_deg)) {
    return;
  }
  if (px < 0 || px >= 80 || py < 0 || py >= 50) {
    return;
  }
  UpdateShadowPixel((uint8_t)px, (uint8_t)py, on);
}

// Plots the four symmetric reflections of an ellipse point, so the rasteriser
// only has to walk one quadrant.
static void eplot4(int cx, int cy, int32_t x, int32_t y, uint8_t on, bool full,
                   int s_deg, int e_deg) {
  ecell(cx + (int)x, cy - (int)y, (int)x, -(int)y, on, full, s_deg, e_deg);
  ecell(cx - (int)x, cy - (int)y, -(int)x, -(int)y, on, full, s_deg, e_deg);
  ecell(cx + (int)x, cy + (int)y, (int)x, (int)y, on, full, s_deg, e_deg);
  ecell(cx - (int)x, cy + (int)y, -(int)x, (int)y, on, full, s_deg, e_deg);
}

// Midpoint ellipse rasteriser, integer-only. Walks the two arcs where the slope
// favours stepping x then y, and draws into the mirror.
static void ellipse_shadow(int cx, int cy, int rx, int ry, uint8_t on,
                           int s_deg, int e_deg) {
  bool full = (s_deg == 0 && e_deg == 360);
  int32_t a2 = (int32_t)rx * rx, b2 = (int32_t)ry * ry;
  int32_t fa2 = 4 * a2, fb2 = 4 * b2;
  int32_t x, y, sigma;

  x = 0;
  y = ry;
  sigma = 2 * b2 + a2 * (1 - 2 * ry);
  for (; b2 * x <= a2 * y; x++) {
    eplot4(cx, cy, x, y, on, full, s_deg, e_deg);
    if (sigma >= 0) {
      sigma += fa2 * (1 - y);
      y--;
    }
    sigma += b2 * (4 * x + 6);
  }

  x = rx;
  y = 0;
  sigma = 2 * a2 + b2 * (1 - 2 * rx);
  for (; a2 * y <= b2 * x; y++) {
    eplot4(cx, cy, x, y, on, full, s_deg, e_deg);
    if (sigma >= 0) {
      sigma += fb2 * (1 - x);
      x--;
    }
    sigma += a2 * (4 * y + 6);
  }
}

// PCIRCLE: draws an ellipse or arc, optionally flood-filled afterwards.
bool CmdPCircle(const char* args) {
  float xf, yf, xrf, yrf, pf;

  args = ParseExpression(args, &xf);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &yf);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &xrf);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &yrf);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &pf);
  if (!args) {
    return false;
  }

  int s_deg = 0, e_deg = 360;
  bool do_fill = false;
  args = SkipWhitespace(args);
  if (*args == ',') {
    args++;
    args = SkipWhitespace(args);
    if (toupper(*args) == 'F' &&
        (!args[1] || isspace(args[1]) || args[1] == ':')) {
      do_fill = true;
      args++;
    } else {
      float sf;
      args = ParseExpression(args, &sf);
      if (!args) {
        return false;
      }
      s_deg = (int)sf;
      if (s_deg < 0) {
        s_deg = 0;
      }
      if (s_deg > 360) {
        s_deg = 360;
      }
      args = SkipWhitespace(args);
      if (*args == ',') {
        args++;
        args = SkipWhitespace(args);
        if (toupper(*args) == 'F' &&
            (!args[1] || isspace(args[1]) || args[1] == ':')) {
          do_fill = true;
          args++;
        } else {
          float ef;
          args = ParseExpression(args, &ef);
          if (!args) {
            return false;
          }
          e_deg = (int)ef;
          if (e_deg < 0) {
            e_deg = 0;
          }
          if (e_deg > 360) {
            e_deg = 360;
          }
          args = SkipWhitespace(args);
          if (*args == ',') {
            args++;
            args = SkipWhitespace(args);
            if (toupper(*args) == 'F') {
              do_fill = true;
              args++;
            }
          }
        }
      }
    }
  }

  int rx = (int)xrf, ry = (int)yrf;
  if (rx < 0) {
    rx = 0;
  }
  if (ry < 0) {
    ry = 0;
  }
  if (rx == 0 && ry == 0) {
    return true;
  }

  ellipse_shadow((int)xf, (int)yf, rx, ry, (uint8_t)(int)pf, s_deg, e_deg);
  VideoMsgSendDrawCircle((uint8_t)(int)xf, (uint8_t)(int)yf, (uint8_t)rx,
                         (uint8_t)ry, (uint8_t)(int)pf, (uint16_t)s_deg,
                         (uint16_t)e_deg);

  if (do_fill) {
    int fv = (int)pf;
    if (fv < 0) {
      fv = 0;
    }
    if (fv > 2) {
      fv = 2;
    }
    DoFloodFill((int)xf, (int)yf, (uint8_t)fv);
  }

  return true;
}

#define PFILL_STACK_SIZE 256

// Reads a pixel for the flood fill, returning an out-of-range marker off screen
// so the edge acts as a boundary.
static uint8_t pfill_get(int px, int py) {
  if (px < 0 || px >= 80 || py < 0 || py >= 50) {
    return 3;
  }
  return ReadPixelState((uint8_t)px, (uint8_t)py);
}

// Four-way flood fill using an explicit stack rather than recursion, since deep
// recursion would overflow the interpreter's stack on a large region.
static bool DoFloodFill(int sx, int sy, uint8_t fill_val) {
  if (sx < 0 || sx >= 80 || sy < 0 || sy >= 50) {
    return true;
  }

  uint8_t target = pfill_get(sx, sy);
  if (target > 2 || target == fill_val) {
    return true;
  }

  struct PFPix {
    uint8_t x, y;
  };
  PFPix stk[PFILL_STACK_SIZE];
  int sp = 0;

  UpdateShadowPixel((uint8_t)sx, (uint8_t)sy, fill_val);
  stk[sp].x = (uint8_t)sx;
  stk[sp].y = (uint8_t)sy;
  sp = 1;

  static const int8_t ndx[4] = {0, 0, 1, -1};
  static const int8_t ndy[4] = {-1, 1, 0, 0};

  while (sp > 0) {
    sp--;
    uint8_t cx = stk[sp].x, cy = stk[sp].y;
    for (int d = 0; d < 4; d++) {
      int nx = (int)cx + ndx[d];
      int ny = (int)cy + ndy[d];
      if (pfill_get(nx, ny) == target) {
        UpdateShadowPixel((uint8_t)nx, (uint8_t)ny, fill_val);
        if (sp < PFILL_STACK_SIZE) {
          stk[sp].x = (uint8_t)nx;
          stk[sp].y = (uint8_t)ny;
          sp++;
        }
      }
    }
  }

  VideoMsgSendFloodFill((uint8_t)sx, (uint8_t)sy, fill_val);
  return true;
}

// PFILL: flood-fills the region containing a point.
bool CmdPFill(const char* args) {
  float xf, yf, pf;

  args = ParseExpression(args, &xf);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &yf);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = ParseExpression(args, &pf);
  if (!args) {
    return false;
  }

  int pval = (int)pf;
  if (pval < 0) {
    pval = 0;
  }
  if (pval > 2) {
    pval = 2;
  }
  return DoFloodFill((int)xf, (int)yf, (uint8_t)pval);
}

// Packs an Nx2 integer array of coordinates into the wire format, rejecting an
// array that is not two columns wide.
static int ExtractPolyPoints(ArrayDescriptor* arr, uint8_t* buf, int maxPts) {
  if (arr->dim2Size != 2) {
    PrintError(ERR_WRONG_DIMENSIONS);
    return -1;
  }
  int n = arr->dim1Size;
  if (n > maxPts) {
    n = maxPts;
  }
  int16_t* data = (int16_t*)arr->data;
  for (int i = 0; i < n; i++) {
    buf[i * 2] = (uint8_t)data[i * 2];
    buf[i * 2 + 1] = (uint8_t)data[i * 2 + 1];
  }
  return n;
}

// Bresenham line into the mirror only, used while building a polygon.
static void ShadowLine(int x0, int y0, int x1, int y1, uint8_t on) {
  int dx = abs(x1 - x0);
  int dy = -abs(y1 - y0);
  int sx = x0 < x1 ? 1 : -1;
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    if (x0 >= 0 && y0 >= 0) {
      UpdateShadowPixel((uint8_t)x0, (uint8_t)y0, on);
    }
    if (x0 == x1 && y0 == y1) {
      break;
    }
    int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

// Draws a closed polygon into the mirror, joining the last point back to the
// first.
static void ShadowPoly(const uint8_t* pts, int n, uint8_t on) {
  if (n < 2) {
    if (n == 1) {
      int px = (int8_t)pts[0], py = (int8_t)pts[1];
      if (px >= 0 && py >= 0) {
        UpdateShadowPixel((uint8_t)px, (uint8_t)py, on);
      }
    }
    return;
  }
  for (int i = 0; i < n; i++) {
    int i2 = (i + 1 < n) ? i + 1 : 0;
    ShadowLine((int8_t)pts[i * 2], (int8_t)pts[i * 2 + 1], (int8_t)pts[i2 * 2],
               (int8_t)pts[i2 * 2 + 1], on);
  }
}

// PPOLY: draws a polygon, or erases one and draws another in a single video
// message so animation does not flicker.
bool CmdPPoly(const char* args) {
  char name1[MAX_VAR_NAME], name2[MAX_VAR_NAME];

  args = SkipWhitespace(args);
  args = ParseVarName(args, name1, sizeof(name1));
  if (!args) {
    PrintError(ERR_SYNTAX);
    return false;
  }

  if (!IsIntArrayVar(name1)) {
    PrintError(ERR_TYPE_MISMATCH);
    return false;
  }
  ArrayDescriptor* arr1 = FindArray(name1);
  if (!arr1) {
    PrintError(ERR_ARRAY_NOT_DIMD);
    return false;
  }

  ArrayDescriptor *arrErase = NULL, *arrDraw = arr1;
  args = SkipWhitespace(args);
  if (*args == ',') {
    args++;
    args = SkipWhitespace(args);
    args = ParseVarName(args, name2, sizeof(name2));
    if (!args) {
      PrintError(ERR_SYNTAX);
      return false;
    }
    if (!IsIntArrayVar(name2)) {
      PrintError(ERR_TYPE_MISMATCH);
      return false;
    }
    ArrayDescriptor* arr2 = FindArray(name2);
    if (!arr2) {
      PrintError(ERR_ARRAY_NOT_DIMD);
      return false;
    }
    arrErase = arr1;
    arrDraw = arr2;
  }

#define PPOLY_MAX_PTS 125
  uint8_t pts[PPOLY_MAX_PTS * 2];
  int nErase = 0, nDraw = 0;

  if (arrErase) {
    nErase = ExtractPolyPoints(arrErase, pts, PPOLY_MAX_PTS / 2);
    if (nErase < 0) {
      return false;
    }
  }
  nDraw = ExtractPolyPoints(arrDraw, &pts[nErase * 2], PPOLY_MAX_PTS - nErase);
  if (nDraw < 0) {
    return false;
  }

  if (nErase > 0) {
    ShadowPoly(pts, nErase, 0);
  }
  ShadowPoly(&pts[nErase * 2], nDraw, 1);

  VideoMsgSendDrawPoly((uint8_t)nErase, (uint8_t)nDraw, pts,
                       (uint8_t)(nErase + nDraw));
  return true;
}

// SCROLL: enables or disables automatic scrolling at the bottom of the screen.
bool CmdScroll(const char* args) {
  args = SkipWhitespace(args);
  if (strncasecmp(args, "ON", 2) == 0 && !isalnum(args[2])) {
    VideoMsgSendSetScrollLock(true);
    return true;
  }
  if (strncasecmp(args, "OFF", 3) == 0 && !isalnum(args[3])) {
    VideoMsgSendSetScrollLock(false);
    return true;
  }
  float val;
  if (ParseExpression(args, &val)) {
    VideoMsgSendSetScrollLock((int)val != 0);
    return true;
  }
  PrintError(ERR_SYNTAX);
  return false;
}

// SCROLLX: rotates a band of rows sideways, wrapping at the edges.
bool CmdScrollX(const char* args) {
  float fy, fh, fdir;

  args = ParseExpression(args, &fy);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    return false;
  }
  args++;

  args = ParseExpression(args, &fh);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    return false;
  }
  args++;

  args = ParseExpression(args, &fdir);
  if (!args) {
    return false;
  }

  int y = (int)fy;
  int h = (int)fh;
  int dir = (int)fdir;
  if (y < 0 || y >= 25 || h < 1) {
    PrintError(ERR_ILLEGAL_QUANTITY);
    return false;
  }
  if (dir == 0) {
    return true;
  }

  ScrollX((uint8_t)y, (uint8_t)h, (int8_t)dir);
  return true;
}

// Parses a CHAR or GFX keyword naming which character set to act on.
static bool ParseCharGfx(const char* args, bool* isGfx, const char** next) {
  args = SkipWhitespace(args);
  if (strncasecmp(args, "GFX", 3) == 0 && !isalnum(args[3])) {
    *isGfx = true;
    *next = args + 3;
    return true;
  }
  if (strncasecmp(args, "CHAR", 4) == 0 && !isalnum(args[4])) {
    *isGfx = false;
    *next = args + 4;
    return true;
  }
  return false;
}

// COPYCHAR: copies glyphs between the two character sets, or duplicates one
// glyph within a set.
bool CmdCopyChar(const char* args) {
  bool srcGfx, dstGfx;

  if (!ParseCharGfx(args, &srcGfx, &args)) {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;

  if (!ParseCharGfx(args, &dstGfx, &args)) {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;

  float fstart;
  args = ParseExpression(args, &fstart);
  if (!args) {
    PrintError(ERR_SYNTAX);
    return false;
  }
  uint8_t start = (uint8_t)(int)fstart;

  uint8_t end = start;
  args = SkipWhitespace(args);
  if (*args == ',') {
    args++;
    float fend;
    const char* next = ParseExpression(args, &fend);
    if (next) {
      end = (uint8_t)(int)fend;
      args = next;
    }
  }

  VideoMsgSendCopyChar(srcGfx, dstGfx, start, end);
  return true;
}
