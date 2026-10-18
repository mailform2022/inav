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


// Get target build configuration
#include <string.h>

#include "platform.h"
#include "build/debug.h"

#include "common/maths.h"
#include "common/log.h"

#include "config/config_eeprom.h"
#include "config/parameter_group.h"
#include "config/parameter_group_ids.h"

#include "drivers/vtx_common.h"
#include "drivers/light_led.h"
#include "drivers/system.h"

#include "fc/config.h"
#include "fc/runtime_config.h"
#include "fc/settings.h"

#include "io/beeper.h"
#include "io/osd.h"
#include "io/vtx.h"
#include "io/vtx_control.h"
#include "io/vtx_string.h"

#include "rx/rx.h"

#if defined(USE_VTX_CONTROL)

PG_REGISTER_WITH_RESET_TEMPLATE(vtxConfig_t, vtxConfig, PG_VTX_CONFIG, 14);

PG_RESET_TEMPLATE(vtxConfig_t, vtxConfig,
      .halfDuplex = SETTING_VTX_HALFDUPLEX_DEFAULT,
      .smartAudioEarlyAkkWorkaroundEnable = SETTING_VTX_SMARTAUDIO_EARLY_AKK_WORKAROUND_DEFAULT,
      .smartAudioAltSoftSerialMethod = SETTING_VTX_SMARTAUDIO_ALTERNATE_SOFTSERIAL_METHOD_DEFAULT,
      .softSerialShortStop = SETTING_VTX_SOFTSERIAL_SHORTSTOP_DEFAULT,
      .smartAudioStopBits = SETTING_VTX_SMARTAUDIO_STOPBITS_DEFAULT,
      .vtx3g3PowerMode = SETTING_VTX_3G3_POWER_MODE_DEFAULT,
      .vtx3g3ClearPitmode = SETTING_VTX_3G3_CLEAR_PITMODE_DEFAULT,
      .vtx3g3Keepalive = SETTING_VTX_3G3_KEEPALIVE_DEFAULT,
      .vtx3g3ChannelMode = SETTING_VTX_3G3_CHANNEL_MODE_DEFAULT,
      .vtx3g3ChanSetMode = SETTING_VTX_3G3_CHAN_SETMODE_DEFAULT,
      .vtx3g3ChanInterByteMs = SETTING_VTX_3G3_CHAN_INTERBYTE_MS_DEFAULT,
      .vtx3g3ChanSettleMs = SETTING_VTX_3G3_CHAN_SETTLE_MS_DEFAULT,
      .vtx3g3ChanReassert = SETTING_VTX_3G3_CHAN_REASSERT_DEFAULT,
      .vtx3g3SaPulldown = SETTING_VTX_3G3_SA_PULLDOWN_DEFAULT,
      .vtx3g3Grid = SETTING_VTX_3G3_GRID_DEFAULT,
      .vtx3g3TrampPwrCode = {
          SETTING_VTX_3G3_TRAMP_PWR1_DEFAULT,
          SETTING_VTX_3G3_TRAMP_PWR2_DEFAULT,
          SETTING_VTX_3G3_TRAMP_PWR3_DEFAULT,
      },
      .vtx3g3ChanFreqFix = SETTING_VTX_3G3_CHAN_FREQFIX_DEFAULT,
);

PG_REGISTER_ARRAY(vtxRcMapEntry_t, MAX_VTX_RC_MAP_ENTRIES, vtxRcMapEntries, PG_VTX_RC_MAP, 1);
PG_REGISTER(vtxCustomGridConfig_t, vtxCustomGridConfig, PG_VTX_CUSTOM_GRID, 0);

PG_REGISTER_WITH_RESET_TEMPLATE(vtxCurtainConfig_t, vtxCurtainConfig, PG_VTX_CURTAIN_CONFIG, 3);

PG_RESET_TEMPLATE(vtxCurtainConfig_t, vtxCurtainConfig,
      .mode = SETTING_VTX_CURTAIN_MODE_DEFAULT,
      .rcChannel = SETTING_VTX_CURTAIN_CHANNEL_DEFAULT,
      .offset = SETTING_VTX_CURTAIN_OFFSET_DEFAULT,
      .rcSync = SETTING_VTX_RC_SYNC_DEFAULT,
      .rcSyncLow = SETTING_VTX_RC_SYNC_LOW_DEFAULT,
      .rcSyncMid = SETTING_VTX_RC_SYNC_MID_DEFAULT,
      .rcSyncHigh = SETTING_VTX_RC_SYNC_HIGH_DEFAULT,
      .preset = SETTING_VTX_RC_PRESET_DEFAULT,
);

static uint8_t locked = 0;

void vtxControlInit(void)
{
    // NOTHING TO DO
}

void vtxControlInputPoll(void)
{
  // Check variuos input sources for VTX config updates

  // XXX: None supported in INAV
}

static void vtxUpdateBandAndChannel(uint8_t bandStep, uint8_t channelStep)
{
    if (ARMING_FLAG(ARMED)) {
        locked = 1;
    }

    if (!locked) {
        vtxDevice_t *vtxDevice = vtxCommonDevice();
        if (vtxDevice) {
            uint8_t band = 0, channel = 0;
            vtxCommonGetBandAndChannel(vtxDevice, &band, &channel);
            vtxCommonSetBandAndChannel(vtxDevice, band + bandStep, channel + channelStep);
        }
    }
}

void vtxIncrementBand(void)
{
    vtxUpdateBandAndChannel(+1, 0);
}

void vtxDecrementBand(void)
{
    vtxUpdateBandAndChannel(-1, 0);
}

void vtxIncrementChannel(void)
{
    vtxUpdateBandAndChannel(0, +1);
}

void vtxDecrementChannel(void)
{
    vtxUpdateBandAndChannel(0, -1);
}

void vtxUpdateActivatedChannel(void)
{
    if (ARMING_FLAG(ARMED)) {
        locked = 1;
    }

    if (!locked) {
        vtxDevice_t *vtxDevice = vtxCommonDevice();
        if (vtxDevice) {
            static uint8_t lastIndex = -1;

            for (uint8_t index = 0; index < MAX_CHANNEL_ACTIVATION_CONDITION_COUNT; index++) {
                const vtxChannelActivationCondition_t *vtxChannelActivationCondition = &vtxConfig()->vtxChannelActivationConditions[index];

                if (isRangeActive(vtxChannelActivationCondition->auxChannelIndex, &vtxChannelActivationCondition->range)
                    && index != lastIndex) {
                    lastIndex = index;

                    vtxCommonSetBandAndChannel(vtxDevice, vtxChannelActivationCondition->band, vtxChannelActivationCondition->channel);
                    break;
                }
            }
        }
    }
}

static bool vtxRcMapEntryIsValid(const vtxRcMapEntry_t *entry)
{
    return entry->rcChannel >= 1 && entry->rcChannel <= VTX_RC_MAP_CHANNEL_COUNT &&
        entry->band >= VTX_SETTINGS_MIN_BAND && entry->channel >= VTX_SETTINGS_MIN_CHANNEL &&
        entry->rangeEnd >= entry->rangeStart;
}

bool vtxRcSyncCalibrated(void)
{
    const vtxCurtainConfig_t *cfg = vtxCurtainConfig();
    return cfg->rcSync && cfg->rcSyncLow && cfg->rcSyncLow < cfg->rcSyncMid && cfg->rcSyncMid < cfg->rcSyncHigh;
}

// Received us -> nominal us (transmitter -100 = 1000, 0 = 1500, +100 = 2000),
// so VTX map values set once keep matching after a change of RC link.
int vtxRcSyncNormalize(int value)
{
    if (!vtxRcSyncCalibrated()) {
        return value;
    }

    const vtxCurtainConfig_t *cfg = vtxCurtainConfig();
    const int halfSpan = VTX_RC_SYNC_NOMINAL_MID - VTX_RC_SYNC_NOMINAL_LOW;
    if (value <= cfg->rcSyncMid) {
        return VTX_RC_SYNC_NOMINAL_MID - ((cfg->rcSyncMid - value) * halfSpan + (cfg->rcSyncMid - cfg->rcSyncLow) / 2) / (cfg->rcSyncMid - cfg->rcSyncLow);
    }
    return VTX_RC_SYNC_NOMINAL_MID + ((value - cfg->rcSyncMid) * halfSpan + (cfg->rcSyncHigh - cfg->rcSyncMid) / 2) / (cfg->rcSyncHigh - cfg->rcSyncMid);
}

static int rcMapChannelValue(uint8_t rcChannel)
{
    return vtxRcSyncNormalize(rxGetChannelValue(rcChannel - 1));
}

// Lowest start and highest end of the pairs on one RC channel
static bool vtxRcMapChannelSpan(uint8_t rcChannel, uint16_t *low, uint16_t *high)
{
    bool found = false;

    for (uint8_t index = 0; index < MAX_VTX_RC_MAP_ENTRIES; index++) {
        const vtxRcMapEntry_t *entry = vtxRcMapEntries(index);
        if (!vtxRcMapEntryIsValid(entry) || entry->rcChannel != rcChannel) {
            continue;
        }
        if (!found || entry->rangeStart < *low) {
            *low = entry->rangeStart;
        }
        if (!found || entry->rangeEnd > *high) {
            *high = entry->rangeEnd;
        }
        found = true;
    }

    return found;
}

// Distance between the centres of the lowest and the highest pair (the
// transmitter's FLOAT offset is computed from the pair values)
static bool vtxRcMapChannelCenterSpan(uint8_t rcChannel, int *span)
{
    bool found = false;
    int low = 0;
    int high = 0;

    for (uint8_t index = 0; index < MAX_VTX_RC_MAP_ENTRIES; index++) {
        const vtxRcMapEntry_t *entry = vtxRcMapEntries(index);
        if (!vtxRcMapEntryIsValid(entry) || entry->rcChannel != rcChannel) {
            continue;
        }
        const int center = (entry->rangeStart + entry->rangeEnd) / 2;
        if (!found || center < low) {
            low = center;
        }
        if (!found || center > high) {
            high = center;
        }
        found = true;
    }

    *span = high - low;
    return found;
}

// rcChannel == 0 matches the pairs of every channel
static int8_t vtxRcMapMatch(uint8_t rcChannel, int shift)
{
    for (uint8_t index = 0; index < MAX_VTX_RC_MAP_ENTRIES; index++) {
        const vtxRcMapEntry_t *entry = vtxRcMapEntries(index);
        if (!vtxRcMapEntryIsValid(entry) || (rcChannel && entry->rcChannel != rcChannel)) {
            continue;
        }

        const int value = rcMapChannelValue(entry->rcChannel) - shift;
        const int slack = entry->rangeStart == entry->rangeEnd ? VTX_RC_MAP_EXACT_SLACK : 0;
        if (value >= entry->rangeStart - slack && value <= entry->rangeEnd + slack) {
            return index;
        }
    }

    return -1;
}

uint8_t vtxCurtainRcChannel(void)
{
    if (vtxCurtainConfig()->mode == VTX_CURTAIN_RC_OFF) {
        return 0;
    }

    if (vtxCurtainConfig()->rcChannel) {
        return vtxCurtainConfig()->rcChannel;
    }

    for (uint8_t index = 0; index < MAX_VTX_RC_MAP_ENTRIES; index++) {
        const vtxRcMapEntry_t *entry = vtxRcMapEntries(index);
        if (vtxRcMapEntryIsValid(entry)) {
            return entry->rcChannel;
        }
    }

    return 0;
}

// Shift in us on the (synced) channel scale
int16_t vtxCurtainRcOffset(uint8_t rcChannel)
{
    int span = 0;

    if (vtxCurtainConfig()->mode == VTX_CURTAIN_RC_FLOAT) {
        if (rcChannel && vtxRcMapChannelCenterSpan(rcChannel, &span)) {
            return span + VTX_RC_PERCENT_US;
        }
        return VTX_CURTAIN_RC_DEFAULT_OFFSET;
    }

    return vtxCurtainConfig()->offset * VTX_RC_PERCENT_US;
}

int8_t vtxRcMapFindEntry(void)
{
    int8_t index = vtxRcMapMatch(0, 0);
    if (index >= 0) {
        return index;
    }

    const uint8_t rcChannel = vtxCurtainRcChannel();
    if (rcChannel) {
        index = vtxRcMapMatch(rcChannel, vtxCurtainRcOffset(rcChannel));
    }

    return index;
}

bool vtxRcCurtainActive(void)
{
    // Between ranges (a switch in travel, an unmapped value) the curtain holds
    static bool active = false;

    const uint8_t rcChannel = vtxCurtainRcChannel();
    if (!rcChannel) {
        active = false;
        return false;
    }

    const int value = rcMapChannelValue(rcChannel);
    const int offset = vtxCurtainRcOffset(rcChannel);
    uint16_t low = 0;
    uint16_t high = 0;

    if (!vtxRcMapChannelSpan(rcChannel, &low, &high)) {
        active = value >= VTX_CURTAIN_RC_BASE + ABS(offset) / 2;
    } else if (vtxRcMapMatch(rcChannel, 0) >= 0) {
        active = false;
    } else if (vtxCurtainConfig()->mode == VTX_CURTAIN_RC_FLOAT) {
        if (value > high) {
            active = true;
        }
    } else if (vtxRcMapMatch(rcChannel, offset) >= 0) {
        active = true;
    }

    return active;
}

void vtxUpdateRcMap(void)
{
    static int8_t appliedEntry = -1;

    const int8_t index = vtxRcMapFindEntry();
    if (index < 0 || index == appliedEntry) {
        return;
    }

    const vtxRcMapEntry_t *entry = vtxRcMapEntries(index);
    appliedEntry = index;
    vtxSettingsConfigMutable()->band = constrain(entry->band, VTX_SETTINGS_MIN_BAND, VTX_SETTINGS_MAX_BAND_ANY);
    vtxSettingsConfigMutable()->channel = constrain(entry->channel, VTX_SETTINGS_MIN_CHANNEL, VTX_SETTINGS_MAX_CHANNEL_ANY);
    if (entry->power) {
        vtxSettingsConfigMutable()->power = entry->power;
    }
}

void vtxCycleBandOrChannel(const uint8_t bandStep, const uint8_t channelStep)
{
    vtxDevice_t *vtxDevice = vtxCommonDevice();

    if (!vtxDevice) {
        return;
    }

    uint8_t band = 0, channel = 0;
    vtxDeviceCapability_t capability;

    bool haveAllNeededInfo = vtxCommonGetBandAndChannel(vtxDevice, &band, &channel) && vtxCommonGetDeviceCapability(vtxDevice, &capability);
    if (!haveAllNeededInfo) {
        return;
    }

    int newChannel = channel + channelStep;
    if (newChannel > capability.channelCount) {
        newChannel = 1;
    } else if (newChannel < 1) {
        newChannel = capability.channelCount;
    }

    int newBand = band + bandStep;
    if (newBand > capability.bandCount) {
        newBand = 1;
    } else if (newBand < 1) {
        newBand = capability.bandCount;
    }

    vtxCommonSetBandAndChannel(vtxDevice, newBand, newChannel);
}

void vtxCyclePower(const uint8_t powerStep)
{
    vtxDevice_t *vtxDevice = vtxCommonDevice();

    if (!vtxDevice) {
        return;
    }

    uint8_t power = 0;
    vtxDeviceCapability_t capability;

    bool haveAllNeededInfo = vtxCommonGetPowerIndex(vtxDevice, &power) && vtxCommonGetDeviceCapability(vtxDevice, &capability);
    if (!haveAllNeededInfo) {
        return;
    }

    int newPower = power + powerStep;
    if (newPower >= capability.powerCount) {
        newPower = 0;
    } else if (newPower < 0) {
        newPower = capability.powerCount;
    }

    vtxCommonSetPowerByIndex(vtxDevice, newPower);
}

#endif

int vtxRcPresetPercent(uint8_t slot)
{
    return slot < VTX_RC_PRESET_PAIRS / 2 ? -90 + 9 * slot : 9 * (slot - VTX_RC_PRESET_PAIRS / 2 + 1);
}

/*
 * Fill the map with the default pairs of a grid: its band/channel cells in table
 * order on the 20 preset transmitter values, maximum power, FIXED curtain.
 */
bool vtxRcPresetApply(uint8_t grid, uint8_t rcChannel)
{
    uint8_t bands, chans, powers;
    if (!rcChannel || rcChannel > VTX_RC_MAP_CHANNEL_COUNT ||
        !vtx3G3_GridPresetInfo(grid, &bands, &chans, &powers)) {
        return false;
    }

    vtxConfigMutable()->vtx3g3Grid = grid;
    vtxSettingsConfigMutable()->frequencyGroup = FREQUENCYGROUP_3G3;
    vtxSettingsConfigMutable()->power = powers;

    uint8_t slot = 0;
    for (uint8_t band = 1; band <= bands && slot < VTX_RC_PRESET_PAIRS; band++) {
        for (uint8_t chan = 1; chan <= chans && slot < VTX_RC_PRESET_PAIRS; chan++) {
            if (!vtx3G3_GridFrequency(grid, band, chan)) {
                continue;
            }
            vtxRcMapEntry_t *entry = vtxRcMapEntriesMutable(slot);
            const uint16_t value = VTX_RC_SYNC_NOMINAL_MID + vtxRcPresetPercent(slot) * VTX_RC_PERCENT_US;
            entry->rcChannel = rcChannel;
            entry->band = band;
            entry->channel = chan;
            entry->power = powers;
            entry->rangeStart = value;
            entry->rangeEnd = value;
            slot++;
        }
    }
    for (; slot < MAX_VTX_RC_MAP_ENTRIES; slot++) {
        memset(vtxRcMapEntriesMutable(slot), 0, sizeof(vtxRcMapEntry_t));
    }

    vtxCurtainConfig_t *cfg = vtxCurtainConfigMutable();
    cfg->mode = VTX_CURTAIN_RC_FIXED;
    cfg->rcChannel = rcChannel;
    cfg->offset = VTX_RC_PRESET_CURTAIN_OFFSET;
    cfg->preset = grid;
    return true;
}

/*
 * Sync burst sent by the transmitter on the VTX map channel while disarmed:
 * -100, +100, 0, -100, +100, 0, each held for 100 ms, optionally followed by a
 * VTX marker step (-85 + 10 x grid %) and a return to 0. The marker makes the
 * FC load the default pairs of that grid; without it only the scale is learned.
 * Every step is shorter than the VTX band/channel settle window, so the VTX is
 * never retuned by the burst.
 */
#define VTX_RC_SYNC_STEPS            6
#define VTX_RC_SYNC_STEP_MIN_MS      40
#define VTX_RC_SYNC_STEP_MAX_MS      250
#define VTX_RC_SYNC_FLAT_US          12
#define VTX_RC_SYNC_REPEAT_US        15
#define VTX_RC_SYNC_MIN_SPAN_US      400
#define VTX_RC_SYNC_MARKER_WAIT_MS   400
#define VTX_RC_SYNC_MARKER_BASE      -85
#define VTX_RC_SYNC_MARKER_STEP      10
#define VTX_RC_SYNC_MARKER_TOL       3

static uint8_t vtxRcSyncChannel(void)
{
    if (vtxCurtainConfig()->rcChannel) {
        return vtxCurtainConfig()->rcChannel;
    }

    for (uint8_t index = 0; index < MAX_VTX_RC_MAP_ENTRIES; index++) {
        const vtxRcMapEntry_t *entry = vtxRcMapEntries(index);
        if (vtxRcMapEntryIsValid(entry)) {
            return entry->rcChannel;
        }
    }

    return 0;
}

static bool vtxRcSyncPatternFound(const uint16_t *step, uint16_t *low, uint16_t *mid, uint16_t *high)
{
    for (int i = 0; i < 3; i++) {
        if (ABS(step[i] - step[i + 3]) > VTX_RC_SYNC_REPEAT_US) {
            return false;
        }
    }

    *low = (step[0] + step[3] + 1) / 2;
    *high = (step[1] + step[4] + 1) / 2;
    *mid = (step[2] + step[5] + 1) / 2;

    if (*high < *low + VTX_RC_SYNC_MIN_SPAN_US) {
        return false;
    }

    const uint16_t span = *high - *low;
    return *mid > *low + span * 3 / 10 && *mid < *low + span * 7 / 10;
}

static int vtxRcSyncMarkerGrid(uint16_t value)
{
    const int percent = (vtxRcSyncNormalize(value) - VTX_RC_SYNC_NOMINAL_MID) / VTX_RC_PERCENT_US;
    const int offset = percent - VTX_RC_SYNC_MARKER_BASE;
    if (offset < -VTX_RC_SYNC_MARKER_TOL) {
        return -1;
    }
    const int grid = (offset + VTX_RC_SYNC_MARKER_STEP / 2) / VTX_RC_SYNC_MARKER_STEP;
    if (ABS(offset - grid * VTX_RC_SYNC_MARKER_STEP) > VTX_RC_SYNC_MARKER_TOL ||
        grid >= VTX_3G3_GRID_COUNT || grid == VTX_3G3_GRID_AUTO) {
        return -1;
    }
    return grid;
}

static bool vtxRcMapIsEmpty(void)
{
    for (uint8_t index = 0; index < MAX_VTX_RC_MAP_ENTRIES; index++) {
        if (vtxRcMapEntryIsValid(vtxRcMapEntries(index))) {
            return false;
        }
    }
    return true;
}

static void vtxRcSyncFinish(bool calibrationChanged, int markerGrid, uint8_t rcChannel)
{
    bool changed = calibrationChanged;

    if (markerGrid >= 0 && (markerGrid != vtxCurtainConfig()->preset || vtxRcMapIsEmpty())) {
        changed |= vtxRcPresetApply(markerGrid, rcChannel);
    }

    if (changed) {
        saveConfigAndNotify();
    } else {
        beeper(BEEPER_ACTION_SUCCESS);
    }
    LOG_INFO(VTX, "RC sync on CH%u: %u/%u/%u us, VTX %d", rcChannel,
        vtxCurtainConfig()->rcSyncLow, vtxCurtainConfig()->rcSyncMid, vtxCurtainConfig()->rcSyncHigh, markerGrid);
}

void vtxRcSyncUpdate(void)
{
    static uint16_t step[VTX_RC_SYNC_STEPS];
    static uint8_t stepCount = 0;
    static int flatRef = -1;
    static uint32_t flatSum = 0;
    static uint16_t flatSamples = 0;
    static timeMs_t flatSinceMs = 0;
    static bool markerPending = false;
    static bool markerCalibrationChanged = false;
    static timeMs_t markerSinceMs = 0;

    const uint8_t rcChannel = vtxRcSyncChannel();
    if (!vtxCurtainConfig()->rcSync || !rcChannel || ARMING_FLAG(ARMED)) {
        stepCount = 0;
        flatRef = -1;
        markerPending = false;
        return;
    }

    const int value = rxGetChannelValue(rcChannel - 1);
    const timeMs_t nowMs = millis();

    if (markerPending && nowMs - markerSinceMs > VTX_RC_SYNC_MARKER_WAIT_MS) {
        markerPending = false;
        vtxRcSyncFinish(markerCalibrationChanged, -1, rcChannel);
    }

    if (flatRef >= 0 && ABS(value - flatRef) <= VTX_RC_SYNC_FLAT_US) {
        flatSum += value;
        flatSamples++;
        return;
    }

    if (flatRef >= 0) {
        const timeMs_t heldMs = nowMs - flatSinceMs;
        const uint16_t held = (flatSum + flatSamples / 2) / flatSamples;
        if (markerPending && heldMs >= VTX_RC_SYNC_STEP_MIN_MS) {
            markerPending = false;
            const int grid = heldMs <= VTX_RC_SYNC_STEP_MAX_MS ? vtxRcSyncMarkerGrid(held) : -1;
            vtxRcSyncFinish(markerCalibrationChanged, grid, rcChannel);
        } else if (heldMs > VTX_RC_SYNC_STEP_MAX_MS) {
            stepCount = 0;
        } else if (heldMs >= VTX_RC_SYNC_STEP_MIN_MS) {
            if (stepCount == VTX_RC_SYNC_STEPS) {
                memmove(&step[0], &step[1], sizeof(step[0]) * (VTX_RC_SYNC_STEPS - 1));
                stepCount--;
            }
            step[stepCount++] = held;
        }
        // A shorter step is a frame caught in transit between two steps
    }

    flatRef = value;
    flatSum = value;
    flatSamples = 1;
    flatSinceMs = nowMs;

    uint16_t low, mid, high;
    if (!markerPending && stepCount == VTX_RC_SYNC_STEPS && vtxRcSyncPatternFound(step, &low, &mid, &high)) {
        stepCount = 0;
        vtxCurtainConfig_t *cfg = vtxCurtainConfigMutable();
        markerCalibrationChanged = cfg->rcSyncLow != low || cfg->rcSyncMid != mid || cfg->rcSyncHigh != high;
        cfg->rcSyncLow = low;
        cfg->rcSyncMid = mid;
        cfg->rcSyncHigh = high;
        markerPending = true;
        markerSinceMs = nowMs;
    }
}
