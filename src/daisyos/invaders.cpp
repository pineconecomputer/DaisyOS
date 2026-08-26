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

#include "invaders.h"
#include "shadow_ram.h"
#include "video_messages.h"
#include "audio_messages.h"
#include "keyboard.h"
#include "buffer.h"
#include "joyport.h"
#include <Arduino.h>
#include <string.h>

static const uint8_t charData[19][8] = {
    {24, 60, 126, 219, 255, 36, 66, 165},
    {24, 60, 126, 219, 255, 90, 129, 66},
    {36, 189, 219, 255, 102, 60, 66, 129},
    {36, 189, 219, 255, 102, 60, 36, 195},
    {126, 255, 219, 255, 102, 219, 129, 66},
    {126, 255, 219, 255, 102, 36, 90, 165},
    {24, 24, 60, 60, 126, 255, 255, 0},
    {0, 24, 24, 24, 24, 0, 0, 0},
    {0, 16, 48, 16, 12, 16, 0, 0},
    {153, 66, 36, 195, 195, 36, 66, 153},
    {126, 255, 255, 255, 255, 231, 195, 0},
    {60, 90, 195, 153, 66, 195, 129, 0},
    {60, 126, 255, 219, 255, 36, 0, 0},
    {0xFF, 0xC3, 0xDB, 0xC3, 0xDB, 0xC3, 0xDB, 0xC3},
    {0x18, 0x3C, 0x7E, 0x66, 0x7E, 0x66, 0x7E, 0x66},
    {0xC3, 0xDB, 0xC3, 0xDB, 0xC3, 0xDB, 0xC3, 0xFF},
    {0x66, 0x7E, 0x66, 0x7E, 0x66, 0x7E, 0x66, 0x7E},
    {0x00, 0x00, 0x7E, 0x42, 0x5A, 0x42, 0x5A, 0x7E},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF},
};

static const uint8_t rowScore[4] = {30, 20, 20, 10};
static const uint8_t rowChar[4] = {128, 130, 130, 132};
static const uint8_t shieldX[4] = {6, 15, 24, 33};

static uint32_t rng;

// Xorshift pseudo-random generator. Cheap and good enough for enemy fire and
// UFO timing, and avoids pulling in the library RNG.
static inline uint32_t xrng(void) {
  rng ^= rng << 13;
  rng ^= rng >> 17;
  rng ^= rng << 5;
  return rng;
}

// Writes an integer as text without printf, which is far too slow and large for
// the HUD to use each frame.
static char* putint(char* p, int val) {
  char tmp[10];
  int i = 0;
  if (val == 0) {
    *p++ = '0';
    return p;
  }
  if (val < 0) {
    *p++ = '-';
    val = -val;
  }
  do {
    tmp[i++] = '0' + (val % 10);
    val /= 10;
  } while (val);
  while (i--) {
    *p++ = tmp[i];
  }
  return p;
}

static int8_t a[8][4];
static int8_t s[4];
static int16_t hi, sc;
static int8_t lv, le;
static int8_t na;
static int8_t px;
static int8_t ix, iy, id;
static int8_t bx, by, ex, ey;
static int8_t ux, ud;
static int8_t af;
static uint16_t fr;
static bool go_flag;

static uint8_t repeatDir;
static uint32_t t_repeat;

#define JOY_LEFT 3
#define JOY_RIGHT 4
#define JOY_FIRE 5

static bool prevJoyLeft, prevJoyRight, prevJoyFire;

static uint32_t t_joyrepeat;

static uint32_t t_bullet, t_ebullet, t_invader, t_ufo, t_ufospawn;
// Starting step interval before recalcSpeed adjusts it for count and level.
#define INVADER_MS_START 1000
static uint32_t invader_ms = INVADER_MS_START;

static uint32_t ufo_delay;

#define BULLET_MS 10
#define EBULLET_MS 35
#define UFO_MS 50
#define MOVE_MS 50
#define REPEAT_INIT 20

#define UFO_MIN_MS 6000
#define UFO_SPREAD_MS 6000

#define LEVEL_SPEEDUP_PCT 12
#define LEVEL_SPEEDUP_MAX_PCT 60
#define INVADER_MS_FLOOR 40

// Playfield rows. The player sits just above the skyline strip along the
// bottom, and invaders reaching that row end the game.
#define PLAYER_ROW 22
#define SKYLINE_ROW 24
#define PLAYFIELD_LEFT 1
#define PLAYFIELD_RIGHT 38

// Glyphs for the player ship and the life markers in the HUD.
#define SHIP_CHAR 134

// Column where the remaining-lives markers begin.
#define HUD_LIVES_COL 30

// Starting step interval before recalcSpeed adjusts it for count and level.

// Takes the next keystroke without waiting, or 0 if none is pending.
static uint8_t getKey(void) { return BufferGet(); }

// Waits, returning early and reporting true if the player pressed BREAK, so
// pauses stay interruptible.
static bool waitMs(uint32_t ms) {
  uint32_t end = millis() + ms;
  while (millis() < end) {
    if (getKey() == CTRL_C_KEY) {
      return true;
    }
  }
  return false;
}

// Clears the screen and homes the cursor.
static inline void cls(void) {
  Clrscr(' ');
  LocateCursor(0, 0);
}

// Prints a string at a cell.
static void printAt(uint8_t x, uint8_t y, const char* str) {
  LocateCursor(x, y);
  VideoMsgSendLocateCursor(x, y);
  while (*str) {
    Chrout((uint8_t)*str++);
  }
}

// Draws a horizontal run of one character.
static void hline(uint8_t x1, uint8_t x2, uint8_t y, uint8_t ch) {
  for (uint8_t x = x1; x <= x2; x++) {
    PlotChar(x, y, ch);
  }
}

// Draws a vertical run of one character.
static void vline(uint8_t x, uint8_t y1, uint8_t y2, uint8_t ch) {
  for (uint8_t y = y1; y <= y2; y++) {
    PlotChar(x, y, ch);
  }
}

// Installs the game's custom glyphs -- invaders, ship, shields -- over spare
// character codes.
static void defineChars(void) {
  for (uint8_t c = 0; c < 19; c++) {
    SetCharDef(128 + c, charData[c]);
    VideoMsgSendDefineChar(128 + c, (uint8_t*)charData[c]);
  }
}

// Restores the redefined glyphs from ROM on exit, so the game does not leave
// the character set altered.
static void resetChars(void) {
  for (uint8_t c = 128; c <= 146; c++) {
    ResetCharDef(c);
    VideoMsgSendResetChar(c);
  }
}

static const uint8_t skyTop[40] = {
    141, 141, ' ', ' ', 142, ' ', 141, 141, ' ', 142, ' ', ' ', 141, 141,
    141, ' ', ' ', 142, ' ', ' ', 141, 141, ' ', ' ', 142, ' ', ' ', 141,
    ' ', 142, ' ', 141, 141, 141, ' ', ' ', 142, ' ', 141, 141};
static const uint8_t skyBot[40] = {
    143, 143, 145, 146, 144, 146, 143, 143, 145, 144, 146, 146, 143, 143,
    143, 145, 146, 144, 146, 146, 143, 143, 145, 146, 144, 146, 146, 143,
    145, 144, 146, 143, 143, 143, 145, 146, 144, 146, 143, 143};

// Draws the static skyline along the bottom of the playfield.
static void drawSkyline(void) {
  for (uint8_t x = 0; x < 40; x++) {
    PlotChar(x, 23, skyTop[x]);
    PlotChar(x, SKYLINE_ROW, skyBot[x]);
  }
}

// Draws the full score, lives and level display.
static void updateHud(void) {
  char hud[40];
  memset(hud, ' ', 40);

  char* p = hud + 1;
  memcpy(p, "SCORE:", 6);
  p += 6;
  p = putint(p, sc);

  p = hud + 16;
  memcpy(p, "HI:", 3);
  p += 3;
  putint(p, hi);

  hud[28] = 'L';
  hud[29] = ':';
  for (int8_t i = 0; i < lv && (30 + i) < 40; i++) {
    hud[HUD_LIVES_COL + i] = (char)SHIP_CHAR;
  }

  PutStringAt(0, hud, 40);
  char attr[40];
  memset(attr, (char)0xFF, 40);
  VideoMsgSendPutAttribsAtCell(0, attr, 40);
}

// Redraws just the score, avoiding a full HUD repaint each time it changes.
static void updateScore(void) {
  char buf[9];
  char* p = putint(buf, sc);
  while (p < buf + 9) {
    *p++ = ' ';
  }
  PutStringAt(7, buf, 9);
  char attr[9];
  memset(attr, (char)0xFF, 9);
  VideoMsgSendPutAttribsAtCell(7, attr, 9);
}

// Redraws just the lives indicator.
static void updateLives(void) {
  char buf[10];
  memset(buf, ' ', 10);
  for (int8_t i = 0; i < lv && i < 10; i++) {
    buf[i] = (char)SHIP_CHAR;
  }
  PutStringAt(30, buf, 10);
  char attr[10];
  memset(attr, (char)0xFF, 10);
  VideoMsgSendPutAttribsAtCell(30, attr, 10);
}

// Draws one shield at its current damage level.
static inline void drawShield(uint8_t idx) {
  static const uint8_t shieldChar[3] = {' ', 139, 138};
  PlotChar(shieldX[idx], 19, shieldChar[s[idx]]);
}

// Maps a column to the shield standing there, or -1 for none. Shields sit at
// fixed columns, so a lookup beats a search.
static inline int8_t shieldHit(int8_t x) {
  switch (x) {
    case 6:
      return (s[0] > 0) ? 0 : -1;
    case 15:
      return (s[1] > 0) ? 1 : -1;
    case 24:
      return (s[2] > 0) ? 2 : -1;
    case 33:
      return (s[3] > 0) ? 3 : -1;
    default:
      return -1;
  }
}

// Redraws the whole invader block in its current animation frame.
static void drawAllInvaders(void) {
  for (uint8_t j = 0; j < 8; j++) {
    for (uint8_t i = 0; i < 4; i++) {
      if (a[j][i]) {
        PlotChar(ix + j * 4, iy + i * 2, rowChar[i] + af);
      }
    }
  }
}

// Recomputes the invader step interval from how many remain and how far the
// player has got. Fewer invaders and higher levels both speed them up, with a
// floor so the game stays playable.
static inline void recalcSpeed(void) {
  uint32_t ms = (uint32_t)na * 50;

  uint32_t cut = (uint32_t)(le - 1) * LEVEL_SPEEDUP_PCT;
  if (cut > LEVEL_SPEEDUP_MAX_PCT) {
    cut = LEVEL_SPEEDUP_MAX_PCT;
  }
  ms = ms * (100 - cut) / 100;

  if (ms < INVADER_MS_FLOOR) {
    ms = INVADER_MS_FLOOR;
  }
  invader_ms = ms;
}

// Moves the player one cell left, erasing the old position first.
static inline void moveLeft(void) {
  if (px <= PLAYFIELD_LEFT) {
    return;
  }
  PlotChar(px, PLAYER_ROW, ' ');
  px--;
  PlotChar(px, PLAYER_ROW, SHIP_CHAR);
}

// Moves the player one cell right.
static inline void moveRight(void) {
  if (px >= PLAYFIELD_RIGHT) {
    return;
  }
  PlotChar(px, PLAYER_ROW, ' ');
  px++;
  PlotChar(px, PLAYER_ROW, SHIP_CHAR);
}

// Shows the title screen and waits for a start or quit choice.
static bool titleScreen(void) {
  cls();
  hline(0, 39, 0, 205);
  hline(0, 39, 24, 205);
  vline(0, 1, 23, 186);
  vline(39, 1, 23, 186);

  SetReverseMode(true);
  printAt(8, 3, "  SPACE INVADERS  ");
  SetReverseMode(false);

  PlotChar(10, 6, 140);
  printAt(15, 6, "= ??? PTS");
  PlotChar(10, 8, 128);
  printAt(15, 8, "= 30 PTS");
  PlotChar(10, 10, 130);
  printAt(15, 10, "= 20 PTS");
  PlotChar(10, 12, 132);
  printAt(15, 12, "= 10 PTS");
  printAt(10, 14, "YOUR SHIP: ");
  Chrout(134);

  printAt(6, 18, "J/L OR STICK");
  printAt(22, 18, "MOVE");
  printAt(6, 19, "I OR TRIGGER");
  printAt(22, 19, "FIRE");
  printAt(6, 22, "PRESS SPACE OR FIRE TO START");

  AudioMsgSendSetPW(0, 64, 1.0f);

  AudioMsgSendToneOn(0, 659, 55);
  AudioMsgSendToneOn(2, 400, 12);
  if (waitMs(65)) {
    goto jingleOut;
  }
  AudioMsgSendToneOn(0, 659, 55);
  AudioMsgSendToneOn(2, 500, 10);
  if (waitMs(65)) {
    goto jingleOut;
  }
  AudioMsgSendToneOn(0, 784, 75);
  AudioMsgSendToneOn(2, 300, 20);
  if (waitMs(85)) {
    goto jingleOut;
  }
  AudioMsgSendToneOn(0, 659, 55);
  if (waitMs(65)) {
    goto jingleOut;
  }
  AudioMsgSendToneOn(0, 988, 110);
  AudioMsgSendToneOn(2, 250, 25);
  if (waitMs(125)) {
    goto jingleOut;
  }
  AudioMsgSendToneOn(0, 880, 45);
  if (waitMs(55)) {
    goto jingleOut;
  }
  AudioMsgSendToneOn(0, 784, 45);
  if (waitMs(55)) {
    goto jingleOut;
  }
  AudioMsgSendToneOn(0, 880, 75);
  AudioMsgSendToneOn(2, 400, 12);
  if (waitMs(85)) {
    goto jingleOut;
  }
  AudioMsgSendToneOn(0, 1047, 90);
  AudioMsgSendToneOn(2, 350, 15);
  if (waitMs(105)) {
    goto jingleOut;
  }
  AudioMsgSendToneOn(0, 1319, 75);
  if (waitMs(85)) {
    goto jingleOut;
  }
  AudioMsgSendToneOn(0, 1047, 55);
  if (waitMs(65)) {
    goto jingleOut;
  }
  AudioMsgSendToneOn(0, 1319, 320);
  AudioMsgSendToneOn(2, 200, 40);
  if (waitMs(350)) {
    goto jingleOut;
  }

  AudioMsgSendSetPW(0, 128, 0);

  {
    bool fireWasDown = JoyRead(JOY_FIRE);
    while (1) {
      uint8_t k = getKey();
      if (k == ' ') {
        break;
      }
      if (k == CTRL_C_KEY) {
        return false;
      }
      bool fireDown = JoyRead(JOY_FIRE);
      if (fireDown && !fireWasDown) {
        break;
      }
      fireWasDown = fireDown;
    }
  }
  return true;

jingleOut:
  AudioMsgSendSetPW(0, 128, 0);
  return false;
}

// Sets up a level: refills the invader block, resets shields and positions, and
// recomputes the starting speed.
static void initLevel(void) {
  cls();
  go_flag = false;
  na = 32;
  memset(a, 1, sizeof(a));
  s[0] = s[1] = s[2] = s[3] = 2;
  px = 19;
  ix = 2;
  iy = 3;
  id = 1;
  bx = by = ex = ey = -1;
  ux = -1;
  ud = 0;
  af = 0;
  fr = 0;
  repeatDir = 0;
  recalcSpeed();

  uint32_t now = millis();
  t_bullet = t_ebullet = t_invader = t_ufo = now;
  t_ufospawn = now;
  ufo_delay = UFO_MIN_MS + (xrng() % UFO_SPREAD_MS);
  t_repeat = 0;
  t_joyrepeat = 0;

  prevJoyLeft = JoyRead(JOY_LEFT);
  prevJoyRight = JoyRead(JOY_RIGHT);
  prevJoyFire = JoyRead(JOY_FIRE);

  updateHud();
  for (uint8_t i = 0; i < 4; i++) {
    drawShield(i);
  }
  drawAllInvaders();
  PlotChar(px, PLAYER_ROW, SHIP_CHAR);
  drawSkyline();

  char lvl[16];
  char* p = lvl;
  memcpy(p, "LEVEL ", 6);
  p += 6;
  p = putint(p, le);
  *p = '\0';
  printAt(15, 12, lvl);
  waitMs(500);
  printAt(15, 12, "        ");
}

// Advances the player's shot and resolves what it hits -- invader, shield or
// UFO -- updating score and speed accordingly.
static void movePlayerBullet(void) {
  PlotChar(bx, by, ' ');
  by--;
  if (by < 1) {
    by = -1;
    return;
  }

  if (by == 19) {
    int8_t si = shieldHit(bx);
    if (si >= 0) {
      s[si]--;
      by = -1;
      drawShield(si);
      return;
    }
  }

  if (by == 1 && ux >= 0 && bx == ux) {
    sc += (int16_t)((xrng() % 11 + 5) * 10);
    PlotChar(ux, 1, 137);
    AudioMsgSendToneOn(0, 1500, 50);
    AudioMsgSendToneOn(1, 750, 50);
    waitMs(10);
    PlotChar(ux, 1, ' ');
    ux = -1;
    by = -1;
    updateScore();
    return;
  }

  int dx = bx - ix;
  int dy = by - iy;
  if (dx >= 0 && dy >= 0 && (dx & 3) == 0 && (dy & 1) == 0) {
    uint8_t j = dx >> 2;
    uint8_t i = dy >> 1;
    if (j < 8 && i < 4 && a[j][i]) {
      a[j][i] = 0;
      na--;
      sc += rowScore[i];
      PlotChar(bx, by, 137);
      AudioMsgSendToneOn(0, 1200, 30);
      AudioMsgSendToneOn(1, 600, 30);
      waitMs(40);
      PlotChar(bx, by, ' ');
      by = -1;
      recalcSpeed();
      updateScore();
      return;
    }
  }

  if (by >= 0) {
    PlotChar(bx, by, 135);
  }
}

// Steps the invader block sideways, dropping and reversing at the edges, and
// ends the game if they reach the player's row.
static void moveInvaders(void) {
  af ^= 1;
  int8_t ox = ix, oy = iy;

  int8_t lc = 7, rc = 0;
  for (uint8_t j = 0; j < 8; j++) {
    for (uint8_t i = 0; i < 4; i++) {
      if (a[j][i]) {
        if ((int8_t)j < lc) {
          lc = j;
        }
        if ((int8_t)j > rc) {
          rc = j;
        }
      }
    }
  }

  if (id == 1 && ix + rc * 4 >= 38) {
    id = -1;
    iy++;
  } else if (id == -1 && ix + lc * 4 <= 1) {
    id = 1;
    iy++;
  } else {
    ix += id;
  }

  for (int8_t i = 3; i >= 0; i--) {
    for (uint8_t j = 0; j < 8; j++) {
      if (a[j][i] && iy + i * 2 >= 21) {
        go_flag = true;
        return;
      }
    }
  }

  for (uint8_t j = 0; j < 8; j++) {
    for (uint8_t i = 0; i < 4; i++) {
      if (a[j][i]) {
        PlotChar(ox + j * 4, oy + i * 2, ' ');
        PlotChar(ix + j * 4, iy + i * 2, rowChar[i] + af);
      }
    }
  }

  AudioMsgSendToneOn(1, af ? 100 : 120, 30);
}

// Randomly launches a shot from the lowest invader in a column, so shots appear
// to come from the front rank.
static void enemyFire(void) {
  if (xrng() % 10 * na) {
    return;
  }
  uint8_t j = xrng() % 8;
  for (int8_t i = 3; i >= 0; i--) {
    if (a[j][i]) {
      ey = iy + i * 2 + 1;
      ex = ix + j * 4;
      if (ey > 22) {
        ey = -1;
        return;
      }
      PlotChar(ex, ey, 136);
      AudioMsgSendToneOn(2, 200, 80);
      return;
    }
  }
}

// Advances an enemy shot and resolves hits on shields or the player.
static void moveEnemyBullet(void) {
  PlotChar(ex, ey, ' ');
  ey++;
  if (ey > 23) {
    ey = -1;
    return;
  }

  if (ey == 19) {
    int8_t si = shieldHit(ex);
    if (si >= 0) {
      s[si]--;
      ey = -1;
      drawShield(si);
      return;
    }
  }

  if (ey == 22 && ex == px) {
    PlotChar(px, 22, 137);
    AudioMsgSendToneOn(0, 400, 80);
    AudioMsgSendToneOn(1, 200, 80);
    waitMs(80);
    AudioMsgSendToneOn(0, 300, 80);
    AudioMsgSendToneOn(1, 150, 80);
    waitMs(80);
    AudioMsgSendToneOn(0, 200, 100);
    AudioMsgSendToneOn(1, 100, 100);
    waitMs(100);
    AudioMsgSendToneOn(0, 100, 200);
    AudioMsgSendToneOn(1, 50, 200);
    waitMs(200);
    PlotChar(px, PLAYER_ROW, ' ');
    lv--;
    ey = -1;
    by = -1;
    if (lv <= 0) {
      go_flag = true;
      return;
    }
    updateLives();
    waitMs(500);
    px = 19;
    PlotChar(px, PLAYER_ROW, SHIP_CHAR);
    return;
  }

  PlotChar(ex, ey, 136);
}

// Occasionally launches the bonus UFO across the top of the screen.
static inline void spawnUfo(void) {
  if (xrng() & 8) {
    ux = 39;
    ud = -1;
  } else {
    ux = 0;
    ud = 1;
  }

  ufo_delay = UFO_MIN_MS + (xrng() % UFO_SPREAD_MS);
}

// Advances the UFO and removes it once it leaves the screen.
static void moveUfo(void) {
  PlotChar(ux, 1, ' ');
  ux += ud;
  if (ux < 0 || ux > 39) {
    ux = -1;
    return;
  }
  PlotChar(ux, 1, 140);
  if (!(fr & 3)) {
    AudioMsgSendToneOn(2, 1200, 10);
  }
}

// Plays the level-complete sequence and advances to the next level.
static void levelComplete(void) {
  AudioMsgSendShutUp();
  SetReverseMode(true);
  printAt(12, 11, "  WAVE CLEAR!  ");
  SetReverseMode(false);

  AudioMsgSendToneOn(0, 1047, 75);
  AudioMsgSendToneOn(1, 523, 75);
  waitMs(200);
  AudioMsgSendToneOn(0, 523, 150);
  AudioMsgSendToneOn(1, 330, 150);
  waitMs(200);
  AudioMsgSendToneOn(0, 659, 150);
  AudioMsgSendToneOn(1, 392, 150);
  waitMs(200);
  AudioMsgSendToneOn(0, 784, 150);
  AudioMsgSendToneOn(1, 523, 150);
  waitMs(200);
  AudioMsgSendToneOn(0, 1047, 400);
  AudioMsgSendToneOn(1, 659, 400);
  waitMs(500);

  le++;
  initLevel();
}

// Shows the game-over screen and asks whether to play again.
static bool gameOver(void) {
  AudioMsgSendShutUp();
  if (sc > hi) {
    hi = sc;
  }

  for (int t = 0; t < 5; t++) {
    SetReverseMode(true);
    printAt(12, 11, "  GAME  OVER  ");
    SetReverseMode(false);
    AudioMsgSendToneOn(0, 150, 150);
    AudioMsgSendToneOn(1, 75, 150);
    if (waitMs(250)) {
      return false;
    }
    printAt(12, 11, "              ");
    if (waitMs(200)) {
      return false;
    }
  }
  SetReverseMode(true);
  printAt(12, 11, "  GAME  OVER  ");
  SetReverseMode(false);

  const char* rank = "RANK: CADET";
  if (sc >= 200) {
    rank = "RANK: PILOT";
  }
  if (sc >= 500) {
    rank = "RANK: ACE";
  }
  if (sc >= 1000) {
    rank = "RANK: LEGEND";
  }
  printAt((40 - strlen(rank)) / 2, 14, rank);

  char buf[24];
  char* p;
  p = buf;
  memcpy(p, "SCORE: ", 7);
  p += 7;
  p = putint(p, sc);
  *p = '\0';
  printAt(11, 15, buf);

  if (sc >= hi && sc > 0) {
    printAt(9, 16, "** HIGH SCORE! **");
  }

  p = buf;
  memcpy(p, "LEVEL REACHED: ", 15);
  p += 15;
  p = putint(p, le);
  *p = '\0';
  printAt(9, 17, buf);
  printAt(7, 20, "PLAY AGAIN? (Y/N)");

  for (int t = 600; t >= 100; t -= 25) {
    AudioMsgSendToneOn(0, t, 10);
    AudioMsgSendToneOn(1, t / 2, 10);
    if (waitMs(15)) {
      return false;
    }
  }
  AudioMsgSendShutUp();

  while (1) {
    uint8_t k = getKey();
    if (k == 'Y' || k == 'y') {
      return true;
    }
    if (k == 'N' || k == 'n' || k == CTRL_C_KEY) {
      return false;
    }
  }
}

// INVADERS: the game's main loop. Redefines the character set on entry and
// restores it on exit, and paces itself with millis deadlines rather than fixed
// delays so movement stays even.
void RunInvaders(void) {
  rng = millis() ^ 0xDEADBEEF;
  defineChars();
  hi = 0;

  if (!titleScreen()) {
    resetChars();
    cls();
    return;
  }

replay:
  lv = 3;
  sc = 0;
  le = 1;
  initLevel();

  while (1) {
    uint32_t now = millis();

    uint8_t k = getKey();

    if (k == CTRL_C_KEY) {
      resetChars();
      cls();
      return;
    }

    bool joyLeft = JoyRead(JOY_LEFT);
    bool joyRight = JoyRead(JOY_RIGHT);
    bool joyFire = JoyRead(JOY_FIRE);

    if (k == 'j') {
      moveLeft();
      repeatDir = 'j';
      t_repeat = now + REPEAT_INIT;
    } else if (k == 'l') {
      moveRight();
      repeatDir = 'l';
      t_repeat = now + REPEAT_INIT;
    } else if (k) {
      repeatDir = 0;
    }

    if (!k && repeatDir && now >= t_repeat) {
      bool held = false;
      if (repeatDir == 'j') {
        held = key_states[2][6].current_state;
      }
      if (repeatDir == 'l') {
        held = key_states[2][3].current_state;
      }
      if (held) {
        if (repeatDir == 'j') {
          moveLeft();
        } else {
          moveRight();
        }
        t_repeat = now + MOVE_MS;
      } else {
        repeatDir = 0;
      }
    }

    if (joyLeft || joyRight) {
      bool joyEdge = (joyLeft && !prevJoyLeft) || (joyRight && !prevJoyRight);
      if (joyEdge || now >= t_joyrepeat) {
        if (joyLeft) {
          moveLeft();
        } else {
          moveRight();
        }
        t_joyrepeat = now + (joyEdge ? REPEAT_INIT : MOVE_MS);
      }
    }

    if ((k == 'i' || (joyFire && !prevJoyFire)) && by < 0) {
      by = 21;
      bx = px;
      PlotChar(bx, by, 135);
      AudioMsgSendToneOn(0, 900, 15);
    }

    prevJoyLeft = joyLeft;
    prevJoyRight = joyRight;
    prevJoyFire = joyFire;

    if (by >= 0 && now - t_bullet >= BULLET_MS) {
      t_bullet = now;
      movePlayerBullet();
      if (go_flag) {
        break;
      }
    }

    if (now - t_invader >= invader_ms) {
      t_invader = now;
      moveInvaders();
      if (go_flag) {
        break;
      }
      if (ey < 0) {
        enemyFire();
      }
    }

    if (ey >= 0 && now - t_ebullet >= EBULLET_MS) {
      t_ebullet = now;
      moveEnemyBullet();
      if (go_flag) {
        break;
      }
    }

    if (ux >= 0) {
      if (now - t_ufo >= UFO_MS) {
        t_ufo = now;
        moveUfo();
      }
    } else if (now - t_ufospawn >= ufo_delay) {
      t_ufospawn = now;
      spawnUfo();
    }

    if (na <= 0) {
      levelComplete();
    }

    fr++;
  }

  if (gameOver()) {
    goto replay;
  }

  cls();
  printAt(0, 0, "THANKS FOR PLAYING!");
  char buf[24];
  char* p;
  p = buf;
  memcpy(p, "FINAL SCORE: ", 13);
  p += 13;
  p = putint(p, sc);
  *p = '\0';
  printAt(0, 1, buf);
  p = buf;
  memcpy(p, "HIGH SCORE: ", 12);
  p += 12;
  p = putint(p, hi);
  *p = '\0';
  printAt(0, 2, buf);
  resetChars();
}
