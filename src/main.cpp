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
#include "keyboard.h"
#include "buffer.h"
#include "timer.h"
#include "audio_messages.h"
#include "wifi.h"
#include "editor.h"
#include "shadow_ram.h"
#include "cursor.h"
#include "daisybasic.h"
#include "joyport.h"
#include "terminal.h"
#include "invaders.h"
#include "zmachine.h"
#include "daisybasic/basic_internal.h"

// ===========================================================================
// How DaisyOS fits together
// ===========================================================================
//
// Daisy splits the work of being a computer across three microcontrollers,
// because no one of these parts can do all of it. Generating a television
// signal and synthesizing audio both demand cycle-accurate timing, and
// neither tolerates sharing a processor with an interpreter that might spend
// an unpredictable amount of time on any given statement. So video and audio
// each get a dedicated chip, and this one, an Arduino Due, runs everything
// else: the BASIC interpreter, the screen editor, the terminal, and the
// keyboard scanner.
//
// The three boards are joined by plain hardware UARTs, one per coprocessor,
// plus a fourth to a WiFi modem. Traffic is one-way by design. DaisyOS sends
// commands and the coprocessors never reply, which removes the need for any
// handshaking in their interrupt-driven inner loops. Messages share a common
// frame: a start byte, a message id, a payload length, the payload, and a
// checksum. A receiver that loses sync simply discards bytes until the next
// start byte appears.
//
// One-way links create a problem: DaisyOS cannot ask what is on the screen.
// It solves this by keeping a complete local copy, the shadow RAM, of
// everything it has sent to the video board, including characters, video
// attributes, and glyph definitions. Anything that needs to read the display
// reads that copy instead. Screen editing, collision checks and the pixel
// layer all depend on it, and it is why an update is often computed locally
// first and then sent as a single batched message.
//
// BASIC programs are not stored as text. Each line is tokenised: keywords
// collapse to one byte each, and string literals and variable names are
// interned into shared pools with the line holding only an index. Lines are
// expanded back to text when listed or executed. The point is memory, since
// storing a useful program as source would not fit.
//
// Expressions are evaluated by converting to postfix with a shunting-yard
// pass and then walking the result with a value stack, rather than by
// recursive descent, which keeps deep expressions from exhausting the stack.

// The sound board shares a pin with the Due's programming header. Holding the
// PROG key releases that pin so DaisySound can be reflashed in place.
const uint8_t kProgKeyCol = 2;
const uint8_t kProgKeyRow = 8;
const uint8_t kSoundSharedPin = 14;

// Held high to keep DaisyVideo showing its logo, dropped once we are ready.
const uint8_t kCpuReadyPin = 52;

// Time for each coprocessor to finish its own reset before it is sent
// anything. The sound board reboots itself and needs the longer wait.
const uint16_t kSoundResetMs = 1500;
const uint16_t kVideoReadyMs = 250;

const uint8_t kSpaceChar = 32;
const uint32_t kConsoleBaud = 115200;
const uint32_t kVideoBaud = 115200;
const uint32_t kAudioBaud = 115200;

const char compile_date[] = __DATE__ " " __TIME__;
bool prog_key_down = false;

// Brings the coprocessors up in order: reboot the sound board, release
// DaisyVideo from its splash screen, then put the display into a known state.
// The delays give each board time to finish its own reset before it is sent
// anything.
void SetupDaisy(void) {
  AudioMsgSendReboot();
  delay(kSoundResetMs);
  digitalWrite(kCpuReadyPin, LOW);
  delay(kVideoReadyMs);
  VideoMsgSendClearScreen(kSpaceChar);
  VideoMsgSendCursorAdvance(true);
  InitCursor();
  KeyboardTimerInit();
}

// Prints the startup banner and free-memory line.
void Splash() {
  VideoMsgSendReverseScreen(true);
  FillBlockShadow(1,1,4,5,235,255);
  VideoMsgSendFillBlock(1, 1, 4, 5, 235, 255);
  LocateCursor(6,2);
  PrintStr("Pinecone Computer");
  LocateCursor(6,3);
  PrintStr("Daisy/1");
  LocateCursor(7,6);
  PrintStr("PRESS");
  LocateCursor(7,8);
  PrintStr("1 for DaisyBASIC");
  LocateCursor(7,10);
  PrintStr("2 for DaisyTerm");
  LocateCursor(7,12);
  PrintStr("3 for Space Invaders");
  LocateCursor(7,14);
  PrintStr("4 for Text Adventures");
  // Park the cursor on the bottom line so the blink shows the machine is
  // waiting for a key without sitting in the middle of the menu text.
  LocateCursor(0, VID_HEIGHT - 1);
}

void BasicSplash(void) {
  VideoMsgSendClearScreen(kSpaceChar);
  LocateCursor(0,20);
  PrintStr("Pinecone DaisyBASIC");
  Newline();
  BasicExecute("print fre(0);\" bytes free\"");
}

// Runs the boot menu until the user picks DaisyBASIC, ignoring any key that is
// not one of the four listed choices. The screen is put back to normal video
// and cleared before a program starts, since the menu leaves it reversed. Each
// program owns the machine until it finishes; when it returns, the menu is
// drawn again and offers the same choices, so the only way through to the BASIC
// prompt is choice 1.
void GetSplashSelection(void) {
  for (;;) {
    uint8_t key = BufferGet();
    if (key < '1' || key > '4') {
      continue;
    }
    // Take the cursor's attribute back off the cell, in case the blink left it
    // set on the half-cycle the key arrived in.
    ClearAttribute();
    VideoMsgSendReverseScreen(false);
    Clrscr(kSpaceChar);
    LocateCursor(0, 0);
    if (key == '1') {
      return;
    }
    switch (key) {
      case '2':
        RunTerminal();
        break;
      case '3':
        RunInvaders();
        break;
      default:  // '4'
        RunZMachine();
        break;
    }
    // Programs leave the screen cleared and in normal video, so the menu can be
    // drawn straight over it. Anything still queued from the program that just
    // exited would otherwise count as a menu choice.
    BufferClear();
    Splash();
  }
}

// Boot: open the three coprocessor links, initialise input, video shadow RAM
// and the clock, then hand off to the coprocessors and show the banner.
void setup() {
  InitWifi();
  Serial.begin(kConsoleBaud);
  Serial2.begin(kVideoBaud);
#if 1
  Serial3.begin(kAudioBaud);
#else
  digitalWrite(kSoundSharedPin, LOW);
  pinMode(kSoundSharedPin, INPUT);
#endif
  pinMode(kCpuReadyPin, OUTPUT);
  digitalWrite(kCpuReadyPin, HIGH);
  InitKeyStates();
  PinConfigurePins();
  InitJoyport();
  BufferInit();
  InitShadowRam();
  RtcInit();
  SetupDaisy();
  Splash();
  GetSplashSelection();
  BasicSplash();
}

// Main superloop. Watches for the PROG key, which releases the shared serial
// pin so the sound board can be reflashed in place, then runs one pass of the
// editor and sounds any pending key click.
void loop() {
  if (!prog_key_down) {
    prog_key_down = IsKeyColRowPressed(kProgKeyCol, kProgKeyRow);
    if (prog_key_down) {
      Serial3.end();
      digitalWrite(kSoundSharedPin, LOW);
      pinMode(kSoundSharedPin, INPUT);
    }
  } else {
    prog_key_down = IsKeyColRowPressed(kProgKeyCol, kProgKeyRow);
    if (!prog_key_down) {
      Serial3.begin(kAudioBaud);
    }
  }
  EditorService();
  TickKeyClick();
}
