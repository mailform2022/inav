/*
 * This file is part of INAV.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <stdbool.h>
#include <stdint.h>

#include "platform.h"

#ifdef USE_ESC_AUTO

#include "common/log.h"
#include "common/maths.h"

#include "drivers/pwm_mapping.h"
#include "drivers/time.h"

#include "fc/config.h"
#include "fc/fc_core.h"
#include "fc/runtime_config.h"

#include "flight/esc_auto.h"
#include "flight/mixer.h"

#include "sensors/battery.h"

#define ESC_AUTO_PHASE_MS           1500    // each motor spins alone this long
#define ESC_AUTO_SAMPLE_FROM_MS     700     // current is averaged after spin-up
#define ESC_AUTO_BASELINE_PERIOD_MS 20
#define ESC_AUTO_MAX_MOTORS         2

extern int motorZeroCommand;

typedef enum {
    ESC_AUTO_WAIT,
    ESC_AUTO_SPIN,
    ESC_AUTO_PASSED,
    ESC_AUTO_FAILED,
} escAutoState_e;

static escAutoState_e state = ESC_AUTO_WAIT;
static uint8_t testMotor;
static timeMs_t phaseStartMs;
static timeMs_t baselineMs;
static int32_t baseline = -1;   // cA, filtered while disarmed
static int32_t sampleSum;
static uint16_t sampleCount;

static bool escAutoNeeded(void)
{
    return motorConfig()->motorPwmProtocol == PWM_TYPE_AUTO &&
           motorConfig()->escAutoResult == ESC_AUTO_RESULT_UNKNOWN;
}

static void escAutoStore(uint8_t result, bool reboot)
{
    motorConfigMutable()->escAutoResult = result;
    saveConfigAndNotify();
    if (reboot) {
        // The motor timers were set up for DSHOT600; STANDARD needs a fresh start
        fcReboot(false);
    }
}

static void escAutoStartPhase(timeMs_t nowMs)
{
    phaseStartMs = nowMs;
    sampleSum = 0;
    sampleCount = 0;
}

static void escAutoDisarmed(timeMs_t nowMs)
{
    const escAutoState_e finished = state;
    state = ESC_AUTO_WAIT;

    if (finished == ESC_AUTO_FAILED) {
        escAutoStore(ESC_AUTO_RESULT_STANDARD, true);
        return;
    }
    if (finished == ESC_AUTO_PASSED) {
        escAutoStore(ESC_AUTO_RESULT_DSHOT600, false);
        return;
    }
    if (!escAutoNeeded()) {
        return;
    }
    if (!isAmperageConfigured()) {
        // Without a current sensor DSHOT600 can never be confirmed
        escAutoStore(ESC_AUTO_RESULT_STANDARD, true);
        return;
    }
    if (nowMs - baselineMs >= ESC_AUTO_BASELINE_PERIOD_MS) {
        baselineMs = nowMs;
        const int32_t amperage = getAmperage();
        baseline = baseline < 0 ? amperage : baseline + (amperage - baseline) / 8;
    }
}

static void escAutoArmed(timeMs_t nowMs)
{
    if (state == ESC_AUTO_WAIT) {
        if (!escAutoNeeded() || baseline < 0) {
            return;
        }
        state = ESC_AUTO_SPIN;
        testMotor = 0;
        escAutoStartPhase(nowMs);
    }
    if (state != ESC_AUTO_SPIN) {
        return;
    }

    const uint8_t motorCount = MIN(getMotorCount(), ESC_AUTO_MAX_MOTORS);
    const timeMs_t elapsed = nowMs - phaseStartMs;
    if (elapsed >= ESC_AUTO_SAMPLE_FROM_MS) {
        sampleSum += getAmperage();
        sampleCount++;
    }
    if (elapsed >= ESC_AUTO_PHASE_MS) {
        const int32_t rise = (sampleCount ? sampleSum / sampleCount : 0) - baseline;
        LOG_INFO(SYSTEM, "ESC AUTO: motor %u current rise %d cA", testMotor + 1, (int)rise);
        if (rise < motorConfig()->escAutoMinCurrent) {
            // This ESC did not run on DSHOT600: stop and come back on STANDARD
            state = ESC_AUTO_FAILED;
            disarm(DISARM_SWITCH);
        } else if (++testMotor < motorCount) {
            escAutoStartPhase(nowMs);
        } else {
            state = ESC_AUTO_PASSED;
        }
    }

    if (state == ESC_AUTO_SPIN || state == ESC_AUTO_FAILED) {
        const int idle = getThrottleIdleValue();
        const int spin = idle + (motorConfig()->maxthrottle - idle) * motorConfig()->escAutoTestThrottle / 100;
        for (int i = 0; i < getMotorCount(); i++) {
            motor[i] = (state == ESC_AUTO_SPIN && i == testMotor) ? spin : motorZeroCommand;
        }
    }
}

void escAutoUpdate(void)
{
    const timeMs_t nowMs = millis();
    if (ARMING_FLAG(ARMED)) {
        escAutoArmed(nowMs);
    } else {
        escAutoDisarmed(nowMs);
    }
}

bool escAutoTestRunning(void)
{
    return state == ESC_AUTO_SPIN;
}

#endif
