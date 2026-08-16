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

typedef enum {
    VTX_3G3_CHAN_CHANNEL = 0,  // Command band/channel with SET_CHANNEL (index)
    VTX_3G3_CHAN_FREQUENCY,    // Command the actual grid frequency with SET_FREQ (MHz)
    VTX_3G3_CHAN_NONE,         // Never command band/channel; leave what the buttons set
} vtx3g3ChannelMode_e;

/* Values are persisted in the config, so new grids are appended rather than
 * inserted in a tidier order. */
typedef enum {
    VTX_3G3_GRID_SX33 = 0,     // SX33 / FF3741: 5 bands A-E, 3200-3700 MHz, 25mW/2W/5W
    VTX_3G3_GRID_TX3339,       // BeastFPV TX3339-32CH: 4 bands A-D, 3060-3480 MHz, 25mW/3W/10W
    VTX_3G3_GRID_AUTO,         // Pick from what the attached VTX reports, SX33 if it says nothing useful
    VTX_3G3_GRID_NONAME1,      // "Noname_1" 3W IRC Tramp: 2 bands, 3200-3500 MHz, 25mW/400mW/1W/3W
    VTX_3G3_GRID_FF37,         // "FF3.7" SmartAudio: 1 band of 20 channels, 3700-4080 MHz
    VTX_3G3_GRID_COUNT,
} vtx3g3Grid_e;

/* How the active 3.3GHz grid was arrived at, reported over CLI and MSP so a wrong
 * guess is visible instead of silently mistuning the VTX. */
typedef enum {
    VTX_3G3_DETECT_NONE = 0,   // Nothing heard from a VTX yet
    VTX_3G3_DETECT_FORCED,     // vtx_3g3_grid names the grid explicitly
    VTX_3G3_DETECT_RANGE,      // Recognised from the frequency range the VTX reported
    VTX_3G3_DETECT_POWER,      // Recognised from the max power the VTX reported
    VTX_3G3_DETECT_PROTOCOL,   // Recognised from the protocol in use (SmartAudio => FF3741)
    VTX_3G3_DETECT_FALLBACK,   // VTX reported nothing usable, fell back to SX33
} vtx3g3Detect_e;

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
    uint8_t vtx3g3ChannelMode;
    bool    vtx3g3ChanSetMode;       // send SET_MODE (unlock/clr-pit) before SET_CHANNEL
    uint8_t vtx3g3ChanInterByteMs;   // gap between SET_CHANNEL frame bytes on the wire
    uint8_t vtx3g3ChanSettleMs;      // pause after the SET_CHANNEL frame
    bool    vtx3g3SaPulldown;        // pull the SmartAudio line LOW while idle (FF3741 video stripes)
    uint8_t vtx3g3Grid;              // Which 3.3GHz band/channel grid the attached VTX uses
    uint16_t vtx3g3TrampPwrCode[3];  // IRC Tramp (SX33) 3.3GHz device power codes per grid level
    bool    vtx3g3ChanFreqFix;       // after SET_CHANNEL, also SET_FREQ the real grid MHz (FF3741 stuck freqMode)
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
