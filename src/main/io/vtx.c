/*
 * This file is part of Cleanflight and Betaflight.
 *
 * Cleanflight and Betaflight are free software. You can redistribute
 * this software and/or modify this software under the terms of the
 * GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Cleanflight and Betaflight are distributed in the hope that they
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.
 *
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <string.h>

#include "platform.h"
#if defined(USE_VTX_CONTROL)

#include "common/maths.h"
#include "common/time.h"
#include "common/utils.h"

#include "config/parameter_group.h"
#include "config/parameter_group_ids.h"

#include "drivers/time.h"
#include "drivers/vtx_common.h"

#include "fc/cli.h"
#include "fc/config.h"
#include "fc/rc_modes.h"
#include "fc/runtime_config.h"
#include "fc/settings.h"

#include "flight/failsafe.h"

#include "io/serial.h"
#include "io/vtx.h"
#include "io/vtx_string.h"
#include "io/vtx_control.h"
#include "io/vtx_smartaudio.h"
#include "io/vtx_tramp.h"

PG_REGISTER_WITH_RESET_TEMPLATE(vtxSettingsConfig_t, vtxSettingsConfig, PG_VTX_SETTINGS_CONFIG, 3);

PG_RESET_TEMPLATE(vtxSettingsConfig_t, vtxSettingsConfig,
    .band = SETTING_VTX_BAND_DEFAULT,
    .channel = SETTING_VTX_CHANNEL_DEFAULT,
    .power = SETTING_VTX_POWER_DEFAULT,
    .pitModeChan = SETTING_VTX_PIT_MODE_CHAN_DEFAULT,
    .lowPowerDisarm = SETTING_VTX_LOW_POWER_DISARM_DEFAULT,
    .maxPowerOverride = SETTING_VTX_MAX_POWER_OVERRIDE_DEFAULT,
    .frequencyGroup = SETTING_VTX_FREQUENCY_GROUP_DEFAULT,
);

typedef enum {
    VTX_PARAM_POWER = 0,
    VTX_PARAM_BANDCHAN,
    VTX_PARAM_PITMODE,
    VTX_PARAM_COUNT
} vtxScheduleParams_e;

void vtxInit(void)
{
}

static vtxSettingsConfig_t * vtxGetRuntimeSettings(void)
{
    static vtxSettingsConfig_t settings;

    settings.band = vtxSettingsConfig()->band;
    settings.channel = vtxSettingsConfig()->channel;
    settings.power = vtxSettingsConfig()->power;
    settings.pitModeChan = vtxSettingsConfig()->pitModeChan;
    settings.lowPowerDisarm = vtxSettingsConfig()->lowPowerDisarm;

    if (!ARMING_FLAG(ARMED) && !failsafeIsActive() &&
        ((settings.lowPowerDisarm == VTX_LOW_POWER_DISARM_ALWAYS) ||
        (settings.lowPowerDisarm == VTX_LOW_POWER_DISARM_UNTIL_FIRST_ARM && !ARMING_FLAG(WAS_EVER_ARMED)))) {

        settings.power = VTX_SETTINGS_DEFAULT_POWER;
    }

    return &settings;
}

// The SX33 and the TX3339 do not report the pair they sit on, so a command sent
// for a passing value - or lost on the wire - is never corrected and leaves the
// VTX one channel off. Act on a pair that held still and repeat it after a change.
// The settle window only has to outlast a switch sweeping through intermediate
// positions; it is paid on every change, so it is kept as short as that allows.
// A pair held for longer than the window during a deliberately slow sweep is
// tuned, but the sweep still ends on the selected pair.
#define VTX_BANDCHAN_SETTLE_MS 250
#define VTX_BANDCHAN_REASSERT_MS 250

static uint8_t settledBand = 0;
static uint8_t settledChannel = 0;

static void vtxTrackBandAndChannel(const vtxSettingsConfig_t * runtimeSettings)
{
    static uint8_t candidateBand = 0;
    static uint8_t candidateChannel = 0;
    static timeMs_t candidateSinceMs = 0;

    const timeMs_t nowMs = millis();

    if (runtimeSettings->band != candidateBand || runtimeSettings->channel != candidateChannel) {
        candidateBand = runtimeSettings->band;
        candidateChannel = runtimeSettings->channel;
        candidateSinceMs = nowMs;
        return;
    }

    if ((nowMs - candidateSinceMs) >= VTX_BANDCHAN_SETTLE_MS) {
        settledBand = candidateBand;
        settledChannel = candidateChannel;
    }
}

static bool vtxProcessBandAndChannel(vtxDevice_t *vtxDevice)
{
    static uint8_t commandedBand = 0;
    static uint8_t commandedChannel = 0;
    static uint8_t reassertsLeft = 0;
    static timeMs_t lastSendMs = 0;

    uint8_t vtxBand;
    uint8_t vtxChan;

    // Shortcut for undefined band
    if (!settledBand) {
        return false;
    }

    if (!vtxCommonGetBandAndChannel(vtxDevice, &vtxBand, &vtxChan)) {
        return false;
    }

    const timeMs_t nowMs = millis();

    if (settledBand != commandedBand || settledChannel != commandedChannel) {
        commandedBand = settledBand;
        commandedChannel = settledChannel;
        reassertsLeft = vtxConfig()->vtx3g3ChanReassert;
    }

    if (vtxBand != settledBand || vtxChan != settledChannel) {
        lastSendMs = nowMs;
        vtxCommonSetBandAndChannel(vtxDevice, settledBand, settledChannel);
        return true;
    }

    // The device reports the pair we asked for - which on these clones is only
    // an echo of the request - so repeat the command to survive a lost frame.
    // A device that does report the frequency it is tuned to (TX3339) confirms
    // the change itself, so the repeats - and the extra video blanks they cause
    // - are dropped as soon as the read-back matches.
#if defined(USE_VTX_TRAMP)
    if (reassertsLeft && vtxCommonGetDeviceType(vtxDevice) == VTXDEV_TRAMP && vtxTrampFrequencyConfirmed()) {
        reassertsLeft = 0;
    }
#endif

    if (reassertsLeft && (nowMs - lastSendMs) >= VTX_BANDCHAN_REASSERT_MS) {
        reassertsLeft--;
        lastSendMs = nowMs;
        vtxCommonSetBandAndChannel(vtxDevice, settledBand, settledChannel);
        return true;
    }

    return false;
}

static bool vtxProcessPower(vtxDevice_t *vtxDevice, const vtxSettingsConfig_t * runtimeSettings)
{
    uint8_t vtxPower;

    if (!vtxCommonGetPowerIndex(vtxDevice, &vtxPower)) {
        return false;
    }

    if (vtxPower != runtimeSettings->power) {
        vtxCommonSetPowerByIndex(vtxDevice, runtimeSettings->power);
        return true;
    }

    return false;
}

static bool vtxProcessPitMode(vtxDevice_t *vtxDevice, const vtxSettingsConfig_t * runtimeSettings)
{
    UNUSED(runtimeSettings);

    uint8_t pitOnOff;

    bool        currPmSwitchState = false;
    static bool prevPmSwitchState = false;

    if (!vtxCommonGetPitMode(vtxDevice, &pitOnOff)) {
        return false;
    }

    if (currPmSwitchState != prevPmSwitchState) {
        prevPmSwitchState = currPmSwitchState;

        if (currPmSwitchState) {
            if (0) {
                if (!pitOnOff) {
                    vtxCommonSetPitMode(vtxDevice, true);
                    return true;
                }
            }
        } else {
            if (pitOnOff) {
                vtxCommonSetPitMode(vtxDevice, false);
                return true;
            }
        }
    }

    return false;
}

#if defined(USE_VTX_SMARTAUDIO) && defined(USE_VTX_TRAMP)
// When a serial port is assigned the FUNCTION_VTX_AUTO function, the flight
// controller probes both protocols on that single UART and keeps the one the
// connected VTX answers. fc_init opens SmartAudio first, so detection starts
// there; if the device stays silent it is handed over to Tramp and back until
// one responds, after which the protocol is locked in.
#define VTX_AUTO_DETECT_TIMEOUT_MS 2500

static void vtxAutoDetectUpdate(void)
{
    static bool initialized = false;
    static bool autoMode = false;
    static bool locked = false;
    static bool probingTramp = false;   // false => SmartAudio is probing
    static timeMs_t lastSwitchMs = 0;

    if (!initialized) {
        initialized = true;
        autoMode = (findSerialPortConfig(FUNCTION_VTX_AUTO) != NULL);
        lastSwitchMs = millis();
    }

    if (!autoMode || locked) {
        return;
    }

    vtxDevice_t *vtxDevice = vtxCommonDevice();
    if (vtxDevice && vtxCommonDeviceIsReady(vtxDevice)) {
        locked = true;  // the VTX answered on this protocol; keep it
        return;
    }

    if ((millis() - lastSwitchMs) < VTX_AUTO_DETECT_TIMEOUT_MS) {
        return;
    }
    lastSwitchMs = millis();

    if (probingTramp) {
        vtxTrampDeinit();
        vtxSmartAudioInit();
        probingTramp = false;
    } else {
        vtxSmartAudioDeinit();
        vtxTrampInit();
        probingTramp = true;
    }
}
#endif

void vtxUpdate(timeUs_t currentTimeUs)
{
    static uint8_t currentSchedule = 0;

    if (cliMode) {
        return;
    }

#if defined(USE_VTX_SMARTAUDIO) && defined(USE_VTX_TRAMP)
    vtxAutoDetectUpdate();
#endif

    vtxDevice_t *vtxDevice = vtxCommonDevice();
    if (vtxDevice) {
        // Check input sources for config updates
        vtxControlInputPoll();
        vtxUpdateRcMap();

        // Build runtime settings
        const vtxSettingsConfig_t * runtimeSettings = vtxGetRuntimeSettings();

        vtxTrackBandAndChannel(runtimeSettings);

        switch (currentSchedule) {
            case VTX_PARAM_POWER:
                vtxProcessPower(vtxDevice, runtimeSettings);
                break;
            case VTX_PARAM_BANDCHAN:
                vtxProcessBandAndChannel(vtxDevice);
                break;
            case VTX_PARAM_PITMODE:
                vtxProcessPitMode(vtxDevice, runtimeSettings);
                break;
            default:
                break;
        }

        vtxCommonProcess(vtxDevice, currentTimeUs);

        currentSchedule = (currentSchedule + 1) % VTX_PARAM_COUNT;
    }
}

#endif
