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
#include "comm_messages.h"

#define MAX_EXPR_STACK 32

typedef enum {
  TOK_NONE = 0,
  TOK_NUMBER,
  TOK_VARIABLE,
  TOK_OPERATOR,
  TOK_LPAREN,
  TOK_RPAREN,
  TOK_COMMA
} TokenType;

typedef enum {
  FUNC_NONE = 0,
  FUNC_RND,
  FUNC_ABS,
  FUNC_INT,
  FUNC_LEN,
  FUNC_ASC,
  FUNC_VAL,
  FUNC_SIN,
  FUNC_COS,
  FUNC_TAN,
  FUNC_SQR,
  FUNC_LOG,
  FUNC_LN,
  FUNC_FRE,
  FUNC_PRESSED,
  FUNC_CHARAT,
  FUNC_MILLIS,
  FUNC_GETCHAR,
  FUNC_KEYDOWN,
  FUNC_INSTR,
  FUNC_FN,
  FUNC_TIME,
  FUNC_DATE,
  FUNC_POINT,
  FUNC_CURX,
  FUNC_CURY,
  FUNC_DEC,
  FUNC_ATTRIBAT,
  FUNC_SIZEARR,
  FUNC_RESULT,
  FUNC_NETCONNECTED,
  FUNC_FBYTES,
  FUNC_CHECKBLOCK,
  FUNC_JOY
} FuncId;

#define OP_AND 0x80
#define OP_OR 0x81
#define OP_XOR 0x82
#define OP_NOT 0x83
#define OP_SHL 0x84
#define OP_SHR 0x85
#define OP_MOD 0x86

typedef struct {
  TokenType type;
  union {
    float numVal;
    uint8_t op;
    char varName[MAX_VAR_NAME];
  };
} Token;

static bool EvalFunction(FuncId funcId, float* args, int argCount,
                         float* result);

// Binding strength of an operator, highest number binds tightest. Drives the
// shunting-yard conversion below.
static int GetPrecedence(uint8_t op) {
  switch (op) {
    case OP_OR:
      return 1;
    case OP_XOR:
      return 2;
    case OP_AND:
      return 3;
    case OP_SHL:
    case OP_SHR:
      return 4;
    case '+':
    case '-':
      return 5;
    case '*':
    case '/':
    case OP_MOD:
      return 6;
    case '^':
      return 7;
    case OP_NOT:
      return 8;
    default:
      return 0;
  }
}

// True for the operators that group right to left, so 2^3^2 is 2^(3^2).
static bool IsRightAssociative(uint8_t op) { return op == '^' || op == OP_NOT; }

// True for the single-character arithmetic operators. Modulo is not here: it
// is spelled MOD, which leaves % free to mean the integer type suffix.
static bool IsOperator(char c) {
  return c == '+' || c == '-' || c == '*' || c == '/' || c == '^';
}

// Recognises AND, OR, XOR, NOT and MOD, which are spelled as words but behave
// as operators rather than functions. MOD has no keyword token of its own --
// the keyword byte range is full -- so the tokenizer stores it as a name and
// this is the only place it is given meaning. That also makes MOD a reserved
// word: it cannot be used as a variable.
static bool IsWordOperator(const char* name, uint8_t* opOut) {
  if (strcasecmp(name, "AND") == 0) {
    *opOut = OP_AND;
    return true;
  }
  if (strcasecmp(name, "OR") == 0) {
    *opOut = OP_OR;
    return true;
  }
  if (strcasecmp(name, "XOR") == 0) {
    *opOut = OP_XOR;
    return true;
  }
  if (strcasecmp(name, "NOT") == 0) {
    *opOut = OP_NOT;
    return true;
  }
  if (strcasecmp(name, "MOD") == 0) {
    *opOut = OP_MOD;
    return true;
  }
  return false;
}

// Maps a name to a built-in numeric function id, or FUNC_NONE.
static FuncId CheckFunction(const char* name) {
  if (strcasecmp(name, "RND") == 0) {
    return FUNC_RND;
  }
  if (strcasecmp(name, "ABS") == 0) {
    return FUNC_ABS;
  }
  if (strcasecmp(name, "INT") == 0) {
    return FUNC_INT;
  }
  if (strcasecmp(name, "LEN") == 0) {
    return FUNC_LEN;
  }
  if (strcasecmp(name, "ASC") == 0) {
    return FUNC_ASC;
  }
  if (strcasecmp(name, "VAL") == 0) {
    return FUNC_VAL;
  }
  if (strcasecmp(name, "SIN") == 0) {
    return FUNC_SIN;
  }
  if (strcasecmp(name, "COS") == 0) {
    return FUNC_COS;
  }
  if (strcasecmp(name, "TAN") == 0) {
    return FUNC_TAN;
  }
  if (strcasecmp(name, "SQR") == 0) {
    return FUNC_SQR;
  }
  if (strcasecmp(name, "LOG") == 0) {
    return FUNC_LOG;
  }
  if (strcasecmp(name, "LN") == 0) {
    return FUNC_LN;
  }
  if (strcasecmp(name, "FRE") == 0) {
    return FUNC_FRE;
  }
  if (strcasecmp(name, "PRESSED") == 0) {
    return FUNC_PRESSED;
  }
  if (strcasecmp(name, "INSTR") == 0) {
    return FUNC_INSTR;
  }
  if (strcasecmp(name, "FN") == 0) {
    return FUNC_FN;
  }
  if (strcasecmp(name, "CHARAT") == 0) {
    return FUNC_CHARAT;
  }
  if (strcasecmp(name, "MILLIS") == 0) {
    return FUNC_MILLIS;
  }
  if (strcasecmp(name, "GETCHAR") == 0) {
    return FUNC_GETCHAR;
  }
  if (strcasecmp(name, "KEYDOWN") == 0) {
    return FUNC_KEYDOWN;
  }
  if (strcasecmp(name, "TIME") == 0) {
    return FUNC_TIME;
  }
  if (strcasecmp(name, "DATE") == 0) {
    return FUNC_DATE;
  }
  if (strcasecmp(name, "POINT") == 0) {
    return FUNC_POINT;
  }
  if (strcasecmp(name, "CURX") == 0) {
    return FUNC_CURX;
  }
  if (strcasecmp(name, "CURY") == 0) {
    return FUNC_CURY;
  }
  if (strcasecmp(name, "DEC") == 0) {
    return FUNC_DEC;
  }
  if (strcasecmp(name, "ATTRIBAT") == 0) {
    return FUNC_ATTRIBAT;
  }
  if (strcasecmp(name, "SIZEARR") == 0) {
    return FUNC_SIZEARR;
  }
  if (strcasecmp(name, "RESULT") == 0) {
    return FUNC_RESULT;
  }
  if (strcasecmp(name, "NETCONNECTED") == 0) {
    return FUNC_NETCONNECTED;
  }
  if (strcasecmp(name, "FBYTES") == 0) {
    return FUNC_FBYTES;
  }
  if (strcasecmp(name, "CHECKBLOCK") == 0) {
    return FUNC_CHECKBLOCK;
  }
  if (strcasecmp(name, "JOY") == 0) {
    return FUNC_JOY;
  }
  return FUNC_NONE;
}

// Parses LEN's string argument and yields its length.
static const char* ParseLenArg(const char* p, float* result) {
  p = SkipWhitespace(p);
  if (*p != '(') {
    return NULL;
  }
  p++;

  char strVal[MAX_STR_EXPR_BUF];
  p = ParseStringExpression(p, strVal, sizeof(strVal));
  if (!p) {
    return NULL;
  }

  p = SkipWhitespace(p);
  if (*p != ')') {
    return NULL;
  }
  p++;

  *result = (float)strlen(strVal);
  return p;
}

// Parses ASC's argument, yielding the first character's code, or 0 if empty.
static const char* ParseAscArg(const char* p, float* result) {
  p = SkipWhitespace(p);
  if (*p != '(') {
    return NULL;
  }
  p++;

  char strVal[MAX_STR_EXPR_BUF];
  p = ParseStringExpression(p, strVal, sizeof(strVal));
  if (!p) {
    return NULL;
  }

  p = SkipWhitespace(p);
  if (*p != ')') {
    return NULL;
  }
  p++;

  *result = (strVal[0] != '\0') ? (float)(unsigned char)strVal[0] : 0.0f;
  return p;
}

// Parses INSTR's two or three arguments and yields the 1-based match position,
// or 0 when the needle is absent.
static const char* ParseInstrArg(const char* p, float* result) {
  p = SkipWhitespace(p);
  if (*p != '(') {
    return NULL;
  }
  p++;

  char str1[MAX_STR_EXPR_BUF];
  p = ParseStringExpression(p, str1, sizeof(str1));
  if (!p) {
    return NULL;
  }

  p = SkipWhitespace(p);
  if (*p != ',') {
    return NULL;
  }
  p++;

  char str2[MAX_STR_EXPR_BUF];
  p = ParseStringExpression(p, str2, sizeof(str2));
  if (!p) {
    return NULL;
  }

  p = SkipWhitespace(p);

  int startPos = 1;
  if (*p == ',') {
    p++;
    float startF;
    const char* afterStart = ParseExpression(p, &startF);
    if (!afterStart) {
      return NULL;
    }
    p = SkipWhitespace(afterStart);
    startPos = (int)startF;
    if (startPos < 1) {
      startPos = 1;
    }
  }

  if (*p != ')') {
    return NULL;
  }
  p++;

  int len1 = (int)strlen(str1);
  int len2 = (int)strlen(str2);

  if (len2 == 0) {
    *result = (float)startPos;
    return p;
  }

  for (int i = startPos - 1; i <= len1 - len2; i++) {
    if (strncmp(str1 + i, str2, (size_t)len2) == 0) {
      *result = (float)(i + 1);
      return p;
    }
  }

  *result = 0.0f;
  return p;
}

// Parses VAL's argument and converts it to a number, flagging a type mismatch
// when the text is not numeric.
static const char* ParseValArg(const char* p, float* result, bool* error) {
  *error = false;
  p = SkipWhitespace(p);
  if (*p != '(') {
    return NULL;
  }
  p++;

  char strVal[MAX_STR_EXPR_BUF];
  p = ParseStringExpression(p, strVal, sizeof(strVal));
  if (!p) {
    return NULL;
  }

  p = SkipWhitespace(p);
  if (*p != ')') {
    return NULL;
  }
  p++;

  const char* numStr = strVal;
  while (*numStr && isspace(*numStr)) {
    numStr++;
  }

  if (*numStr == '\0') {
    *error = true;
    return NULL;
  }

  char* endPtr;
  float val = strtof(numStr, &endPtr);

  while (*endPtr && isspace(*endPtr)) {
    endPtr++;
  }

  if (*endPtr != '\0') {
    *error = true;
    return NULL;
  }

  *result = val;
  return p;
}

// Parses DEC's argument, converting a $-prefixed hex or b-prefixed binary
// string to an integer.
static const char* ParseDecArg(const char* p, float* result, bool* error) {
  *error = false;
  p = SkipWhitespace(p);
  if (*p != '(') {
    return NULL;
  }
  p++;

  char strVal[MAX_STR_EXPR_BUF];
  p = ParseStringExpression(p, strVal, sizeof(strVal));
  if (!p) {
    return NULL;
  }

  p = SkipWhitespace(p);
  if (*p != ')') {
    return NULL;
  }
  p++;

  const char* s = strVal;
  while (*s && isspace(*s)) {
    s++;
  }

  if (*s == '\0') {
    *error = true;
    return NULL;
  }

  if (*s == '$') {
    s++;
    if (*s == '\0') {
      *error = true;
      return NULL;
    }
    char* endPtr;
    unsigned long val = strtoul(s, &endPtr, 16);
    while (*endPtr && isspace(*endPtr)) {
      endPtr++;
    }
    if (*endPtr != '\0') {
      *error = true;
      return NULL;
    }
    *result = (float)val;
    return p;
  } else if (*s == 'b' || *s == 'B') {
    s++;
    if (*s == '\0') {
      *error = true;
      return NULL;
    }
    unsigned long val = 0;
    while (*s == '0' || *s == '1') {
      val = (val << 1) | (*s - '0');
      s++;
    }
    while (*s && isspace(*s)) {
      s++;
    }
    if (*s != '\0') {
      *error = true;
      return NULL;
    }
    *result = (float)val;
    return p;
  } else {
    *error = true;
    return NULL;
  }
}

// Reads a parenthesised, comma-separated numeric argument list.
static const char* ParseFunctionArgs(const char* p, float* args, int* argCount,
                                     int maxArgs) {
  *argCount = 0;
  p = SkipWhitespace(p);
  if (*p != '(') {
    return NULL;
  }
  p++;

  while (*argCount < maxArgs) {
    p = SkipWhitespace(p);
    if (*p == ')') {
      p++;
      return p;
    }

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
      } else if (*p == ',' && parenDepth == 0) {
        break;
      }
      p++;
    }

    int len = p - start;
    char argBuf[64];
    if (len >= (int)sizeof(argBuf)) {
      len = sizeof(argBuf) - 1;
    }
    strncpy(argBuf, start, len);
    argBuf[len] = '\0';

    float val;
    if (!EvalExpression(argBuf, &val)) {
      return NULL;
    }
    args[(*argCount)++] = val;

    p = SkipWhitespace(p);
    if (*p == ',') {
      p++;
    } else if (*p == ')') {
      p++;
      return p;
    } else {
      return NULL;
    }
  }

  return NULL;
}

// Splits an expression into numbers, variables, operators and function calls.
// Also resolves unary minus, which is otherwise indistinguishable from
// subtraction, by looking at what preceded it.
static int Tokenize(const char* expr, Token* tokens, int maxTokens) {
  int count = 0;
  const char* p = expr;

  while (*p && count < maxTokens) {
    while (*p && isspace(*p)) {
      p++;
    }
    if (!*p) {
      break;
    }

    if (*p == '+' && (count == 0 || tokens[count - 1].type == TOK_OPERATOR ||
                      tokens[count - 1].type == TOK_LPAREN)) {
      p++;
      while (*p && isspace(*p)) {
        p++;
      }
      continue;
    }

    if (*p == '-' && (count == 0 || tokens[count - 1].type == TOK_OPERATOR ||
                      tokens[count - 1].type == TOK_LPAREN)) {
      p++;
      while (*p && isspace(*p)) {
        p++;
      }

      if (isdigit(*p) || (*p == '.' && isdigit(*(p + 1)))) {
        tokens[count].type = TOK_NUMBER;
        char* end;
        tokens[count].numVal = -strtof(p, &end);
        p = end;
        count++;
        continue;
      } else if (isalpha(*p) || *p == '(') {
        tokens[count].type = TOK_NUMBER;
        tokens[count].numVal = 0;
        count++;
        if (count >= maxTokens) {
          return -1;
        }
        tokens[count].type = TOK_OPERATOR;
        tokens[count].op = '-';
        count++;
        continue;
      } else {
        return -1;
      }
    }

    if (isdigit(*p) || (*p == '.' && isdigit(*(p + 1)))) {
      tokens[count].type = TOK_NUMBER;
      char* end;
      tokens[count].numVal = strtof(p, &end);
      p = end;
      count++;
    } else if (isalpha(*p)) {
      char name[24];
      int i = 0;
      while (i < 23 && isalnum(*p)) {
        name[i++] = toupper(*p++);
      }
      if (i < 23 && IsTypeSuffixAt(p)) {
        name[i++] = toupper(*p++);
      }
      name[i] = '\0';

      FuncId funcId = CheckFunction(name);
      if (funcId == FUNC_LEN) {
        float lenResult;
        p = ParseLenArg(p, &lenResult);
        if (!p) {
          return -1;
        }
        tokens[count].type = TOK_NUMBER;
        tokens[count].numVal = lenResult;
        count++;
      } else if (funcId == FUNC_ASC) {
        float ascResult;
        p = ParseAscArg(p, &ascResult);
        if (!p) {
          return -1;
        }
        tokens[count].type = TOK_NUMBER;
        tokens[count].numVal = ascResult;
        count++;
      } else if (funcId == FUNC_INSTR) {
        float instrResult;
        p = ParseInstrArg(p, &instrResult);
        if (!p) {
          return -1;
        }
        tokens[count].type = TOK_NUMBER;
        tokens[count].numVal = instrResult;
        count++;
      } else if (funcId == FUNC_FN) {
        p = SkipWhitespace(p);
        char fnName[MAX_VAR_NAME];
        int fi = 0;
        while (isalnum((unsigned char)*p) && fi < MAX_VAR_NAME - 1) {
          fnName[fi++] = toupper(*p++);
        }
        fnName[fi] = '\0';
        if (fi == 0) {
          PrintError(ERR_SYNTAX);
          return -1;
        }

        p = SkipWhitespace(p);
        if (*p != '(') {
          PrintError(ERR_SYNTAX);
          return -1;
        }
        p++;
        float argVal;
        const char* afterArg = ParseExpression(p, &argVal);
        if (!afterArg) {
          return -1;
        }
        p = SkipWhitespace(afterArg);
        if (*p != ')') {
          PrintError(ERR_SYNTAX);
          return -1;
        }
        p++;

        UserFunction* func = NULL;
        for (int fi2 = 0; fi2 < userFuncCount; fi2++) {
          if (strcasecmp(userFunctions[fi2].name, fnName) == 0) {
            func = &userFunctions[fi2];
            break;
          }
        }
        if (!func) {
          PrintError(ERR_UNDEF_STATEMENT);
          return -1;
        }

        char substExpr[MAX_FN_EXPR_LEN + 64];
        if (!SubstituteParam(func->expr, func->paramName, argVal, substExpr,
                             sizeof(substExpr))) {
          return -1;
        }
        float fnResult;
        if (!EvalExpression(substExpr, &fnResult)) {
          return -1;
        }

        tokens[count].type = TOK_NUMBER;
        tokens[count].numVal = fnResult;
        count++;
      } else if (funcId == FUNC_SIZEARR) {
        p = SkipWhitespace(p);
        if (*p != '(') {
          PrintError(ERR_SYNTAX);
          return -1;
        }
        p++;
        char saName[MAX_VAR_NAME];
        p = ParseVarName(p, saName, sizeof(saName));
        if (!p) {
          PrintError(ERR_SYNTAX);
          return -1;
        }
        p = SkipWhitespace(p);
        if (*p != ',') {
          PrintError(ERR_SYNTAX);
          return -1;
        }
        p++;
        float dimF;
        const char* afterDim = ParseExpression(p, &dimF);
        if (!afterDim) {
          return -1;
        }
        p = SkipWhitespace(afterDim);
        if (*p != ')') {
          PrintError(ERR_SYNTAX);
          return -1;
        }
        p++;
        {
          ArrayDescriptor* saArr = FindArray(saName);
          if (!saArr || !saArr->isDimmed) {
            PrintError(ERR_ARRAY_NOT_DIMD);
            return -1;
          }
          int dim = (int)dimF;
          float saResult;
          if (dim == 1) {
            saResult = (float)saArr->dim1Size;
          } else if (dim == 2) {
            if (saArr->dim2Size == 0) {
              PrintError(ERR_ARRAY_NOT_DIMD);
              return -1;
            }
            saResult = (float)saArr->dim2Size;
          } else {
            PrintError(ERR_ARRAY_NOT_DIMD);
            return -1;
          }
          tokens[count].type = TOK_NUMBER;
          tokens[count].numVal = saResult;
          count++;
        }
      } else if (funcId == FUNC_VAL) {
        float valResult;
        bool valError;
        p = ParseValArg(p, &valResult, &valError);
        if (!p) {
          if (valError) {
            PrintError(ERR_TYPE_MISMATCH);
          }
          return -1;
        }
        tokens[count].type = TOK_NUMBER;
        tokens[count].numVal = valResult;
        count++;
      } else if (funcId == FUNC_DEC) {
        float decResult;
        bool decError;
        p = ParseDecArg(p, &decResult, &decError);
        if (!p) {
          if (decError) {
            PrintError(ERR_SYNTAX);
          }
          return -1;
        }
        tokens[count].type = TOK_NUMBER;
        tokens[count].numVal = decResult;
        count++;
      } else if (funcId != FUNC_NONE) {
        float funcArgs[4];
        int funcArgCount;
        p = ParseFunctionArgs(p, funcArgs, &funcArgCount, 4);
        if (!p) {
          return -1;
        }

        float funcResult;
        if (!EvalFunction(funcId, funcArgs, funcArgCount, &funcResult)) {
          return -1;
        }
        tokens[count].type = TOK_NUMBER;
        tokens[count].numVal = funcResult;
        count++;
      } else {
        uint8_t wordOp;
        if (IsWordOperator(name, &wordOp)) {
          tokens[count].type = TOK_OPERATOR;
          tokens[count].op = wordOp;
          count++;
        } else if (IsBuiltinStringFunction(name)) {
          PrintError(ERR_TYPE_MISMATCH);
          return -1;
        } else {
          const char* peek = p;
          while (*peek && isspace(*peek)) {
            peek++;
          }
          if (*peek == '(') {
            peek++;
            float idx1F;
            const char* afterIndex = ParseExpression(peek, &idx1F);
            if (!afterIndex) {
              return -1;
            }
            afterIndex = SkipWhitespace(afterIndex);

            int idx1 = (int)idx1F;
            int idx2 = 0;
            bool has2ndIndex = false;

            if (*afterIndex == ',') {
              afterIndex++;
              float idx2F;
              const char* afterIdx2 = ParseExpression(afterIndex, &idx2F);
              if (!afterIdx2) {
                return -1;
              }
              afterIndex = SkipWhitespace(afterIdx2);
              idx2 = (int)idx2F;
              has2ndIndex = true;
            }

            if (*afterIndex != ')') {
              return -1;
            }
            afterIndex++;
            p = afterIndex;

            ArrayDescriptor* arr = FindArray(name);
            if (!arr) {
              PrintError(ERR_ARRAY_NOT_DIMD);
              return -1;
            }

            int linearIdx = CalculateArrayIndex(arr, idx1, idx2, has2ndIndex);
            if (linearIdx == -2) {
              PrintError(ERR_WRONG_DIMENSIONS);
              return -1;
            }
            if (linearIdx < 0) {
              PrintError(ERR_BAD_SUBSCRIPT);
              return -1;
            }

            float value;
            void* ptr = GetArrayElementPtr(arr, linearIdx);
            if (arr->type == ARRAY_TYPE_INT) {
              value = (float)(*(int16_t*)ptr);
            } else {
              value = *(float*)ptr;
            }

            tokens[count].type = TOK_NUMBER;
            tokens[count].numVal = value;
            count++;
          } else {
            tokens[count].type = TOK_VARIABLE;
            strncpy(tokens[count].varName, name, MAX_VAR_NAME - 1);
            tokens[count].varName[MAX_VAR_NAME - 1] = '\0';
            count++;
          }
        }
      }
    } else if (*p == '<' && *(p + 1) == '<') {
      tokens[count].type = TOK_OPERATOR;
      tokens[count].op = OP_SHL;
      p += 2;
      count++;
    } else if (*p == '>' && *(p + 1) == '>') {
      tokens[count].type = TOK_OPERATOR;
      tokens[count].op = OP_SHR;
      p += 2;
      count++;
    } else if (IsOperator(*p)) {
      tokens[count].type = TOK_OPERATOR;
      tokens[count].op = *p++;
      count++;
    } else if (*p == '(') {
      tokens[count].type = TOK_LPAREN;
      p++;
      count++;
    } else if (*p == ')') {
      tokens[count].type = TOK_RPAREN;
      p++;
      count++;
    } else if (*p == ',') {
      tokens[count].type = TOK_COMMA;
      p++;
      count++;
    } else {
      return -1;
    }
  }
  return count;
}

// Dijkstra's shunting-yard: reorders infix tokens into postfix using precedence
// and associativity, so evaluation needs only a stack and no recursion. Keeping
// the interpreter non-recursive matters on a machine with a small stack.
static int ShuntingYard(Token* infix, int infixCount, Token* rpn, int maxRpn) {
  Token opStack[MAX_EXPR_STACK];
  int opTop = 0;
  int rpnCount = 0;

  for (int i = 0; i < infixCount; i++) {
    Token* tok = &infix[i];

    if (tok->type == TOK_NUMBER || tok->type == TOK_VARIABLE) {
      if (rpnCount >= maxRpn) {
        return -1;
      }
      rpn[rpnCount++] = *tok;
    } else if (tok->type == TOK_OPERATOR) {
      while (opTop > 0) {
        Token* top = &opStack[opTop - 1];
        if (top->type != TOK_OPERATOR) {
          break;
        }

        int topPrec = GetPrecedence(top->op);
        int tokPrec = GetPrecedence(tok->op);

        if ((IsRightAssociative(tok->op) && tokPrec < topPrec) ||
            (!IsRightAssociative(tok->op) && tokPrec <= topPrec)) {
          if (rpnCount >= maxRpn) {
            return -1;
          }
          rpn[rpnCount++] = opStack[--opTop];
        } else {
          break;
        }
      }
      if (opTop >= MAX_EXPR_STACK) {
        return -1;
      }
      opStack[opTop++] = *tok;
    } else if (tok->type == TOK_LPAREN) {
      if (opTop >= MAX_EXPR_STACK) {
        return -1;
      }
      opStack[opTop++] = *tok;
    } else if (tok->type == TOK_RPAREN) {
      while (opTop > 0 && opStack[opTop - 1].type != TOK_LPAREN) {
        if (rpnCount >= maxRpn) {
          return -1;
        }
        rpn[rpnCount++] = opStack[--opTop];
      }
      if (opTop == 0) {
        return -1;
      }
      opTop--;
    }
  }

  while (opTop > 0) {
    if (opStack[opTop - 1].type == TOK_LPAREN) {
      return -1;
    }
    if (rpnCount >= maxRpn) {
      return -1;
    }
    rpn[rpnCount++] = opStack[--opTop];
  }

  return rpnCount;
}

// Evaluates one built-in numeric function against its parsed arguments.
static bool EvalFunction(FuncId funcId, float* args, int argCount,
                         float* result) {
  switch (funcId) {
    case FUNC_RND:
      if (argCount != 2) {
        return false;
      }
      {
        int minVal = (int)args[0];
        int maxVal = (int)args[1];
        if (minVal > maxVal) {
          int t = minVal;
          minVal = maxVal;
          maxVal = t;
        }
        *result = (float)(minVal + (random() % (maxVal - minVal + 1)));
      }
      return true;

    case FUNC_ABS:
      if (argCount != 1) {
        return false;
      }
      *result = fabsf(args[0]);
      return true;

    case FUNC_INT:
      if (argCount != 1) {
        return false;
      }
      *result = (float)(int)args[0];
      return true;

    case FUNC_SIN:
      if (argCount != 1) {
        return false;
      }
      *result = sinf(trigDegMode ? args[0] * 0.017453292519943295f : args[0]);
      return true;

    case FUNC_COS:
      if (argCount != 1) {
        return false;
      }
      *result = cosf(trigDegMode ? args[0] * 0.017453292519943295f : args[0]);
      return true;

    case FUNC_TAN:
      if (argCount != 1) {
        return false;
      }
      *result = tanf(trigDegMode ? args[0] * 0.017453292519943295f : args[0]);
      return true;

    case FUNC_SQR:
      if (argCount != 1) {
        return false;
      }
      *result = sqrtf(args[0]);
      return true;

    case FUNC_LOG:
      if (argCount != 1) {
        return false;
      }
      *result = log10f(args[0]);
      return true;

    case FUNC_LN:
      if (argCount != 1) {
        return false;
      }
      *result = logf(args[0]);
      return true;

    case FUNC_FRE:
      if (argCount != 1) {
        return false;
      }
      *result = (float)(DAISY_BASIC_HEAP - heapBytesUsed);
      return true;

    case FUNC_PRESSED:
      if (argCount != 2) {
        return false;
      }
      if (!programRunning) {
        return false;
      }
      *result =
          IsKeyColRowPressed((uint8_t)args[0], (uint8_t)args[1]) ? 1.0f : 0.0f;
      return true;

    case FUNC_CHARAT:
      if (argCount != 2) {
        return false;
      }
      *result = (float)GetCharAt((uint8_t)args[0], (uint8_t)args[1]);
      return true;

    case FUNC_ATTRIBAT:
      if (argCount != 2) {
        return false;
      }
      *result = (float)GetAttribAt((uint8_t)args[0], (uint8_t)args[1]);
      return true;

    case FUNC_CHECKBLOCK: {
      if (argCount != 4) {
        return false;
      }
      int sx = (int)args[0];
      int sy = (int)args[1];
      int w = (int)args[2];
      int h = (int)args[3];
      if (w <= 0 || h <= 0) {
        *result = 0.0f;
        return true;
      }
      if (sx < 0) {
        w += sx;
        sx = 0;
      }
      if (sy < 0) {
        h += sy;
        sy = 0;
      }
      if (sx >= VID_WIDTH || sy >= VID_HEIGHT || w <= 0 || h <= 0) {
        *result = 0.0f;
        return true;
      }
      int xEnd = sx + w;
      int yEnd = sy + h;
      if (xEnd > VID_WIDTH) {
        xEnd = VID_WIDTH;
      }
      if (yEnd > VID_HEIGHT) {
        yEnd = VID_HEIGHT;
      }
      *result = 0.0f;
      for (int yy = sy; yy < yEnd; yy++) {
        for (int xx = sx; xx < xEnd; xx++) {
          uint8_t c = GetCharAt((uint8_t)xx, (uint8_t)yy);
          if (c != 0 && c != 32) {
            *result = 1.0f;
            return true;
          }
        }
      }
      return true;
    }

    case FUNC_RESULT: {
      if (argCount != 1) {
        return false;
      }
      if (returnValCount == 0) {
        PrintError(ERR_RESULT_WITHOUT_RETURN);
        return false;
      }
      int n = (int)args[0];
      if (n < 1 || n > returnValCount) {
        PrintError(ERR_RESULT_WITHOUT_RETURN);
        return false;
      }
      *result = returnVals[n - 1];
      return true;
    }

    case FUNC_NETCONNECTED:
      *result = ServerConnected() ? 1.0f : 0.0f;
      return true;

    case FUNC_FBYTES: {
      if (argCount != 1) {
        return false;
      }
      int channel = (int)args[0];
      if (channel < 0 || channel > 3) {
        PrintError(ERR_ILLEGAL_QUANTITY);
        return false;
      }
      CommMsgSendFbytes((uint8_t)channel);
      uint8_t resp[4];
      if (WifiReadBytes(resp, 4, 5000) != 4) {
        *result = 0;
        return true;
      }
      uint32_t bytes = ((uint32_t)resp[0] << 24) | ((uint32_t)resp[1] << 16) |
                       ((uint32_t)resp[2] << 8) | (uint32_t)resp[3];
      *result = (float)bytes;
      return true;
    }

    case FUNC_MILLIS:
      if (argCount != 0) {
        return false;
      }
      *result = (float)millis();
      return true;

    case FUNC_GETCHAR:
      if (argCount != 2) {
        return false;
      }
      *result = (float)GetCharAt((uint8_t)args[0], (uint8_t)args[1]);
      return true;

    case FUNC_TIME:
      if (argCount != 1) {
        return false;
      }
      {
        int sel = (int)args[0];
        if (sel == 0) {
          *result = (float)RtcGetHours();
        } else if (sel == 1) {
          *result = (float)RtcGetMinutes();
        } else if (sel == 2) {
          *result = (float)RtcGetSeconds();
        } else {
          PrintError(ERR_ILLEGAL_QUANTITY);
          return false;
        }
      }
      return true;

    case FUNC_DATE:
      if (argCount != 1) {
        return false;
      }
      {
        int sel = (int)args[0];
        if (sel == 0) {
          *result = (float)RtcGetMonth();
        } else if (sel == 1) {
          *result = (float)RtcGetDay();
        } else if (sel == 2) {
          *result = (float)RtcGetYear();
        } else {
          PrintError(ERR_ILLEGAL_QUANTITY);
          return false;
        }
      }
      return true;

    case FUNC_POINT: {
      if (argCount != 2) {
        return false;
      }
      uint8_t px = (uint8_t)(int)args[0];
      uint8_t py = (uint8_t)(int)args[1];
      *result = (float)ReadPixelState(px, py);
      return true;
    }

    case FUNC_KEYDOWN: {
      if (argCount != 1) {
        return false;
      }
      uint8_t target = (uint8_t)(int)args[0];
      float found = 0.0f;
      for (uint8_t col = 0; col < kNumCols; col++) {
        for (uint8_t row = 0; row < kNumRows; row++) {
          if ((current_keymap[col][row] == target ||
               ac_keymap[col][row] == target) &&
              IsKeyColRowPressed(col, row)) {
            found = 1.0f;
          }
        }
      }
      BufferClear();
      *result = found;
      return true;
    }

    case FUNC_JOY: {
      if (argCount != 1) {
        return false;
      }
      int sel = (int)args[0];
      if (sel < 0 || sel > kNumJoyButtons) {
        PrintError(ERR_ILLEGAL_QUANTITY);
        return false;
      }
      *result = (float)JoyRead((uint8_t)sel);
      return true;
    }

    case FUNC_CURX:
      if (argCount != 0) {
        return false;
      }
      *result = (float)GetCursorX();
      return true;

    case FUNC_CURY:
      if (argCount != 0) {
        return false;
      }
      *result = (float)GetCursorY();
      return true;

    default:
      return false;
  }
}

// Evaluates the postfix form with a value stack, resolving variables as it
// goes. Undefined variables read as 0 rather than raising an error.
static bool EvalRPN(Token* rpn, int rpnCount, float* result) {
  float stack[MAX_EXPR_STACK];
  int top = 0;

  for (int i = 0; i < rpnCount; i++) {
    Token* tok = &rpn[i];

    if (tok->type == TOK_NUMBER) {
      if (top >= MAX_EXPR_STACK) {
        return false;
      }
      stack[top++] = tok->numVal;
    } else if (tok->type == TOK_VARIABLE) {
      Variable* v = FindVariable(tok->varName);
      if (!v) {
        if (tok->varName[strlen(tok->varName) - 1] == '$') {
          v = CreateVariable(tok->varName, VAR_STRING);
          if (!v) {
            return false;
          }
          if (!v->strVal) {
            SetStringVar(v, "");
          }
        } else {
          v = CreateVariable(tok->varName, VAR_INT);
          if (!v) {
            return false;
          }
          v->intVal = 0;
        }
      }
      if (top >= MAX_EXPR_STACK) {
        return false;
      }
      if (v->type == VAR_INT) {
        stack[top++] = (float)v->intVal;
      } else if (v->type == VAR_FLOAT) {
        stack[top++] = v->floatVal;
      } else {
        return false;
      }
    } else if (tok->type == TOK_OPERATOR) {
      if (tok->op == OP_NOT) {
        if (top < 1) {
          return false;
        }
        int32_t a = (int32_t)stack[--top];
        stack[top++] = (float)(~a);
        continue;
      }
      if (top < 2) {
        return false;
      }
      float b = stack[--top];
      float a = stack[--top];
      float r;
      switch (tok->op) {
        case '+':
          r = a + b;
          break;
        case '-':
          r = a - b;
          break;
        case '*':
          r = a * b;
          break;
        case '/':
          if (b == 0) {
            PrintError(ERR_DIVISION_BY_ZERO);
            return false;
          }
          r = a / b;
          break;
        case OP_MOD:
          if (b == 0) {
            PrintError(ERR_DIVISION_BY_ZERO);
            return false;
          }
          r = fmodf(a, b);
          break;
        case '^':
          r = powf(a, b);
          break;
        case OP_AND:
          r = (float)((int32_t)a & (int32_t)b);
          break;
        case OP_OR:
          r = (float)((int32_t)a | (int32_t)b);
          break;
        case OP_XOR:
          r = (float)((int32_t)a ^ (int32_t)b);
          break;
        case OP_SHL:
          r = (float)((int32_t)a << (int32_t)b);
          break;
        case OP_SHR:
          r = (float)((int32_t)a >> (int32_t)b);
          break;
        default:
          return false;
      }
      stack[top++] = r;
    }
  }

  if (top != 1) {
    return false;
  }
  *result = stack[0];
  return true;
}

// Evaluates a numeric expression: tokenise, convert to postfix, then evaluate.
bool EvalExpression(const char* expr, float* result) {
  Token tokens[MAX_EXPR_STACK];
  Token rpn[MAX_EXPR_STACK];

  int tokenCount = Tokenize(expr, tokens, MAX_EXPR_STACK);
  if (tokenCount < 0) {
    return false;
  }

  int rpnCount = ShuntingYard(tokens, tokenCount, rpn, MAX_EXPR_STACK);
  if (rpnCount < 0) {
    return false;
  }

  return EvalRPN(rpn, rpnCount, result);
}

// Parses one string term -- a literal, a string variable or array element, or a
// built-in string function call.
static const char* ParseStringValue(const char* p, char* outStr, size_t maxLen,
                                    size_t* outLen) {
  p = SkipWhitespace(p);

  if (*p == '"') {
    const char* ret = ParseString(p, outStr, maxLen);
    if (ret && outLen) {
      *outLen = strlen(outStr);
    }
    return ret;
  }

  if (isalpha(*p)) {
    char name[24];
    int i = 0;
    while (isalnum(*p) && i < 23) {
      name[i++] = toupper(*p++);
    }
    if (IsTypeSuffixAt(p) && i < 23) {
      name[i++] = toupper(*p++);
    }
    name[i] = '\0';

    if (strcasecmp(name, "MID$") == 0) {
      p = SkipWhitespace(p);
      if (*p != '(') {
        return NULL;
      }
      p++;

      char srcStr[MAX_STR_EXPR_BUF];
      p = ParseStringExpression(p, srcStr, sizeof(srcStr));
      if (!p) {
        return NULL;
      }

      p = SkipWhitespace(p);
      if (*p != ',') {
        return NULL;
      }
      p++;

      float startF;
      const char* afterStart = ParseExpression(p, &startF);
      if (!afterStart) {
        return NULL;
      }
      p = SkipWhitespace(afterStart);

      int start = (int)startF;
      int len = strlen(srcStr);

      if (*p == ',') {
        p++;
        float lenF;
        const char* afterLen = ParseExpression(p, &lenF);
        if (!afterLen) {
          return NULL;
        }
        p = afterLen;
        len = (int)lenF;
      }

      p = SkipWhitespace(p);
      if (*p != ')') {
        return NULL;
      }
      p++;

      int srcLen = strlen(srcStr);
      if (start < 1) {
        start = 1;
      }
      start--;

      if (start >= srcLen || len <= 0) {
        outStr[0] = '\0';
      } else {
        if (start + len > srcLen) {
          len = srcLen - start;
        }
        if (len > (int)maxLen - 1) {
          len = (int)maxLen - 1;
        }
        strncpy(outStr, srcStr + start, len);
        outStr[len] = '\0';
      }
      if (outLen) {
        *outLen = strlen(outStr);
      }
      return p;
    } else if (strcasecmp(name, "LEFT$") == 0) {
      p = SkipWhitespace(p);
      if (*p != '(') {
        return NULL;
      }
      p++;

      char srcStr[MAX_STR_EXPR_BUF];
      p = ParseStringExpression(p, srcStr, sizeof(srcStr));
      if (!p) {
        return NULL;
      }

      p = SkipWhitespace(p);
      if (*p != ',') {
        return NULL;
      }
      p++;

      float lenF;
      const char* afterLen = ParseExpression(p, &lenF);
      if (!afterLen) {
        return NULL;
      }
      p = SkipWhitespace(afterLen);
      if (*p != ')') {
        return NULL;
      }
      p++;

      int len = (int)lenF;
      int srcLen = strlen(srcStr);

      if (len <= 0) {
        outStr[0] = '\0';
      } else {
        if (len > srcLen) {
          len = srcLen;
        }
        if (len > (int)maxLen - 1) {
          len = (int)maxLen - 1;
        }
        strncpy(outStr, srcStr, len);
        outStr[len] = '\0';
      }
      if (outLen) {
        *outLen = strlen(outStr);
      }
      return p;
    } else if (strcasecmp(name, "RIGHT$") == 0) {
      p = SkipWhitespace(p);
      if (*p != '(') {
        return NULL;
      }
      p++;

      char srcStr[MAX_STR_EXPR_BUF];
      p = ParseStringExpression(p, srcStr, sizeof(srcStr));
      if (!p) {
        return NULL;
      }

      p = SkipWhitespace(p);
      if (*p != ',') {
        return NULL;
      }
      p++;

      float lenF;
      const char* afterLen = ParseExpression(p, &lenF);
      if (!afterLen) {
        return NULL;
      }
      p = SkipWhitespace(afterLen);
      if (*p != ')') {
        return NULL;
      }
      p++;

      int len = (int)lenF;
      int srcLen = strlen(srcStr);

      if (len <= 0) {
        outStr[0] = '\0';
      } else {
        if (len > srcLen) {
          len = srcLen;
        }
        if (len > (int)maxLen - 1) {
          len = (int)maxLen - 1;
        }
        int start = srcLen - len;
        strncpy(outStr, srcStr + start, len);
        outStr[len] = '\0';
      }
      if (outLen) {
        *outLen = strlen(outStr);
      }
      return p;
    } else if (strcasecmp(name, "CHR$") == 0) {
      p = SkipWhitespace(p);
      if (*p != '(') {
        return NULL;
      }
      p++;

      float numVal;
      const char* afterNum = ParseExpression(p, &numVal);
      if (!afterNum) {
        return NULL;
      }
      p = SkipWhitespace(afterNum);
      if (*p != ')') {
        return NULL;
      }
      p++;

      int charCode = (int)numVal;
      if (charCode < 0) {
        charCode = 0;
      }
      if (charCode > 255) {
        charCode = 255;
      }
      outStr[0] = (char)charCode;
      outStr[1] = '\0';
      if (outLen) {
        *outLen = 1;
      }
      return p;
    } else if (strcasecmp(name, "TOUPPER$") == 0) {
      p = SkipWhitespace(p);
      if (*p != '(') {
        return NULL;
      }
      p++;

      char srcStr[MAX_STR_EXPR_BUF];
      p = ParseStringExpression(p, srcStr, sizeof(srcStr));
      if (!p) {
        return NULL;
      }

      p = SkipWhitespace(p);
      if (*p != ')') {
        return NULL;
      }
      p++;

      int i = 0;
      for (; srcStr[i] && i < (int)maxLen - 1; i++) {
        outStr[i] = toupper(srcStr[i]);
      }
      outStr[i] = '\0';
      if (outLen) {
        *outLen = strlen(outStr);
      }
      return p;
    } else if (strcasecmp(name, "TOLOWER$") == 0) {
      p = SkipWhitespace(p);
      if (*p != '(') {
        return NULL;
      }
      p++;

      char srcStr[MAX_STR_EXPR_BUF];
      p = ParseStringExpression(p, srcStr, sizeof(srcStr));
      if (!p) {
        return NULL;
      }

      p = SkipWhitespace(p);
      if (*p != ')') {
        return NULL;
      }
      p++;

      int i = 0;
      for (; srcStr[i] && i < (int)maxLen - 1; i++) {
        outStr[i] = tolower(srcStr[i]);
      }
      outStr[i] = '\0';
      if (outLen) {
        *outLen = strlen(outStr);
      }
      return p;
    } else if (strcasecmp(name, "CHOMP$") == 0) {
      p = SkipWhitespace(p);
      if (*p != '(') {
        return NULL;
      }
      p++;

      char srcStr[MAX_STR_EXPR_BUF];
      p = ParseStringExpression(p, srcStr, sizeof(srcStr));
      if (!p) {
        return NULL;
      }

      p = SkipWhitespace(p);
      if (*p != ')') {
        return NULL;
      }
      p++;

      int end = (int)strlen(srcStr) - 1;
      while (end >= 0 && (srcStr[end] == ' ' || srcStr[end] == '\t' ||
                          srcStr[end] == '\n' || srcStr[end] == '\r')) {
        end--;
      }
      int start = 0;
      while (srcStr[start] == ' ' || srcStr[start] == '\t') {
        start++;
      }
      int len = end - start + 1;
      if (len <= 0) {
        outStr[0] = '\0';
      } else {
        if (len > (int)maxLen - 1) {
          len = (int)maxLen - 1;
        }
        strncpy(outStr, srcStr + start, len);
        outStr[len] = '\0';
      }
      if (outLen) {
        *outLen = strlen(outStr);
      }
      return p;
    } else if (strcasecmp(name, "STR$") == 0) {
      p = SkipWhitespace(p);
      if (*p != '(') {
        return NULL;
      }
      p++;

      float numVal;
      const char* afterNum = ParseExpression(p, &numVal);
      if (!afterNum) {
        PrintError(ERR_TYPE_MISMATCH);
        return NULL;
      }
      p = SkipWhitespace(afterNum);
      if (*p != ')') {
        return NULL;
      }
      p++;

      if (numVal == (int)numVal && numVal >= -2147483648.0f &&
          numVal <= 2147483647.0f) {
        snprintf(outStr, maxLen, "%d", (int)numVal);
      } else {
        snprintf(outStr, maxLen, "%g", numVal);
      }
      if (outLen) {
        *outLen = strlen(outStr);
      }
      return p;
    } else if (strcasecmp(name, "HEX$") == 0) {
      p = SkipWhitespace(p);
      if (*p != '(') {
        return NULL;
      }
      p++;

      float numVal;
      const char* afterNum = ParseExpression(p, &numVal);
      if (!afterNum) {
        PrintError(ERR_TYPE_MISMATCH);
        return NULL;
      }
      p = SkipWhitespace(afterNum);
      if (*p != ')') {
        return NULL;
      }
      p++;

      int32_t ival = (int32_t)numVal;
      if (ival < 0 || ival > 0xFFFF) {
        PrintError(ERR_ILLEGAL_QUANTITY);
        return NULL;
      }
      if (ival <= 0xFF) {
        snprintf(outStr, maxLen, "%02X", (unsigned)ival);
      } else {
        snprintf(outStr, maxLen, "%04X", (unsigned)ival);
      }
      if (outLen) {
        *outLen = strlen(outStr);
      }
      return p;
    } else if (strcasecmp(name, "BIN$") == 0) {
      p = SkipWhitespace(p);
      if (*p != '(') {
        return NULL;
      }
      p++;

      float numVal;
      const char* afterNum = ParseExpression(p, &numVal);
      if (!afterNum) {
        PrintError(ERR_TYPE_MISMATCH);
        return NULL;
      }
      p = SkipWhitespace(afterNum);
      if (*p != ')') {
        return NULL;
      }
      p++;

      int32_t ival = (int32_t)numVal;
      if (ival < 0 || ival > 0xFFFF) {
        PrintError(ERR_ILLEGAL_QUANTITY);
        return NULL;
      }
      char* dst = outStr;
      char* dstEnd = outStr + maxLen - 1;
      bool started = false;
      for (int bit = 15; bit >= 0 && dst < dstEnd; bit--) {
        if (ival & (1 << bit)) {
          *dst++ = '1';
          started = true;
        } else if (started) {
          *dst++ = '0';
        }
      }
      if (!started && dst < dstEnd) {
        *dst++ = '0';
      }
      *dst = '\0';
      if (outLen) {
        *outLen = strlen(outStr);
      }
      return p;
    } else if (strcasecmp(name, "DATE$") == 0) {
      p = SkipWhitespace(p);
      if (*p != '(') {
        return NULL;
      }
      p++;

      float selF;
      const char* afterSel = ParseExpression(p, &selF);
      if (!afterSel) {
        return NULL;
      }
      p = SkipWhitespace(afterSel);
      if (*p != ')') {
        return NULL;
      }
      p++;

      int sel = (int)selF;
      int mo = RtcGetMonth();
      int dy = RtcGetDay();
      int yr = RtcGetYear() % 100;
      if (sel == 0) {
        snprintf(outStr, maxLen, "%02d/%02d/%02d", mo, dy, yr);
      } else if (sel == 1) {
        snprintf(outStr, maxLen, "%02d/%02d/%02d", dy, mo, yr);
      } else {
        PrintError(ERR_ILLEGAL_QUANTITY);
        return NULL;
      }
      if (outLen) {
        *outLen = strlen(outStr);
      }
      return p;
    } else if (strcasecmp(name, "WIFI$") == 0) {
      p = SkipWhitespace(p);
      if (*p != '(') {
        return NULL;
      }
      p++;
      float nf;
      const char* afterArg = ParseExpression(p, &nf);
      if (!afterArg) {
        return NULL;
      }
      p = SkipWhitespace(afterArg);
      if (*p != ')') {
        return NULL;
      }
      p++;
      WifiQueryNet(outStr, (int)maxLen, (int)nf);
      if (outLen) {
        *outLen = strlen(outStr);
      }
      return p;
    } else if (strcasecmp(name, "ERR$") == 0) {
      p = SkipWhitespace(p);
      if (*p != '(') {
        return NULL;
      }
      p++;
      float nf;
      const char* afterArg = ParseExpression(p, &nf);
      if (!afterArg) {
        return NULL;
      }
      p = SkipWhitespace(afterArg);
      if (*p != ')') {
        return NULL;
      }
      p++;
      switch ((int)nf) {
        case 0:
          strncpy(outStr, GetErrorMessage(trapErrorCode), maxLen - 1);
          break;
        case 1:
          if (trapErrorLineIndex >= 0 &&
              trapErrorLineIndex < programLineCount) {
            snprintf(outStr, maxLen, "%d", program[trapErrorLineIndex].lineNum);
          } else {
            strncpy(outStr, "0", maxLen - 1);
          }
          break;
        case 2:
          snprintf(outStr, maxLen, "%d", (int)trapErrorCode);
          break;
        default:
          strncpy(outStr, "", maxLen - 1);
          break;
      }
      outStr[maxLen - 1] = '\0';
      if (outLen) {
        *outLen = strlen(outStr);
      }
      return p;
    } else if (strcasecmp(name, "TIME$") == 0) {
      p = SkipWhitespace(p);
      if (*p != '(') {
        return NULL;
      }
      p++;

      float selF;
      const char* afterSel = ParseExpression(p, &selF);
      if (!afterSel) {
        return NULL;
      }
      p = SkipWhitespace(afterSel);
      if (*p != ')') {
        return NULL;
      }
      p++;

      int sel = (int)selF;
      int h = RtcGetHours();
      int m = RtcGetMinutes();
      int s = RtcGetSeconds();
      if (sel == 0) {
        const char* ampm = (h < 12) ? "AM" : "PM";
        int h12 = h % 12;
        if (h12 == 0) {
          h12 = 12;
        }
        snprintf(outStr, maxLen, "%02d:%02d:%02d %s", h12, m, s, ampm);
      } else if (sel == 1) {
        snprintf(outStr, maxLen, "%02d:%02d:%02d", h, m, s);
      } else {
        PrintError(ERR_ILLEGAL_QUANTITY);
        return NULL;
      }
      if (outLen) {
        *outLen = strlen(outStr);
      }
      return p;
    } else if (name[strlen(name) - 1] == '$') {
      p = SkipWhitespace(p);
      if (*p == '(') {
        p++;
        float idx1F;
        const char* afterIndex = ParseExpression(p, &idx1F);
        if (!afterIndex) {
          return NULL;
        }
        afterIndex = SkipWhitespace(afterIndex);

        int idx1 = (int)idx1F;
        int idx2 = 0;
        bool has2ndIndex = false;

        if (*afterIndex == ',') {
          afterIndex++;
          float idx2F;
          const char* afterIdx2 = ParseExpression(afterIndex, &idx2F);
          if (!afterIdx2) {
            return NULL;
          }
          afterIndex = SkipWhitespace(afterIdx2);
          idx2 = (int)idx2F;
          has2ndIndex = true;
        }

        if (*afterIndex != ')') {
          return NULL;
        }
        afterIndex++;

        ArrayDescriptor* arr = FindArray(name);
        if (!arr) {
          PrintError(ERR_ARRAY_NOT_DIMD);
          return NULL;
        }

        int linearIdx = CalculateArrayIndex(arr, idx1, idx2, has2ndIndex);
        if (linearIdx == -2) {
          PrintError(ERR_WRONG_DIMENSIONS);
          return NULL;
        }
        if (linearIdx < 0) {
          PrintError(ERR_BAD_SUBSCRIPT);
          return NULL;
        }

        char* ptr = (char*)GetArrayElementPtr(arr, linearIdx);
        strncpy(outStr, ptr, maxLen - 1);
        outStr[maxLen - 1] = '\0';
        if (outLen) {
          *outLen = strlen(outStr);
        }
        return afterIndex;
      } else {
        Variable* v = FindVariable(name);
        if (v && v->type == VAR_STRING) {
          strncpy(outStr, v->strVal ? v->strVal : "", maxLen - 1);
          outStr[maxLen - 1] = '\0';
        } else {
          outStr[0] = '\0';
        }
        if (outLen) {
          *outLen = strlen(outStr);
        }
        return p;
      }
    }
  }

  return NULL;
}

// Evaluates a string expression, joining terms separated by +.
const char* ParseStringExpression(const char* p, char* outStr, size_t maxLen) {
  char tempStr[MAX_STR_EXPR_BUF];

  p = ParseStringValue(p, outStr, maxLen, (size_t*)NULL);
  if (!p) {
    return NULL;
  }

  while (1) {
    p = SkipWhitespace(p);
    if (*p != '+') {
      break;
    }
    p++;

    p = ParseStringValue(p, tempStr, sizeof(tempStr), (size_t*)NULL);
    if (!p) {
      PrintError(ERR_TYPE_MISMATCH);
      return NULL;
    }

    size_t currentLen = strlen(outStr);
    size_t addLen = strlen(tempStr);
    if (currentLen + addLen + 1 <= maxLen) {
      strcat(outStr, tempStr);
    } else {
      strncat(outStr, tempStr, maxLen - currentLen - 1);
      outStr[maxLen - 1] = '\0';
    }
  }

  return p;
}

// ParseStringExpression that also reports the result length, so callers can
// handle embedded NULs rather than relying on strlen.
const char* ParseStringExpressionLen(const char* p, char* outStr, size_t maxLen,
                                     size_t* outLen) {
  char tempStr[MAX_STR_EXPR_BUF];
  size_t curLen = 0;

  p = ParseStringValue(p, outStr, maxLen, &curLen);
  if (!p) {
    return NULL;
  }

  while (1) {
    p = SkipWhitespace(p);
    if (*p != '+') {
      break;
    }
    p++;

    size_t addLen = 0;
    p = ParseStringValue(p, tempStr, sizeof(tempStr), &addLen);
    if (!p) {
      PrintError(ERR_TYPE_MISMATCH);
      return NULL;
    }

    if (curLen + addLen + 1 <= maxLen) {
      memcpy(outStr + curLen, tempStr, addLen);
      outStr[curLen + addLen] = '\0';
      curLen += addLen;
    } else {
      size_t canAdd = maxLen - curLen - 1;
      memcpy(outStr + curLen, tempStr, canAdd);
      outStr[maxLen - 1] = '\0';
      curLen = maxLen - 1;
    }
  }

  *outLen = curLen;
  return p;
}

