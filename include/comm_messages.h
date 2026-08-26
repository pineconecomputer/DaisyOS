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

#ifndef INCLUDE_COMM_MESSAGES_H_
#define INCLUDE_COMM_MESSAGES_H_

#include <Arduino.h>

typedef enum {
  BTIO_MSG_CATALOG = 0x0A,
  BTIO_MSG_LOAD = 0x0B,
  BTIO_MSG_SAVE = 0x0C,
  BTIO_MSG_LOADCHAR = 0x0D,
  BTIO_MSG_SAVECHAR = 0x0E,
  BTIO_MSG_FOPEN = 0x0F,
  BTIO_MSG_FCLOSE = 0x10,
  BTIO_MSG_FPRINT = 0x11,
  BTIO_MSG_FINPUT = 0x12,
  BTIO_MSG_FGET = 0x13,
  BTIO_MSG_FPUT = 0x14,
  BTIO_MSG_FSEEK = 0x15,
  BTIO_MSG_FBYTES = 0x16,
  BTIO_MSG_DEL = 0x17,
  BTIO_MSG_REN = 0x18,
  BTIO_MSG_COPY = 0x19,
  BTIO_MSG_CHDIR = 0x1A,
  BTIO_MSG_MKDIR = 0x1B,
  BTIO_MSG_FREWIND = 0x1C
} FileIoMsg;

const uint8_t kSOP = 0x5c;
const uint8_t kMinFrameSize = 4;
const uint8_t kOffsetPayloadLen = 2;
const uint32_t kFrameTimeoutMs = 10000;

void CommMsgSendCatalog(void);
void CommMsgSendLoad(char* file);
void CommMsgSendSave(char* file);
void CommMsgSendLoadChar(char* file, bool isGfx, uint8_t startIdx);
void CommMsgSendSaveChar(char* file, bool isGfx, uint8_t startIdx);
void CommMsgSendFopen(uint8_t channel, uint8_t mode, const char* filename);
void CommMsgSendFclose(uint8_t channel);
void CommMsgSendFprint(uint8_t channel, const char* text, uint8_t textLen);
void CommMsgSendFinput(uint8_t channel);
void CommMsgSendFget(uint8_t channel);
void CommMsgSendFput(uint8_t channel, uint8_t byte);
void CommMsgSendFseek(uint8_t channel, uint32_t offset);
void CommMsgSendFrewind(uint8_t channel);
void CommMsgSendFbytes(uint8_t channel);
void CommMsgSendDel(char* file);
void CommMsgSendRen(char* oldname, char* newname);
void CommMsgSendCopy(char* src, char* dst);
void CommMsgSendChdir(const char* path);
void CommMsgSendMkdir(const char* path);

#endif  // INCLUDE_COMM_MESSAGES_H_
