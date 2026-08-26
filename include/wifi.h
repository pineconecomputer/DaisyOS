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

#ifndef INCLUDE_WIFI_H_
#define INCLUDE_WIFI_H_

#include <stdint.h>

void InitWifi(void);
void WifiEcho(void);
bool WifiReadProgramLine(void);
void WifiSend(const char* str);
void WifiSendLine(const char* str);
void WifiSendBytes(const uint8_t* buf, uint8_t count);
void WifiSendByte(const uint8_t v);
int WifiReadLine(char* outBuf, uint16_t maxLen);
int WifiReadBytes(uint8_t* buf, uint8_t count, uint32_t perByteTimeoutMs);

bool WifiConnect(const char* ssid, const char* password);
bool WifiQuery(void);
bool ServerConnect(const char* host, uint16_t port);
bool ServerDisconnect(void);
bool ServerConnected(void);
void WifiQueryNet(char* buf, int maxLen, int n);

#endif  // INCLUDE_WIFI_H_
