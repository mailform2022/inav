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

/* Created by jflyper */

#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>

#include "platform.h"
#include "build/debug.h"
#include "common/utils.h"

#include "config/parameter_group.h"
#include "drivers/vtx_common.h"
#include "io/vtx.h"
#include "io/vtx_control.h"
#include "io/vtx_string.h"

#define VTX_STRING_5G8_BAND_COUNT  5
#define VTX_STRING_5G8_CHAN_COUNT  8
#define VTX_STRING_5G8_POWER_COUNT 5

#define VTX_STRING_1G3_BAND_COUNT  2
#define VTX_STRING_1G3_CHAN_COUNT  8
#define VTX_STRING_1G3_POWER_COUNT 3

#define VTX_STRING_3G3_BAND_COUNT  5
#define VTX_STRING_3G3_CHAN_COUNT   8
#define VTX_STRING_3G3_POWER_COUNT 3

#define VTX_STRING_3G3_TX3339_BAND_COUNT 4

#define VTX_STRING_3G3_NONAME1_BAND_COUNT  2
#define VTX_STRING_3G3_NONAME1_POWER_COUNT 4

#define VTX_STRING_3G3_FF37_BAND_COUNT 1
#define VTX_STRING_3G3_FF37_CHAN_COUNT 20

const uint16_t vtx58frequencyTable[VTX_STRING_5G8_BAND_COUNT][VTX_STRING_5G8_CHAN_COUNT] =
{
    { 5865, 5845, 5825, 5805, 5785, 5765, 5745, 5725 }, // A
    { 5733, 5752, 5771, 5790, 5809, 5828, 5847, 5866 }, // B
    { 5705, 5685, 5665, 5645, 5885, 5905, 5925, 5945 }, // E
    { 5740, 5760, 5780, 5800, 5820, 5840, 5860, 5880 }, // F
    { 5658, 5695, 5732, 5769, 5806, 5843, 5880, 5917 }, // R
};

const char * const vtx58BandNames[VTX_STRING_5G8_BAND_COUNT + 1] = {
    "-",
    "A",
    "B",
    "E",
    "F",
    "R",
};

const char vtx58BandLetter[VTX_STRING_5G8_BAND_COUNT + 1] = {'-', 'A', 'B', 'E', 'F', 'R'};

const char * const vtx58ChannelNames[VTX_STRING_5G8_CHAN_COUNT + 1] = {
    "-", "1", "2", "3", "4", "5", "6", "7", "8",
};

const char * const vtx58DefaultPowerNames[VTX_STRING_5G8_POWER_COUNT + 1] = {
    "---", "PL1", "PL2", "PL3", "PL4", "PL5"
};

const uint16_t vtx1G3frequencyTable[VTX_STRING_1G3_BAND_COUNT][VTX_STRING_1G3_CHAN_COUNT] =
{
    { 1080, 1120, 1160, 1200, 1240, 1280, 1320, 1360 }, // A
    { 1080, 1120, 1160, 1200, 1258, 1280, 1320, 1360 }, // B
};

const char * const vtx1G3BandNames[VTX_STRING_1G3_BAND_COUNT + 1] = {
    "-",
    "A",
    "B",
};

const char vtx1G3BandLetter[VTX_STRING_1G3_BAND_COUNT + 1] = {'-', 'A', 'B'};

const char * const vtx1G3ChannelNames[VTX_STRING_1G3_CHAN_COUNT + 1] = {
    "-", "1", "2", "3", "4", "5", "6", "7", "8",
};

const char * const vtx1G3DefaultPowerNames[VTX_STRING_1G3_POWER_COUNT + 1] = {
    "---", "PL1", "PL2", "PL3"
};

bool vtx58_Freq2Bandchan(uint16_t freq, uint8_t *pBand, uint8_t *pChannel)
{
    int8_t band;
    uint8_t channel;

    // Use reverse lookup order so that 5880Mhz
    // get Raceband 7 instead of Fatshark 8.
    for (band = 4 ; band >= 0 ; band--) {
        for (channel = 0 ; channel < 8 ; channel++) {
            if (vtx58frequencyTable[band][channel] == freq) {
                *pBand = band + 1;
                *pChannel = channel + 1;
                return true;
            }
        }
    }

    *pBand = 0;
    *pChannel = 0;

    return false;
}

// Converts band and channel values to a frequency (in MHz) value.
// band: Band value (1 to 5).
// channel:  Channel value (1 to 8).
// Returns frequency value (in MHz), or 0 if band/channel out of range.
uint16_t vtx58_Bandchan2Freq(uint8_t band, uint8_t channel)
{
    if (band > 0 && band <= VTX_STRING_5G8_BAND_COUNT &&
                          channel > 0 && channel <= VTX_STRING_5G8_CHAN_COUNT) {
        return vtx58frequencyTable[band - 1][channel - 1];
    }
    return 0;
}

// Converts band and channel values to a frequency (in MHz) value.
// band: Band value (1 to 2).
// channel:  Channel value (1 to 8).
// Returns frequency value (in MHz), or 0 if band/channel out of range.
uint16_t vtx1G3_Bandchan2Freq(uint8_t band, uint8_t channel)
{
    if (band > 0 && band <= VTX_STRING_1G3_BAND_COUNT &&
                          channel > 0 && channel <= VTX_STRING_1G3_CHAN_COUNT) {
        return vtx1G3frequencyTable[band - 1][channel - 1];
    }
    return 0;
}

// 3.3 GHz SX33 IRC Tramp frequency table
const uint16_t vtx3G3frequencyTable[VTX_STRING_3G3_BAND_COUNT][VTX_STRING_3G3_CHAN_COUNT] =
{
    { 3200, 3220, 3240, 3260, 3280, 3300, 3320, 3340 }, // A
    { 3360, 3380, 3400, 3420, 3440, 3460, 3480, 3500 }, // B
    { 3520, 3540, 3560, 3580, 3600, 3620, 3640, 3680 }, // C
    { 3210, 3250, 3290, 3330, 3370, 3410, 3450, 3490 }, // D
    { 3230, 3290, 3350, 3410, 3470, 3530, 3590, 3700 }, // E
};

const char * const vtx3G3BandNames[VTX_STRING_3G3_BAND_COUNT + 1] = {
    "-",
    "A",
    "B",
    "C",
    "D",
    "E",
};

const char vtx3G3BandLetter[VTX_STRING_3G3_BAND_COUNT + 1] = {'-', 'A', 'B', 'C', 'D', 'E'};

const char * const vtx3G3ChannelNames[VTX_STRING_3G3_CHAN_COUNT + 1] = {
    "-", "1", "2", "3", "4", "5", "6", "7", "8",
};

const char * const vtx3G3DefaultPowerNames[VTX_STRING_3G3_POWER_COUNT + 1] = {
    "---", "25 ", "2W ", "5W "
};

static const uint16_t vtx3G3PowerLevels[VTX_STRING_3G3_POWER_COUNT] = { 25, 200, 5000 };
static const uint8_t vtx3G3PowerDbm[VTX_STRING_3G3_POWER_COUNT] = { 14, 33, 37 };

// 3.3 GHz BeastFPV TX3339-32CH IRC Tramp frequency table. Unlike the SX33 this
// VTX has only four grids (FR1..FR4) and its range is 3060-3480 MHz, so several
// of its channels sit below the SX33 lower limit.
const uint16_t vtx3G3frequencyTableTx3339[VTX_STRING_3G3_TX3339_BAND_COUNT][VTX_STRING_3G3_CHAN_COUNT] =
{
    { 3330, 3350, 3370, 3390, 3410, 3430, 3450, 3470 }, // A (FR1)
    { 3340, 3360, 3380, 3400, 3420, 3440, 3460, 3480 }, // B (FR2)
    { 3170, 3190, 3210, 3230, 3250, 3270, 3290, 3310 }, // C (FR3)
    { 3060, 3080, 3100, 3120, 3140, 3160, 3180, 3200 }, // D (FR4)
};

const char * const vtx3G3Tx3339PowerNames[VTX_STRING_3G3_POWER_COUNT + 1] = {
    "---", "25 ", "3W ", "10W"
};

static const uint16_t vtx3G3Tx3339PowerLevels[VTX_STRING_3G3_POWER_COUNT] = { 25, 3000, 10000 };
static const uint8_t vtx3G3Tx3339PowerDbm[VTX_STRING_3G3_POWER_COUNT] = { 14, 35, 40 };

// "Noname_1": 3 W IRC Tramp unit, 16 channels in two rows of eight covering
// 3200-3500 MHz. Its two rows are the SX33 A and B bands, but it has four power
// levels and stops at 3500 MHz, so it gets its own grid rather than sharing.
const uint16_t vtx3G3frequencyTableNoname1[VTX_STRING_3G3_NONAME1_BAND_COUNT][VTX_STRING_3G3_CHAN_COUNT] =
{
    { 3200, 3220, 3240, 3260, 3280, 3300, 3320, 3340 }, // A (row 1)
    { 3360, 3380, 3400, 3420, 3440, 3460, 3480, 3500 }, // B (row 2)
};

const char * const vtx3G3Noname1PowerNames[VTX_STRING_3G3_NONAME1_POWER_COUNT + 1] = {
    "---", "25 ", "400", "1W ", "3W "
};

static const uint16_t vtx3G3Noname1PowerLevels[VTX_STRING_3G3_NONAME1_POWER_COUNT] = { 25, 400, 1000, 3000 };
static const uint8_t vtx3G3Noname1PowerDbm[VTX_STRING_3G3_NONAME1_POWER_COUNT] = { 14, 26, 30, 35 };

// "FF3.7": FF3741-alike SmartAudio unit, but a single flat row of 20 channels
// from 3700 to 4080 MHz in 20 MHz steps. Power levels and protocol match the
// FF3741, so it reuses the SX33 power grid.
const uint16_t vtx3G3frequencyTableFf37[VTX_STRING_3G3_FF37_BAND_COUNT][VTX_STRING_3G3_FF37_CHAN_COUNT] =
{
    { 3700, 3720, 3740, 3760, 3780, 3800, 3820, 3840, 3860, 3880,
      3900, 3920, 3940, 3960, 3980, 4000, 4020, 4040, 4060, 4080 },
};

// The device labels its last four channels with non-ASCII glyphs; the OSD font
// has no such characters, so they are spelled out as G/U/P/H in device order.
const char * const vtx3G3Ff37ChannelNames[VTX_STRING_3G3_FF37_CHAN_COUNT + 1] = {
    "-", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    "A", "B", "C", "D", "E", "F", "G", "U", "P", "H",
};

static const char * const vtx3G3Ff37BandNames[VTX_STRING_3G3_FF37_BAND_COUNT + 1] = {
    "-",
    "A",
};

typedef struct vtx3G3GridInfo_s {
    const char *        name;
    bool                trampMilliwatt;  // device takes real mW, not SX33 scale codes
    const uint16_t *    freq;         // bandCount rows of chanCount entries
    const char * const *bandNames;    // [bandCount + 1], index 0 is "-"
    const char * const *chanNames;    // [chanCount + 1], index 0 is "-"
    const char * const *powerNames;   // [powerCount + 1], index 0 is "---"
    const uint16_t *    powerLevels;  // [powerCount], real mW
    const uint8_t *     powerDbm;     // [powerCount], for SmartAudio V2.1
    uint8_t             bandCount;
    uint8_t             chanCount;
    uint8_t             powerCount;
    uint16_t            freqMin;
    uint16_t            freqMax;
} vtx3G3GridInfo_t;

static const vtx3G3GridInfo_t vtx3G3Grids[] = {
    [VTX_3G3_GRID_SX33] = {
        .name = "SX33", .freq = &vtx3G3frequencyTable[0][0],
        .bandNames = vtx3G3BandNames, .chanNames = vtx3G3ChannelNames,
        .powerNames = vtx3G3DefaultPowerNames,
        .powerLevels = vtx3G3PowerLevels, .powerDbm = vtx3G3PowerDbm,
        .bandCount = VTX_STRING_3G3_BAND_COUNT, .chanCount = VTX_STRING_3G3_CHAN_COUNT,
        .powerCount = VTX_STRING_3G3_POWER_COUNT,
        .freqMin = 3200, .freqMax = 3700,
    },
    [VTX_3G3_GRID_TX3339] = {
        .name = "TX3339", .freq = &vtx3G3frequencyTableTx3339[0][0],
        .bandNames = vtx3G3BandNames, .chanNames = vtx3G3ChannelNames,
        .powerNames = vtx3G3Tx3339PowerNames,
        .powerLevels = vtx3G3Tx3339PowerLevels, .powerDbm = vtx3G3Tx3339PowerDbm,
        .bandCount = VTX_STRING_3G3_TX3339_BAND_COUNT, .chanCount = VTX_STRING_3G3_CHAN_COUNT,
        .powerCount = VTX_STRING_3G3_POWER_COUNT,
        .freqMin = 3060, .freqMax = 3480,
    },
    [VTX_3G3_GRID_NONAME1] = {
        .name = "Noname_1", .trampMilliwatt = true,
        .freq = &vtx3G3frequencyTableNoname1[0][0],
        .bandNames = vtx3G3BandNames, .chanNames = vtx3G3ChannelNames,
        .powerNames = vtx3G3Noname1PowerNames,
        .powerLevels = vtx3G3Noname1PowerLevels, .powerDbm = vtx3G3Noname1PowerDbm,
        .bandCount = VTX_STRING_3G3_NONAME1_BAND_COUNT, .chanCount = VTX_STRING_3G3_CHAN_COUNT,
        .powerCount = VTX_STRING_3G3_NONAME1_POWER_COUNT,
        .freqMin = 3200, .freqMax = 3500,
    },
    [VTX_3G3_GRID_FF37] = {
        .name = "FF3.7", .freq = &vtx3G3frequencyTableFf37[0][0],
        .bandNames = vtx3G3Ff37BandNames, .chanNames = vtx3G3Ff37ChannelNames,
        .powerNames = vtx3G3DefaultPowerNames,
        .powerLevels = vtx3G3PowerLevels, .powerDbm = vtx3G3PowerDbm,
        .bandCount = VTX_STRING_3G3_FF37_BAND_COUNT, .chanCount = VTX_STRING_3G3_FF37_CHAN_COUNT,
        .powerCount = VTX_STRING_3G3_POWER_COUNT,
        .freqMin = 3700, .freqMax = 4080,
    },
};

/* What the attached VTX said about itself, kept raw so a wrong guess can be traced
 * back to the device's own answer instead of to the classifier. */
static vtx3G3DeviceReport_t vtx3G3Report = {
    .grid = VTX_3G3_GRID_SX33,
    .detect = VTX_3G3_DETECT_NONE,
};

/* Classification describes a 3.3GHz device, so a VTX answering while another
 * frequency group is selected must not leave a verdict behind for the 3G3 grid. */
static bool vtx3G3_GroupSelected(void)
{
#if defined(USE_VTX_COMMON)
    return vtxSettingsConfig()->frequencyGroup == FREQUENCYGROUP_3G3;
#else
    return false;
#endif
}

/* Recognising a 3.3GHz VTX from its IRC Tramp capability answer only works for
 * devices whose answer is actually distinctive:
 *   TX3339-32CH reports its real 3060-3480 MHz range and a four-figure max power;
 *   Noname_1 reports a wide 850-5999 MHz range with a ~1600 mW maximum;
 *   SX33 reports a generic 5100-6000 MHz / ~600 mW answer no matter what it is
 *   tuned to, which is indistinguishable from a plain 5.8GHz clone.
 * Anything else is not guessed at - SX33 stays the fallback, because that is the
 * grid this firmware has always used for the 3G3 group. */
void vtx3G3_ReportTrampCapabilities(uint16_t freqMin, uint16_t freqMax, uint16_t powerMax)
{
    if (!vtx3G3_GroupSelected()) {
        return;
    }

    vtx3G3Report.freqMin = freqMin;
    vtx3G3Report.freqMax = freqMax;
    vtx3G3Report.powerMax = powerMax;

    const bool sane = (freqMin != 0) && (freqMin < freqMax);

    if (sane && freqMin <= 3100 && freqMax >= 3450 && freqMax <= 3600) {
        vtx3G3Report.grid = VTX_3G3_GRID_TX3339;
        vtx3G3Report.detect = VTX_3G3_DETECT_RANGE;
    } else if (sane && freqMin >= 3150 && freqMin <= 3250 && freqMax >= 3650) {
        vtx3G3Report.grid = VTX_3G3_GRID_SX33;
        vtx3G3Report.detect = VTX_3G3_DETECT_RANGE;
    } else if (sane && freqMin <= 1000 && powerMax >= 1200 && powerMax <= 2500) {
        // Measured Noname_1 answer: 850-5999 MHz / 1600 mW. Neither the SX33 nor
        // the TX3339 claims a sub-GHz lower limit, so this is unambiguous.
        vtx3G3Report.grid = VTX_3G3_GRID_NONAME1;
        vtx3G3Report.detect = VTX_3G3_DETECT_RANGE;
    } else if (powerMax >= 3000) {
        // Only the 10W unit claims this much; the SX33's generic answer is ~600 mW.
        vtx3G3Report.grid = VTX_3G3_GRID_TX3339;
        vtx3G3Report.detect = VTX_3G3_DETECT_POWER;
    } else {
        vtx3G3Report.grid = VTX_3G3_GRID_SX33;
        vtx3G3Report.detect = VTX_3G3_DETECT_FALLBACK;
    }
}

/* SmartAudio on the 3.3GHz group is either an FF3741 (SX33 grid) or an FF3.7,
 * and the two answer identically, so the protocol only rules out the IRC Tramp
 * devices. SX33 is reported because it is the long-standing default; an FF3.7
 * has to be selected by hand. */
void vtx3G3_ReportSmartAudioDevice(void)
{
    if (!vtx3G3_GroupSelected()) {
        return;
    }

    vtx3G3Report.grid = VTX_3G3_GRID_SX33;
    vtx3G3Report.detect = VTX_3G3_DETECT_PROTOCOL;
}

const vtx3G3DeviceReport_t * vtx3G3_DeviceReport(void)
{
    return &vtx3G3Report;
}

uint8_t vtx3G3_DetectSource(void)
{
#if defined(USE_VTX_CONTROL)
    if (vtxConfig()->vtx3g3Grid == VTX_3G3_GRID_AUTO) {
        return vtx3G3Report.detect;
    }
#endif
    return VTX_3G3_DETECT_FORCED;
}

uint8_t vtx3G3_EffectiveGrid(void)
{
#if defined(USE_VTX_CONTROL)
    const uint8_t configured = vtxConfig()->vtx3g3Grid;

    if (configured == VTX_3G3_GRID_AUTO) {
        return vtx3G3Report.grid;
    }
    if (configured < VTX_3G3_GRID_COUNT) {
        return configured;
    }
#endif
    return VTX_3G3_GRID_SX33;
}

static const vtx3G3GridInfo_t * vtx3G3_Grid(void)
{
    const uint8_t grid = vtx3G3_EffectiveGrid();

    // AUTO never survives vtx3G3_EffectiveGrid(), but the enum value sits inside
    // the array bounds with no table row behind it.
    if (grid >= ARRAYLEN(vtx3G3Grids) || vtx3G3Grids[grid].freq == NULL) {
        return &vtx3G3Grids[VTX_3G3_GRID_SX33];
    }
    return &vtx3G3Grids[grid];
}

bool vtx3G3_GridIsTx3339(void)
{
    return vtx3G3_EffectiveGrid() == VTX_3G3_GRID_TX3339;
}

const char * vtx3G3_GridName(void)
{
    return vtx3G3_Grid()->name;
}

bool vtx3G3_TrampPowerIsMilliwatt(void)
{
    return vtx3G3_Grid()->trampMilliwatt;
}

uint8_t vtx3G3_BandCount(void)
{
    return vtx3G3_Grid()->bandCount;
}

uint8_t vtx3G3_ChannelCount(void)
{
    return vtx3G3_Grid()->chanCount;
}

uint8_t vtx3G3_PowerCount(void)
{
    return vtx3G3_Grid()->powerCount;
}

const char * const * vtx3G3_BandNames(void)
{
    return vtx3G3_Grid()->bandNames;
}

const char * const * vtx3G3_ChannelNames(void)
{
    return vtx3G3_Grid()->chanNames;
}

const char * const * vtx3G3_PowerNames(void)
{
    return vtx3G3_Grid()->powerNames;
}

const uint16_t * vtx3G3_PowerLevels(void)
{
    return vtx3G3_Grid()->powerLevels;
}

uint8_t vtx3G3_PowerDbm(uint8_t index)
{
    const vtx3G3GridInfo_t *grid = vtx3G3_Grid();

    if (index == 0 || index > grid->powerCount) {
        return grid->powerDbm[0];
    }
    return grid->powerDbm[index - 1];
}

uint16_t vtx3G3_FreqMin(void)
{
    return vtx3G3_Grid()->freqMin;
}

uint16_t vtx3G3_FreqMax(void)
{
    return vtx3G3_Grid()->freqMax;
}

uint16_t vtx3G3_MaxPowerMw(void)
{
    const vtx3G3GridInfo_t *grid = vtx3G3_Grid();
    return grid->powerLevels[grid->powerCount - 1];
}

uint16_t vtx3G3_Bandchan2Freq(uint8_t band, uint8_t channel)
{
    const vtx3G3GridInfo_t *grid = vtx3G3_Grid();

    if (band == 0 || band > grid->bandCount ||
        channel == 0 || channel > grid->chanCount) {
        return 0;
    }

    return grid->freq[(band - 1) * grid->chanCount + (channel - 1)];
}
