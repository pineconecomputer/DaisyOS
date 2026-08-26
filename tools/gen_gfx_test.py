# DaisyOS - main firmware for the Daisy computer.
# Copyright (C) 2026 Joe Cassara
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

"""Generate a binary test file for VT52 graphics character mode."""

import struct, os

ESC = b'\x1b'
CRLF = b'\r\n'
GFX_ON  = ESC + b'F'
GFX_OFF = ESC + b'G'
CLR     = ESC + b'E'

out = bytearray()

def emit(data):
    if isinstance(data, str):
        data = data.encode('ascii')
    out.extend(data)

def nl():
    out.extend(CRLF)

emit(CLR)

emit("VT52 GRAPHICS MODE TEST")
nl(); nl()

emit("5F 60 61 62 63 64 65 66 67 68 69 6A 6B 6C 6D 6E")
nl()
emit(GFX_ON)
for ch in range(0x5F, 0x6F):
    emit(bytes([ch]))
    emit(b'  ')
emit(GFX_OFF)
nl(); nl()

emit("6F 70 71 72 73 74 75 76 77 78 79 7A 7B 7C 7D 7E")
nl()
emit(GFX_ON)
for ch in range(0x6F, 0x7F):
    emit(bytes([ch]))
    emit(b'  ')
emit(GFX_OFF)
nl(); nl()

emit("BOX DRAWING:")
nl()

emit(GFX_ON)
emit("lqqqwqqqwqqqk");  nl()
emit("x   x   x   x");  nl()
emit("tqqqnqqqnqqqu");  nl()
emit("x   x   x   x");  nl()
emit("mqqqvqqqvqqqj");  nl()
emit(GFX_OFF)
nl()

emit("SCAN LINES:")
nl()
emit(GFX_ON)
emit("oooooooooooooooo");  nl()
emit("pppppppppppppppp");  nl()
emit("qqqqqqqqqqqqqqqq");  nl()
emit("rrrrrrrrrrrrrrrr");  nl()
emit("ssssssssssssssss");  nl()
emit(GFX_OFF)
nl()

emit("MATH: ")
emit(GFX_ON)
emit("{")
emit(GFX_OFF)
emit("  ")
emit(GFX_ON)
emit("y")
emit(GFX_OFF)
emit("  ")
emit(GFX_ON)
emit("z")
emit(GFX_OFF)
emit("  ")
emit(GFX_ON)
emit("|")
emit(GFX_OFF)
emit("  ")
emit(GFX_ON)
emit("f")
emit(GFX_OFF)
emit("  ")
emit(GFX_ON)
emit("g")
emit(GFX_OFF)
emit("  ")
emit(GFX_ON)
emit("}")
emit(GFX_OFF)
emit("  ")
emit(GFX_ON)
emit("~")
emit(GFX_OFF)
nl(); nl()

emit("DONE - ALL 32 GLYPHS SHOWN")
nl()

outpath = os.path.join(os.path.dirname(__file__), '..', 'gfx_test.bin')
outpath = os.path.normpath(outpath)
with open(outpath, 'wb') as f:
    f.write(out)
print(f"Wrote {len(out)} bytes to {outpath}")
