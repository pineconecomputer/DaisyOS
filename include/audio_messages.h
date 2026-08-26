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

#ifndef INCLUDE_AUDIO_MESSAGES_H_
#define INCLUDE_AUDIO_MESSAGES_H_

#include <Arduino.h>

typedef struct {
  uint16_t freq;
  uint16_t durationMs;
} Note;

#warning remove these once public api is implemented
uint8_t SendAudioMessage(uint8_t* msg, size_t msg_size);

void AudioMsgSendVoiceOn(uint8_t voice, uint16_t freq);
void AudioMsgSendVoiceOff(uint8_t voice);
void AudioMsgProgram(uint8_t program_num, Note* notes, uint8_t len);
void AudioMsgSendPlayProgram(uint8_t program_num);
void AudioMsgSendSetRepeat(uint8_t program_num, bool en);
void AudioMsgSendStopProgram(uint8_t program_num);
void AudioMsgSendShutUp(void);
void AudioMsgSendReboot(void);
void AudioMsgSendToneOn(uint8_t voice, uint16_t freq, uint32_t time);
void AudioMsgSendToneOff(uint8_t voice);
void AudioMsgSendSetPW(uint8_t voice, uint8_t pw, float lfo_hz);
void AudioMsgSendSetPortamento(uint8_t voice, uint16_t ms);

#define AUDIO_MAX_NOTES 64

#endif  // INCLUDE_AUDIO_MESSAGES_H_
