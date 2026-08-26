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

// BEEP with no argument plays a short C6. The per-keystroke click layers a
// brief pulse on voice 0 with noise on voice 2, which is what gives it the
// clacky character rather than a pure tone.
const uint16_t kBeepHz = 1047;
const uint16_t kBeepMs = 75;
const uint16_t kKeyClickHz = 999;
const uint16_t kKeyClickMs = 10;

// Pulse width is a full 8-bit range.
const int kPulseWidthMax = 255;

// BEEP: with no argument, a short tone. BEEP ON/OFF instead toggles the
// per-keystroke click, which persists across RUN and NEW.
bool CmdBeep(const char* args) {
  args = SkipWhitespace(args);
  if (strncasecmp(args, "ON", 2) == 0 &&
      (args[2] == '\0' || isspace((unsigned char)args[2]) || args[2] == ':')) {
    keyClickEnabled = true;
    return true;
  }
  if (strncasecmp(args, "OFF", 3) == 0 &&
      (args[3] == '\0' || isspace((unsigned char)args[3]) || args[3] == ':')) {
    keyClickEnabled = false;
    keyClickPending = false;
    return true;
  }
  AudioMsgSendToneOn(0, kBeepHz, kBeepMs);
  return true;
}

// Sounds a pending key click from the main loop. The keyboard ISR only sets a
// flag, since sending an audio message from interrupt context would be far too
// slow.
void TickKeyClick(void) {
  if (!keyClickPending) {
    return;
  }
  keyClickPending = false;
  AudioMsgSendToneOn(0, kKeyClickHz, kKeyClickMs);
  AudioMsgSendToneOn(2, kKeyClickHz, kKeyClickMs);
}

// SOUND: starts a tone for a given duration. An optional final argument makes
// the statement wait for the tone to finish instead of returning at once.
bool CmdToneOn(const char* args) {
  float voiceF, freqF, timeF;
  bool do_sleep = false;

  args = ParseExpression(args, &voiceF);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    return false;
  }
  args++;

  args = ParseExpression(args, &freqF);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    return false;
  }
  args++;

  args = ParseExpression(args, &timeF);
  if (!args) {
    return false;
  }

  args = SkipWhitespace(args);
  if (*args == ',') {
    args++;
    float sleepF;
    args = ParseExpression(args, &sleepF);
    if (!args) {
      return false;
    }
    do_sleep = ((int)sleepF == 1);
  }

  AudioMsgSendToneOn((uint8_t)(int)voiceF, (uint16_t)(int)freqF,
                     (uint32_t)(int)timeF);
  if (do_sleep) {
    DoSleep(timeF);
  }
  return true;
}

// SHUSH: silences one voice, or everything when given no argument.
bool CmdToneOff(const char* args) {
  args = SkipWhitespace(args);
  if (*args == '\0' || *args == ':') {
    AudioMsgSendShutUp();
    return true;
  }
  float voiceF;
  args = ParseExpression(args, &voiceF);
  if (!args) {
    return false;
  }
  uint8_t voice = (uint8_t)(int)voiceF;
  AudioMsgSendToneOff(voice);
  AudioMsgSendStopProgram(voice);
  return true;
}

// SOUNDPGM: loads a note array into a program slot, sets its repeat mode, or
// starts it playing, depending on the arguments given.
bool CmdSoundPgm(const char* args) {
  float pgmF;
  args = ParseExpression(args, &pgmF);
  if (!args) {
    return false;
  }

  int pgm = (int)pgmF;
  if (pgm < 0 || pgm > 2) {
    PrintError(ERR_ILLEGAL_QUANTITY);
    return false;
  }

  args = SkipWhitespace(args);
  if (*args == '\0' || *args == ':') {
    AudioMsgSendPlayProgram((uint8_t)pgm);
    return true;
  }

  if (*args != ',') {
    return false;
  }
  args++;
  args = SkipWhitespace(args);

  bool hasArray = false, repeatFlag = false, hasFlag = false;
  ArrayDescriptor* arr = NULL;

  if (isalpha(*args)) {
    char varName[MAX_VAR_NAME];
    const char* afterName = ParseVarName(args, varName, sizeof(varName));
    if (afterName) {
      if ((varName[0] == 'R' || varName[0] == 'r') && varName[1] == '\0') {
        repeatFlag = true;
        hasFlag = true;
        args = afterName;
      } else if ((varName[0] == 'S' || varName[0] == 's') &&
                 varName[1] == '\0') {
        repeatFlag = false;
        hasFlag = true;
        args = afterName;
      } else if (IsIntArrayVar(varName)) {
        arr = FindArray(varName);
        if (!arr) {
          PrintError(ERR_ARRAY_NOT_DIMD);
          return false;
        }
        hasArray = true;
        args = afterName;

        args = SkipWhitespace(args);
        if (*args == ',') {
          args++;
          args = SkipWhitespace(args);
          if (*args == 'R' || *args == 'r') {
            repeatFlag = true;
            hasFlag = true;
            args++;
          } else if (*args == 'S' || *args == 's') {
            repeatFlag = false;
            hasFlag = true;
            args++;
          }
        }
      } else {
        PrintError(ERR_TYPE_MISMATCH);
        return false;
      }
    }
  }

  if (hasArray && arr) {
    if (arr->dim2Size != 2) {
      PrintError(ERR_WRONG_DIMENSIONS);
      return false;
    }
    Note notes[AUDIO_MAX_NOTES];
    int noteCount = 0;
    for (int i = 0; i < arr->dim1Size && noteCount < AUDIO_MAX_NOTES; i++) {
      int16_t* freqPtr = (int16_t*)GetArrayElementPtr(arr, i * 2 + 0);
      int16_t* durPtr = (int16_t*)GetArrayElementPtr(arr, i * 2 + 1);
      if (*freqPtr == 0 && *durPtr == 0) {
        break;
      }
      notes[noteCount].freq = (uint16_t)*freqPtr;
      notes[noteCount].durationMs = (uint16_t)*durPtr;
      noteCount++;
    }
    if (noteCount > 0) {
      AudioMsgProgram((uint8_t)pgm, notes, (uint8_t)noteCount);
    }
  }

  if (hasFlag) {
    AudioMsgSendSetRepeat((uint8_t)pgm, repeatFlag);
  } else if (hasArray) {
    AudioMsgSendSetRepeat((uint8_t)pgm, false);
  }

  return true;
}

// SOUNDPWM: sets pulse width and LFO rate for a voice, shaping its timbre.
bool CmdSoundPwm(const char* args) {
  float voiceF, pwF, lfoF;

  args = ParseExpression(args, &voiceF);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    return false;
  }
  args++;

  args = ParseExpression(args, &pwF);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    return false;
  }
  args++;

  args = ParseExpression(args, &lfoF);
  if (!args) {
    return false;
  }

  int voice = (int)voiceF;
  if (voice < 0 || voice > 1) {
    PrintError(ERR_ILLEGAL_QUANTITY);
    return false;
  }
  int pw = (int)pwF;
  if (pw < 0 || pw > kPulseWidthMax) {
    PrintError(ERR_ILLEGAL_QUANTITY);
    return false;
  }
  if (lfoF < 0.0f) {
    PrintError(ERR_ILLEGAL_QUANTITY);
    return false;
  }

  AudioMsgSendSetPW((uint8_t)voice, (uint8_t)pw, lfoF);
  return true;
}

// SOUNDPRT: sets portamento glide time for a voice; 0 turns it off.
bool CmdSoundPrt(const char* args) {
  float voiceF, msF;

  args = ParseExpression(args, &voiceF);
  if (!args) {
    return false;
  }
  args = SkipWhitespace(args);
  if (*args != ',') {
    return false;
  }
  args++;

  args = ParseExpression(args, &msF);
  if (!args) {
    return false;
  }

  int voice = (int)voiceF;
  if (voice < 0 || voice > 1) {
    PrintError(ERR_ILLEGAL_QUANTITY);
    return false;
  }
  int ms = (int)msF;
  if (ms < 0 || ms > 65535) {
    PrintError(ERR_ILLEGAL_QUANTITY);
    return false;
  }

  AudioMsgSendSetPortamento((uint8_t)voice, (uint16_t)ms);
  return true;
}

// Waits out a delay while keeping the machine responsive: services key clicks,
// watches for BREAK, and still fires TIMER subroutines on schedule, so a long
// SLEEP does not stall periodic work.
void DoSleep(float sleep_val) {
  unsigned long deadline =
      millis() + (unsigned long)(sleep_val > 0 ? sleep_val : 0);
  while (millis() < deadline) {
    TickKeyClick();
    if (BufferScanAndRemove(CTRL_C_KEY)) {
      breakRequested = true;
      return;
    }
    if (timerEnabled && programRunning) {
      uint32_t now = (uint32_t)millis();
      if (now - timerLastFireMs >= timerIntervalMs) {
        timerLastFireMs = now;
        FireTimerGosub();
        if (breakRequested || !programRunning) {
          return;
        }
      }
    }
    if (!programRunning) {
      return;
    }
  }
}

// SLEEP and WAITMS: pauses for a number of milliseconds.
bool CmdSleep(const char* args) {
  float sleep_val;
  args = ParseExpression(args, &sleep_val);
  if (!args) {
    return false;
  }
  DoSleep(sleep_val);
  return true;
}

static const uint16_t freqTable4[12] = {262, 277, 294, 311, 330, 349,
                                        370, 392, 415, 440, 466, 494};

// Maps a note letter to its semitone offset within an octave.
static int NoteNameToSemitone(char c) {
  switch (c | 0x20) {
    case 'c':
      return 0;
    case 'd':
      return 2;
    case 'e':
      return 4;
    case 'f':
      return 5;
    case 'g':
      return 7;
    case 'a':
      return 9;
    case 'b':
      return 11;
  }
  return -1;
}

// Frequency for a semitone in an octave, taken from the octave-4 table and
// shifted, since doubling or halving moves by exactly one octave.
static uint16_t SemitoneToFreq(int semitone, int octave) {
  if (octave < 1) {
    octave = 1;
  }
  if (octave > 8) {
    octave = 8;
  }

  uint16_t base = freqTable4[semitone];
  int shift = octave - 4;
  if (shift > 0) {
    return base << shift;
  }
  if (shift < 0) {
    return base >> (-shift);
  }
  return base;
}

// Reads an optional numeric suffix from an MML string, advancing the cursor.
static int ParseMMLNumber(const char*& p) {
  int val = 0;
  if (!isdigit(*p)) {
    return -1;
  }
  while (isdigit(*p)) {
    val = val * 10 + (*p - '0');
    p++;
  }
  return val;
}

// PLAY: interprets an MML string -- notes, accidentals, octave and length
// changes, tempo, rests and dotted notes -- and turns it into a note sequence.
bool CmdPlay(const char* args) {
  char mml[256];
  args = ParseStringExpression(args, mml, sizeof(mml));
  if (!args) {
    PrintError(ERR_SYNTAX);
    return false;
  }

  int octave = 4;
  int defLen = 4;
  int tempo = 120;
  int voice = 0;
  bool repeat = false;

  Note notes[AUDIO_MAX_NOTES];
  int noteCount = 0;

  const char* p = mml;

  while (*p && noteCount < AUDIO_MAX_NOTES) {
    char c = *p;

    if (c == ' ') {
      p++;
      continue;
    }

    int semitone = NoteNameToSemitone(c);
    if (semitone >= 0) {
      p++;
      if (*p == '#' || *p == '+') {
        semitone++;
        if (semitone > 11) {
          semitone = 0;
          octave++;
        }
        p++;
      } else if (*p == '-') {
        semitone--;
        if (semitone < 0) {
          semitone = 11;
          octave--;
        }
        p++;
      }

      int len = ParseMMLNumber(p);
      if (len < 0) {
        len = defLen;
      }

      uint16_t dur = (uint16_t)(60000UL * 4 / ((uint32_t)tempo * len));
      if (*p == '.') {
        dur = dur + dur / 2;
        p++;
      }

      uint16_t freq = SemitoneToFreq(semitone, octave);
      notes[noteCount].freq = freq;
      notes[noteCount].durationMs = dur;
      noteCount++;
      continue;
    }

    if ((c | 0x20) == 'r' || (c | 0x20) == 'p') {
      p++;
      int len = ParseMMLNumber(p);
      if (len < 0) {
        len = defLen;
      }
      uint16_t dur = (uint16_t)(60000UL * 4 / ((uint32_t)tempo * len));
      if (*p == '.') {
        dur = dur + dur / 2;
        p++;
      }
      notes[noteCount].freq = 0;
      notes[noteCount].durationMs = dur;
      noteCount++;
      continue;
    }

    if ((c | 0x20) == 'o') {
      p++;
      int val = ParseMMLNumber(p);
      if (val >= 1 && val <= 8) {
        octave = val;
      }
      continue;
    }
    if (c == '>') {
      p++;
      if (octave < 8) {
        octave++;
      }
      continue;
    }
    if (c == '<') {
      p++;
      if (octave > 1) {
        octave--;
      }
      continue;
    }

    if ((c | 0x20) == 'l') {
      p++;
      int val = ParseMMLNumber(p);
      if (val > 0) {
        defLen = val;
      }
      continue;
    }

    if ((c | 0x20) == 't') {
      p++;
      int val = ParseMMLNumber(p);
      if (val > 0) {
        tempo = val;
      }
      continue;
    }

    if ((c | 0x20) == 'v') {
      p++;
      int val = ParseMMLNumber(p);
      if (val >= 0 && val <= 2) {
        voice = val;
      }
      continue;
    }

    if (c == '!') {
      p++;
      repeat = true;
      continue;
    }

    p++;
  }

  if (noteCount > 0) {
    AudioMsgProgram((uint8_t)voice, notes, (uint8_t)noteCount);
    AudioMsgSendSetRepeat((uint8_t)voice, repeat);
    AudioMsgSendPlayProgram((uint8_t)voice);
  }
  return true;
}
