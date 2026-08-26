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

#include "audio_messages.h"

// Pacing for the outgoing link. The receiver services its UART only in the
// gaps left by its own real-time work, so bytes cannot be sent back to back.
// The trailing gap lets the last byte land before the next message starts.
const uint16_t kInterByteUs = 350;
const uint16_t kInterFrameUs = 150;

const uint8_t kPayloadByteOffset = 3;
const uint8_t kSOP = 0x5c;

typedef enum {
  kAudioSilence = 0x01,
  kAudioVoiceOn = 0x02,
  kAudioVoiceOff = 0x03,
  kAudioProgram = 0x04,
  kAudioPlayProgram = 0x05,
  kAudioStopProgram = 0x06,
  kAudioClearProgram = 0x07,
  kAudioSetProgramRepeat = 0x08,
  kAudioToneOn,
  kAudioToneOff,
  kAudioSetPW,
  kAudioProgramAppend,
  kAudioSetWaveform,
  kAudioSetEnvelope,
  kAudioSetSync,
  kAudioSetPortamento,
  kAudioShutUp = 0x99,
  kAudioReboot = 0x9A,
} AudioMsgId;

// Two's-complement checksum, so a valid frame plus its checksum byte sums to
// zero. Must match the verifier in DaisySound.
static uint8_t calcchecksum(uint8_t* data_in, size_t data_len) {
  uint8_t raw_sum = 0;

  for (size_t i = 0; i < data_len; i++) {
    raw_sum += data_in[i];
  }

  return (uint8_t)(~raw_sum + 1);
}

// Writes a framed message to DaisySound and appends its checksum. The
// inter-byte delays hold the sender back to a rate the Uno can keep up with
// while its audio ISR is running.
uint8_t SendAudioMessage(uint8_t* msg, size_t msg_size) {
  uint8_t msg_checksum = calcchecksum(msg, msg_size);
  for (uint8_t pos = 0; pos < msg_size; pos++) {
    Serial3.write(msg[pos]);
    delayMicroseconds(kInterByteUs);
  }
  Serial3.write(msg_checksum);
  delayMicroseconds(kInterFrameUs);
  return msg_size + 1;
}

// Starts a voice sounding at a frequency, and leaves it sounding.
void AudioMsgSendVoiceOn(uint8_t voice, uint16_t freq) {
  uint8_t VoiceOnMsg[6];
  VoiceOnMsg[0] = kSOP;
  VoiceOnMsg[1] = kAudioVoiceOn;
  VoiceOnMsg[2] = 3;
  VoiceOnMsg[3] = voice;
  VoiceOnMsg[4] = (uint8_t)(freq >> 8);
  VoiceOnMsg[5] = (uint8_t)(freq & 0x00FF);
  SendAudioMessage(VoiceOnMsg, 6);
}

// Silences a voice.
void AudioMsgSendVoiceOff(uint8_t voice) {
  uint8_t VoiceOffMsg[4];
  VoiceOffMsg[0] = kSOP;
  VoiceOffMsg[1] = kAudioVoiceOff;
  VoiceOffMsg[2] = 1;
  VoiceOffMsg[3] = voice;
  SendAudioMessage(VoiceOffMsg, 4);
}

#define NOTES_PER_MSG 62

// Packs a run of notes into one frame as big-endian frequency and duration
// pairs.
static void SendNoteMsg(uint8_t opcode, uint8_t program_num, Note* notes,
                        uint8_t len) {
  uint8_t msg[255];
  msg[0] = kSOP;
  msg[1] = opcode;
  msg[2] = (4 * len) + 1;
  msg[3] = program_num;
  uint8_t pos = 4;
  for (uint8_t i = 0; i < len; i++) {
    msg[pos++] = notes[i].freq >> 8;
    msg[pos++] = notes[i].freq & 0xFF;
    msg[pos++] = notes[i].durationMs >> 8;
    msg[pos++] = notes[i].durationMs & 0xFF;
  }
  SendAudioMessage(msg, 4 + (len * 4));
}

// Loads a note sequence into a program slot, splitting into a load followed by
// an append when the sequence is longer than one frame can carry.
void AudioMsgProgram(uint8_t program_num, Note* notes, uint8_t len) {
  uint8_t first = (len > NOTES_PER_MSG) ? NOTES_PER_MSG : len;
  SendNoteMsg(kAudioProgram, program_num, notes, first);
  if (len > NOTES_PER_MSG) {
    SendNoteMsg(kAudioProgramAppend, program_num, notes + first, len - first);
  }
}

// Starts a loaded program playing.
void AudioMsgSendPlayProgram(uint8_t program_num) {
  uint8_t PlayProgramMsg[4];
  PlayProgramMsg[0] = kSOP;
  PlayProgramMsg[1] = kAudioPlayProgram;
  PlayProgramMsg[2] = 1;
  PlayProgramMsg[3] = program_num;
  SendAudioMessage(PlayProgramMsg, 4);
}

// Stops a program and silences its voice.
void AudioMsgSendStopProgram(uint8_t program_num) {
  uint8_t StopProgramMsg[4];
  StopProgramMsg[0] = kSOP;
  StopProgramMsg[1] = kAudioStopProgram;
  StopProgramMsg[2] = 1;
  StopProgramMsg[3] = program_num;
  SendAudioMessage(StopProgramMsg, 4);
}

// Sets whether a program loops or plays once.
void AudioMsgSendSetRepeat(uint8_t program_num, bool en) {
  uint8_t RepeatProgramMsg[5];
  RepeatProgramMsg[0] = kSOP;
  RepeatProgramMsg[1] = kAudioSetProgramRepeat;
  RepeatProgramMsg[2] = 2;
  RepeatProgramMsg[3] = program_num;
  RepeatProgramMsg[4] = en ? 1 : 0;
  SendAudioMessage(RepeatProgramMsg, 5);
}

// Silences everything and resets the audio board's state.
void AudioMsgSendShutUp(void) {
  uint8_t ShutUpMsg[4];
  ShutUpMsg[0] = kSOP;
  ShutUpMsg[1] = kAudioShutUp;
  ShutUpMsg[2] = 0;
  SendAudioMessage(ShutUpMsg, 3);
}

// Asks DaisySound to reset, used during startup to get it into a known state.
void AudioMsgSendReboot(void) {
  uint8_t msg[3];
  msg[0] = kSOP;
  msg[1] = kAudioReboot;
  msg[2] = 0;
  SendAudioMessage(msg, 3);
}

// Plays a tone for a fixed duration. DaisySound times the note itself so the
// caller does not have to stay and stop it.
void AudioMsgSendToneOn(uint8_t voice, uint16_t freq, uint32_t time) {
  uint8_t data[10];
  data[0] = kSOP;
  data[1] = kAudioToneOn;
  data[2] = 7;
  data[3] = voice;
  data[4] = (uint8_t)(freq >> 8);
  data[5] = (uint8_t)(freq & 0x00FF);
  data[6] = (uint8_t)(time >> 24);
  data[7] = (uint8_t)(time >> 16);
  data[8] = (uint8_t)(time >> 8);
  data[9] = (uint8_t)(time & 0x00ff);
  SendAudioMessage(data, 10);
}

// Cuts a timed tone short.
void AudioMsgSendToneOff(uint8_t voice) {
  uint8_t data[4];
  data[0] = kSOP;
  data[1] = kAudioToneOff;
  data[2] = 1;
  data[3] = voice;
  SendAudioMessage(data, 4);
}

// Sets pulse width and the LFO rate that sweeps it, shaping the voice timbre.
void AudioMsgSendSetPW(uint8_t voice, uint8_t pw, float lfo_hz) {
  uint16_t lfo_rate_x100 = (uint16_t)(lfo_hz * 100.0f);
  uint8_t data[7];
  data[0] = kSOP;
  data[1] = kAudioSetPW;
  data[2] = 4;
  data[3] = voice;
  data[4] = pw;
  data[5] = (uint8_t)(lfo_rate_x100 >> 8);
  data[6] = (uint8_t)(lfo_rate_x100 & 0x00FF);
  SendAudioMessage(data, 7);
}

// Sets the glide time between notes; zero turns glide off.
void AudioMsgSendSetPortamento(uint8_t voice, uint16_t ms) {
  uint8_t data[6];
  data[0] = kSOP;
  data[1] = kAudioSetPortamento;
  data[2] = 3;
  data[3] = voice;
  data[4] = (uint8_t)(ms >> 8);
  data[5] = (uint8_t)(ms & 0x00FF);
  SendAudioMessage(data, 6);
}
