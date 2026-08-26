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
#include <RTCDue.h>

static RTCDue rtc(XTAL);

// Starts the SAM3X real-time clock.
void RtcInit(void) {
  rtc.begin();
  rtc.setClock(__DATE__, __TIME__);
}

// Current hour from the real-time clock, 0-23.
int RtcGetHours(void) { return rtc.getHours(); }
// Current minute.
int RtcGetMinutes(void) { return rtc.getMinutes(); }
// Current second.
int RtcGetSeconds(void) { return rtc.getSeconds(); }
// Day of month.
int RtcGetDay(void) { return rtc.getDay(); }
// Month, 1-12.
int RtcGetMonth(void) { return rtc.getMonth(); }
// Four-digit year.
int RtcGetYear(void) { return (int)rtc.getYear(); }

// SETTIME: sets the clock from hours, minutes and seconds, on a 24-hour dial.
bool CmdSetTime(const char* args) {
  float hf, mf, sf;

  args = ParseExpression(args, &hf);
  if (!args) {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;

  args = ParseExpression(args, &mf);
  if (!args) {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;

  args = ParseExpression(args, &sf);
  if (!args) {
    PrintError(ERR_SYNTAX);
    return false;
  }

  int h = (int)hf;
  int m = (int)mf;
  int s = (int)sf;

  if (h < 0 || h > 23 || m < 0 || m > 59 || s < 0 || s > 59) {
    PrintError(ERR_ILLEGAL_QUANTITY);
    return false;
  }

  rtc.setTime(h, m, s);
  return true;
}

// Gregorian leap-year test.
static bool IsLeapYear(int y) {
  return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

// Length of a month, accounting for leap years.
static int DaysInMonth(int month, int year) {
  static const int days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) {
    return 0;
  }
  int d = days[month];
  if (month == 2 && IsLeapYear(year)) {
    d = 29;
  }
  return d;
}

// SETDATE: sets the calendar from day, month and year, rejecting dates the
// calendar does not have.
bool CmdSetDate(const char* args) {
  float df, mf, yf;

  args = ParseExpression(args, &df);
  if (!args) {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;

  args = ParseExpression(args, &mf);
  if (!args) {
    PrintError(ERR_SYNTAX);
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
    PrintError(ERR_SYNTAX);
    return false;
  }

  int d = (int)df;
  int m = (int)mf;
  int y = (int)yf;

  if (y < 2000 || y > 2099) {
    PrintError(ERR_ILLEGAL_QUANTITY);
    return false;
  }
  if (m < 1 || m > 12) {
    PrintError(ERR_ILLEGAL_QUANTITY);
    return false;
  }
  if (d < 1 || d > DaysInMonth(m, y)) {
    PrintError(ERR_ILLEGAL_QUANTITY);
    return false;
  }

  rtc.setDate(d, m, (uint16_t)y);
  return true;
}
