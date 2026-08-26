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

#include "comm_messages.h"
#include <Arduino.h>

// Pacing for the outgoing link. The WiFi modem cannot absorb bytes back to
// back, and the trailing gap lets the last byte land before the next message.
const uint16_t kInterByteUs = 350;
const uint16_t kInterFrameUs = 150;

#define kSOP 0x5c

#define kBufferMaxLen 256

typedef struct {
  uint8_t buffer[kBufferMaxLen];
  uint8_t buffer_len;
  uint8_t read_index;
  uint32_t timestamp;
  bool in_frame;
} Buffer;

void BufferInit(Buffer* buffer);
void BufferAdd(uint8_t b, Buffer* buffer);
bool BufferHasBytes(Buffer* buffer);
bool BufferIsFull(Buffer* buffer);

Buffer buffer;

// Two's-complement checksum, so a valid frame plus its checksum byte sums to
// zero. Must match the file server.
static uint8_t calcchecksum(uint8_t* data_in, size_t data_len) {
  uint8_t raw_sum = 0;

  for (size_t i = 0; i < data_len; i++) {
    raw_sum += data_in[i];
  }

  return (uint8_t)(~raw_sum + 1);
}

// Debug hook for an inbound frame: echoes the header fields and both checksums
// back over the link.
bool HandleCommCommand(Buffer* buffer) {
  const uint8_t msg_id = buffer->buffer[1];
  const uint8_t payload_len = buffer->buffer[2];
  const uint8_t checksum = buffer->buffer[buffer->buffer_len - 1];
  uint8_t* payload = &(buffer->buffer[3]);
  uint8_t calc_checksum = calcchecksum(buffer->buffer, buffer->buffer_len - 1);

  static char buf[50];
  sprintf(buf, "%d %d %d %d\n", msg_id, payload_len, checksum, calc_checksum);
  Serial1.println(buf);

  return true;
}

// Resets the inbound frame assembler.
void InitPolling(void) { BufferInit(&buffer); }

// Assembles inbound frames from the WiFi link. A frame left unfinished past the
// timeout is abandoned, so a dropped connection mid-frame cannot wedge the
// reader waiting for bytes that will never arrive.
bool PollCommSerial(void) {
  static uint8_t data = 0;

  if ((buffer.in_frame == true) &&
      (millis() > (buffer.timestamp + kFrameTimeoutMs))) {
    BufferInit(&buffer);
    Serial1.println("frame timeout");
    return false;
  }

  while (Serial1.available()) {
    data = Serial1.read();
    if (buffer.in_frame == false && data == 'g') {
      BufferInit(&buffer);
      buffer.in_frame = true;
      buffer.timestamp = millis();
      Serial1.println("in frame");
    }
    if (buffer.in_frame == true) {
      BufferAdd(data, &buffer);
      if (buffer.buffer_len ==
          (uint16_t)kMinFrameSize + buffer.buffer[kOffsetPayloadLen]) {
        HandleCommCommand(&buffer);
        BufferInit(&buffer);
      }
    } else {
    }
  }
  return true;
}

// Discards anything still unread on the link. Called before each request so a
// stale reply from a timed-out command is not mistaken for this one's.
static void CommClearOutBuffer(void) {
  while (Serial1.available()) {
    Serial1.read();
  }
}

// Writes a framed request to the file server and appends its checksum. The
// inter-byte delays keep the sender within what the WiFi modem can absorb.
static uint8_t SendCommMessage(uint8_t* msg, size_t msg_size) {
  CommClearOutBuffer();
  uint8_t msg_checksum = calcchecksum(msg, msg_size);
  for (uint8_t pos = 0; pos < msg_size; pos++) {
    Serial1.write(msg[pos]);
    delayMicroseconds(kInterByteUs);
  }
  Serial1.write(msg_checksum);
  delayMicroseconds(kInterFrameUs);
  return msg_size + 1;
}

// Asks the server for a directory listing.
void CommMsgSendCatalog(void) {
  uint8_t msg[3];
  msg[0] = kSOP;
  msg[1] = BTIO_MSG_CATALOG;
  msg[2] = 0;
  SendCommMessage(msg, 3);
}

// Requests a BASIC program by name; the server streams the text back.
void CommMsgSendLoad(char* file) {
  uint8_t fnLen = (uint8_t)strlen(file);
  uint8_t msg[68];
  msg[0] = kSOP;
  msg[1] = BTIO_MSG_LOAD;
  msg[2] = fnLen;
  if (fnLen > 0) {
    memcpy(&msg[3], file, fnLen);
  }
  SendCommMessage(msg, 3 + fnLen);
}

// Announces a save; the program text follows.
void CommMsgSendSave(char* file) {
  uint8_t fnLen = (uint8_t)strlen(file);
  uint8_t msg[68];
  msg[0] = kSOP;
  msg[1] = BTIO_MSG_SAVE;
  msg[2] = fnLen;
  if (fnLen > 0) {
    memcpy(&msg[3], file, fnLen);
  }
  SendCommMessage(msg, 3 + fnLen);
}

// Requests a character set, naming which RAM to load and where to start.
void CommMsgSendLoadChar(char* file, bool isGfx, uint8_t startIdx) {
  uint8_t msg[48];
  uint8_t fnLen = (uint8_t)strlen(file);
  msg[0] = kSOP;
  msg[1] = BTIO_MSG_LOADCHAR;
  msg[2] = 2 + fnLen;
  msg[3] = isGfx ? 1 : 0;
  msg[4] = startIdx;
  if (fnLen > 0) {
    memcpy(&msg[5], file, fnLen);
  }
  SendCommMessage(msg, 5 + fnLen);
}

// Sends a character set to the server for storage.
void CommMsgSendSaveChar(char* file, bool isGfx, uint8_t startIdx) {
  uint8_t msg[48];
  uint8_t fnLen = (uint8_t)strlen(file);
  msg[0] = kSOP;
  msg[1] = BTIO_MSG_SAVECHAR;
  msg[2] = 2 + fnLen;
  msg[3] = isGfx ? 1 : 0;
  msg[4] = startIdx;
  if (fnLen > 0) {
    memcpy(&msg[5], file, fnLen);
  }
  SendCommMessage(msg, 5 + fnLen);
}

// Opens a file on a numbered channel for read, write, or append.
void CommMsgSendFopen(uint8_t channel, uint8_t mode, const char* filename) {
  uint8_t fnLen = (uint8_t)strlen(filename);
  uint8_t msg[48];
  msg[0] = kSOP;
  msg[1] = BTIO_MSG_FOPEN;
  msg[2] = 2 + fnLen;
  msg[3] = channel;
  msg[4] = mode;
  if (fnLen > 0) {
    memcpy(&msg[5], filename, fnLen);
  }
  SendCommMessage(msg, 5 + fnLen);
}

// Closes a channel, flushing whatever is buffered server-side.
void CommMsgSendFclose(uint8_t channel) {
  uint8_t msg[4];
  msg[0] = kSOP;
  msg[1] = BTIO_MSG_FCLOSE;
  msg[2] = 1;
  msg[3] = channel;
  SendCommMessage(msg, 4);
}

// Writes text to an open channel.
void CommMsgSendFprint(uint8_t channel, const char* text, uint8_t textLen) {
  uint8_t msg[260];
  msg[0] = kSOP;
  msg[1] = BTIO_MSG_FPRINT;
  msg[2] = 1 + textLen;
  msg[3] = channel;
  if (textLen > 0) {
    memcpy(&msg[4], text, textLen);
  }
  SendCommMessage(msg, 4 + textLen);
}

// Requests the next line from a channel.
void CommMsgSendFinput(uint8_t channel) {
  uint8_t msg[4];
  msg[0] = kSOP;
  msg[1] = BTIO_MSG_FINPUT;
  msg[2] = 1;
  msg[3] = channel;
  SendCommMessage(msg, 4);
}

// Requests a single byte from a channel.
void CommMsgSendFget(uint8_t channel) {
  uint8_t msg[4];
  msg[0] = kSOP;
  msg[1] = BTIO_MSG_FGET;
  msg[2] = 1;
  msg[3] = channel;
  SendCommMessage(msg, 4);
}

// Writes a single byte to a channel.
void CommMsgSendFput(uint8_t channel, uint8_t byte) {
  uint8_t msg[5];
  msg[0] = kSOP;
  msg[1] = BTIO_MSG_FPUT;
  msg[2] = 2;
  msg[3] = channel;
  msg[4] = byte;
  SendCommMessage(msg, 5);
}

// Moves a channel's file cursor by a signed byte offset, sent big-endian.
void CommMsgSendFseek(uint8_t channel, uint32_t offset) {
  uint8_t msg[8];
  msg[0] = kSOP;
  msg[1] = BTIO_MSG_FSEEK;
  msg[2] = 5;
  msg[3] = channel;
  msg[4] = (uint8_t)(offset >> 24);
  msg[5] = (uint8_t)(offset >> 16);
  msg[6] = (uint8_t)(offset >> 8);
  msg[7] = (uint8_t)(offset & 0xFF);
  SendCommMessage(msg, 8);
}

// Moves a channel's cursor back to the start of the file.
void CommMsgSendFrewind(uint8_t channel) {
  uint8_t msg[4];
  msg[0] = kSOP;
  msg[1] = BTIO_MSG_FREWIND;
  msg[2] = 1;
  msg[3] = channel;
  SendCommMessage(msg, 4);
}

// Asks how many bytes remain from the cursor to end of file.
void CommMsgSendFbytes(uint8_t channel) {
  uint8_t msg[4];
  msg[0] = kSOP;
  msg[1] = BTIO_MSG_FBYTES;
  msg[2] = 1;
  msg[3] = channel;
  SendCommMessage(msg, 4);
}

// Asks the server to delete a file.
void CommMsgSendDel(char* file) {
  uint8_t fnLen = (uint8_t)strlen(file);
  uint8_t msg[68];
  msg[0] = kSOP;
  msg[1] = BTIO_MSG_DEL;
  msg[2] = fnLen;
  if (fnLen > 0) {
    memcpy(&msg[3], file, fnLen);
  }
  SendCommMessage(msg, 3 + fnLen);
}

// Asks the server to rename a file.
void CommMsgSendRen(char* oldname, char* newname) {
  uint8_t oldLen = (uint8_t)strlen(oldname);
  uint8_t newLen = (uint8_t)strlen(newname);
  uint8_t msg[132];
  msg[0] = kSOP;
  msg[1] = BTIO_MSG_REN;
  msg[2] = (uint8_t)(1 + oldLen + newLen);
  msg[3] = oldLen;
  if (oldLen > 0) {
    memcpy(&msg[4], oldname, oldLen);
  }
  if (newLen > 0) {
    memcpy(&msg[4 + oldLen], newname, newLen);
  }
  SendCommMessage(msg, 4 + oldLen + newLen);
}

// Changes the server's working directory for subsequent file operations.
void CommMsgSendChdir(const char* path) {
  uint8_t pLen = (uint8_t)strlen(path);
  uint8_t msg[68];
  msg[0] = kSOP;
  msg[1] = BTIO_MSG_CHDIR;
  msg[2] = pLen;
  if (pLen > 0) {
    memcpy(&msg[3], path, pLen);
  }
  SendCommMessage(msg, 3 + pLen);
}

// Asks the server to create a directory.
void CommMsgSendMkdir(const char* path) {
  uint8_t pLen = (uint8_t)strlen(path);
  uint8_t msg[68];
  msg[0] = kSOP;
  msg[1] = BTIO_MSG_MKDIR;
  msg[2] = pLen;
  if (pLen > 0) {
    memcpy(&msg[3], path, pLen);
  }
  SendCommMessage(msg, 3 + pLen);
}

// Asks the server to copy a file.
void CommMsgSendCopy(char* src, char* dst) {
  uint8_t srcLen = (uint8_t)strlen(src);
  uint8_t dstLen = (uint8_t)strlen(dst);
  uint8_t msg[132];
  msg[0] = kSOP;
  msg[1] = BTIO_MSG_COPY;
  msg[2] = (uint8_t)(1 + srcLen + dstLen);
  msg[3] = srcLen;
  if (srcLen > 0) {
    memcpy(&msg[4], src, srcLen);
  }
  if (dstLen > 0) {
    memcpy(&msg[4 + srcLen], dst, dstLen);
  }
  SendCommMessage(msg, 4 + srcLen + dstLen);
}
