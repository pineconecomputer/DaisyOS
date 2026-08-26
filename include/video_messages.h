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

#ifndef INCLUDE_VIDEO_MESSAGES_H_
#define INCLUDE_VIDEO_MESSAGES_H_

#include <Arduino.h>
#include "cursortype.h"

#warning remove these once public api is implemented
uint8_t calcchecksum(uint8_t* data_in, size_t data_len);
uint8_t SendVideoMessage(uint8_t* msg, size_t msg_size);

void VideoMsgSendCursorMove(CursorDirection direction);
void VideoMsgSendChrOut(uint8_t c);
void VideoMsgSendNewline(void);
void VideoMsgSendLocateCursor(uint8_t x, uint8_t y);
void VideoMsgSendPutStringAtCell(uint16_t cell, char* mess, uint8_t len);
void VideoMsgSendPutAttribsAtCell(uint16_t cell, char* attr, uint8_t len);
void VideoMsgSendClearAttribAtCursor(void);
void VideoMsgSendToggleAttribAtCursor(void);
void VideoMsgSendSetScrollLock(bool en);
void VideoMsgSendClearScreen(uint8_t c);
void VideoMsgSendSetReverseMode(bool en);
void VideoMsgSendReverseScreen(bool en);
void VideoMsgSendCursorAdvance(bool en);
void VideoMsgSendSetPixel(uint8_t x, uint8_t y, uint8_t p);
void VideoMsgSendDrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1,
                          uint8_t p);
void VideoMsgSendScrollScreen(void);
void VideoMsgSendDefineChar(uint8_t ch, uint8_t* bitrows);
void VideoMsgSendResetChar(uint8_t ch);
void VideoMsgSendDefineGfx(uint8_t ch, uint8_t* bitrows);
void VideoMsgSendResetGfx(uint8_t ch);
void VideoMsgSendDefineCharBulk(uint8_t startCh, const uint8_t* bitrows,
                                uint8_t count);
void VideoMsgSendDefineGfxBulk(uint8_t startCh, const uint8_t* bitrows,
                               uint8_t count);
void VideoMsgSendPlotChar(uint8_t x, uint8_t y, uint8_t ch);
void VideoMsgSendFillCells(uint8_t x, uint8_t y, uint8_t ch, uint8_t count);
void VideoMsgSendMoveBlock(uint8_t fromX, uint8_t fromY, uint8_t toX,
                           uint8_t toY, uint8_t w, uint8_t h,
                           bool hasFill = false, uint8_t fillChar = 0);
void VideoMsgSendFillBlock(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                           uint8_t startChar, uint8_t endChar);
void VideoMsgSendDrawCircle(uint8_t x, uint8_t y, uint8_t xr, uint8_t yr,
                            uint8_t p, uint16_t s_deg, uint16_t e_deg);
void VideoMsgSendFloodFill(uint8_t x, uint8_t y, uint8_t p);
void VideoMsgSendCopyChar(bool srcGfx, bool dstGfx, uint8_t start, uint8_t end);
void VideoMsgSendScrollX(uint8_t y, uint8_t height, int8_t direction);
void VideoMsgSendDrawPoly(uint8_t nErase, uint8_t nDraw, const uint8_t* pts,
                          uint8_t totalPts);
#endif  // INCLUDE_VIDEO_MESSAGES_H_
