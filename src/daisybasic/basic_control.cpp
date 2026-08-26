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

// Grows the GOSUB return stack so nesting depth is limited by RAM, not a fixed
// constant.
bool GosubStackEnsureCapacity(int needed) {
  return EnsureCapacity((void**)&gosubStack, &gosubStackCapacity, needed,
                        sizeof(GosubEntry), 8, false);
}

// Grows the FOR loop stack.
static bool ForStackEnsureCapacity(int needed) {
  return EnsureCapacity((void**)&forStack, &forStackCapacity, needed,
                        sizeof(ForLoopEntry), 4, false);
}

// GOTO: jumps to a line number.
bool CmdGoto(const char* args) {
  float lineNumF;
  args = ParseExpression(args, &lineNumF);
  if (!args) {
    return false;
  }
  int lineNum = (int)lineNumF;
  if (lineNum < 0) {
    return false;
  }
  gotoLineNum = lineNum;
  return true;
}

// END: stops the program and returns to the prompt.
bool CmdEnd(const char* args) {
  (void)args;
  programRunning = false;
  return true;
}

// GOSUB: pushes the return point and jumps. The saved position includes the
// statement within the line, so RETURN resumes mid-line correctly.
bool CmdGosub(const char* args) {
  float lineNumF;
  args = ParseExpression(args, &lineNumF);
  if (!args) {
    return false;
  }
  int lineNum = (int)lineNumF;
  if (lineNum < 0) {
    return false;
  }

  if (!GosubStackEnsureCapacity(gosubStackTop + 1)) {
    PrintError(ERR_OUT_OF_MEMORY);
    return false;
  }
  gosubStack[gosubStackTop].lineIndex = currentExecLine;
  gosubStack[gosubStackTop].stmtSkip = (uint8_t)outerStmtSkipForGosub;
  gosubStack[gosubStackTop].thenClauseSkip = (uint8_t)thenClauseSkipForGosub;
  gosubStackTop++;
  gotoLineNum = lineNum;
  return true;
}

// RETURN: resumes after the matching GOSUB, optionally carrying up to two
// values for RESULT() to read back.
bool CmdReturn(const char* args) {
  if (gosubStackTop <= 0) {
    PrintError(ERR_RETURN_WITHOUT_GOSUB);
    return false;
  }

  returnValCount = 0;
  returnVals[0] = 0.0f;
  returnVals[1] = 0.0f;

  args = SkipWhitespace(args);
  if (*args) {
    float val1;
    const char* p = ParseExpression(args, &val1);
    if (p) {
      returnVals[0] = val1;
      returnValCount = 1;
      p = SkipWhitespace(p);
      if (*p == ',') {
        p++;
        float val2;
        const char* p2 = ParseExpression(p, &val2);
        if (p2) {
          returnVals[1] = val2;
          returnValCount = 2;
        }
      }
    }
  }

  GosubEntry entry = gosubStack[--gosubStackTop];
  int returnLine = entry.lineIndex;
  if (returnLine >= 0 && returnLine < programLineCount) {
    gotoLineNum = program[returnLine].lineNum;
    compoundStmtIndex = -(int)entry.stmtSkip;
    pendingThenClauseSkip = entry.thenClauseSkip;
  } else {
    programRunning = false;
  }
  return true;
}

// FOR: begins a counted loop. Re-entering the same FOR line reuses the existing
// stack frame instead of nesting, which is what makes single-line FOR/NEXT and
// jumping back into a loop behave sensibly.
bool CmdFor(const char* args) {
  char varName[MAX_VAR_NAME];
  float startVal, endVal, stepVal = 1.0f;

  args = ParseVarName(args, varName, sizeof(varName));
  if (!args) {
    return false;
  }

  args = SkipWhitespace(args);
  if (*args != '=') {
    return false;
  }
  args++;

  args = SkipWhitespace(args);
  const char* toPos = args;
  while (*toPos &&
         !(toupper(toPos[0]) == 'T' && toupper(toPos[1]) == 'O' &&
           (toPos[2] == ' ' || toPos[2] == '\t' || toPos[2] == '\0'))) {
    toPos++;
  }
  if (!*toPos) {
    return false;
  }

  int startLen = toPos - args;
  char startBuf[64];
  if (startLen >= (int)sizeof(startBuf)) {
    startLen = sizeof(startBuf) - 1;
  }
  strncpy(startBuf, args, startLen);
  startBuf[startLen] = '\0';
  while (startLen > 0 && isspace(startBuf[startLen - 1])) {
    startBuf[--startLen] = '\0';
  }
  if (!EvalExpression(startBuf, &startVal)) {
    return false;
  }

  args = toPos + 2;
  args = SkipWhitespace(args);

  const char* stepPos = args;
  while (*stepPos &&
         !(toupper(stepPos[0]) == 'S' && toupper(stepPos[1]) == 'T' &&
           toupper(stepPos[2]) == 'E' && toupper(stepPos[3]) == 'P' &&
           (stepPos[4] == ' ' || stepPos[4] == '\t' || stepPos[4] == '\0'))) {
    stepPos++;
  }

  if (*stepPos) {
    int endLen = stepPos - args;
    char endBuf[64];
    if (endLen >= (int)sizeof(endBuf)) {
      endLen = sizeof(endBuf) - 1;
    }
    strncpy(endBuf, args, endLen);
    endBuf[endLen] = '\0';
    while (endLen > 0 && isspace(endBuf[endLen - 1])) {
      endBuf[--endLen] = '\0';
    }
    if (!EvalExpression(endBuf, &endVal)) {
      return false;
    }
    args = stepPos + 4;
    if (!ParseExpression(args, &stepVal)) {
      return false;
    }
  } else {
    if (!EvalExpression(args, &endVal)) {
      return false;
    }
  }

  int existingIdx = -1;
  for (int i = forStackTop - 1; i >= 0; i--) {
    if (strcasecmp(forStack[i].varName, varName) == 0) {
      existingIdx = i;
      break;
    }
  }

  Variable* v = CreateVariable(varName, VAR_FLOAT);
  if (!v) {
    return false;
  }
  if (existingIdx < 0) {
    v->floatVal = startVal;
    v->type = VAR_FLOAT;
  }

  if (existingIdx >= 0) {
    forStackTop = existingIdx + 1;
    ForLoopEntry* entry = &forStack[existingIdx];
    entry->limitVal = endVal;
    entry->stepVal = stepVal;
    entry->returnLine = currentExecLine;
  } else {
    if (!ForStackEnsureCapacity(forStackTop + 1)) {
      PrintError(ERR_OUT_OF_MEMORY);
      return false;
    }
    strncpy(forStack[forStackTop].varName, varName, MAX_VAR_NAME - 1);
    forStack[forStackTop].varName[MAX_VAR_NAME - 1] = '\0';
    forStack[forStackTop].limitVal = endVal;
    forStack[forStackTop].stepVal = stepVal;
    forStack[forStackTop].returnLine = currentExecLine;
    forStackTop++;
  }
  return true;
}

// NEXT: steps the loop variable and either jumps back to the FOR or pops the
// frame. A named variable must match the innermost open loop.
bool CmdNext(const char* args) {
  char varName[MAX_VAR_NAME];
  args = SkipWhitespace(args);

  if (*args && isalpha(*args)) {
    args = ParseVarName(args, varName, sizeof(varName));
    if (!args) {
      return false;
    }
  } else if (forStackTop > 0) {
    strcpy(varName, forStack[forStackTop - 1].varName);
  } else {
    PrintError(ERR_NEXT_WITHOUT_FOR);
    return false;
  }

  int stackIdx = -1;
  for (int i = forStackTop - 1; i >= 0; i--) {
    if (strcasecmp(forStack[i].varName, varName) == 0) {
      stackIdx = i;
      break;
    }
  }
  if (stackIdx < 0) {
    PrintError(ERR_NEXT_WITHOUT_FOR);
    return false;
  }

  ForLoopEntry* entry = &forStack[stackIdx];
  Variable* v = FindVariable(varName);
  if (!v) {
    return false;
  }

  float currentVal = (v->type == VAR_INT) ? (float)v->intVal : v->floatVal;
  currentVal += entry->stepVal;
  v->floatVal = currentVal;
  v->type = VAR_FLOAT;

  bool done = (entry->stepVal > 0) ? (currentVal > entry->limitVal)
                                   : (currentVal < entry->limitVal);

  if (done) {
    forStackTop = stackIdx;
  } else {
    gotoLineNum = program[entry->returnLine].lineNum;
  }
  return true;
}

// Evaluates one comparison with no AND/OR, choosing string or numeric
// comparison from the operand types. Any parentheses wrapping the whole
// condition have already been peeled off by EvalCondition.
static bool EvalSimpleComparison(const char* cond, bool* out) {
  cond = SkipWhitespace(cond);

  const char* opPos = NULL;
  int opLen = 0;
  const char* p = cond;
  int parenDepth = 0;

  while (*p) {
    if (*p == '(') {
      parenDepth++;
    } else if (*p == ')') {
      parenDepth--;
    } else if (parenDepth == 0) {
      if (*p == '<' && *(p + 1) == '<') {
        p += 2;
        continue;
      }
      if (*p == '>' && *(p + 1) == '>') {
        p += 2;
        continue;
      }
      if (*p == '<' && *(p + 1) == '>') {
        opPos = p;
        opLen = 2;
        break;
      }
      if (*p == '<' && *(p + 1) == '=') {
        opPos = p;
        opLen = 2;
        break;
      }
      if (*p == '>' && *(p + 1) == '=') {
        opPos = p;
        opLen = 2;
        break;
      }
      if (*p == '<') {
        opPos = p;
        opLen = 1;
        break;
      }
      if (*p == '>') {
        opPos = p;
        opLen = 1;
        break;
      }
      if (*p == '=') {
        opPos = p;
        opLen = 1;
        break;
      }
    }
    p++;
  }

  if (!opPos) {
    float val;
    if (EvalExpression(cond, &val)) {
      *out = (val != 0.0f);
      return true;
    }
    return false;
  }

  char leftBuf[64], rightBuf[64];
  int leftLen = opPos - cond;
  if (leftLen >= (int)sizeof(leftBuf)) {
    PrintError(ERR_FORMULA_TOO_COMPLEX);
    return false;
  }
  strncpy(leftBuf, cond, leftLen);
  leftBuf[leftLen] = '\0';
  while (leftLen > 0 && isspace(leftBuf[leftLen - 1])) {
    leftBuf[--leftLen] = '\0';
  }

  const char* rightStart = opPos + opLen;
  while (*rightStart && isspace(*rightStart)) {
    rightStart++;
  }
  if (strlen(rightStart) >= sizeof(rightBuf)) {
    PrintError(ERR_FORMULA_TOO_COMPLEX);
    return false;
  }
  strncpy(rightBuf, rightStart, sizeof(rightBuf) - 1);
  rightBuf[sizeof(rightBuf) - 1] = '\0';

  char op1 = opPos[0];
  char op2 = (opLen == 2) ? opPos[1] : 0;

  const char* leftTrimmed = leftBuf;
  while (*leftTrimmed && isspace(*leftTrimmed)) {
    leftTrimmed++;
  }

  bool isStringCompare = false;
  if (*leftTrimmed == '"') {
    isStringCompare = true;
  } else {
    const char* lp = leftTrimmed;
    while (*lp && (isalnum(*lp) || *lp == '$')) {
      lp++;
    }
    if (lp > leftTrimmed && *(lp - 1) == '$') {
      isStringCompare = true;
    }
  }

  if (isStringCompare) {
    char leftStr[MAX_STR_EXPR_BUF], rightStr[MAX_STR_EXPR_BUF];
    if (!ParseStringExpression(leftTrimmed, leftStr, sizeof(leftStr))) {
      return false;
    }
    const char* rightTrimmed = rightBuf;
    while (*rightTrimmed && isspace(*rightTrimmed)) {
      rightTrimmed++;
    }
    if (!ParseStringExpression(rightTrimmed, rightStr, sizeof(rightStr))) {
      return false;
    }
    int cmp = strcmp(leftStr, rightStr);
    if (op1 == '=' && op2 == 0) {
      *out = (cmp == 0);
    } else if (op1 == '<' && op2 == 0) {
      *out = (cmp < 0);
    } else if (op1 == '>' && op2 == 0) {
      *out = (cmp > 0);
    } else if (op1 == '<' && op2 == '=') {
      *out = (cmp <= 0);
    } else if (op1 == '>' && op2 == '=') {
      *out = (cmp >= 0);
    } else if (op1 == '<' && op2 == '>') {
      *out = (cmp != 0);
    } else {
      return false;
    }
  } else {
    float leftVal, rightVal;
    if (!EvalExpression(leftBuf, &leftVal)) {
      return false;
    }
    if (!EvalExpression(rightBuf, &rightVal)) {
      return false;
    }
    if (op1 == '=' && op2 == 0) {
      *out = (leftVal == rightVal);
    } else if (op1 == '<' && op2 == 0) {
      *out = (leftVal < rightVal);
    } else if (op1 == '>' && op2 == 0) {
      *out = (leftVal > rightVal);
    } else if (op1 == '<' && op2 == '=') {
      *out = (leftVal <= rightVal);
    } else if (op1 == '>' && op2 == '=') {
      *out = (leftVal >= rightVal);
    } else if (op1 == '<' && op2 == '>') {
      *out = (leftVal != rightVal);
    } else {
      return false;
    }
  }
  return true;
}

// Finds the lowest-precedence AND or OR outside quotes and parentheses, so a
// condition can be split there. Skipping quoted text keeps a literal containing
// "AND" from being mistaken for the operator.
static const char* FindLogicalOp(const char* str, const char* start,
                                 const char** opName, int* opLen) {
  int parenDepth = 0;
  bool inString = false;
  const char *orFound = NULL, *andFound = NULL;

  while (*str) {
    if (*str == '"') {
      inString = !inString;
    } else if (!inString) {
      if (*str == '(') {
        parenDepth++;
      } else if (*str == ')') {
        parenDepth--;
      } else if (parenDepth == 0) {
        if (!orFound && strncasecmp(str, "OR", 2) == 0 &&
            (str == start || !isalnum(*(str - 1))) && !isalnum(str[2])) {
          orFound = str;
        }
        if (!andFound && strncasecmp(str, "AND", 3) == 0 &&
            (str == start || !isalnum(*(str - 1))) && !isalnum(str[3])) {
          andFound = str;
        }
      }
    }
    str++;
  }

  if (orFound) {
    *opName = "OR";
    *opLen = 2;
    return orFound;
  }
  if (andFound) {
    *opName = "AND";
    *opLen = 3;
    return andFound;
  }
  return NULL;
}

// Evaluates a full condition, splitting on AND/OR and recursing, then falling
// back to a simple comparison.
bool EvalCondition(const char* cond, bool* result) {
  cond = SkipWhitespace(cond);
  if (*cond == '\0') {
    return false;
  }

  // Peel off parentheses that wrap the whole condition, e.g. "(A=1 OR B=2)",
  // so the logical split below can see the operators inside them. The opening
  // paren must match the final one: "(A=1) AND (B=2)" is left alone, as is
  // "(A+1)*2 = 6", where the parens are part of an arithmetic expression.
  char stripBuf[128];
  while (*cond == '(') {
    int depth = 0;
    bool inString = false;
    const char* match = NULL;
    for (const char* p = cond; *p; p++) {
      if (*p == '"') {
        inString = !inString;
      } else if (!inString) {
        if (*p == '(') {
          depth++;
        } else if (*p == ')') {
          depth--;
          if (depth == 0) {
            match = p;
            break;
          }
        }
      }
    }
    if (!match || *SkipWhitespace(match + 1) != '\0') {
      break;
    }
    int innerLen = match - (cond + 1);
    if (innerLen >= (int)sizeof(stripBuf)) {
      PrintError(ERR_FORMULA_TOO_COMPLEX);
      return false;
    }
    memmove(stripBuf, cond + 1, innerLen);
    stripBuf[innerLen] = '\0';
    while (innerLen > 0 && isspace(stripBuf[innerLen - 1])) {
      stripBuf[--innerLen] = '\0';
    }
    cond = SkipWhitespace(stripBuf);
    if (*cond == '\0') {
      return false;
    }
  }

  const char* opName;
  int opLen;
  const char* opPos = FindLogicalOp(cond, cond, &opName, &opLen);

  if (opPos) {
    char leftBuf[128], rightBuf[128];
    int leftLen = opPos - cond;
    if (leftLen >= (int)sizeof(leftBuf)) {
      PrintError(ERR_FORMULA_TOO_COMPLEX);
      return false;
    }
    strncpy(leftBuf, cond, leftLen);
    leftBuf[leftLen] = '\0';
    while (leftLen > 0 && isspace(leftBuf[leftLen - 1])) {
      leftBuf[--leftLen] = '\0';
    }

    const char* rightStart = opPos + opLen;
    while (*rightStart && isspace(*rightStart)) {
      rightStart++;
    }
    if (strlen(rightStart) >= sizeof(rightBuf)) {
      PrintError(ERR_FORMULA_TOO_COMPLEX);
      return false;
    }
    strncpy(rightBuf, rightStart, sizeof(rightBuf) - 1);
    rightBuf[sizeof(rightBuf) - 1] = '\0';

    bool leftResult, rightResult;
    if (!EvalCondition(leftBuf, &leftResult)) {
      return false;
    }
    if (!EvalCondition(rightBuf, &rightResult)) {
      return false;
    }

    *result = (strcasecmp(opName, "AND") == 0) ? (leftResult && rightResult)
                                               : (leftResult || rightResult);
    return true;
  }

  if (strncasecmp(cond, "NOT", 3) == 0 && !isalnum(cond[3])) {
    const char* rest = SkipWhitespace(cond + 3);
    bool subResult;
    if (!EvalCondition(rest, &subResult)) {
      return false;
    }
    *result = !subResult;
    return true;
  }

  return EvalSimpleComparison(cond, result);
}

// IF/THEN: runs the rest of the line when the condition holds. A bare number
// after THEN is treated as a GOTO.
bool CmdIf(const char* args) {
  args = SkipWhitespace(args);

  const char* thenPos = args;
  bool inString = false;
  while (*thenPos) {
    if (*thenPos == '"') {
      inString = !inString;
    } else if (!inString && toupper(thenPos[0]) == 'T' &&
               toupper(thenPos[1]) == 'H' && toupper(thenPos[2]) == 'E' &&
               toupper(thenPos[3]) == 'N' &&
               (thenPos[4] == ' ' || thenPos[4] == '\t' ||
                thenPos[4] == '\0')) {
      break;
    }
    thenPos++;
  }
  if (!*thenPos) {
    return false;
  }

  int condLen = thenPos - args;
  char condBuf[128];
  if (condLen >= (int)sizeof(condBuf)) {
    PrintError(ERR_FORMULA_TOO_COMPLEX);
    return false;
  }
  strncpy(condBuf, args, condLen);
  condBuf[condLen] = '\0';
  while (condLen > 0 && isspace(condBuf[condLen - 1])) {
    condBuf[--condLen] = '\0';
  }

  bool result;
  if (!EvalCondition(condBuf, &result)) {
    return false;
  }
  if (!result) {
    return true;
  }

  const char* thenStatement = SkipWhitespace(thenPos + 4);
  if (isdigit(*thenStatement)) {
    float lineNumF;
    if (ParseExpression(thenStatement, &lineNumF)) {
      gotoLineNum = (int)lineNumF;
      return true;
    }
  }
  return ExecuteStatement(thenStatement);
}

// Grows the stack shared by WHILE and DO loops.
static bool WhileStackEnsureCapacity(int needed) {
  return EnsureCapacity((void**)&whileStack, &whileStackCapacity, needed,
                        sizeof(WhileEntry), 4, false);
}

// WHILE: tests before each pass, skipping to past the WEND when false. Program
// mode only, since it needs following lines to jump to.
bool CmdWhile(const char* args) {
  if (!programRunning) {
    PrintError(ERR_ILLEGAL_DIRECT);
    return false;
  }

  args = SkipWhitespace(args);
  bool result;
  if (!EvalCondition(args, &result)) {
    return false;
  }

  if (result) {
    if (whileStackTop > 0 &&
        whileStack[whileStackTop - 1].lineIndex == currentExecLine) {
      return true;
    }
    if (!WhileStackEnsureCapacity(whileStackTop + 1)) {
      PrintError(ERR_OUT_OF_MEMORY);
      return false;
    }
    whileStack[whileStackTop].lineIndex = currentExecLine;
    whileStack[whileStackTop].type = LOOP_WHILE;
    whileStackTop++;
    return true;
  } else {
    if (whileStackTop > 0 &&
        whileStack[whileStackTop - 1].lineIndex == currentExecLine) {
      whileStackTop--;
    }
    int depth = 1;
    for (int i = currentExecLine + 1; i < programLineCount; i++) {
      char lineBuf[256];
      DetokenizeLine(GetLineTokens(i), program[i].tokenLen, lineBuf,
                     sizeof(lineBuf));
      const char* p = SkipWhitespace(lineBuf);
      if (strncasecmp(p, "WHILE", 5) == 0 && !isalnum((unsigned char)p[5])) {
        depth++;
      } else if (strncasecmp(p, "WEND", 4) == 0 &&
                 (p[4] == '\0' || isspace((unsigned char)p[4]))) {
        depth--;
        if (depth == 0) {
          if (i + 1 < programLineCount) {
            gotoLineNum = program[i + 1].lineNum;
          } else {
            currentExecLine = i;
            gotoLineNum = -1;
          }
          return true;
        }
      }
    }
    PrintError(ERR_WHILE_WITHOUT_WEND);
    return false;
  }
}

// WEND: jumps back to the matching WHILE to re-test.
bool CmdWend(const char* args) {
  (void)args;
  if (!programRunning) {
    PrintError(ERR_ILLEGAL_DIRECT);
    return false;
  }

  if (whileStackTop <= 0 || whileStack[whileStackTop - 1].type != LOOP_WHILE) {
    PrintError(ERR_WEND_WITHOUT_WHILE);
    return false;
  }

  int whileLine = whileStack[whileStackTop - 1].lineIndex;
  gotoLineNum = program[whileLine].lineNum;
  return true;
}

// DO: marks the top of a post-test loop, so the body always runs once.
bool CmdDo(const char* args) {
  (void)args;
  if (!programRunning) {
    PrintError(ERR_ILLEGAL_DIRECT);
    return false;
  }

  if (whileStackTop > 0 &&
      whileStack[whileStackTop - 1].lineIndex == currentExecLine &&
      whileStack[whileStackTop - 1].type == LOOP_DO) {
    return true;
  }
  if (!WhileStackEnsureCapacity(whileStackTop + 1)) {
    PrintError(ERR_OUT_OF_MEMORY);
    return false;
  }
  whileStack[whileStackTop].lineIndex = currentExecLine;
  whileStack[whileStackTop].type = LOOP_DO;
  whileStackTop++;
  return true;
}

// UNTIL: repeats from the matching DO until the condition becomes true.
bool CmdUntil(const char* args) {
  if (!programRunning) {
    PrintError(ERR_ILLEGAL_DIRECT);
    return false;
  }

  if (whileStackTop <= 0 || whileStack[whileStackTop - 1].type != LOOP_DO) {
    PrintError(ERR_UNTIL_WITHOUT_DO);
    return false;
  }

  args = SkipWhitespace(args);
  bool result;
  if (!EvalCondition(args, &result)) {
    return false;
  }

  if (result) {
    whileStackTop--;
    return true;
  } else {
    int doLine = whileStack[whileStackTop - 1].lineIndex;
    gotoLineNum = program[doLine].lineNum;
    return true;
  }
}

// Advances to the next colon-separated statement, ignoring colons inside string
// literals.
static const char* NextSubStmt(const char* p) {
  bool in_string = false;
  while (*p) {
    if (*p == '"') {
      in_string = !in_string;
    } else if (*p == ':' && !in_string) {
      return SkipWhitespace(p + 1);
    }
    p++;
  }
  return NULL;
}

// Tests whether a statement begins with a keyword, requiring a separator after
// it so a longer identifier is not matched by mistake.
static bool StmtStartsWith(const char* p, const char* kw) {
  size_t n = strlen(kw);
  if (strncasecmp(p, kw, n) != 0) {
    return false;
  }
  char trailing = p[n];
  return trailing == '\0' || trailing == ':' || trailing == '"' ||
         trailing == '(' || isspace((unsigned char)trailing);
}

// Scans forward for the WEND or UNTIL that closes the current loop, tracking
// nesting so an inner loop's terminator is not taken for this one's. Used to
// skip a loop whose condition failed and to implement EXIT.
static bool ScanForLoopEnd(LoopType type, int fromLine) {
  int whileDepth = 0;
  int doDepth = 0;

  if (type == LOOP_WHILE) {
    whileDepth = 1;
  } else {
    doDepth = 1;
  }

  for (int i = fromLine + 1; i < programLineCount; i++) {
    char lineBuf[256];
    DetokenizeLine(GetLineTokens(i), program[i].tokenLen, lineBuf,
                   sizeof(lineBuf));

    for (const char* p = SkipWhitespace(lineBuf); p; p = NextSubStmt(p)) {
      if (StmtStartsWith(p, "REM")) {
        break;
      }
      if (StmtStartsWith(p, "WHILE")) {
        whileDepth++;
      } else if (StmtStartsWith(p, "WEND")) {
        whileDepth--;
        if (type == LOOP_WHILE && whileDepth == 0) {
          if (i + 1 < programLineCount) {
            gotoLineNum = program[i + 1].lineNum;
          } else {
            currentExecLine = i;
            gotoLineNum = -1;
          }
          return true;
        }
      } else if (StmtStartsWith(p, "DO")) {
        doDepth++;
      } else if (StmtStartsWith(p, "UNTIL")) {
        doDepth--;
        if (type == LOOP_DO && doDepth == 0) {
          if (i + 1 < programLineCount) {
            gotoLineNum = program[i + 1].lineNum;
          } else {
            currentExecLine = i;
            gotoLineNum = -1;
          }
          return true;
        }
      }
    }
  }
  return false;
}

// EXIT: leaves the innermost WHILE or DO immediately, continuing after its
// closing keyword.
bool CmdExit(const char* args) {
  (void)args;
  if (!programRunning) {
    PrintError(ERR_ILLEGAL_DIRECT);
    return false;
  }

  if (whileStackTop <= 0) {
    PrintError(ERR_EXIT_NOT_IN_LOOP);
    return false;
  }

  WhileEntry* top = &whileStack[whileStackTop - 1];
  LoopType type = top->type;
  int loopLine = top->lineIndex;
  whileStackTop--;

  if (!ScanForLoopEnd(type, loopLine)) {
    if (type == LOOP_WHILE) {
      PrintError(ERR_WHILE_WITHOUT_WEND);
    } else {
      PrintError(ERR_DO_WITHOUT_UNTIL);
    }
    return false;
  }
  return true;
}

// ON ... GOTO/GOSUB: picks a target from the list by index.
bool CmdOn(const char* args) {
  args = SkipWhitespace(args);

  const char* p = args;
  const char* kwPos = NULL;
  bool isGosub = false;
  while (*p) {
    if (strncasecmp(p, "GOSUB", 5) == 0 && !isalnum((unsigned char)p[5]) &&
        p[5] != '_') {
      kwPos = p;
      isGosub = true;
      break;
    }
    if (strncasecmp(p, "GOTO", 4) == 0 && !isalnum((unsigned char)p[4]) &&
        p[4] != '_') {
      kwPos = p;
      isGosub = false;
      break;
    }
    p++;
  }
  if (!kwPos) {
    PrintError(ERR_SYNTAX);
    return false;
  }

  int exprLen = kwPos - args;
  char exprBuf[64];
  if (exprLen <= 0 || exprLen >= (int)sizeof(exprBuf)) {
    PrintError(ERR_SYNTAX);
    return false;
  }
  strncpy(exprBuf, args, exprLen);
  exprBuf[exprLen] = '\0';
  while (exprLen > 0 && isspace((unsigned char)exprBuf[exprLen - 1])) {
    exprBuf[--exprLen] = '\0';
  }

  float xF;
  if (!EvalExpression(exprBuf, &xF)) {
    return false;
  }
  int x = (int)xF;
  if (x < 0) {
    PrintError(ERR_ILLEGAL_QUANTITY);
    return false;
  }

  const char* lineList = SkipWhitespace(kwPos + (isGosub ? 5 : 4));
  int count = 0;
  p = lineList;
  while (*p) {
    float lineNumF;
    const char* after = ParseExpression(p, &lineNumF);
    if (!after) {
      break;
    }
    if (count == x) {
      if (isGosub) {
        if (!GosubStackEnsureCapacity(gosubStackTop + 1)) {
          PrintError(ERR_OUT_OF_MEMORY);
          return false;
        }
        gosubStack[gosubStackTop].lineIndex = currentExecLine;
        gosubStack[gosubStackTop].stmtSkip = (uint8_t)outerStmtSkipForGosub;
        gosubStack[gosubStackTop].thenClauseSkip =
            (uint8_t)thenClauseSkipForGosub;
        gosubStackTop++;
      }
      gotoLineNum = (int)lineNumF;
      return true;
    }
    count++;
    after = SkipWhitespace(after);
    if (*after != ',') {
      break;
    }
    p = after + 1;
  }
  PrintError(ERR_ILLEGAL_QUANTITY);
  return false;
}

// Substitutes a DEF FN argument into the function body by textual replacement.
// Only whole-word matches are replaced, so a parameter X does not corrupt a
// variable named X1 or X%, and the value is parenthesised to preserve
// precedence.
bool SubstituteParam(const char* expr, const char* param, float value,
                     char* out, int maxLen) {
  char numStr[32];
  if (value == (float)(int)value && value >= -32768.0f && value <= 32767.0f) {
    snprintf(numStr, sizeof(numStr), "(%d)", (int)value);
  } else {
    snprintf(numStr, sizeof(numStr), "(%g)", value);
  }

  int paramLen = (int)strlen(param);
  int numLen = (int)strlen(numStr);
  const char* p = expr;
  int outPos = 0;

  while (*p && outPos < maxLen - 1) {
    if (strncasecmp(p, param, paramLen) == 0) {
      bool prevOk = (p == expr) || !isalnum((unsigned char)p[-1]);
      char next = p[paramLen];
      bool nextOk = !isalnum((unsigned char)next) && next != '_' &&
                    !IsTypeSuffixAt(p + paramLen);
      if (prevOk && nextOk) {
        if (outPos + numLen >= maxLen - 1) {
          PrintError(ERR_FORMULA_TOO_COMPLEX);
          return false;
        }
        memcpy(out + outPos, numStr, numLen);
        outPos += numLen;
        p += paramLen;
        continue;
      }
    }
    out[outPos++] = *p++;
  }
  if (*p) {
    PrintError(ERR_FORMULA_TOO_COMPLEX);
    return false;
  }
  out[outPos] = '\0';
  return true;
}

// DEF FN: records a single-expression user function and its parameter name.
bool CmdDef(const char* args) {
  args = SkipWhitespace(args);
  if (strncasecmp(args, "FN", 2) != 0 || isalnum((unsigned char)args[2])) {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args = SkipWhitespace(args + 2);

  char fnName[MAX_VAR_NAME];
  int i = 0;
  while (isalnum((unsigned char)*args) && i < MAX_VAR_NAME - 1) {
    fnName[i++] = toupper(*args++);
  }
  fnName[i] = '\0';
  if (i == 0) {
    PrintError(ERR_SYNTAX);
    return false;
  }

  args = SkipWhitespace(args);
  if (*args != '(') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;

  char paramName[MAX_VAR_NAME];
  i = 0;
  while (isalnum((unsigned char)*args) && i < MAX_VAR_NAME - 1) {
    paramName[i++] = toupper(*args++);
  }
  if (IsTypeSuffixAt(args) && i < MAX_VAR_NAME - 1) {
    paramName[i++] = toupper(*args++);
  }
  paramName[i] = '\0';
  if (i == 0) {
    PrintError(ERR_SYNTAX);
    return false;
  }

  args = SkipWhitespace(args);
  if (*args != ')') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args++;
  args = SkipWhitespace(args);
  if (*args != '=') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  args = SkipWhitespace(args + 1);

  UserFunction* slot = NULL;
  for (int j = 0; j < userFuncCount; j++) {
    if (strcasecmp(userFunctions[j].name, fnName) == 0) {
      slot = &userFunctions[j];
      break;
    }
  }
  if (!slot) {
    if (userFuncCount >= MAX_USER_FUNCS) {
      PrintError(ERR_OUT_OF_MEMORY);
      return false;
    }
    slot = &userFunctions[userFuncCount++];
  }

  strncpy(slot->name, fnName, MAX_VAR_NAME - 1);
  slot->name[MAX_VAR_NAME - 1] = '\0';
  strncpy(slot->paramName, paramName, MAX_VAR_NAME - 1);
  slot->paramName[MAX_VAR_NAME - 1] = '\0';
  strncpy(slot->expr, args, MAX_FN_EXPR_LEN - 1);
  slot->expr[MAX_FN_EXPR_LEN - 1] = '\0';
  return true;
}

// TIMER: arms a periodic GOSUB, or disarms it. The call fires between
// statements rather than truly asynchronously, so it never interrupts a
// half-evaluated expression.
bool CmdTimer(const char* args) {
  args = SkipWhitespace(args);
  if (strncasecmp(args, "OFF", 3) == 0 && !isalnum((unsigned char)args[3]) &&
      args[3] != '_') {
    timerEnabled = false;
    return true;
  }
  if (!programRunning) {
    PrintError(ERR_ILLEGAL_DIRECT);
    return false;
  }

  float intervalF;
  const char* after = ParseExpression(args, &intervalF);
  if (!after) {
    return false;
  }
  after = SkipWhitespace(after);
  if (*after != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  after++;
  float lineNumF;
  after = ParseExpression(after, &lineNumF);
  if (!after) {
    return false;
  }

  timerIntervalMs = (uint32_t)intervalF;
  timerTargetLine = (int)lineNumF;
  timerLastFireMs = (uint32_t)millis();
  timerEnabled = true;
  return true;
}

// CHUNK: splits a string on a delimiter into successive elements of a
// pre-dimensioned string array.
bool CmdChunk(const char* args) {
  args = SkipWhitespace(args);

  char arrName[MAX_VAR_NAME];
  const char* p = ParseVarName(args, arrName, sizeof(arrName));
  if (!p) {
    return false;
  }

  size_t nameLen = strlen(arrName);
  if (nameLen == 0 || arrName[nameLen - 1] != '$') {
    PrintError(ERR_TYPE_MISMATCH);
    return false;
  }

  p = SkipWhitespace(p);
  if (*p != ',') {
    PrintError(ERR_SYNTAX);
    return false;
  }
  p++;

  char srcStr[MAX_STR_EXPR_BUF];
  p = ParseStringExpression(p, srcStr, sizeof(srcStr));
  if (!p) {
    return false;
  }

  char delim[MAX_STR_EXPR_BUF];
  delim[0] = ' ';
  delim[1] = '\0';

  p = SkipWhitespace(p);
  if (*p == ',') {
    p++;
    p = ParseStringExpression(p, delim, sizeof(delim));
    if (!p) {
      return false;
    }
  }
  if (delim[0] == '\0') {
    PrintError(ERR_ILLEGAL_QUANTITY);
    return false;
  }

  ArrayDescriptor* arr = FindArray(arrName);
  if (!arr) {
    PrintError(ERR_ARRAY_NOT_DIMD);
    return false;
  }
  if (arr->type != ARRAY_TYPE_STRING) {
    PrintError(ERR_TYPE_MISMATCH);
    return false;
  }
  if (arr->dim2Size != 0) {
    PrintError(ERR_WRONG_DIMENSIONS);
    return false;
  }

  int delimLen = (int)strlen(delim);
  int slot = 0;
  const char* src = srcStr;

  while (*src && slot < arr->dim1Size) {
    const char* found = strstr(src, delim);
    int chunkLen;
    if (found) {
      chunkLen = (int)(found - src);
    } else {
      chunkLen = (int)strlen(src);
    }

    char* ptr = (char*)GetArrayElementPtr(arr, slot);
    if (chunkLen > STRING_ELEMENT_LEN - 1) {
      chunkLen = STRING_ELEMENT_LEN - 1;
    }
    strncpy(ptr, src, chunkLen);
    ptr[chunkLen] = '\0';
    slot++;

    if (found) {
      src = found + delimLen;
    } else {
      break;
    }
  }

  return true;
}

// VERSION: prints the firmware build date and time.
bool CmdVersion(const char* args) {
  char buf[80];
  snprintf(buf, sizeof(buf), "%s %s [%s]", __DATE__, __TIME__, GIT_BRANCH);
  Newline();
  PrintStr(buf);
  return true;
}

// TRAP: arms an error handler at a line number, or disarms it so errors stop
// the program normally.
bool CmdTrap(const char* args) {
  args = SkipWhitespace(args);
  if (strncasecmp(args, "OFF", 3) == 0 && !isalnum((unsigned char)args[3])) {
    trapLineNum = -1;
    return true;
  }
  float lineF;
  const char* p = ParseExpression(args, &lineF);
  if (!p) {
    PrintError(ERR_SYNTAX);
    return false;
  }
  int lineNum = (int)lineF;
  if (lineNum < 1 || lineNum > 65535) {
    PrintError(ERR_ILLEGAL_LINE_NUMBER);
    return false;
  }
  trapLineNum = lineNum;
  return true;
}

// Reports RESUME used with no trapped error outstanding.
static void ResumeWithoutTrapError() {
  int save = trapLineNum;
  trapLineNum = -1;
  PrintError(ERR_RESUME_WITHOUT_TRAP);
  trapLineNum = save;
}

// RESUME: continues after a trapped error -- retrying the failed line, skipping
// to the next, or jumping to a given line -- and re-arms the trap.
bool CmdResume(const char* args) {
  args = SkipWhitespace(args);

  if (strncasecmp(args, "NEXT", 4) == 0 && !isalnum((unsigned char)args[4])) {
    if (!trapActive) {
      ResumeWithoutTrapError();
      return false;
    }
    int nextIdx = trapErrorLineIndex + 1;
    trapActive = false;
    if (nextIdx >= programLineCount) {
      programRunning = false;
      return true;
    }
    gotoLineNum = program[nextIdx].lineNum;
    return true;
  }

  if (*args == '\0') {
    if (!trapActive) {
      ResumeWithoutTrapError();
      return false;
    }
    trapActive = false;
    gotoLineNum = program[trapErrorLineIndex].lineNum;
    return true;
  }

  if (!trapActive) {
    ResumeWithoutTrapError();
    return false;
  }
  float lineF;
  const char* p = ParseExpression(args, &lineF);
  if (!p) {
    PrintError(ERR_SYNTAX);
    return false;
  }
  int lineNum = (int)lineF;
  int idx = FindProgramLine((uint16_t)lineNum);
  if (idx < 0) {
    PrintError(ERR_UNDEF_STATEMENT);
    return false;
  }
  trapActive = false;
  gotoLineNum = lineNum;
  return true;
}

// DEG: switches the trig functions to degrees.
bool CmdDeg(const char* args) {
  (void)args;
  trigDegMode = true;
  Newline();
  return true;
}

// RAD: switches the trig functions back to radians.
bool CmdRad(const char* args) {
  (void)args;
  trigDegMode = false;
  Newline();
  return true;
}
