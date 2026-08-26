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

#pragma once

#include <Arduino.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>

#include "daisybasic.h"
#include "shadow_ram.h"
#include "audio_messages.h"
#include "keyboard.h"
#include "joyport.h"
#include "buffer.h"
#include "wifi.h"
#include "cursor.h"
#include "timer.h"
#include "invaders.h"
#include "conway.h"
#include "terminal.h"
#include "zmachine.h"
#include "video_messages.h"

#define MAX_VAR_NAME 14
#define MAX_STR_EXPR_BUF 255
#define MAX_LINE_TOKENS 255
#define MAX_STRING_VAR_LEN 80
#define STRING_ELEMENT_LEN (MAX_STRING_VAR_LEN + 1)
#define STRING_POOL_SIZE 8192
#define MAX_VAR_NAMES 128
#define MAX_VAR_NAME_LEN 14
#define MAX_USER_FUNCS 16
#define MAX_FN_EXPR_LEN 128
#define DAISY_BASIC_HEAP (64 * 1024)

typedef enum {
  ERR_TYPE_MISMATCH = 0,
  ERR_ARRAY_NOT_DIMD,
  ERR_WRONG_DIMENSIONS,
  ERR_BAD_SUBSCRIPT,
  ERR_DIVISION_BY_ZERO,
  ERR_ILLEGAL_QUANTITY,
  ERR_SYNTAX,
  ERR_OUT_OF_MEMORY,
  ERR_RETURN_WITHOUT_GOSUB,
  ERR_OUT_OF_DATA,
  ERR_NEXT_WITHOUT_FOR,
  ERR_LOAD,
  ERR_MISSING_FILE_NAME,
  ERR_CATALOG,
  ERR_UNDEF_STATEMENT,
  ERR_ILLEGAL_LINE_NUMBER,
  ERR_ILLEGAL_DIRECT,
  ERR_WEND_WITHOUT_WHILE,
  ERR_WHILE_WITHOUT_WEND,
  ERR_UNTIL_WITHOUT_DO,
  ERR_DO_WITHOUT_UNTIL,
  ERR_EXIT_NOT_IN_LOOP,
  ERR_RESULT_WITHOUT_RETURN,
  ERR_FOPEN,
  ERR_FCLOSE,
  ERR_FPRINT,
  ERR_FPUT,
  ERR_FSEEK,
  ERR_WIFI_CONNECT,
  ERR_NETCONNECT,
  ERR_RESUME_WITHOUT_TRAP,
  ERR_LINE_TOO_LONG,
  ERR_FORMULA_TOO_COMPLEX
} BasicError;

typedef enum { VAR_NONE = 0, VAR_INT, VAR_FLOAT, VAR_STRING } VarType;

typedef struct {
  char name[MAX_VAR_NAME];
  VarType type;
  union {
    int intVal;
    float floatVal;
    char* strVal;
  };
} Variable;

typedef enum {
  ARRAY_TYPE_INT = 0,
  ARRAY_TYPE_FLOAT = 1,
  ARRAY_TYPE_STRING = 2
} ArrayType;

typedef struct {
  char name[MAX_VAR_NAME];
  ArrayType type;
  uint16_t dim1Size;
  uint16_t dim2Size;
  uint8_t* data;
  uint32_t totalBytes;
  bool isDimmed;
} ArrayDescriptor;

typedef struct {
  char varName[MAX_VAR_NAME];
  float limitVal;
  float stepVal;
  int returnLine;
} ForLoopEntry;

typedef enum { LOOP_WHILE = 0, LOOP_DO = 1 } LoopType;

typedef struct {
  int lineIndex;
  LoopType type;
} WhileEntry;

typedef enum {
  TOK_KW_NONE = 0,
  TOK_KW_PRINT = 128,
  TOK_KW_LET,
  TOK_KW_LOCATE,
  TOK_KW_LINE,
  TOK_KW_BEEP,
  TOK_KW_CLS,
  TOK_KW_GOTO,
  TOK_KW_LIST,
  TOK_KW_RUN,
  TOK_KW_NEW,
  TOK_KW_END,
  TOK_KW_REM,
  TOK_KW_FOR,
  TOK_KW_TO,
  TOK_KW_STEP,
  TOK_KW_NEXT,
  TOK_KW_IF,
  TOK_KW_THEN,
  TOK_KW_INPUT,
  TOK_KW_GET,
  TOK_KW_CLR,
  TOK_KW_GOSUB,
  TOK_KW_RETURN,
  TOK_KW_DATA,
  TOK_KW_READ,
  TOK_KW_RESTORE,
  TOK_KW_DEFCHAR,
  TOK_KW_RESETCHAR,
  TOK_KW_DEFGFX,
  TOK_KW_RESETGFX,
  TOK_KW_CHARMODE,
  TOK_KW_TONEON,
  TOK_KW_TONEOFF,
  TOK_KW_SLEEP,
  TOK_KW_AND,
  TOK_KW_OR,
  TOK_KW_XOR,
  TOK_KW_NOT,
  TOK_KW_DIM,
  TOK_KW_SOUNDPGM,
  TOK_KW_REVERSE,
  TOK_KW_NORMAL,
  TOK_KW_REBOOT,
  TOK_KW_CATALOG,
  TOK_KW_CONT,
  TOK_KW_PLOTCHAR,
  TOK_KW_FILLCELLS,
  TOK_KW_HLINE,
  TOK_KW_VLINE,
  TOK_KW_WAITMS,
  TOK_KW_READMAT,
  TOK_KW_ON,
  TOK_KW_DEF,
  TOK_KW_TIMER,
  TOK_KW_MOVEBLOCK,
  TOK_KW_PPLOT,
  TOK_KW_SCROLL,
  TOK_KW_SOUNDPWM,
  TOK_KW_FILLBLOCK,
  TOK_KW_PLINE,
  TOK_KW_PCIRCLE,
  TOK_KW_PFILL,
  TOK_KW_LOADCHAR,
  TOK_KW_SAVECHAR,
  TOK_KW_COPYCHAR,
  TOK_KW_PLAY,
  TOK_KW_SOUNDPRT,
  TOK_KW_SCROLLX,
  TOK_KW_NETGET,
  TOK_KW_NETINPUT,
  TOK_KW_NETPRINT,
  TOK_KW_WHILE,
  TOK_KW_WEND,
  TOK_KW_DO,
  TOK_KW_UNTIL,
  TOK_KW_EXIT,
  TOK_KW_VERSION,
  TOK_KW_CHUNK,
  TOK_KW_SETTIME,
  TOK_KW_SETDATE,
  TOK_KW_PPOLY,
  TOK_KW_BOX,
  TOK_KW_SETATTRIB,
  TOK_KW_CLEARARR,
  TOK_KW_SWAPARR,
  TOK_KW_RENUMBER,
  TOK_KW_LOAD,
  TOK_KW_SAVE,
  TOK_KW_WIFI,
  TOK_KW_NETCONNECT,
  TOK_KW_NETDISCONNECT,
  TOK_KW_FOPEN = 219,
  TOK_KW_FCLOSE = 220,
  TOK_KW_FPRINT = 221,
  TOK_KW_FINPUT = 222,
  TOK_KW_FGET = 223,
  TOK_KW_FPUT = 224,
  TOK_KW_FSEEK = 225,
  TOK_KW_TRAP = 226,
  TOK_KW_RESUME = 227,
  TOK_KW_MORE = 228,
  TOK_KW_DEG = 229,
  TOK_KW_RAD = 230,
  TOK_KW_DEL = 231,
  TOK_KW_REN = 232,
  TOK_KW_COPY = 233,
  TOK_KW_CHDIR = 234,
  TOK_KW_MKDIR = 235,
  TOK_STRING_LIT = 236,
  TOK_NUMBER_INT = 237,
  TOK_NUMBER_FLOAT = 238,
  TOK_VARNAME = 239,
  TOK_LINENUM = 240,
  TOK_STRING_REF = 241,
  TOK_VAR_EXT = 242,
  TOK_SMALL_INT_BASE = 243,
  TOK_SMALL_INT_MAX = 254,
  TOK_EOL = 255
} TokenKeyword;

typedef struct {
  const char* keyword;
  uint8_t token;
} KeywordEntry;

typedef struct {
  char name[MAX_VAR_NAME_LEN];
} VarNameEntry;

typedef struct {
  uint16_t lineNum;
  uint16_t offset;
  uint8_t tokenLen;
} ProgramLine;

typedef struct {
  char name[MAX_VAR_NAME];
  char paramName[MAX_VAR_NAME];
  char expr[MAX_FN_EXPR_LEN];
} UserFunction;

extern Variable* variables;
extern int variableCount;
extern int variableCapacity;

extern ArrayDescriptor* arrayDescriptors;
extern int arrayDescriptorCount;
extern int arrayDescriptorCapacity;

extern ForLoopEntry* forStack;
extern int forStackTop;
extern int forStackCapacity;

typedef struct {
  int lineIndex;
  uint8_t stmtSkip;
  uint8_t thenClauseSkip;
} GosubEntry;
extern GosubEntry* gosubStack;
extern int gosubStackTop;
extern int gosubStackCapacity;
extern int compoundStmtIndex;
extern int outerStmtSkipForGosub;
extern int thenClauseSkipForGosub;
extern int pendingThenClauseSkip;

extern ProgramLine* program;
extern int programLineCount;
extern int programCapacity;
extern uint8_t* tokenPool;
extern uint16_t tokenPoolUsed;
extern uint16_t tokenPoolCap;

extern char stringPool[STRING_POOL_SIZE];
extern uint16_t stringPoolTop;
extern VarNameEntry varNameTable[MAX_VAR_NAMES];
extern uint8_t varNameCount;

extern const KeywordEntry keywords[];

extern const char* const errorMessages[];

extern int currentExecLine;
extern int continueLineIndex;
extern bool programRunning;
extern volatile bool breakRequested;
extern bool errorPrinted;
extern int gotoLineNum;

extern int dataLineIndex;
extern int dataItemIndex;

extern UserFunction userFunctions[MAX_USER_FUNCS];
extern int userFuncCount;

extern WhileEntry* whileStack;
extern int whileStackTop;
extern int whileStackCapacity;

extern bool timerEnabled;
extern uint32_t timerIntervalMs;
extern int timerTargetLine;
extern uint32_t timerLastFireMs;

extern size_t heapBytesUsed;

extern float returnVals[2];
extern int returnValCount;

extern bool trigDegMode;

extern int trapLineNum;
extern bool trapActive;
extern bool trapTriggered;
extern int trapErrorLineIndex;
extern BasicError trapErrorCode;

bool EnsureCapacity(void** ptr, int* capacity, int needed, size_t elemSize,
                    int initialCap, bool zeroNew);
bool EnsureTokenPoolCapacity(int needed);

void PrintError(BasicError err);
const char* GetErrorMessage(BasicError err);
void DoReset(void);
void CmdDemo(void);
void CmdShapeDemo(void);

Variable* FindVariable(const char* name);
Variable* CreateVariable(const char* name, VarType type);
bool SetStringVar(Variable* v, const char* str);
bool IsIntArrayVar(const char* name);
bool IsBuiltinStringFunction(const char* name);
bool IsStringArrayVar(const char* name);
uint16_t GetElementSize(ArrayType type);
ArrayType GetArrayTypeFromName(const char* name);
ArrayDescriptor* FindArray(const char* name);
int CalculateArrayIndex(ArrayDescriptor* arr, int idx1, int idx2,
                        bool has2ndIndex);
void* GetArrayElementPtr(ArrayDescriptor* arr, int linearIndex);
bool AllocateArray(ArrayDescriptor* desc, uint16_t dim1, uint16_t dim2);
void ClearAllArrays(void);

int FindStringInPool(const char* str, int len);
int AddStringToPool(const char* str, int len);
const char* GetStringFromPool(uint16_t offset, uint8_t* outLen);
int FindVarNameInTable(const char* name, int len);
int AddVarNameToTable(const char* name, int len);
const char* GetVarNameFromTable(uint8_t index);
uint8_t* GetLineTokens(int lineIndex);
int TokenizeLine(const char* line, uint8_t* tokens, int maxTokens);
int DetokenizeLine(const uint8_t* tokens, int tokenLen, char* out, int maxOut);
int FindProgramLine(uint16_t lineNum);
bool InsertProgramLine(uint16_t lineNum, const uint8_t* tokens, int tokenLen);
void ClearProgram(void);

const char* ParseStringExpression(const char* p, char* out, size_t maxLen);
const char* ParseStringExpressionLen(const char* p, char* out, size_t maxLen,
                                     size_t* outLen);
bool EvalExpression(const char* expr, float* result);

const char* SkipWhitespace(const char* p);
const char* MatchCommand(const char* line, const char* cmd);
const char* ParseString(const char* p, char* out, size_t max_len);
const char* ParseVarName(const char* p, char* name, size_t maxLen);
bool IsTypeSuffixAt(const char* p);
bool IsStringVar(const char* name);
const char* ParseExpression(const char* p, float* result);
void FormatNumber(float val, char* buf, size_t bufLen);

bool BasicReadLine(char* buf, size_t maxLen);
uint8_t BasicGetChar(void);
bool CmdLet(const char* args);
bool CmdPrint(const char* args);
bool CmdInput(const char* args);
bool CmdGet(const char* args);
bool CmdNetGet(const char* args);
bool CmdNetInput(const char* args);
bool CmdNetPrint(const char* args);
bool CmdWifi(const char* args);
bool CmdNetConnect(const char* args);
bool CmdNetDisconnect(const char* args);
bool CmdLocate(const char* args);

bool CmdCls(const char* args);
bool CmdLine(const char* args);
bool CmdDefChar(const char* args);
bool CmdResetChar(const char* args);
bool CmdDefGfx(const char* args);
bool CmdResetGfx(const char* args);
bool CmdCharMode(const char* args);
bool CmdReverse(const char* args);
bool CmdNormal(const char* args);
bool CmdPlotChar(const char* args);
bool CmdFillCells(const char* args);
bool CmdHLine(const char* args);
bool CmdVLine(const char* args);
bool CmdMoveBlock(const char* args);
bool CmdFillBlock(const char* args);
bool CmdPPlot(const char* args);
bool CmdPLine(const char* args);
bool CmdPCircle(const char* args);
bool CmdPFill(const char* args);
bool CmdPPoly(const char* args);
bool CmdBox(const char* args);
bool CmdSetAttrib(const char* args);
bool CmdClearArr(const char* args);
bool CmdSwapArr(const char* args);
bool CmdScroll(const char* args);
bool CmdScrollX(const char* args);

bool CmdBeep(const char* args);
bool CmdToneOn(const char* args);
bool CmdToneOff(const char* args);
bool CmdSoundPgm(const char* args);
bool CmdSoundPwm(const char* args);
bool CmdSoundPrt(const char* args);
void DoSleep(float sleep_val);
bool CmdSleep(const char* args);
void TickKeyClick(void);

bool GosubStackEnsureCapacity(int needed);
bool CmdGoto(const char* args);
bool CmdEnd(const char* args);
bool CmdGosub(const char* args);
bool CmdReturn(const char* args);
bool CmdFor(const char* args);
bool CmdNext(const char* args);
bool EvalCondition(const char* cond, bool* result);
bool CmdIf(const char* args);
bool CmdOn(const char* args);
bool SubstituteParam(const char* expr, const char* paramName, float paramVal,
                     char* outExpr, int maxLen);
bool CmdDef(const char* args);
bool CmdTimer(const char* args);
bool CmdTrap(const char* args);
bool CmdResume(const char* args);
bool CmdWhile(const char* args);
bool CmdWend(const char* args);
bool CmdDo(const char* args);
bool CmdUntil(const char* args);
bool CmdExit(const char* args);
bool CmdVersion(const char* args);
bool CmdChunk(const char* args);

bool CmdData(const char* args);
bool CmdRead(const char* args);
bool CmdReadMat(const char* args);
bool CmdRestore(const char* args);

bool CmdList(const char* args);
bool CmdDim(const char* args);
bool CmdClr(const char* args);
bool CmdNew(const char* args);
bool CmdRenumber(const char* args);
bool CmdLoad(const char* args);
bool CmdSave(const char* args);
bool CmdCatalog(const char* args);
bool CmdLoadchar(const char* args);
bool CmdSavechar(const char* args);
bool CmdCopyChar(const char* args);
bool CmdPlay(const char* args);

bool CmdFopen(const char* args);
bool CmdFclose(const char* args);
void CloseAllFileChannels(void);
bool CmdFprint(const char* args);
bool CmdFinput(const char* args);
bool CmdFget(const char* args);
bool CmdFput(const char* args);
bool CmdFseek(const char* args);
bool CmdFrewind(const char* args);
bool CmdMore(const char* args);
bool CmdDeg(const char* args);
bool CmdRad(const char* args);
bool CmdDel(const char* args);
bool CmdRen(const char* args);
bool CmdCopy(const char* args);
bool CmdChdir(const char* args);
bool CmdMkdir(const char* args);

void RtcInit(void);
bool CmdSetTime(const char* args);
bool CmdSetDate(const char* args);
int RtcGetHours(void);
int RtcGetMinutes(void);
int RtcGetSeconds(void);
int RtcGetDay(void);
int RtcGetMonth(void);
int RtcGetYear(void);

void FireTimerGosub(void);
bool RunFromLine(int startLine);
bool CmdRun(const char* args);
bool CmdContinue(const char* args);
bool ExecuteSingleStatement(const char* line);
bool ExecuteStatement(const char* line);
bool ExecuteTokenizedLine(const uint8_t* tokens, int tokenLen);
bool _BasicExecute(const char* line, bool showReady);
