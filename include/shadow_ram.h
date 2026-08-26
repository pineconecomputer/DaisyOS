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

#include <Arduino.h>
#include "cursortype.h"
#include "video_messages.h"

#ifndef INCLUDE_SHADOW_RAM_H_
#define INCLUDE_SHADOW_RAM_H_

#define VID_WIDTH 40
#define VID_HEIGHT 25
#define CELL_WIDTH 8
#define CELL_HEIGHT 8

void PutStringAt(uint16_t vidmemcell, char* str, uint8_t len);
void Clrscr(byte val);
void PlotChar(uint8_t x, uint8_t y, uint8_t ch);
void FillCells(uint8_t x, uint8_t y, uint8_t ch, uint8_t count);
void Chrout(uint8_t ascii);
void LocateCursor(int x, int y);
void ToggleAttribute(void);
void ClearAttribute(void);
void Newline(void);
void MoveCursor(CursorDirection direction);
void SetScrollLock(bool en);
bool GetScrollLock(void);
bool GetVideoRamLine(uint8_t line, char* str);
void InitShadowRam(void);
void PrintStr(char* string);

bool HasContinue(uint8_t line);
void ClearContinue(uint8_t line);
uint8_t GetCurrentLine(void);
uint8_t GetCursorX(void);
uint8_t GetCursorY(void);
void WriteString(const char* str, uint16_t len);
void InsertCharAtCursor(uint8_t ch);
void DeleteCharAtCursor(void);
void SetLineGfxMode(uint8_t line, bool isGfx);
void ScrollX(uint8_t y, uint8_t height, int8_t direction);
void SetReverseMode(bool reverse);
bool GetReverseMode(void);
void SetReverseScreen(bool reverse);
uint8_t GetCharAt(uint8_t x, uint8_t y);
void SetAttribAt(uint8_t x, uint8_t y, uint8_t reversed);
uint8_t GetAttribAt(uint8_t x, uint8_t y);
void UpdateShadowPixel(uint8_t px, uint8_t py, uint8_t on);
void PlotPixel(uint8_t px, uint8_t py, uint8_t on);
uint8_t ReadPixelState(uint8_t px, uint8_t py);

void GetCharDef(uint8_t ch, uint8_t* bitrows);
void SetCharDef(uint8_t ch, const uint8_t* bitrows);
void GetCharROMDef(uint8_t ch, uint8_t* bitrows);
void ResetCharDef(uint8_t ch);
void GetGfxDef(uint8_t ch, uint8_t* bitrows);
void SetGfxDef(uint8_t ch, const uint8_t* bitrows);
void ResetGfxDef(uint8_t ch);
void FillBlockShadow(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                     uint8_t startChar, uint8_t endChar);

#endif  // INCLUDE_SHADOW_RAM_H_
