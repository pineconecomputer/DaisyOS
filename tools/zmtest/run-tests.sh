#!/bin/sh
#
# Regression tests for the Z-machine module.
#
# Runs the interpreter against real story files on a virtual 40x25 VT52 screen
# and checks what actually lands on it. Every test asserts on rendered output
# rather than on the interpreter's internal state, so a change that breaks the
# screen driver fails here even if the interpreter is still correct.
#
#   ./run-tests.sh [directory-holding-ZORK1.DAT]
#
# The story files are not in this repository. Point the script at a directory
# containing ZORK1.DAT, ZORK2.DAT and ZORK3.DAT, or set ZORK_DIR.

set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
ZMTEST="${HERE}/zmtest"
ZORK_DIR="${1:-${ZORK_DIR:-}}"

if [ -z "${ZORK_DIR}" ]; then
    echo "usage: $0 DIRECTORY_WITH_ZORK_DAT_FILES" >&2
    echo "   or: ZORK_DIR=... $0" >&2
    exit 2
fi

if [ ! -x "${ZMTEST}" ]; then
    echo "building harness first..."
    "${HERE}/build.sh" >/dev/null || exit 1
fi

# A regression in the abandon path shows up as a hang, not as wrong output, so
# every run is time-limited. Without this the suite would simply stop.
if command -v timeout >/dev/null 2>&1; then
    TIMEOUT="timeout 20"
elif command -v gtimeout >/dev/null 2>&1; then
    TIMEOUT="gtimeout 20"
else
    TIMEOUT=""
    echo "note: no timeout command found, a hanging test will block the suite" >&2
fi

pass=0
fail=0
last_rc=0
tmp="$(mktemp -d)"
trap 'rm -rf "${tmp}"' EXIT

# run STORY SCRIPT -> writes combined screen+scrollback to ${tmp}/out
run() {
    story="$1"
    shift
    printf '%s\n' "$@" > "${tmp}/script"
    rm -f "${HERE}/zmtest-save-0.dat" 2>/dev/null
    ( cd "${HERE}" && ZM_SCROLLBACK=1 ${TIMEOUT} "${ZMTEST}" "${story}" "${tmp}/script" ) \
        > "${tmp}/out" 2>&1
    last_rc=$?
}

check() {
    what="$1"
    pattern="$2"
    if grep -qF "${pattern}" "${tmp}/out"; then
        echo "  ok       ${what}"
        pass=$((pass + 1))
    else
        echo "  FAIL     ${what}"
        echo "           expected to find: ${pattern}"
        fail=$((fail + 1))
    fi
}

# Fails if the previous run did not finish, which is what a hang looks like.
checkfinished() {
    what="$1"
    if [ "${last_rc}" -eq 124 ]; then
        echo "  FAIL     ${what}"
        echo "           the interpreter hung and had to be killed"
        fail=$((fail + 1))
    else
        echo "  ok       ${what}"
        pass=$((pass + 1))
    fi
}

checknot() {
    what="$1"
    pattern="$2"
    if grep -qF "${pattern}" "${tmp}/out"; then
        echo "  FAIL     ${what}"
        echo "           did not expect: ${pattern}"
        fail=$((fail + 1))
    else
        echo "  ok       ${what}"
        pass=$((pass + 1))
    fi
}

Z1="${ZORK_DIR}/ZORK1.DAT"
Z2="${ZORK_DIR}/ZORK2.DAT"
Z3="${ZORK_DIR}/ZORK3.DAT"

for f in "${Z1}" "${Z2}" "${Z3}"; do
    [ -f "${f}" ] || { echo "missing story file: ${f}" >&2; exit 2; }
done

echo "== boot and parser =="
run "${Z1}" look
check "Zork I banner renders"          "ZORK I: The Great Underground Empire"
check "opening room description"       "You are standing in an open field west"
check "parser answers a command"       ">look"
checknot "no interpreter abort"        "*** stopped:"
checknot "no unrenderable escapes"     ", 1 unrenderable"

echo
echo "== movement and world model =="
run "${Z1}" north north east
check "moved to North of House"        "North of House"
check "moved to Forest Path"           "Forest Path"
check "moved to Forest"                "This is a dimly lit forest"

echo
echo "== objects =="
run "${Z1}" "open mailbox" "take leaflet" inventory
check "container opens"                "Opening the small mailbox reveals"
check "object is taken"                "Taken."
check "inventory lists it"             "A leaflet"

echo
echo "== status line fits 40 columns =="
run "${Z1}" look
check "room, score and moves all present" " West of House             S:0"
checknot "room name not clipped by score" "West of Score"
check "status line is in reverse video"   "=== 40 cells in reverse video ==="

echo
echo "== save and restore =="
run "${Z1}" "open mailbox" "take leaflet" save north north restore look inventory
check "save reports success"           ">save"
check "restore reports success"        ">restore"
check "restored to saved room"         "West of House"
check "restored inventory"             "A leaflet"
checknot "restore did not crash"       "*** stopped:"

echo
echo "== restart =="
run "${Z1}" north restart look
check "restart returns to start room"  "West of House"
checknot "restart did not crash"       "*** stopped:"

echo
echo "== [MORE] paging =="
run "${Z3}" look
check "Zork III boots"                 "ZORK III: The Dungeon Master"
checknot "MORE prompt is erased"       "[MORE]"

echo
echo "== other stories =="
run "${Z2}" look
check "Zork II boots"                  "Inside the Barrow"
checknot "Zork II runs clean"          "*** stopped:"

echo
echo "== STOP abandons the story =="
# The machine has no QUIT to fall back on when the player presses STOP, and
# ending only the current read is not enough: the story is handed an empty
# line, prints "Beg pardon?" and asks again, which loops for ever. <STOP> in a
# script stands in for the key.
run "${Z1}" "open mailbox" "<STOP>"
checkfinished "STOP returns instead of looping"
check "the game did run first"          "Opening the small mailbox reveals"
checknot "no parser error from a blank line" "Beg pardon"
checknot "STOP did not look like an error"   "*** stopped:"

# STOP at a [MORE] prompt, where a story printing a long passage waits.
run "${Z3}" "<STOP>"
checkfinished "STOP at a [MORE] prompt returns"

echo
echo "== corrupt story is rejected without hanging =="
head -c 4096 /dev/urandom > "${tmp}/junk.dat"
run "${tmp}/junk.dat" look
check "bad story stops cleanly"        "*** stopped:"

echo
echo "-------------------------------------------"
echo "passed ${pass}, failed ${fail}"
[ "${fail}" -eq 0 ] || exit 1
