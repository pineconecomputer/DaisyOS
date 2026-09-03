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

// The boot menu is mostly reverse video, so leaving it up indefinitely would
// hold a bright, unchanging image on the tube. After this long with no key it
// gives way to the screen saver, which shows nothing but the pinecone and
// moves it to a fresh spot at this interval.
const uint32_t kScreenSaverIdleMs = 60UL * 1000UL;
const uint32_t kScreenSaverMoveMs = 5UL * 1000UL;

// The pinecone is drawn as a block of consecutive glyphs, one per cell, so its
// size is fixed by the number of glyphs that make it up.
const uint8_t kPineconeWidth = 4;
const uint8_t kPineconeHeight = 5;
const uint8_t kPineconeFirstChar = 235;
const uint8_t kPineconeLastChar = 255;

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

// Paints the pinecone with its top left corner at (x, y), keeping the local
// copy of the screen in step with the video board.
void DrawPinecone(uint8_t x, uint8_t y) {
  FillBlockShadow(x, y, kPineconeWidth, kPineconeHeight, kPineconeFirstChar,
                  kPineconeLastChar);
  VideoMsgSendFillBlock(x, y, kPineconeWidth, kPineconeHeight,
                        kPineconeFirstChar, kPineconeLastChar);
}

// Draws the boot menu: the pinecone, the machine's name, and the numbered
// choices. Split out from Splash so the screen saver can put the menu back
// without sounding the startup jingle a second time.
void DrawSplash() {
  VideoMsgSendClearScreen(kSpaceChar);
  VideoMsgSendReverseScreen(true);
  DrawPinecone(1, 1);
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
  LocateCursor(7,16);
  PrintStr("5 for Audio/Video demo");
  // Park the cursor on the bottom line so the blink shows the machine is
  // waiting for a key without sitting in the middle of the menu text.
  LocateCursor(0, VID_HEIGHT - 1);
}

// Prints the startup banner and sounds the startup jingle.
void Splash() {
  BasicExecute("play \"t650o5c8ro4c4ro3c4ro4c8ro5c8r\"");
  DrawSplash();
}

// Blanks the menu down to the pinecone alone on an unreversed screen and hops
// it to a new spot every few minutes. Runs until any key is pressed. The key
// that wakes the machine is thrown away rather than returned, so a sleeping
// Daisy can never be nudged straight into a program by whoever touches it.
void RunScreenSaver(void) {
  // Seeded here rather than at boot because the moment the user stops typing
  // is the one unpredictable thing available; without it the pinecone would
  // walk the same path after every reset.
  randomSeed(micros());
  VideoMsgSendReverseScreen(false);
  Clrscr(kSpaceChar);
  uint32_t last_move_ms = millis();
  DrawPinecone(random(VID_WIDTH - kPineconeWidth + 1),
               random(VID_HEIGHT - kPineconeHeight + 1));
  for (;;) {
    if (BufferGet() != 0) {
      // Anything else typed while the screen was dark goes with it.
      BufferClear();
      return;
    }
    if (millis() - last_move_ms >= kScreenSaverMoveMs) {
      Clrscr(kSpaceChar);
      DrawPinecone(random(VID_WIDTH - kPineconeWidth + 1),
                   random(VID_HEIGHT - kPineconeHeight + 1));
      last_move_ms = millis();
    }
  }
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
// prompt is choice 1. A menu left untouched long enough drops into the screen
// saver and comes back from it on any key, ready for a fresh choice.
void GetSplashSelection(void) {
  uint32_t last_key_ms = millis();
  for (;;) {
    uint8_t key = BufferGet();
    if (key == 0) {
      if (millis() - last_key_ms >= kScreenSaverIdleMs) {
        RunScreenSaver();
        DrawSplash();
        last_key_ms = millis();
      }
      continue;
    }
    // Any key at all counts as touching the machine, including the ones the
    // menu has no use for.
    last_key_ms = millis();
    if (key < '1' || key > '5') {
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
      case '4':
        RunZMachine();
        break;
      default:  // '5', the demo, which is a BASIC program rather than an app.
        CmdShapeDemo();
        // The demo runs until BREAK, which stops the interpreter but leaves the
        // sound board playing its last program.
        AudioMsgSendShutUp();
        break;
    }
    // Clear away whatever the program left behind and redraw. Anything still
    // queued from the program that just exited would otherwise count as a menu
    // choice.
    Clrscr(kSpaceChar);
    BufferClear();
    Splash();
    last_key_ms = millis();
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
