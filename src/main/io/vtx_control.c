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
PG_REGISTER(vtxRcCurveConfig_t, vtxRcCurveConfig, PG_VTX_RC_CURVE, 0);

// Bumped whenever the map, the grid or the RC scale changes: the pair is re-applied
static uint8_t vtxRcMapGeneration = 0;

void vtxRcMapChanged(void)
{
    vtxRcMapGeneration++;
}

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

bool vtxRcCurveValid(void)
{
    const uint16_t *point = vtxRcCurveConfig()->point;
    for (int i = 0; i < VTX_RC_CURVE_POINTS; i++) {
        if (!point[i] || (i && point[i] <= point[i - 1])) {
            return false;
        }
    }
    return true;
}

bool vtxRcSyncCalibrated(void)
{
    const vtxCurtainConfig_t *cfg = vtxCurtainConfig();
    return cfg->rcSync && cfg->rcSyncLow && cfg->rcSyncLow < cfg->rcSyncMid && cfg->rcSyncMid < cfg->rcSyncHigh;
}

// Received us -> nominal us (transmitter -100 = 988, 0 = 1500, +100 = 2012),
// so VTX map values set once keep matching after a change of RC link.
int vtxRcSyncNormalize(int value)
{
    if (!vtxRcSyncCalibrated()) {
        return value;
    }

    if (vtxRcCurveValid()) {
        // Piecewise linear over the sweep points: removes curvature of the link
        const uint16_t *point = vtxRcCurveConfig()->point;
        int seg = 0;
        while (seg < VTX_RC_CURVE_POINTS - 2 && value >= point[seg + 1]) {
            seg++;
        }
        const int32_t segNominal100 = (int32_t)VTX_RC_SYNC_NOMINAL_MID * 100 + (-100 + VTX_RC_CURVE_STEP_PCT * seg) * 512;
        const int32_t segWidth100 = VTX_RC_CURVE_STEP_PCT * 512;
        const int32_t span = point[seg + 1] - point[seg];
        const int32_t nominal100 = segNominal100 + ((int32_t)(value - point[seg]) * segWidth100 + (value >= point[seg] ? span / 2 : -span / 2)) / span;
        return (nominal100 + (nominal100 >= 0 ? 50 : -50)) / 100;
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
            return span + VTX_RC_PCT_TO_US(1);
        }
        return VTX_CURTAIN_RC_DEFAULT_OFFSET;
    }

    return VTX_RC_PCT_TO_US(vtxCurtainConfig()->offset);
}

static bool vtxRcMapEntryOnChannel(const vtxRcMapEntry_t *entry, uint8_t rcChannel)
{
    return vtxRcMapEntryIsValid(entry) && entry->rcChannel == rcChannel;
}

// Matching value of an exact pair (curtain = 0) or of its FIXED curtain (curtain = 1)
static int vtxRcMapCandidate(const vtxRcMapEntry_t *entry, int curtain, int curtainOffset)
{
    return entry->rangeStart + (curtain ? curtainOffset : 0);
}

static int vtxRcMapCandidateTolerance(uint8_t rcChannel, int candidate, int curtains, int curtainOffset)
{
    int tolerance = VTX_RC_MAP_NEAR_MAX_US;

    for (uint8_t index = 0; index < MAX_VTX_RC_MAP_ENTRIES; index++) {
        const vtxRcMapEntry_t *entry = vtxRcMapEntries(index);
        if (!vtxRcMapEntryOnChannel(entry, rcChannel) || entry->rangeStart != entry->rangeEnd) {
            continue;
        }
        for (int curtain = 0; curtain < curtains; curtain++) {
            const int gap = ABS(vtxRcMapCandidate(entry, curtain, curtainOffset) - candidate);
            if (gap && gap / 2 < tolerance) {
                tolerance = gap / 2;
            }
        }
    }

    return tolerance;
}

/*
 * Pair selected by a (nominal) value on rcChannel. Range pairs match inclusively;
 * exact pairs and, in FIXED mode, their curtain values (pair + offset) match the
 * nearest one. *curtain tells that the value is a curtain value.
 */
static int8_t vtxRcMapLookup(uint8_t rcChannel, int value, bool *curtain)
{
    const bool fixedCurtain = vtxCurtainConfig()->mode == VTX_CURTAIN_RC_FIXED && vtxCurtainRcChannel() == rcChannel;
    const int curtainOffset = fixedCurtain ? vtxCurtainRcOffset(rcChannel) : 0;
    const int curtains = fixedCurtain ? 2 : 1;

    *curtain = false;

    for (int pass = 0; pass < curtains; pass++) {
        for (uint8_t index = 0; index < MAX_VTX_RC_MAP_ENTRIES; index++) {
            const vtxRcMapEntry_t *entry = vtxRcMapEntries(index);
            if (!vtxRcMapEntryOnChannel(entry, rcChannel) || entry->rangeStart == entry->rangeEnd) {
                continue;
            }
            const int shifted = value - (pass ? curtainOffset : 0);
            if (shifted >= entry->rangeStart && shifted <= entry->rangeEnd) {
                *curtain = pass;
                return index;
            }
        }
    }

    int8_t best = -1;
    int bestDistance = 0;
    for (uint8_t index = 0; index < MAX_VTX_RC_MAP_ENTRIES; index++) {
        const vtxRcMapEntry_t *entry = vtxRcMapEntries(index);
        if (!vtxRcMapEntryOnChannel(entry, rcChannel) || entry->rangeStart != entry->rangeEnd) {
            continue;
        }
        for (int pass = 0; pass < curtains; pass++) {
            const int candidate = vtxRcMapCandidate(entry, pass, curtainOffset);
            const int distance = ABS(value - candidate);
            if ((best >= 0 && distance >= bestDistance) ||
                distance > vtxRcMapCandidateTolerance(rcChannel, candidate, curtains, curtainOffset)) {
                continue;
            }
            best = index;
            bestDistance = distance;
            *curtain = pass;
        }
    }

    return best;
}

int8_t vtxRcMapFindEntry(void)
{
    for (uint8_t index = 0; index < MAX_VTX_RC_MAP_ENTRIES; index++) {
        const vtxRcMapEntry_t *entry = vtxRcMapEntries(index);
        if (!vtxRcMapEntryIsValid(entry)) {
            continue;
        }
        bool seen = false;
        for (uint8_t prev = 0; prev < index && !seen; prev++) {
            seen = vtxRcMapEntryOnChannel(vtxRcMapEntries(prev), entry->rcChannel);
        }
        if (seen) {
            continue;
        }
        bool curtain;
        const int8_t found = vtxRcMapLookup(entry->rcChannel, rcMapChannelValue(entry->rcChannel), &curtain);
        if (found >= 0) {
            return found;
        }
    }

    const uint8_t rcChannel = vtxCurtainRcChannel();
    if (rcChannel && vtxCurtainConfig()->mode == VTX_CURTAIN_RC_FLOAT) {
        bool curtain;
        return vtxRcMapLookup(rcChannel, rcMapChannelValue(rcChannel) - vtxCurtainRcOffset(rcChannel), &curtain);
    }

    return -1;
}

bool vtxRcCurtainActive(void)
{
    // Between pairs (a switch in travel, an unmapped value) and during the
    // sync burst the curtain holds
    static bool active = false;

    const uint8_t rcChannel = vtxCurtainRcChannel();
    if (!rcChannel) {
        active = false;
        return false;
    }
    if (vtxRcSyncHold()) {
        return active;
    }

    const int value = rcMapChannelValue(rcChannel);
    const int offset = vtxCurtainRcOffset(rcChannel);
    uint16_t low = 0;
    uint16_t high = 0;
    bool curtain = false;

    if (!vtxRcMapChannelSpan(rcChannel, &low, &high)) {
        active = value >= VTX_CURTAIN_RC_BASE + ABS(offset) / 2;
    } else if (vtxRcMapLookup(rcChannel, value, &curtain) >= 0) {
        active = curtain;
    } else if (vtxCurtainConfig()->mode == VTX_CURTAIN_RC_FLOAT && value > high) {
        active = true;
    }

    return active;
}

static void vtxRcMapFollowGrid(void);

void vtxUpdateRcMap(void)
{
    static int8_t appliedEntry = -1;
    static uint8_t appliedGeneration = 0;

    if (vtxRcSyncHold()) {
        return;
    }
    vtxRcMapFollowGrid();
    if (appliedGeneration != vtxRcMapGeneration) {
        appliedGeneration = vtxRcMapGeneration;
        appliedEntry = -1;
    }

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

    // An AUTO grid that already detected this VTX stays AUTO, so a VTX swap keeps being followed
    if (vtxConfig()->vtx3g3Grid != VTX_3G3_GRID_AUTO || vtx3G3_EffectiveGrid() != grid) {
        vtxConfigMutable()->vtx3g3Grid = grid;
    }
    vtxSettingsConfigMutable()->frequencyGroup = FREQUENCYGROUP_3G3;
    vtxSettingsConfigMutable()->power = powers;

    uint8_t slot = 0;
    for (uint8_t band = 1; band <= bands && slot < VTX_RC_PRESET_PAIRS; band++) {
        for (uint8_t chan = 1; chan <= chans && slot < VTX_RC_PRESET_PAIRS; chan++) {
            if (!vtx3G3_GridFrequency(grid, band, chan)) {
                continue;
            }
            vtxRcMapEntry_t *entry = vtxRcMapEntriesMutable(slot);
            const uint16_t value = VTX_RC_SYNC_NOMINAL_MID + VTX_RC_PCT_TO_US(vtxRcPresetPercent(slot));
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
    vtxRcMapGeneration++;
    return true;
}

/*
 * Sync burst sent by the transmitter on the VTX map channel while disarmed:
 * header -100, +100, 0, -100, +100, 0 (each step 40..600 ms, 100-300 ms
 * recommended), then an optional VTX marker step (-85 + 10 x grid %), a 0 step
 * and an optional sweep -100, -90 ... +100 % (21 steps). The header teaches the
 * 3-point scale, the marker loads the default pairs of that grid, the sweep
 * teaches the transfer curve of the link (curvature, quantisation). A burst
 * without the sweep falls back to the 3-point scale. Nothing is written to the
 * EEPROM and the VTX is not retuned until the burst is over.
 */
#define VTX_RC_SYNC_STEPS            6
#define VTX_RC_SYNC_STEP_MIN_MS      40
#define VTX_RC_SYNC_STEP_MAX_MS      600
#define VTX_RC_SYNC_FLAT_US          12
#define VTX_RC_SYNC_REPEAT_US        15
#define VTX_RC_SYNC_MIN_SPAN_US      400
#define VTX_RC_SYNC_PHASE_WAIT_MS    (VTX_RC_SYNC_STEP_MAX_MS + 400)
#define VTX_RC_SYNC_MARKER_BASE      -85
#define VTX_RC_SYNC_MARKER_STEP      10
#define VTX_RC_SYNC_MARKER_TOL       3
#define VTX_RC_SYNC_SWEEP_END_US     40
#define VTX_RC_SYNC_SWEEP_RISE_US    20

typedef enum {
    VTX_RC_SYNC_IDLE = 0,
    VTX_RC_SYNC_MARKER,
    VTX_RC_SYNC_SWEEP,
} vtxRcSyncState_e;

static vtxRcSyncState_e syncState = VTX_RC_SYNC_IDLE;
static timeMs_t syncPhaseSinceMs;
static bool syncChanged;
static int syncMarkerGrid;
static uint16_t syncMarkerRaw;      // received marker step, decoded once the scale is final
static uint16_t syncSweep[VTX_RC_CURVE_POINTS];
static uint8_t syncSweepCount;
static uint16_t syncPrevLow, syncPrevMid, syncPrevHigh;   // restored when a burst is interrupted
static bool syncHold;
static vtxRcSyncStats_t syncStats;

const vtxRcSyncStats_t *vtxRcSyncStats(void)
{
    return &syncStats;
}

bool vtxRcSyncInProgress(void)
{
    return syncState != VTX_RC_SYNC_IDLE;
}

// From the first -100 step of a burst until the channel rests: the VTX keeps
// its pair and the curtain its state, even if the burst is never recognised
bool vtxRcSyncHold(void)
{
    return syncHold || syncState != VTX_RC_SYNC_IDLE;
}

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

// Decoded on the new curve when the burst had a sweep, else on the 3-point scale
static int vtxRcSyncMarkerGrid(uint16_t value)
{
    const int delta = vtxRcSyncNormalize(value) - VTX_RC_SYNC_NOMINAL_MID;
    const int percent = (delta * 100 + (delta < 0 ? -256 : 256)) / 512;
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

/*
 * The map follows the VTX in use: when the effective grid (detected, chosen in
 * Configuration or by the transmitter marker) is not the one the map was filled
 * for, or the map is empty, its default pairs are loaded and saved once, while
 * disarmed. A hand-made map (vtx_rc_preset = NONE) is left alone.
 */
static void vtxRcMapFollowGrid(void)
{
    static uint8_t followedGrid = VTX_3G3_GRID_COUNT;

    if (ARMING_FLAG(ARMED) || vtxSettingsConfig()->frequencyGroup != FREQUENCYGROUP_3G3 ||
        vtx3G3_DetectSource() == VTX_3G3_DETECT_NONE) {
        return;
    }
    const uint8_t grid = vtx3G3_EffectiveGrid();
    const uint8_t preset = vtxCurtainConfig()->preset;
    const bool empty = vtxRcMapIsEmpty();
    if (grid == followedGrid || (!empty && (preset == VTX_RC_PRESET_NONE || preset == grid))) {
        followedGrid = grid;
        return;
    }
    followedGrid = grid;
    uint8_t rcChannel = vtxRcSyncChannel();
    if (!rcChannel) {
        if (!vtxCurtainConfig()->rcSync) {
            return;
        }
        rcChannel = VTX_RC_PRESET_DEFAULT_CHANNEL;
    }
    if (vtxRcPresetApply(grid, rcChannel)) {
        syncStats.followed++;
        LOG_INFO(VTX, "VTX map follows grid %u on CH%u", grid, rcChannel);
        saveConfigAndNotify();
    }
}

static void vtxRcSyncMarker(int markerGrid, uint8_t rcChannel)
{
    syncMarkerGrid = markerGrid;
    // The grid chosen in Configuration may differ from the last marker:
    // the transmitter profile always wins
    if (markerGrid >= 0 && (markerGrid != vtxCurtainConfig()->preset ||
                            markerGrid != vtx3G3_EffectiveGrid() || vtxRcMapIsEmpty())) {
        syncChanged |= vtxRcPresetApply(markerGrid, rcChannel);
    }
}

static void vtxRcSyncFinish(bool sweepComplete, uint8_t rcChannel)
{
    vtxRcCurveConfig_t *curve = vtxRcCurveConfigMutable();
    if (sweepComplete) {
        syncChanged |= memcmp(curve->point, syncSweep, sizeof(syncSweep)) != 0;
        memcpy(curve->point, syncSweep, sizeof(syncSweep));
    } else if (vtxRcCurveValid()) {
        // A curve learned on another link must not distort the new 3-point scale
        memset(curve, 0, sizeof(*curve));
        syncChanged = true;
    }
    if (syncMarkerRaw) {
        vtxRcSyncMarker(vtxRcSyncMarkerGrid(syncMarkerRaw), rcChannel);
    }

    syncState = VTX_RC_SYNC_IDLE;
    vtxRcMapGeneration++;
    syncStats.completed++;
    syncStats.lastSweep = sweepComplete ? syncSweepCount : 0;
    syncStats.lastMarker = syncMarkerGrid;

    if (syncChanged) {
        saveConfigAndNotify();
    } else {
        beeper(BEEPER_ACTION_SUCCESS);
    }
    LOG_INFO(VTX, "RC sync on CH%u: %u/%u/%u us, VTX %d, curve %u", rcChannel,
        vtxCurtainConfig()->rcSyncLow, vtxCurtainConfig()->rcSyncMid, vtxCurtainConfig()->rcSyncHigh,
        syncMarkerGrid, vtxRcCurveValid());
}

static bool vtxRcSyncNear(uint16_t value, uint16_t reference)
{
    return ABS((int)value - (int)reference) <= VTX_RC_SYNC_SWEEP_END_US;
}

static void vtxRcSyncSweepStep(uint16_t held, uint8_t rcChannel, timeMs_t nowMs)
{
    const vtxCurtainConfig_t *cfg = vtxCurtainConfig();

    if (syncSweepCount == 0) {
        if (vtxRcSyncNear(held, cfg->rcSyncMid)) {
            syncPhaseSinceMs = nowMs;   // 0 step between the marker and the sweep
            return;
        }
        if (!vtxRcSyncNear(held, cfg->rcSyncLow)) {
            vtxRcSyncFinish(false, rcChannel);
            return;
        }
    } else if (held < syncSweep[syncSweepCount - 1] + VTX_RC_SYNC_SWEEP_RISE_US) {
        vtxRcSyncFinish(false, rcChannel);
        return;
    }

    syncSweep[syncSweepCount++] = held;
    syncPhaseSinceMs = nowMs;
    if (syncSweepCount == VTX_RC_CURVE_POINTS) {
        vtxRcSyncFinish(vtxRcSyncNear(held, cfg->rcSyncHigh) &&
                        vtxRcSyncNear(syncSweep[VTX_RC_CURVE_POINTS / 2], cfg->rcSyncMid), rcChannel);
    }
}

void vtxRcSyncUpdate(void)
{
    static uint16_t step[VTX_RC_SYNC_STEPS];
    static uint8_t stepCount = 0;
    static int flatRef = -1;
    static uint32_t flatSum = 0;
    static uint16_t flatSamples = 0;
    static timeMs_t flatSinceMs = 0;

    const uint8_t rcChannel = vtxRcSyncChannel();
    if (!vtxCurtainConfig()->rcSync || !rcChannel || ARMING_FLAG(ARMED)) {
        if (syncState != VTX_RC_SYNC_IDLE) {
            // Interrupted burst: drop the scale learned in RAM, the map is untouched
            vtxCurtainConfig_t *cfg = vtxCurtainConfigMutable();
            cfg->rcSyncLow = syncPrevLow;
            cfg->rcSyncMid = syncPrevMid;
            cfg->rcSyncHigh = syncPrevHigh;
            syncState = VTX_RC_SYNC_IDLE;
            vtxRcMapGeneration++;
        }
        syncHold = false;
        stepCount = 0;
        flatRef = -1;
        return;
    }

    const int value = rxGetChannelValue(rcChannel - 1);
    const timeMs_t nowMs = millis();

    {
        static int holdRef;
        static timeMs_t holdSinceMs;
        const vtxCurtainConfig_t *cfg = vtxCurtainConfig();
        bool curtain;
        // -100 starts every burst: near the stored -100, or below the lowest pair
        // when the link (and its scale) changed since the last sync
        uint16_t spanLow = 0, spanHigh = 0;
        const int mapped = rcMapChannelValue(rcChannel);
        const bool belowPairs = vtxRcMapChannelSpan(rcChannel, &spanLow, &spanHigh) &&
                                mapped < (int)spanLow - VTX_RC_SYNC_FLAT_US;
        if (!syncHold && syncState == VTX_RC_SYNC_IDLE &&
            (vtxRcSyncNear(value, cfg->rcSyncLow) || belowPairs) &&
            vtxRcMapLookup(rcChannel, mapped, &curtain) < 0) {
            syncHold = true;
            holdRef = value;
            holdSinceMs = nowMs;
        }
        if (syncHold) {
            if (ABS(value - holdRef) > VTX_RC_SYNC_FLAT_US) {
                holdRef = value;
                holdSinceMs = nowMs;
            } else if (syncState == VTX_RC_SYNC_IDLE && nowMs - holdSinceMs > VTX_RC_SYNC_PHASE_WAIT_MS) {
                syncHold = false;
            }
        }
    }

    if (syncState == VTX_RC_SYNC_MARKER && nowMs - syncPhaseSinceMs > VTX_RC_SYNC_PHASE_WAIT_MS) {
        syncState = VTX_RC_SYNC_SWEEP;
        syncPhaseSinceMs = nowMs;
    }
    if (syncState == VTX_RC_SYNC_SWEEP && nowMs - syncPhaseSinceMs > VTX_RC_SYNC_PHASE_WAIT_MS) {
        // The transmitter may stay on +100 after the last sweep step
        if (syncSweepCount == VTX_RC_CURVE_POINTS - 1 && flatRef >= 0 && flatSamples &&
            nowMs - flatSinceMs >= VTX_RC_SYNC_STEP_MIN_MS) {
            vtxRcSyncSweepStep((flatSum + flatSamples / 2) / flatSamples, rcChannel, nowMs);
        }
        if (syncState == VTX_RC_SYNC_SWEEP) {
            vtxRcSyncFinish(false, rcChannel);
        }
        stepCount = 0;
    }

    if (flatRef >= 0 && ABS(value - flatRef) <= VTX_RC_SYNC_FLAT_US) {
        flatSum += value;
        flatSamples++;
        // Resolve a phase on a step that is still held, so a burst without a
        // sweep (or a marker) is saved as soon as the transmitter is back on a pair
        const timeMs_t heldMs = nowMs - flatSinceMs;
        const uint16_t held = (flatSum + flatSamples / 2) / flatSamples;
        if (syncState == VTX_RC_SYNC_MARKER && heldMs > VTX_RC_SYNC_STEP_MAX_MS) {
            syncState = VTX_RC_SYNC_SWEEP;
            syncPhaseSinceMs = nowMs;
        }
        if (syncState == VTX_RC_SYNC_SWEEP && heldMs >= VTX_RC_SYNC_STEP_MIN_MS) {
            const bool sweepValue = syncSweepCount ? held >= syncSweep[syncSweepCount - 1] + VTX_RC_SYNC_SWEEP_RISE_US
                                                   : vtxRcSyncNear(held, vtxCurtainConfig()->rcSyncLow) ||
                                                     vtxRcSyncNear(held, vtxCurtainConfig()->rcSyncMid);
            if (heldMs > VTX_RC_SYNC_STEP_MAX_MS && syncSweepCount == VTX_RC_CURVE_POINTS - 1 && sweepValue) {
                vtxRcSyncSweepStep(held, rcChannel, nowMs);   // stays on +100 after the sweep
            } else if (!sweepValue || heldMs > VTX_RC_SYNC_STEP_MAX_MS) {
                vtxRcSyncFinish(false, rcChannel);
            }
            if (syncState == VTX_RC_SYNC_IDLE) {
                stepCount = 0;
                flatRef = -1;
            }
        }
        return;
    }

    if (flatRef >= 0) {
        const timeMs_t heldMs = nowMs - flatSinceMs;
        const uint16_t held = (flatSum + flatSamples / 2) / flatSamples;
        // A step shorter than the minimum is a frame caught in transit between two steps
        if (heldMs >= VTX_RC_SYNC_STEP_MIN_MS) {
            const bool tooLong = heldMs > VTX_RC_SYNC_STEP_MAX_MS;
            switch (syncState) {
            case VTX_RC_SYNC_IDLE:
                if (tooLong) {
                    stepCount = 0;
                } else {
                    if (stepCount == VTX_RC_SYNC_STEPS) {
                        memmove(&step[0], &step[1], sizeof(step[0]) * (VTX_RC_SYNC_STEPS - 1));
                        stepCount--;
                    }
                    step[stepCount++] = held;
                    syncStats.step[syncStats.stepIndex++ % VTX_RC_SYNC_STATS_STEPS] = held;
                }
                break;
            case VTX_RC_SYNC_MARKER:
                syncState = VTX_RC_SYNC_SWEEP;
                syncPhaseSinceMs = nowMs;
                if (tooLong) {
                    break;
                }
                if (vtxRcSyncNear(held, vtxCurtainConfig()->rcSyncLow)) {
                    // No marker: the first sweep step follows the header directly
                    vtxRcSyncSweepStep(held, rcChannel, nowMs);
                } else if (ABS((int)held - (int)vtxCurtainConfig()->rcSyncMid) > VTX_RC_SYNC_FLAT_US) {
                    syncMarkerRaw = held;
                }
                break;
            case VTX_RC_SYNC_SWEEP:
                if (tooLong) {
                    vtxRcSyncFinish(false, rcChannel);
                } else {
                    vtxRcSyncSweepStep(held, rcChannel, nowMs);
                }
                break;
            }
        }
    }

    flatRef = value;
    flatSum = value;
    flatSamples = 1;
    flatSinceMs = nowMs;

    uint16_t low, mid, high;
    if (syncState == VTX_RC_SYNC_IDLE && stepCount == VTX_RC_SYNC_STEPS && vtxRcSyncPatternFound(step, &low, &mid, &high)) {
        stepCount = 0;
        vtxCurtainConfig_t *cfg = vtxCurtainConfigMutable();
        syncPrevLow = cfg->rcSyncLow;
        syncPrevMid = cfg->rcSyncMid;
        syncPrevHigh = cfg->rcSyncHigh;
        syncChanged = cfg->rcSyncLow != low || cfg->rcSyncMid != mid || cfg->rcSyncHigh != high;
        cfg->rcSyncLow = low;
        cfg->rcSyncMid = mid;
        cfg->rcSyncHigh = high;
        syncMarkerGrid = -1;
        syncMarkerRaw = 0;
        syncSweepCount = 0;
        syncState = VTX_RC_SYNC_MARKER;
        syncPhaseSinceMs = nowMs;
        syncStats.detected++;
    }
}
