/*
 * This file is part of Cleanflight.
 *
 * Cleanflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "fc/rc_modes.h"

#define MAX_CHANNEL_ACTIVATION_CONDITION_COUNT  10

typedef struct vtxChannelActivationCondition_s {
    uint8_t auxChannelIndex;
    uint8_t band;
    uint8_t channel;
    channelRange_t range;
} vtxChannelActivationCondition_t;

typedef enum {
    VTX_3G3_POWER_AUTO = 0,    // Use the dBm levels reported by the VTX (SmartAudio V2.1, like Betaflight)
    VTX_3G3_POWER_FIXED_DBM,   // Send fixed grid dBm 14/33/37 with the V2.1 dBm flag (MSB)
    VTX_3G3_POWER_INDEX,       // Send the power level index, no flag (SmartAudio V2.0 style)
    VTX_3G3_POWER_RAW_DBM,     // Send fixed grid dBm 14/33/37 without the V2.1 flag
    VTX_3G3_POWER_NONE,        // Never command power; leave whatever the VTX/buttons set
} vtx3g3PowerMode_e;

typedef struct vtxConfig_s {
    vtxChannelActivationCondition_t vtxChannelActivationConditions[MAX_CHANNEL_ACTIVATION_CONDITION_COUNT];
    uint8_t halfDuplex;
    uint8_t smartAudioEarlyAkkWorkaroundEnable;
    bool    smartAudioAltSoftSerialMethod;
    bool    softSerialShortStop;
    uint8_t smartAudioStopBits;
    uint8_t vtx3g3PowerMode;
    bool    vtx3g3ClearPitmode;
    bool    vtx3g3Keepalive;
    bool    vtx3g3SetChannel;
} vtxConfig_t;

PG_DECLARE(vtxConfig_t, vtxConfig);

void vtxControlInit(void);
void vtxControlInputPoll(void);

void vtxIncrementBand(void);
void vtxDecrementBand(void);
void vtxIncrementChannel(void);
void vtxDecrementChannel(void);

void vtxCyclePower(const uint8_t powerStep);
void vtxCycleBandOrChannel(const uint8_t bandStep, const uint8_t channelStep);

void vtxUpdateActivatedChannel(void);

void handleVTXControlButton(void);
