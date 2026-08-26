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

static const char* const shapeDemoLines[] = {
    "5 REM *** ROTATING CUBE + PYRAMID ***",
    "6 REM USES PPOLY FOR FLICKER-FREE ANIMATION",
    "10 CLS",
    "20 DIM CV(8,3),PV(5,3)",
    "30 DIM CX(8),CY(8),PPX(5),PPY(5)",
    "40 DIM CP%(16,2),CQ%(16,2)",
    "45 DIM PP%(10,2),PQ%(10,2)",
    "50 DIM CE(16),PE(10)",
    "55 SC=10:D=60",
    "60 REM -- CUBE VERTICES (LEFT SIDE)",
    "70 OX=20:OY=25",
    "80 DATA -1,-1,-1, 1,-1,-1, 1,1,-1, -1,1,-1",
    "90 DATA -1,-1,1, 1,-1,1, 1,1,1, -1,1,1",
    "100 FOR I=0 TO 7",
    "110 READ CV(I,0),CV(I,1),CV(I,2)",
    "120 CV(I,0)=CV(I,0)*SC:CV(I,1)=CV(I,1)*SC:CV(I,2)=CV(I,2)*SC",
    "130 NEXT",
    "140 REM -- CUBE EDGE PATH (16 PTS, ALL 12 EDGES)",
    "150 DATA 0,1,2,3,0,4,5,1,5,6,2,6,7,3,7,4",
    "160 FOR I=0 TO 15:READ CE(I):NEXT",
    "170 REM -- PYRAMID: APEX + 4 BASE VERTS",
    "175 PX=60:PY=25",
    "180 DATA 0,-1.8,0",
    "185 DATA -1,0.6,-1, 1,0.6,-1, 1,0.6,1, -1,0.6,1",
    "190 FOR I=0 TO 4",
    "195 READ PV(I,0),PV(I,1),PV(I,2)",
    "197 PV(I,0)=PV(I,0)*SC:PV(I,1)=PV(I,1)*SC:PV(I,2)=PV(I,2)*SC",
    "198 NEXT",
    "199 REM -- PYRAMID EDGE PATH (ALL 8 EDGES)",
    "200 DATA 0,1,2,3,4,0,3,4,1,2",
    "205 FOR I=0 TO 9:READ PE(I):NEXT",
    "210 AX=0:AY=0:AZ=0:F=0:W=0",
    "211 REM -- AUDIO: SNAKE CHARMER THEME",
    "212 SOUNDPWM 0,180,3:SOUNDPRT 0,60",
    "213 PLAY \"! T125 O4 L8 E F G+ A B4 A G+ F E4. R8 E F G+ A4. G+ F E D E2 "
    "E F G+ A B4 A G+ F E4. R8 E F G+ A4. G+ F E D E2 >E4 D <B A4 G+ F E2.\"",
    "214 PLAY \"! V1 T125 O2 L2 E B E B E B E B E B E B E B E B E\"",
    "220 REM === MAIN LOOP ===",
    "230 SA=SIN(AX):CA=COS(AX)",
    "240 SB=SIN(AY):CB=COS(AY)",
    "250 SZ=SIN(AZ):CZ=COS(AZ)",
    "260 REM -- ROTATE+PROJECT CUBE",
    "270 FOR I=0 TO 7",
    "280 X=CV(I,0):Y=CV(I,1):Z=CV(I,2)",
    "290 Y1=Y*CA-Z*SA:Z1=Y*SA+Z*CA",
    "300 X1=X*CB+Z1*SB:Z2=-X*SB+Z1*CB",
    "310 X2=X1*CZ-Y1*SZ:Y2=X1*SZ+Y1*CZ",
    "320 T=D/(Z2+D)",
    "330 CX(I)=INT(OX+X2*T):CY(I)=INT(OY+Y2*T)",
    "340 NEXT",
    "350 REM -- ROTATE+PROJECT PYRAMID",
    "360 FOR I=0 TO 4",
    "370 X=PV(I,0):Y=PV(I,1):Z=PV(I,2)",
    "380 Y1=Y*CA-Z*SA:Z1=Y*SA+Z*CA",
    "390 X1=X*CB+Z1*SB:Z2=-X*SB+Z1*CB",
    "400 X2=X1*CZ-Y1*SZ:Y2=X1*SZ+Y1*CZ",
    "410 T=D/(Z2+D)",
    "420 PPX(I)=INT(PX+X2*T):PPY(I)=INT(PY+Y2*T)",
    "430 NEXT",
    "440 REM -- BUILD + DRAW",
    "450 IF W=0 THEN GOSUB 600:GOTO 470",
    "460 GOSUB 700",
    "470 IF F=0 THEN PPOLY CP%:PPOLY PP%:F=1:W=1:GOTO 510",
    "480 IF W=0 THEN PPOLY CQ%,CP%:PPOLY PQ%,PP%:W=1:GOTO 510",
    "490 PPOLY CP%,CQ%:PPOLY PP%,PQ%:W=0",
    "510 AX=AX+0.05:AY=AY+0.03:AZ=AZ+0.02",
    "520 GOTO 220",
    "600 REM -- BUILD INTO CP%/PP%",
    "610 FOR I=0 TO 15:J=CE(I)",
    "620 CP%(I,0)=CX(J):CP%(I,1)=CY(J):NEXT",
    "630 FOR I=0 TO 9:J=PE(I)",
    "640 PP%(I,0)=PPX(J):PP%(I,1)=PPY(J):NEXT",
    "650 RETURN",
    "700 REM -- BUILD INTO CQ%/PQ%",
    "710 FOR I=0 TO 15:J=CE(I)",
    "720 CQ%(I,0)=CX(J):CQ%(I,1)=CY(J):NEXT",
    "730 FOR I=0 TO 9:J=PE(I)",
    "740 PQ%(I,0)=PPX(J):PQ%(I,1)=PPY(J):NEXT",
    "750 RETURN",
    NULL};

// SHAPEDEMO: clears the program, enters the built-in pixel-graphics listing,
// and runs it.
void CmdShapeDemo(void) {
  BasicExecute("new");
  for (int i = 0; shapeDemoLines[i] != NULL; i++) {
    BasicExecute((char*)shapeDemoLines[i]);
  }
  BasicExecute("run");
}
