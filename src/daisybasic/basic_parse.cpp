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

// Advances past spaces and tabs.
const char* SkipWhitespace(const char* p) {
  while (*p && isspace(*p)) {
    p++;
  }
  return p;
}

// Matches a keyword case-insensitively, returning the rest of the line or NULL.
// The keyword must be followed by a separator so that, say, PRINTX is treated
// as a variable rather than as PRINT.
const char* MatchCommand(const char* line, const char* cmd) {
  while (*cmd) {
    if (toupper(*line) != toupper(*cmd)) {
      return NULL;
    }
    line++;
    cmd++;
  }
  if (*line && !isspace(*line) && *line != '"' && *line != '(') {
    return NULL;
  }
  return line;
}

// Reads a double-quoted string literal, truncating at the caller's buffer size.
const char* ParseString(const char* p, char* out, size_t max_len) {
  p = SkipWhitespace(p);
  if (*p != '"') {
    return NULL;
  }
  p++;

  size_t i = 0;
  while (*p && *p != '"' && i < max_len - 1) {
    out[i++] = *p++;
  }
  out[i] = '\0';

  if (*p == '"') {
    p++;
  }
  return p;
}

// True when the character at p closes a name as a type suffix: $ for string,
// % for integer. Modulo is spelled MOD, so % is never an operator and needs no
// disambiguation. Every name scanner shares this one rule, so a program means
// the same thing wherever it is read from.
bool IsTypeSuffixAt(const char* p) { return *p == '$' || *p == '%'; }

// Reads a variable name, folding to upper case so names are case-insensitive.
// A trailing $ or % is part of the name, since it selects the type.
const char* ParseVarName(const char* p, char* name, size_t maxLen) {
  p = SkipWhitespace(p);
  if (!isalpha(*p)) {
    return NULL;
  }

  size_t i = 0;
  while (isalnum(*p) && i < maxLen - 1) {
    name[i++] = toupper(*p++);
  }
  if (IsTypeSuffixAt(p) && i < maxLen - 1) {
    name[i++] = toupper(*p++);
  }
  name[i] = '\0';
  return p;
}

// True for a $-suffixed name that is not one of the built-in string functions,
// which would otherwise look like string variables.
bool IsStringVar(const char* name) {
  size_t len = strlen(name);
  if (len == 0 || name[len - 1] != '$') {
    return false;
  }
  return !IsBuiltinStringFunction(name);
}

// Evaluates one numeric expression and returns where it stopped. Scans to the
// first comma or semicolon outside parentheses, so argument lists split
// correctly while nested calls stay intact.
const char* ParseExpression(const char* p, float* result) {
  p = SkipWhitespace(p);

  const char* start = p;
  int parenDepth = 0;
  while (*p) {
    if (*p == '(') {
      parenDepth++;
    } else if (*p == ')') {
      if (parenDepth == 0) {
        break;
      }
      parenDepth--;
    } else if ((*p == ',' || *p == ';') && parenDepth == 0) {
      break;
    }
    p++;
  }

  int len = p - start;
  if (len == 0) {
    return NULL;
  }

  char exprBuf[128];
  if (len >= (int)sizeof(exprBuf)) {
    len = sizeof(exprBuf) - 1;
  }
  strncpy(exprBuf, start, len);
  exprBuf[len] = '\0';

  while (len > 0 && isspace(exprBuf[len - 1])) {
    exprBuf[--len] = '\0';
  }

  if (!EvalExpression(exprBuf, result)) {
    return NULL;
  }
  return p;
}

// Formats a number for display, printing whole values without a decimal point
// the way a user expects.
void FormatNumber(float val, char* buf, size_t bufLen) {
  if (val == (int)val) {
    snprintf(buf, bufLen, "%d", (int)val);
  } else {
    snprintf(buf, bufLen, "%.6g", val);
  }
}
