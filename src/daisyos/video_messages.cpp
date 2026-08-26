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

#include "video_messages.h"
#include "cursortype.h"

const uint8_t kPayloadByteOffset = 3;
// Pacing for the outgoing link. The receiver services its UART only in the
// gaps left by its own real-time work, so bytes cannot be sent back to back.
// The trailing gap lets the last byte land before the next message starts.
const uint16_t kInterByteUs = 350;
const uint16_t kInterFrameUs = 150;

const uint8_t kSOP = 0x5c;

typedef enum {
  kVideoPutStringAt = 0x01,
  kVideoPutAttribsAt = 0x02,
  kVideoToggleAttribAtCursor = 0x03,
  kVideoClearAttributeAtCursor = 0x06,
  kVideoCharOutAt = 0x04,
  kVideoLocateCursor = 0x05,
  kVideoClearScreen = 0x10,
  kVideoReverseScreen = 0x11,
  kVideoSetCursorAdvance = 0x40,
  kVideoNewline = 0x41,
  kVideoCursorMove = 0x43,
  kVideoSetScrollLock = 0x44,
  kVideoScrollScreen = 0x45,
  kVideoSetPixel = 0x50,
  kVideoDefineChar = 0x60,
  kVideoResetChar = 0x61,
  kVideoDefineGfx = 0x62,
  kVideoResetGfx = 0x63,
  kVideoDefineCharBulk = 0x64,
  kVideoDefineGfxBulk = 0x65,
  kVideoCopyChar = 0x66,
  kVideoPlotChar = 0x70,
  kVideoFillCells = 0x71,
  kVideoMoveBlock = 0x72,
  kVideoFillBlock = 0x73,
  kVideoScrollX = 0x77,
  kVideoDrawPoly = 0x54,
} VideoMsgId;

// Two's-complement checksum, so a valid frame plus its checksum byte sums to
// zero. Must match the verifier in DaisyVideo.
uint8_t calcchecksum(uint8_t* data_in, size_t data_len) {
  uint8_t raw_sum = 0;

  for (size_t i = 0; i < data_len; i++) {
    raw_sum += data_in[i];
  }

  return (uint8_t)(~raw_sum + 1);
}

// Writes a framed message to DaisyVideo and appends its checksum. The
// inter-byte delays throttle the sender: DaisyVideo only services its UART in
// the gaps between scanlines, so it cannot take bytes back to back.
uint8_t SendVideoMessage(uint8_t* msg, size_t msg_size) {
  uint8_t msg_checksum = calcchecksum(msg, msg_size);
  for (uint8_t pos = 0; pos < msg_size; pos++) {
    Serial2.write(msg[pos]);
    delayMicroseconds(kInterByteUs);
  }
  Serial2.write(msg_checksum);
  delayMicroseconds(kInterFrameUs);
  return msg_size + 1;
}

// Moves the hardware cursor one cell.
void VideoMsgSendCursorMove(CursorDirection direction) {
  uint8_t movecursor[4];
  movecursor[0] = kSOP;
  movecursor[1] = kVideoCursorMove;
  movecursor[2] = 1;
  movecursor[kPayloadByteOffset] = (uint8_t)direction;
  SendVideoMessage(movecursor, sizeof(movecursor));
}

// Writes one character at the cursor and advances it.
void VideoMsgSendChrOut(uint8_t c) {
  uint8_t chrout[4];
  chrout[0] = kSOP;
  chrout[1] = kVideoCharOutAt;
  chrout[2] = 1;
  chrout[kPayloadByteOffset] = c;
  SendVideoMessage(chrout, sizeof(chrout));
}

// Moves the cursor to the start of the next line, scrolling if needed.
void VideoMsgSendNewline(void) {
  uint8_t chrout[3];
  chrout[0] = kSOP;
  chrout[1] = kVideoNewline;
  chrout[2] = 0;
  SendVideoMessage(chrout, sizeof(chrout));
}

// Positions the cursor without drawing anything.
void VideoMsgSendLocateCursor(uint8_t x, uint8_t y) {
  uint8_t chrout[5];
  chrout[0] = kSOP;
  chrout[1] = kVideoLocateCursor;
  chrout[2] = 2;
  chrout[kPayloadByteOffset] = x;
  chrout[kPayloadByteOffset + 1] = y;
  SendVideoMessage(chrout, sizeof(chrout));
}

// Writes a run of characters at an absolute cell index. Cheaper than moving the
// cursor and emitting each character separately.
void VideoMsgSendPutStringAtCell(uint16_t cell, char* mess, uint8_t len) {
  if (len > 58) {
    len = 58;
  }
  uint8_t chrout[64];
  chrout[0] = kSOP;
  chrout[1] = kVideoPutStringAt;
  chrout[2] = 2 + len;
  chrout[kPayloadByteOffset] = (uint8_t)(cell >> 8);
  chrout[kPayloadByteOffset + 1] = (uint8_t)(cell & 0x00ff);

  memcpy(&chrout[kPayloadByteOffset + 2], mess, len);
  SendVideoMessage(chrout, 5 + len);
}

// Writes a run of attribute bytes, changing highlighting without touching the
// characters underneath.
void VideoMsgSendPutAttribsAtCell(uint16_t cell, char* attr, uint8_t len) {
  uint8_t chrout[64];
  chrout[0] = kSOP;
  chrout[1] = kVideoPutAttribsAt;
  chrout[2] = 2 + len;
  chrout[kPayloadByteOffset] = (uint8_t)(cell >> 8);
  chrout[kPayloadByteOffset + 1] = (uint8_t)(cell & 0x00ff);

  memcpy(&chrout[kPayloadByteOffset + 2], attr, len);
  SendVideoMessage(chrout, 5 + len);
}

// Returns the cursor cell to normal video.
void VideoMsgSendClearAttribAtCursor(void) {
  uint8_t chrout[3];
  chrout[0] = kSOP;
  chrout[1] = kVideoClearAttributeAtCursor;
  chrout[2] = 0;
  SendVideoMessage(chrout, sizeof(chrout));
}

// Inverts the cursor cell, which is how the blinking cursor is drawn.
void VideoMsgSendToggleAttribAtCursor(void) {
  uint8_t chrout[3];
  chrout[0] = kSOP;
  chrout[1] = kVideoToggleAttribAtCursor;
  chrout[2] = 0;
  SendVideoMessage(chrout, sizeof(chrout));
}

// Enables or disables scroll lock. When locked, output at the bottom row stops
// scrolling the screen.
void VideoMsgSendSetScrollLock(bool en) {
  uint8_t chrout[4];
  chrout[0] = kSOP;
  chrout[1] = kVideoSetScrollLock;
  chrout[2] = 1;
  chrout[3] = en ? 1 : 0;
  SendVideoMessage(chrout, sizeof(chrout));
}

// Fills the screen with one character and homes the cursor.
void VideoMsgSendClearScreen(uint8_t c) {
  uint8_t chrout[4];
  chrout[0] = kSOP;
  chrout[1] = kVideoClearScreen;
  chrout[2] = 1;
  chrout[3] = c;
  SendVideoMessage(chrout, sizeof(chrout));
}

// Selects whether subsequently written characters are in reverse video.
void VideoMsgSendSetReverseMode(bool en) {
  uint8_t msg[4];
  msg[0] = kSOP;
  msg[1] = 0x12;
  msg[2] = 1;
  msg[3] = en ? 1 : 0;
  SendVideoMessage(msg, sizeof(msg));
}

// Inverts the whole display at once, without touching per-cell attributes.
void VideoMsgSendReverseScreen(bool en) {
  uint8_t chrout[4];
  chrout[0] = kSOP;
  chrout[1] = kVideoReverseScreen;
  chrout[2] = 1;
  chrout[3] = en ? 1 : 0;
  SendVideoMessage(chrout, sizeof(chrout));
}

// Controls whether writing a character moves the cursor. Turning it off lets
// the caller overwrite one cell repeatedly.
void VideoMsgSendCursorAdvance(bool en) {
  uint8_t chrout[4];
  chrout[0] = kSOP;
  chrout[1] = kVideoSetCursorAdvance;
  chrout[2] = 1;
  chrout[3] = en ? 1 : 0;
  SendVideoMessage(chrout, sizeof(chrout));
}

// Draws a pixel line, rasterised on the video board so only the endpoints go
// over the wire.
void VideoMsgSendDrawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1,
                          uint8_t p) {
  uint8_t msg[8];
  msg[0] = kSOP;
  msg[1] = 0x51;
  msg[2] = 5;
  msg[3] = x0;
  msg[4] = y0;
  msg[5] = x1;
  msg[6] = y1;
  msg[7] = p;
  SendVideoMessage(msg, sizeof(msg));
}

// Sets one pixel in the block-graphics layer.
void VideoMsgSendSetPixel(uint8_t x, uint8_t y, uint8_t p) {
  uint8_t chrout[6];
  chrout[0] = kSOP;
  chrout[1] = kVideoSetPixel;
  chrout[2] = 3;
  chrout[3] = x;
  chrout[4] = y;
  chrout[5] = p;
  SendVideoMessage(chrout, sizeof(chrout));
}

// Scrolls the display up one text row.
void VideoMsgSendScrollScreen(void) {
  uint8_t chrout[3];
  chrout[0] = kSOP;
  chrout[1] = kVideoScrollScreen;
  chrout[2] = 0;
  SendVideoMessage(chrout, sizeof(chrout));
}

// Redefines one character's bitmap from eight rows.
void VideoMsgSendDefineChar(uint8_t ch, uint8_t* bitrows) {
  uint8_t chrout[12];
  chrout[0] = kSOP;
  chrout[1] = kVideoDefineChar;
  chrout[2] = 9;
  chrout[3] = ch;
  memcpy(&chrout[4], bitrows, 8);
  SendVideoMessage(chrout, sizeof(chrout));
}

// Restores one character from the ROM font.
void VideoMsgSendResetChar(uint8_t ch) {
  uint8_t chrout[4];
  chrout[0] = kSOP;
  chrout[1] = kVideoResetChar;
  chrout[2] = 1;
  chrout[3] = ch;

  SendVideoMessage(chrout, sizeof(chrout));
}

// Redefines one glyph in the alternate graphics character set.
void VideoMsgSendDefineGfx(uint8_t ch, uint8_t* bitrows) {
  uint8_t chrout[12];
  chrout[0] = kSOP;
  chrout[1] = kVideoDefineGfx;
  chrout[2] = 9;
  chrout[3] = ch;
  memcpy(&chrout[4], bitrows, 8);
  SendVideoMessage(chrout, sizeof(chrout));
}

// Restores one graphics-set glyph from ROM.
void VideoMsgSendResetGfx(uint8_t ch) {
  uint8_t chrout[4];
  chrout[0] = kSOP;
  chrout[1] = kVideoResetGfx;
  chrout[2] = 1;
  chrout[3] = ch;

  SendVideoMessage(chrout, sizeof(chrout));
}

// Writes a character at a cell, leaving the cursor and the cell's attribute
// untouched.
void VideoMsgSendPlotChar(uint8_t x, uint8_t y, uint8_t ch) {
  uint8_t msg[6];
  msg[0] = kSOP;
  msg[1] = kVideoPlotChar;
  msg[2] = 3;
  msg[3] = x;
  msg[4] = y;
  msg[5] = ch;
  SendVideoMessage(msg, sizeof(msg));
}

// Fills a run of consecutive cells, wrapping onto following rows.
void VideoMsgSendFillCells(uint8_t x, uint8_t y, uint8_t ch, uint8_t count) {
  if (count == 0) {
    return;
  }
  uint8_t msg[7];
  msg[0] = kSOP;
  msg[1] = kVideoFillCells;
  msg[2] = 4;
  msg[3] = x;
  msg[4] = y;
  msg[5] = ch;
  msg[6] = count;
  SendVideoMessage(msg, sizeof(msg));
}

// Copies a rectangle of cells, optionally filling the source afterwards. Done
// on the video board so overlapping moves stay correct and stay fast.
void VideoMsgSendMoveBlock(uint8_t fromX, uint8_t fromY, uint8_t toX,
                           uint8_t toY, uint8_t w, uint8_t h, bool hasFill,
                           uint8_t fillChar) {
  if (hasFill) {
    uint8_t msg[10];
    msg[0] = kSOP;
    msg[1] = kVideoMoveBlock;
    msg[2] = 7;
    msg[3] = fromX;
    msg[4] = fromY;
    msg[5] = toX;
    msg[6] = toY;
    msg[7] = w;
    msg[8] = h;
    msg[9] = fillChar;
    SendVideoMessage(msg, sizeof(msg));
  } else {
    uint8_t msg[9];
    msg[0] = kSOP;
    msg[1] = kVideoMoveBlock;
    msg[2] = 6;
    msg[3] = fromX;
    msg[4] = fromY;
    msg[5] = toX;
    msg[6] = toY;
    msg[7] = w;
    msg[8] = h;
    SendVideoMessage(msg, sizeof(msg));
  }
}

// Flood-fills the pixel region containing a point.
void VideoMsgSendFloodFill(uint8_t x, uint8_t y, uint8_t p) {
  uint8_t msg[6];
  msg[0] = kSOP;
  msg[1] = 0x53;
  msg[2] = 3;
  msg[3] = x;
  msg[4] = y;
  msg[5] = p;
  SendVideoMessage(msg, sizeof(msg));
}

// Draws an ellipse or arc, optionally filled.
void VideoMsgSendDrawCircle(uint8_t x, uint8_t y, uint8_t xr, uint8_t yr,
                            uint8_t p, uint16_t s_deg, uint16_t e_deg) {
  uint8_t msg[12];
  msg[0] = kSOP;
  msg[1] = 0x52;
  msg[2] = 9;
  msg[3] = x;
  msg[4] = y;
  msg[5] = xr;
  msg[6] = yr;
  msg[7] = p;
  msg[8] = (uint8_t)(s_deg >> 8);
  msg[9] = (uint8_t)(s_deg & 0xFF);
  msg[10] = (uint8_t)(e_deg >> 8);
  msg[11] = (uint8_t)(e_deg & 0xFF);
  SendVideoMessage(msg, sizeof(msg));
  uint8_t r_max = (xr > yr) ? xr : yr;
  bool full = (s_deg == 0 && e_deg == 360);
  delayMicroseconds((uint32_t)r_max * (full ? 30 : 300));
}

// Fills a rectangle, cycling through a character range. The trailing delay is
// scaled to the area so the next message does not arrive while the video board
// is still painting.
void VideoMsgSendFillBlock(uint8_t x, uint8_t y, uint8_t w, uint8_t h,
                           uint8_t startChar, uint8_t endChar) {
  uint8_t msg[9];
  msg[0] = kSOP;
  msg[1] = kVideoFillBlock;
  msg[2] = 6;
  msg[3] = x;
  msg[4] = y;
  msg[5] = w;
  msg[6] = h;
  msg[7] = startChar;
  msg[8] = endChar;
  SendVideoMessage(msg, sizeof(msg));
  delayMicroseconds((uint32_t)w * h * 8);
}

// Copies a range of glyphs between the text and graphics character sets.
void VideoMsgSendCopyChar(bool srcGfx, bool dstGfx, uint8_t start,
                          uint8_t end) {
  uint8_t msg[6];
  msg[0] = kSOP;
  msg[1] = kVideoCopyChar;
  msg[2] = 3;
  msg[3] = (srcGfx ? 0x01 : 0x00) | (dstGfx ? 0x02 : 0x00);
  msg[4] = start;
  msg[5] = end;
  SendVideoMessage(msg, sizeof(msg));
}

// Sends many consecutive glyph definitions in one frame, far cheaper than one
// message per character when loading a whole font.
static void SendDefineBulk(uint8_t opcode, uint8_t startCh,
                           const uint8_t* bitrows, uint8_t count) {
  uint8_t msg[4 + 28 * 8];
  msg[0] = kSOP;
  msg[1] = opcode;
  msg[2] = (uint8_t)(1 + count * 8);
  msg[3] = startCh;
  memcpy(&msg[4], bitrows, (size_t)count * 8);
  SendVideoMessage(msg, (size_t)4 + (size_t)count * 8);
}

// Bulk-defines a run of text-set glyphs.
void VideoMsgSendDefineCharBulk(uint8_t startCh, const uint8_t* bitrows,
                                uint8_t count) {
  SendDefineBulk(kVideoDefineCharBulk, startCh, bitrows, count);
}

// Bulk-defines a run of graphics-set glyphs.
void VideoMsgSendDefineGfxBulk(uint8_t startCh, const uint8_t* bitrows,
                               uint8_t count) {
  SendDefineBulk(kVideoDefineGfxBulk, startCh, bitrows, count);
}

// Rotates a band of rows sideways, wrapping at the edges. The delay lets the
// video board finish before another command lands.
void VideoMsgSendScrollX(uint8_t y, uint8_t height, int8_t direction) {
  uint8_t msg[6];
  msg[0] = kSOP;
  msg[1] = kVideoScrollX;
  msg[2] = 3;
  msg[3] = y;
  msg[4] = height;
  msg[5] = (uint8_t)direction;
  SendVideoMessage(msg, sizeof(msg));
  delay(10);
}

// Draws two polygons in one frame: the first erased, the second drawn. Doing
// both in a single message makes animation flicker-free without double
// buffering.
void VideoMsgSendDrawPoly(uint8_t nErase, uint8_t nDraw, const uint8_t* pts,
                          uint8_t totalPts) {
  uint8_t payloadLen = 2 + totalPts * 2;
  uint8_t msg[256];
  msg[0] = kSOP;
  msg[1] = kVideoDrawPoly;
  msg[2] = payloadLen;
  msg[3] = nErase;
  msg[4] = nDraw;
  memcpy(&msg[5], pts, totalPts * 2);
  SendVideoMessage(msg, 3 + payloadLen);
}
