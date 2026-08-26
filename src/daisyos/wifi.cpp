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

#include "wifi.h"
#include "arduino.h"
#include "shadow_ram.h"
#include "daisybasic.h"
#include "keyboard.h"
#include "buffer.h"
#include <string.h>

#define WifiResetPin 8
#define WifiDataMode HIGH
#define WifiResetMode LOW
#define BT_MAX_LINE_LEN 255

// Modem link speed. Zimodem defaults to this rate.
const uint32_t kModemBaud = 38400;

// Working buffer for AT commands and their replies.
const size_t kAtBufLen = 128;

// AT command timeouts. Joining a network can take many seconds, so it gets
// the longest allowance; a plain status query should answer promptly.
const uint32_t kAtQuickMs = 3000;
const uint32_t kAtDialMs = 10000;
const uint32_t kAtJoinMs = 15000;

// Zimodem only recognises the +++ escape when it is bracketed by about a
// second of silence either side, so both guard periods are required.
const uint32_t kEscapeGuardMs = 1100;


static bool serverConnected = false;

// Pulses the ESP module's reset line and opens the link to it. The modem runs
// Zimodem, so it is driven with Hayes AT commands.
void InitWifi(void) {
  pinMode(WifiResetPin, OUTPUT);
  digitalWrite(WifiResetPin, WifiResetMode);
  delay(10);
  digitalWrite(WifiResetPin, WifiDataMode);
  static bool is_init = false;
  Serial1.begin(kModemBaud);
}

// Discards anything unread, so a reply left over from a previous command is not
// mistaken for the answer to the next one. The delay lets in-flight bytes land
// first.
static void WifiFlush(void) {
  delay(20);
  while (Serial1.available()) {
    Serial1.read();
  }
}

// Sends an AT command and waits for the expected response or a timeout. Polls
// for the BREAK key throughout so a hung modem does not lock up the machine.
static bool WifiATCommand(const char* cmd, const char* expected,
                          uint32_t timeoutMs) {
  WifiFlush();
  Serial1.println(cmd);
  char buf[kAtBufLen];
  uint32_t deadline = millis() + timeoutMs;
  while ((int32_t)(millis() - deadline) < 0) {
    if (BufferScanAndRemove(STOP_KEY)) {
      return false;
    }
    if (WifiReadLine(buf, sizeof(buf)) == 1) {
      if (strncasecmp(buf, "AT", 2) == 0) {
        continue;
      }
      if (strstr(buf, expected) != NULL) {
        return true;
      }
      if (strcmp(buf, "ERROR") == 0) {
        return false;
      }
    }
  }
  return false;
}

// Joins a WiFi network.
bool WifiConnect(const char* ssid, const char* password) {
  char cmd[kAtBufLen];
  snprintf(cmd, sizeof(cmd), "ATWIFI=%s,%s", ssid, password);
  return WifiATCommand(cmd, "OK", kAtJoinMs);
}

// Checks whether the modem is present and responding.
bool WifiQuery(void) {
  WifiFlush();
  Serial1.println("ATWIFI?");
  char buf[kAtBufLen];
  bool connected = false;
  uint32_t deadline = millis() + kAtQuickMs;
  while ((int32_t)(millis() - deadline) < 0) {
    if (BufferScanAndRemove(STOP_KEY)) {
      return false;
    }
    if (WifiReadLine(buf, sizeof(buf)) == 1) {
      if (strcmp(buf, "OK") == 0 || strcmp(buf, "ERROR") == 0) {
        return connected;
      }
      if (strncasecmp(buf, "AT", 2) == 0) {
        continue;
      }
      if (buf[0] != '\0') {
        connected = true;
      }
    }
  }
  return false;
}

// Dials a TCP host and enters transparent mode, after which bytes written to
// the link go straight to the peer.
bool ServerConnect(const char* host, uint16_t port) {
  char cmd[kAtBufLen];
  snprintf(cmd, sizeof(cmd), "ATDT%s:%u", host, port);
  bool ok = WifiATCommand(cmd, "CONNECT", kAtDialMs);
  if (ok) {
    serverConnected = true;
  }
  return ok;
}

// Leaves transparent mode and hangs up. Zimodem only recognises the +++ escape
// when it is surrounded by about a second of silence, hence the guard delays
// either side; both remain BREAK-interruptible.
bool ServerDisconnect(void) {
  serverConnected = false;
  uint32_t deadline = millis() + kEscapeGuardMs;
  while ((int32_t)(millis() - deadline) < 0) {
    if (BufferScanAndRemove(STOP_KEY)) {
      return false;
    }
    delay(1);
  }
  Serial1.print("+++");
  deadline = millis() + kEscapeGuardMs;
  while ((int32_t)(millis() - deadline) < 0) {
    if (BufferScanAndRemove(STOP_KEY)) {
      return false;
    }
    delay(1);
  }
  return WifiATCommand("ATH", "OK", kAtQuickMs);
}

// True while a TCP session is open.
bool ServerConnected(void) { return serverConnected; }

// Queries the modem for network details -- SSID, address, or reachability --
// and returns the answer as text.
void WifiQueryNet(char* buf, int maxLen, int n) {
  buf[0] = '\0';
  const char* cmd;
  uint32_t timeout;
  switch (n) {
    case 0:
      cmd = "ATI3";
      timeout = kAtQuickMs;
      break;
    case 1:
      cmd = "ATI2";
      timeout = kAtQuickMs;
      break;
    case 2:
      cmd = "AT+PING\"google.com\"";
      timeout = kAtJoinMs;
      break;
    default:
      return;
  }
  WifiFlush();
  Serial1.println(cmd);
  char line[kAtBufLen];
  uint32_t deadline = millis() + timeout;
  while ((int32_t)(millis() - deadline) < 0) {
    if (BufferScanAndRemove(STOP_KEY)) {
      return;
    }
    if (WifiReadLine(line, sizeof(line)) == 1) {
      if (strcmp(line, "ERROR") == 0) {
        return;
      }
      if (strncasecmp(line, "AT", 2) == 0) {
        continue;
      }
      if (strcmp(line, "OK") == 0) {
        if (n == 2) {
          strncpy(buf, "OK", maxLen - 1);
        }
        return;
      }
      if (line[0] != '\0') {
        strncpy(buf, line, maxLen - 1);
        buf[maxLen - 1] = '\0';
        return;
      }
    }
  }
}

// Loops received bytes straight back, for link testing.
void WifiEcho(void) {
  static uint8_t ch;
  if (Serial1.available()) {
    ch = Serial1.read();
    Serial1.write(ch);
  }
}

// Writes a string to the link with no terminator.
void WifiSend(const char* str) { Serial1.print(str); }

// Writes a string followed by a newline.
void WifiSendLine(const char* str) { Serial1.println(str); }

// Writes a single byte.
void WifiSendByte(const uint8_t v) { Serial1.write(v); }

// Writes a block of bytes.
void WifiSendBytes(const uint8_t* buf, uint8_t count) {
  Serial1.write(buf, count);
}

// Accumulates one line of an incoming BASIC program and executes it, which is
// how LOAD enters lines. Returns without blocking if no full line has arrived.
bool WifiReadProgramLine(void) {
  static char lineBuf[BT_MAX_LINE_LEN];
  static uint8_t linePos = 0;

  while (Serial1.available()) {
    char ch = Serial1.read();

    if (ch == '\n' || ch == '\r') {
      if (linePos > 0) {
        lineBuf[linePos] = '\0';
        BasicExecute(lineBuf);
        linePos = 0;
        return true;
      }
    } else if (linePos < BT_MAX_LINE_LEN - 1) {
      lineBuf[linePos++] = ch;
    }
  }
  return false;
}

// Reads a fixed number of bytes, returning short on timeout. The timeout is
// per byte, so a slow but progressing transfer is not cut off.
int WifiReadBytes(uint8_t* buf, uint8_t count, uint32_t perByteTimeoutMs) {
  uint8_t received = 0;
  uint32_t deadline = millis() + perByteTimeoutMs;
  while (received < count) {
    if (Serial1.available()) {
      buf[received++] = (uint8_t)Serial1.read();
      deadline = millis() + perByteTimeoutMs;
    } else if (millis() > deadline) {
      break;
    }
  }
  return received;
}

// Assembles one newline-terminated line across calls without blocking.
// Returns 1 for a complete line, -1 if it was too long for the caller's buffer.
int WifiReadLine(char* outBuf, uint16_t maxLen) {
  static char lineBuf[BT_MAX_LINE_LEN];
  static uint8_t linePos = 0;

  while (Serial1.available()) {
    char ch = Serial1.read();

    if (ch == '\r') {
    } else if (ch == '\n') {
      if (linePos > 0) {
        lineBuf[linePos] = '\0';
        if (linePos < maxLen) {
          strcpy(outBuf, lineBuf);
          linePos = 0;
          return 1;
        } else {
          linePos = 0;
          return -1;
        }
      }
    } else if (linePos < BT_MAX_LINE_LEN - 1) {
      lineBuf[linePos++] = ch;
    }
  }
  return 0;
}
