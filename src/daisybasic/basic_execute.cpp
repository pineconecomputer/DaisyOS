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

// Stops a running program after BREAK: reports the line, remembers it so CONT
// can resume, and tears down timer, trap and file state so nothing keeps firing
// after the program has stopped.
static void HandleBreak(void) {
  char breakBuf[32];
  snprintf(breakBuf, sizeof(breakBuf), "BREAK IN LINE %d",
           program[currentExecLine].lineNum);
  Newline();
  PrintStr(breakBuf);
  continueLineIndex = currentExecLine;
  programRunning = false;
  breakRequested = false;
  timerEnabled = false;
  trapActive = false;
  trapTriggered = false;
  CloseAllFileChannels();
}

// Runs the TIMER subroutine. Saves and restores the interpreter's position
// around the call, and pushes a sentinel return frame so the handler's RETURN
// unwinds back here rather than into the interrupted program.
void FireTimerGosub(void) {
  int targetIdx = FindProgramLine((uint16_t)timerTargetLine);
  if (targetIdx < 0) {
    return;
  }

  if (!GosubStackEnsureCapacity(gosubStackTop + 1)) {
    PrintError(ERR_OUT_OF_MEMORY);
    return;
  }
  gosubStack[gosubStackTop].lineIndex = programLineCount;
  gosubStack[gosubStackTop].stmtSkip = 0;
  gosubStack[gosubStackTop].thenClauseSkip = 0;
  gosubStackTop++;

  int savedExecLine = currentExecLine;
  int savedGoto = gotoLineNum;
  bool savedRunning = programRunning;

  gotoLineNum = -1;
  RunFromLine(targetIdx);

  currentExecLine = savedExecLine;
  gotoLineNum = savedGoto;
  if (continueLineIndex < 0) {
    programRunning = savedRunning;
  }
}

// The main execution loop: runs lines in order from a starting index, following
// jumps, firing timers between statements, and stopping on END, BREAK or an
// untrapped error.
bool RunFromLine(int startLine) {
  programRunning = true;
  breakRequested = false;
  currentExecLine = startLine;

  while (programRunning && currentExecLine >= 0 &&
         currentExecLine < programLineCount) {
    TickKeyClick();

    if (BufferScanAndRemove(CTRL_C_KEY)) {
      breakRequested = true;
    } else if (BufferScanAndRemove(SCROLL_LOCK_KEY)) {
      uint8_t k;
      do {
        k = BufferGet();
      } while (k != SCROLL_LOCK_KEY && k != CTRL_C_KEY);
      if (k == CTRL_C_KEY) {
        breakRequested = true;
      }
    }

    if (breakRequested) {
      HandleBreak();
      return true;
    }

    gotoLineNum = -1;
    errorPrinted = false;

    if (!ExecuteTokenizedLine(GetLineTokens(currentExecLine),
                              program[currentExecLine].tokenLen)) {
      if (breakRequested) {
        HandleBreak();
        return true;
      }
      if (trapTriggered) {
        trapTriggered = false;
        trapActive = true;
        int trapIdx = FindProgramLine((uint16_t)trapLineNum);
        if (trapIdx < 0) {
          PrintError(ERR_UNDEF_STATEMENT);
          continueLineIndex = currentExecLine;
          programRunning = false;
          return true;
        }
        currentExecLine = trapIdx;
        continue;
      }
      if (!errorPrinted) {
        PrintError(ERR_SYNTAX);
      }
      continueLineIndex = currentExecLine;
      programRunning = false;
      return true;
    }

    if (timerEnabled && gotoLineNum < 0) {
      uint32_t now = (uint32_t)millis();
      if (now - timerLastFireMs >= timerIntervalMs) {
        timerLastFireMs = now;
        if (!GosubStackEnsureCapacity(gosubStackTop + 1)) {
          PrintError(ERR_OUT_OF_MEMORY);
          breakRequested = true;
        } else {
          gosubStack[gosubStackTop].lineIndex = currentExecLine + 1;
          gosubStack[gosubStackTop].stmtSkip = 0;
          gosubStack[gosubStackTop].thenClauseSkip = 0;
          gosubStackTop++;
          gotoLineNum = timerTargetLine;
        }
      }
    }

    if (gotoLineNum >= 0) {
      int idx = FindProgramLine((uint16_t)gotoLineNum);
      if (idx < 0) {
        PrintError(ERR_UNDEF_STATEMENT);
        continueLineIndex = currentExecLine;
        programRunning = false;
        return false;
      }
      currentExecLine = idx;
    } else {
      currentExecLine++;
    }
  }

  continueLineIndex = -1;
  programRunning = false;
  return true;
}

// CONT: resumes from where BREAK or END stopped the program.
bool CmdContinue(const char* args) {
  (void)args;
  if (continueLineIndex < 0 || continueLineIndex >= programLineCount) {
    PrintError(ERR_UNDEF_STATEMENT);
    return false;
  }
  int resumeAt = continueLineIndex;
  continueLineIndex = -1;
  return RunFromLine(resumeAt);
}

// RUN: clears variables and starts from the first line, or from a given one.
bool CmdRun(const char* args) {
  if (programLineCount == 0) {
    return true;
  }

  int startLine = 0;
  args = SkipWhitespace(args);
  if (*args && isdigit(*args)) {
    float lineNumF;
    if (ParseExpression(args, &lineNumF)) {
      int lineNum = (int)lineNumF;
      int idx = FindProgramLine((uint16_t)lineNum);
      if (idx < 0) {
        PrintError(ERR_UNDEF_STATEMENT);
        return false;
      }
      startLine = idx;
    }
  }

  CmdClr(NULL);
  continueLineIndex = -1;
  return RunFromLine(startLine);
}

// Dispatches one statement to its command handler by matching the leading
// keyword.
bool ExecuteSingleStatement(const char* line) {
  const char* rest;

  line = SkipWhitespace(line);
  if (*line == '\0') {
    return true;
  }

  if ((rest = MatchCommand(line, "rem")) != NULL) {
    return true;
  }
  if ((rest = MatchCommand(line, "print")) != NULL) {
    return CmdPrint(rest);
  }
  if ((rest = MatchCommand(line, "let")) != NULL) {
    return CmdLet(rest);
  }
  if ((rest = MatchCommand(line, "locate")) != NULL) {
    return CmdLocate(rest);
  }
  if ((rest = MatchCommand(line, "line")) != NULL) {
    return CmdLine(rest);
  }
  if ((rest = MatchCommand(line, "beep")) != NULL) {
    return CmdBeep(rest);
  }
  if ((rest = MatchCommand(line, "cls")) != NULL) {
    return CmdCls(rest);
  }
  if ((rest = MatchCommand(line, "defchar")) != NULL) {
    return CmdDefChar(rest);
  }
  if ((rest = MatchCommand(line, "resetchar")) != NULL) {
    return CmdResetChar(rest);
  }
  if ((rest = MatchCommand(line, "defgfx")) != NULL) {
    return CmdDefGfx(rest);
  }
  if ((rest = MatchCommand(line, "resetgfx")) != NULL) {
    return CmdResetGfx(rest);
  }
  if ((rest = MatchCommand(line, "charmode")) != NULL) {
    return CmdCharMode(rest);
  }
  if ((rest = MatchCommand(line, "reverse")) != NULL) {
    return CmdReverse(rest);
  }
  if ((rest = MatchCommand(line, "normal")) != NULL) {
    return CmdNormal(rest);
  }
  if ((rest = MatchCommand(line, "sound")) != NULL) {
    return CmdToneOn(rest);
  }
  if ((rest = MatchCommand(line, "shush")) != NULL) {
    return CmdToneOff(rest);
  }
  if ((rest = MatchCommand(line, "soundpgm")) != NULL) {
    return CmdSoundPgm(rest);
  }
  if ((rest = MatchCommand(line, "soundpwm")) != NULL) {
    return CmdSoundPwm(rest);
  }
  if ((rest = MatchCommand(line, "soundprt")) != NULL) {
    return CmdSoundPrt(rest);
  }
  if ((rest = MatchCommand(line, "play")) != NULL) {
    return CmdPlay(rest);
  }
  if ((rest = MatchCommand(line, "sleep")) != NULL) {
    return CmdSleep(rest);
  }
  if ((rest = MatchCommand(line, "goto")) != NULL) {
    return CmdGoto(rest);
  }
  if ((rest = MatchCommand(line, "end")) != NULL) {
    return CmdEnd(rest);
  }
  if ((rest = MatchCommand(line, "for")) != NULL) {
    return CmdFor(rest);
  }
  if ((rest = MatchCommand(line, "next")) != NULL) {
    return CmdNext(rest);
  }
  if ((rest = MatchCommand(line, "if")) != NULL) {
    return CmdIf(rest);
  }
  if ((rest = MatchCommand(line, "netinput")) != NULL) {
    return CmdNetInput(rest);
  }
  if ((rest = MatchCommand(line, "netget")) != NULL) {
    return CmdNetGet(rest);
  }
  if ((rest = MatchCommand(line, "netprint")) != NULL) {
    return CmdNetPrint(rest);
  }
  if ((rest = MatchCommand(line, "netdisconnect")) != NULL) {
    return CmdNetDisconnect(rest);
  }
  if ((rest = MatchCommand(line, "netconnect")) != NULL) {
    return CmdNetConnect(rest);
  }
  if ((rest = MatchCommand(line, "wifi")) != NULL) {
    return CmdWifi(rest);
  }
  if ((rest = MatchCommand(line, "input")) != NULL) {
    return CmdInput(rest);
  }
  if ((rest = MatchCommand(line, "get")) != NULL) {
    return CmdGet(rest);
  }
  if ((rest = MatchCommand(line, "gosub")) != NULL) {
    return CmdGosub(rest);
  }
  if ((rest = MatchCommand(line, "on")) != NULL) {
    return CmdOn(rest);
  }
  if ((rest = MatchCommand(line, "def")) != NULL) {
    return CmdDef(rest);
  }
  if ((rest = MatchCommand(line, "timer")) != NULL) {
    return CmdTimer(rest);
  }
  if ((rest = MatchCommand(line, "while")) != NULL) {
    return CmdWhile(rest);
  }
  if ((rest = MatchCommand(line, "wend")) != NULL) {
    return CmdWend(rest);
  }
  if ((rest = MatchCommand(line, "do")) != NULL) {
    return CmdDo(rest);
  }
  if ((rest = MatchCommand(line, "until")) != NULL) {
    return CmdUntil(rest);
  }
  if ((rest = MatchCommand(line, "exit")) != NULL) {
    return CmdExit(rest);
  }
  if ((rest = MatchCommand(line, "return")) != NULL) {
    return CmdReturn(rest);
  }
  if ((rest = MatchCommand(line, "data")) != NULL) {
    return CmdData(rest);
  }
  if ((rest = MatchCommand(line, "read")) != NULL) {
    return CmdRead(rest);
  }
  if ((rest = MatchCommand(line, "readmat")) != NULL) {
    return CmdReadMat(rest);
  }
  if ((rest = MatchCommand(line, "restore")) != NULL) {
    return CmdRestore(rest);
  }
  if ((rest = MatchCommand(line, "dim")) != NULL) {
    return CmdDim(rest);
  }
  if ((rest = MatchCommand(line, "plotchar")) != NULL) {
    return CmdPlotChar(rest);
  }
  if ((rest = MatchCommand(line, "fillcells")) != NULL) {
    return CmdFillCells(rest);
  }
  if ((rest = MatchCommand(line, "hline")) != NULL) {
    return CmdHLine(rest);
  }
  if ((rest = MatchCommand(line, "vline")) != NULL) {
    return CmdVLine(rest);
  }
  if ((rest = MatchCommand(line, "waitms")) != NULL) {
    return CmdSleep(rest);
  }
  if ((rest = MatchCommand(line, "moveblock")) != NULL) {
    return CmdMoveBlock(rest);
  }
  if ((rest = MatchCommand(line, "fillblock")) != NULL) {
    return CmdFillBlock(rest);
  }
  if ((rest = MatchCommand(line, "pplot")) != NULL) {
    return CmdPPlot(rest);
  }
  if ((rest = MatchCommand(line, "pline")) != NULL) {
    return CmdPLine(rest);
  }
  if ((rest = MatchCommand(line, "pcircle")) != NULL) {
    return CmdPCircle(rest);
  }
  if ((rest = MatchCommand(line, "pfill")) != NULL) {
    return CmdPFill(rest);
  }
  if ((rest = MatchCommand(line, "ppoly")) != NULL) {
    return CmdPPoly(rest);
  }
  if ((rest = MatchCommand(line, "box")) != NULL) {
    return CmdBox(rest);
  }
  if ((rest = MatchCommand(line, "setattrib")) != NULL) {
    return CmdSetAttrib(rest);
  }
  if ((rest = MatchCommand(line, "cleararr")) != NULL) {
    return CmdClearArr(rest);
  }
  if ((rest = MatchCommand(line, "swaparr")) != NULL) {
    return CmdSwapArr(rest);
  }
  if ((rest = MatchCommand(line, "scrollx")) != NULL) {
    return CmdScrollX(rest);
  }
  if ((rest = MatchCommand(line, "scroll")) != NULL) {
    return CmdScroll(rest);
  }
  if ((rest = MatchCommand(line, "copychar")) != NULL) {
    return CmdCopyChar(rest);
  }
  if ((rest = MatchCommand(line, "version")) != NULL) {
    return CmdVersion(rest);
  }
  if ((rest = MatchCommand(line, "chunk")) != NULL) {
    return CmdChunk(rest);
  }
  if ((rest = MatchCommand(line, "settime")) != NULL) {
    return CmdSetTime(rest);
  }
  if ((rest = MatchCommand(line, "setdate")) != NULL) {
    return CmdSetDate(rest);
  }
  if ((rest = MatchCommand(line, "renumber")) != NULL) {
    return CmdRenumber(rest);
  }
  if ((rest = MatchCommand(line, "catalog")) != NULL) {
    return CmdCatalog(rest);
  }
  if ((rest = MatchCommand(line, "cat")) != NULL) {
    return CmdCatalog(rest);
  }
  if ((rest = MatchCommand(line, "load")) != NULL) {
    return CmdLoad(rest);
  }
  if ((rest = MatchCommand(line, "save")) != NULL) {
    return CmdSave(rest);
  }
  if ((rest = MatchCommand(line, "delete")) != NULL) {
    return CmdDel(rest);
  }
  if ((rest = MatchCommand(line, "del")) != NULL) {
    return CmdDel(rest);
  }
  if ((rest = MatchCommand(line, "rename")) != NULL) {
    return CmdRen(rest);
  }
  if ((rest = MatchCommand(line, "ren")) != NULL) {
    return CmdRen(rest);
  }
  if ((rest = MatchCommand(line, "copy")) != NULL) {
    return CmdCopy(rest);
  }
  if ((rest = MatchCommand(line, "loadc")) != NULL) {
    return CmdLoadchar(rest);
  }
  if ((rest = MatchCommand(line, "savec")) != NULL) {
    return CmdSavechar(rest);
  }
  if ((rest = MatchCommand(line, "fopen")) != NULL) {
    return CmdFopen(rest);
  }
  if ((rest = MatchCommand(line, "fclose")) != NULL) {
    return CmdFclose(rest);
  }
  if ((rest = MatchCommand(line, "fprint")) != NULL) {
    return CmdFprint(rest);
  }
  if ((rest = MatchCommand(line, "finput")) != NULL) {
    return CmdFinput(rest);
  }
  if ((rest = MatchCommand(line, "fget")) != NULL) {
    return CmdFget(rest);
  }
  if ((rest = MatchCommand(line, "fput")) != NULL) {
    return CmdFput(rest);
  }
  if ((rest = MatchCommand(line, "fseek")) != NULL) {
    return CmdFseek(rest);
  }
  if ((rest = MatchCommand(line, "frewind")) != NULL) {
    return CmdFrewind(rest);
  }
  if ((rest = MatchCommand(line, "trap")) != NULL) {
    return CmdTrap(rest);
  }
  if ((rest = MatchCommand(line, "resume")) != NULL) {
    return CmdResume(rest);
  }
  if ((rest = MatchCommand(line, "more")) != NULL) {
    return CmdMore(rest);
  }
  if ((rest = MatchCommand(line, "deg")) != NULL) {
    return CmdDeg(rest);
  }
  if ((rest = MatchCommand(line, "rad")) != NULL) {
    return CmdRad(rest);
  }
  if ((rest = MatchCommand(line, "chdir")) != NULL) {
    return CmdChdir(rest);
  }
  if ((rest = MatchCommand(line, "mkdir")) != NULL) {
    return CmdMkdir(rest);
  }

  if (isalpha(*line)) {
    const char* p = line;
    while (isalnum(*p)) {
      p++;
    }
    if (IsTypeSuffixAt(p)) {
      p++;
    }
    while (*p && isspace(*p)) {
      p++;
    }
    if (*p == '=' || *p == '(') {
      return CmdLet(line);
    }
  }

  PrintError(ERR_SYNTAX);
  return false;
}

// Finds the next statement separator, ignoring colons inside string literals.
static const char* FindColonSeparator(const char* line) {
  bool inString = false;
  while (*line) {
    if (*line == '"') {
      inString = !inString;
    } else if (*line == ':' && !inString) {
      return line;
    }
    line++;
  }
  return NULL;
}

static int execStmtDepth = 0;

// Runs every colon-separated statement on a line, stopping early if one jumps
// or fails.
bool ExecuteStatement(const char* line) {
  char stmtBuf[256];

  line = SkipWhitespace(line);
  if (*line == '\0') {
    return true;
  }

  const char* p = line;
  while (*p && isalpha(*p)) {
    p++;
  }
  int cmdLen = p - line;
  if (cmdLen == 3 && strncasecmp(line, "rem", 3) == 0) {
    return true;
  }

  int skipCount = 0;
  if (compoundStmtIndex < 0) {
    skipCount = -compoundStmtIndex;
    compoundStmtIndex = 0;
  }

  execStmtDepth++;

  if (cmdLen == 2 && strncasecmp(line, "if", 2) == 0) {
    if (skipCount > 0) {
      const char* tp = line;
      bool inStr = false;
      while (*tp) {
        if (*tp == '"') {
          inStr = !inStr;
        } else if (!inStr && toupper(tp[0]) == 'T' && toupper(tp[1]) == 'H' &&
                   toupper(tp[2]) == 'E' && toupper(tp[3]) == 'N' &&
                   (tp[4] == ' ' || tp[4] == '\t' || tp[4] == '\0')) {
          break;
        }
        tp++;
      }
      if (!*tp) {
        execStmtDepth--;
        return false;
      }

      if (pendingThenClauseSkip > 0) {
        compoundStmtIndex = -(int)pendingThenClauseSkip;
        pendingThenClauseSkip = 0;
      } else {
        compoundStmtIndex = -skipCount;
      }
      bool result = ExecuteStatement(SkipWhitespace(tp + 4));
      execStmtDepth--;
      return result;
    }

    if (execStmtDepth == 1) {
      outerStmtSkipForGosub = 1;
      thenClauseSkipForGosub = 0;
    }
    if (execStmtDepth == 2) {
      thenClauseSkipForGosub = 1;
    }
    compoundStmtIndex = 0;
    bool result = ExecuteSingleStatement(line);
    execStmtDepth--;
    return result;
  }

  int stmtNum = 0;
  while (line && *line) {
    line = SkipWhitespace(line);
    if (*line == '\0') {
      break;
    }

    const char* colon = FindColonSeparator(line);
    if (colon) {
      if (stmtNum >= skipCount) {
        int len = colon - line;
        if (len >= (int)sizeof(stmtBuf)) {
          len = sizeof(stmtBuf) - 1;
        }
        strncpy(stmtBuf, line, len);
        stmtBuf[len] = '\0';

        if (execStmtDepth == 1) {
          outerStmtSkipForGosub = stmtNum + 1;
          thenClauseSkipForGosub = 0;
        }
        if (execStmtDepth == 2) {
          thenClauseSkipForGosub = stmtNum + 1;
        }
        compoundStmtIndex = stmtNum;
        if (!ExecuteSingleStatement(stmtBuf)) {
          execStmtDepth--;
          return false;
        }
        if (gotoLineNum >= 0) {
          execStmtDepth--;
          return true;
        }
      }
      stmtNum++;
      line = colon + 1;
    } else {
      if (stmtNum >= skipCount) {
        if (execStmtDepth == 1) {
          outerStmtSkipForGosub = stmtNum + 1;
          thenClauseSkipForGosub = 0;
        }
        if (execStmtDepth == 2) {
          thenClauseSkipForGosub = stmtNum + 1;
        }
        compoundStmtIndex = stmtNum;
        bool result = ExecuteSingleStatement(line);
        execStmtDepth--;
        return result;
      }
      break;
    }
  }
  execStmtDepth--;
  return true;
}

// Expands a stored line back to text and runs it. Programs are kept tokenised,
// so execution detokenises on the fly.
bool ExecuteTokenizedLine(const uint8_t* tokens, int tokenLen) {
  if (tokenLen == 0 || tokens[0] == TOK_EOL) {
    return true;
  }
  char lineBuf[256];
  DetokenizeLine(tokens, tokenLen, lineBuf, sizeof(lineBuf));
  return ExecuteStatement(lineBuf);
}

// Detects a leading line number, which decides whether input is stored as a
// program line or executed immediately. Overlong numbers are clamped so the
// caller can reject them.
static bool StartsWithLineNumber(const char* line, int* lineNum,
                                 const char** rest) {
  line = SkipWhitespace(line);
  if (!isdigit(*line)) {
    return false;
  }
  int num = 0;
  while (isdigit(*line)) {
    num = num * 10 + (*line - '0');
    if (num > 65535) {
      num = 65536;
    }
    line++;
  }
  *lineNum = num;
  *rest = line;
  return true;
}

// Prints the Ready prompt and clears the rest of the line.
static void PrintReady(void) {
  PrintStr("Ready.");
  Newline();
  FillCells(0, GetCurrentLine(), ' ', VID_WIDTH);
}

// Entry point for a line of input: store it as a program line if numbered,
// otherwise execute it immediately.
bool _BasicExecute(const char* line, bool showReady) {
  const char* rest;
  int lineNum;
  bool result;

  line = SkipWhitespace(line);
  if (*line == '\0') {
    return true;
  }

  if (StartsWithLineNumber(line, &lineNum, &rest)) {
    if (lineNum < 1 || lineNum > 65535) {
      PrintError(ERR_ILLEGAL_LINE_NUMBER);
      if (showReady) {
        PrintReady();
      }
      return false;
    }
    uint8_t tokens[MAX_LINE_TOKENS];
    int tokenLen = TokenizeLine(rest, tokens, MAX_LINE_TOKENS);
    if (tokenLen < 0) {
      return false;
    }
    if (!InsertProgramLine((uint16_t)lineNum, tokens, tokenLen)) {
      PrintError(ERR_OUT_OF_MEMORY);
      return false;
    }
    return true;
  }

  if ((rest = MatchCommand(line, "demo")) != NULL) {
    CmdDemo();
    Newline();
    if (showReady) {
      PrintReady();
    }
    return true;
  }
  if ((rest = MatchCommand(line, "shapedemo")) != NULL) {
    CmdShapeDemo();
    Newline();
    if (showReady) {
      PrintReady();
    }
    return true;
  }
  if ((rest = MatchCommand(line, "reboot")) != NULL) {
    DoReset();
    return true;
  }
  if ((rest = MatchCommand(line, "list")) != NULL) {
    result = CmdList(rest);
    Newline();
    if (showReady) {
      PrintReady();
    }
    return result;
  }
  if ((rest = MatchCommand(line, "run")) != NULL) {
    result = CmdRun(rest);
    Newline();
    if (showReady) {
      PrintReady();
    }
    return result;
  }
  if ((rest = MatchCommand(line, "cont")) != NULL) {
    result = CmdContinue(rest);
    Newline();
    if (showReady) {
      PrintReady();
    }
    return result;
  }
  if ((rest = MatchCommand(line, "new")) != NULL) {
    result = CmdNew(rest);
    Newline();
    if (showReady) {
      PrintReady();
    }
    return result;
  }
  if ((rest = MatchCommand(line, "clr")) != NULL) {
    result = CmdClr(rest);
    if (showReady) {
      PrintReady();
    }
    return result;
  }
  if ((rest = MatchCommand(line, "version")) != NULL) {
    result = CmdVersion(rest);
    Newline();
    if (showReady) {
      PrintReady();
    }
    return result;
  }
  if ((rest = MatchCommand(line, "chunk")) != NULL) {
    result = CmdChunk(rest);
    if (showReady) {
      PrintReady();
    }
    return result;
  }
  if ((rest = MatchCommand(line, "load")) != NULL) {
    result = CmdLoad(rest);
    if (showReady) {
      PrintReady();
    }
    return result;
  }
  if ((rest = MatchCommand(line, "save")) != NULL) {
    result = CmdSave(rest);
    if (showReady) {
      PrintReady();
    }
    return result;
  }
  if ((rest = MatchCommand(line, "catalog")) != NULL) {
    result = CmdCatalog(rest);
    if (showReady) {
      PrintReady();
    }
    return result;
  }
  if ((rest = MatchCommand(line, "cat")) != NULL) {
    result = CmdCatalog(rest);
    if (showReady) {
      PrintReady();
    }
    return result;
  }
  if ((rest = MatchCommand(line, "delete")) != NULL) {
    result = CmdDel(rest);
    if (showReady) {
      PrintReady();
    }
    return result;
  }
  if ((rest = MatchCommand(line, "del")) != NULL) {
    result = CmdDel(rest);
    if (showReady) {
      PrintReady();
    }
    return result;
  }
  if ((rest = MatchCommand(line, "rename")) != NULL) {
    result = CmdRen(rest);
    if (showReady) {
      PrintReady();
    }
    return result;
  }
  if ((rest = MatchCommand(line, "ren")) != NULL) {
    result = CmdRen(rest);
    if (showReady) {
      PrintReady();
    }
    return result;
  }
  if ((rest = MatchCommand(line, "copy")) != NULL) {
    result = CmdCopy(rest);
    if (showReady) {
      PrintReady();
    }
    return result;
  }
  if ((rest = MatchCommand(line, "deg")) != NULL) {
    result = CmdDeg(rest);
    if (showReady) {
      PrintReady();
    }
    return result;
  }
  if ((rest = MatchCommand(line, "rad")) != NULL) {
    result = CmdRad(rest);
    if (showReady) {
      PrintReady();
    }
    return result;
  }
  if ((rest = MatchCommand(line, "chdir")) != NULL) {
    result = CmdChdir(rest);
    if (showReady) {
      PrintReady();
    }
    return result;
  }
  if ((rest = MatchCommand(line, "mkdir")) != NULL) {
    result = CmdMkdir(rest);
    if (showReady) {
      PrintReady();
    }
    return result;
  }
  if ((rest = MatchCommand(line, "loadc")) != NULL) {
    result = CmdLoadchar(rest);
    if (showReady) {
      PrintReady();
    }
    return result;
  }
  if ((rest = MatchCommand(line, "savec")) != NULL) {
    result = CmdSavechar(rest);
    if (showReady) {
      PrintReady();
    }
    return result;
  }
  if ((rest = MatchCommand(line, "invaders")) != NULL) {
    RunInvaders();
    Newline();
    if (showReady) {
      PrintReady();
    }
    return true;
  }
  if ((rest = MatchCommand(line, "conway")) != NULL) {
    RunConway();
    Newline();
    if (showReady) {
      PrintReady();
    }
    return true;
  }
  if ((rest = MatchCommand(line, "term")) != NULL) {
    RunTerminal();
    Newline();
    if (showReady) {
      PrintReady();
    }
    return true;
  }
  if ((rest = MatchCommand(line, "zork")) != NULL) {
    RunZMachine();
    Newline();
    if (showReady) {
      PrintReady();
    }
    return true;
  }

  if ((rest = MatchCommand(line, "goto")) != NULL) {
    result = CmdRun(rest);
    Newline();
    if (showReady) {
      PrintReady();
    }
    return result;
  }

  if ((rest = MatchCommand(line, "gosub")) != NULL) {
    PrintError(ERR_ILLEGAL_DIRECT);
    if (showReady) {
      PrintReady();
    }
    return false;
  }

  errorPrinted = false;
  result = ExecuteStatement(line);
  if (result) {
    if (showReady) {
      Newline();
      PrintReady();
    }
  } else {
    if (!errorPrinted) {
      PrintError(ERR_SYNTAX);
    }
    if (showReady) {
      PrintReady();
    }
  }
  return result;
}

// Runs a line of input and prints the Ready prompt afterwards.
bool BasicExecute(const char* line) { return _BasicExecute(line, true); }
