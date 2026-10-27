/*
 * This file is part of INAV.
 *
 * INAV is free software. You can redistribute this software
 * and/or modify this software under the terms of the
 * GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version.
 */

#include <stdbool.h>
#include <stdint.h>

#ifndef FW_TAKEOFF_HEADING_TEST
#include "platform.h"
#endif

#if defined(USE_FW_LAUNCH_HEADING_HOLD) || defined(FW_TAKEOFF_HEADING_TEST)

#include "navigation/fw_takeoff_heading.h"

#define TAKEOFF_HEADING_ROLL_GAIN   1.0f    // deg of bank per deg of heading error
#define TAKEOFF_HEADING_MAX_STEP    50      // decidegrees per PID cycle, more = IMU snapping to the GPS course
#define TAKEOFF_HEADING_FULL_PART   0.6f    // full correction for this part of the hold, then fade to 0

static int16_t takeoffHeadingWrap(int32_t decidegrees)
{
    while (decidegrees > 1800) {
        decidegrees -= 3600;
    }
    while (decidegrees < -1800) {
        decidegrees += 3600;
    }
    return decidegrees;
}

static bool takeoffHeadingThrow(const fwTakeoffHeadingParams_t *p, const fwTakeoffHeadingInput_t *in)
{
    return in->angleMode && !in->navOwnsRoll && !in->throttleLow && in->level && in->accelX > p->accelThresh;
}

float fwTakeoffHeadingUpdate(fwTakeoffHeading_t *s, const fwTakeoffHeadingParams_t *p, const fwTakeoffHeadingInput_t *in)
{
    if (!in->armed) {
        s->state = FW_TAKEOFF_HEADING_WAIT;
        s->last = in->yaw;
        s->target = in->yaw;
        return 0.0f;
    }

    const int16_t step = takeoffHeadingWrap(in->yaw - s->last);
    s->last = in->yaw;
    if (step > TAKEOFF_HEADING_MAX_STEP || step < -TAKEOFF_HEADING_MAX_STEP) {
        s->target = takeoffHeadingWrap(s->target + step);
    }

    if (in->launchMode) {
        s->state = FW_TAKEOFF_HEADING_DONE;     // NAV LAUNCH holds the heading itself
    }

    switch (s->state) {
    case FW_TAKEOFF_HEADING_WAIT:
        if (p->enabled && takeoffHeadingThrow(p, in)) {
            s->state = FW_TAKEOFF_HEADING_DETECT;
            s->sinceMs = in->nowMs;
            s->target = in->yaw;
        }
        return 0.0f;

    case FW_TAKEOFF_HEADING_DETECT:
        if (!takeoffHeadingThrow(p, in)) {
            s->state = FW_TAKEOFF_HEADING_WAIT;
        } else if (in->nowMs - s->sinceMs >= p->detectMs) {
            s->state = FW_TAKEOFF_HEADING_HOLD;
            s->sinceMs = in->nowMs;
            s->maxError = 0;
            s->detected++;
        }
        return 0.0f;

    case FW_TAKEOFF_HEADING_HOLD:
        break;

    default:
        return 0.0f;
    }

    const uint32_t elapsed = in->nowMs - s->sinceMs;
    if (elapsed >= p->holdMs || !in->angleMode || in->navOwnsRoll) {
        s->state = FW_TAKEOFF_HEADING_DONE;
        return 0.0f;
    }
    if (in->rollStick > p->abortDeadband || in->rollStick < -p->abortDeadband) {
        s->state = FW_TAKEOFF_HEADING_DONE;
        s->aborted++;
        return 0.0f;
    }

    const int16_t errorDeci = takeoffHeadingWrap(s->target - in->yaw);
    if ((errorDeci < 0 ? -errorDeci : errorDeci) > (s->maxError < 0 ? -s->maxError : s->maxError)) {
        s->maxError = errorDeci;
    }

    const float fullMs = p->holdMs * TAKEOFF_HEADING_FULL_PART;
    const float scale = elapsed <= fullMs ? 1.0f : (p->holdMs - elapsed) / (p->holdMs - fullMs);
    float roll = errorDeci / 10.0f * TAKEOFF_HEADING_ROLL_GAIN;
    if (roll > p->maxRoll) {
        roll = p->maxRoll;
    } else if (roll < -p->maxRoll) {
        roll = -p->maxRoll;
    }
    return scale * roll;
}

#ifndef FW_TAKEOFF_HEADING_TEST

#include "common/axis.h"
#include "common/maths.h"
#include "common/utils.h"

#include "fc/rc_controls.h"
#include "fc/runtime_config.h"

#include "flight/failsafe.h"
#include "flight/imu.h"
#include "flight/pid.h"

#include "navigation/navigation.h"
#include "navigation/navigation_private.h"

static fwTakeoffHeading_t takeoffHeading;

void fwTakeoffHeadingApply(timeUs_t currentTimeUs)
{
    if (!STATE(AIRPLANE)) {
        return;
    }

    const fwTakeoffHeadingParams_t params = {
        .enabled = navConfig()->fw.launch_heading_hold,
        .accelThresh = navConfig()->fw.launch_accel_thresh,
        .detectMs = navConfig()->fw.launch_time_thresh,
        .holdMs = navConfig()->fw.launch_heading_time,
        .maxRoll = navConfig()->fw.launch_heading_max_roll,
        .abortDeadband = navConfig()->fw.launch_land_abort_deadband,
    };
    const fwTakeoffHeadingInput_t input = {
        .armed = ARMING_FLAG(ARMED),
        .angleMode = FLIGHT_MODE(ANGLE_MODE),
        .navOwnsRoll = (navGetCurrentStateFlags() & NAV_CTL_POS) || failsafeIsActive(),
        .launchMode = FLIGHT_MODE(NAV_LAUNCH_MODE),
        .throttleLow = throttleStickIsLow(),
        .level = calculateCosTiltAngle() >= cos_approx(DEGREES_TO_RADIANS(navConfig()->fw.launch_max_angle)),
        .accelX = imuMeasuredAccelBF.x,
        .yaw = attitude.values.yaw,
        .rollStick = rcCommand[ROLL],
        .nowMs = US2MS(currentTimeUs),
    };

    const float roll = fwTakeoffHeadingUpdate(&takeoffHeading, &params, &input);
    if (roll != 0.0f) {
        const int16_t bank = pidAngleToRcCommand(DEGREES_TO_DECIDEGREES(roll), pidProfile()->max_angle_inclination[FD_ROLL]);
        rcCommand[ROLL] = constrain(rcCommand[ROLL] + bank, -500, 500);
    }
}

const fwTakeoffHeading_t *fwTakeoffHeadingStatus(void)
{
    return &takeoffHeading;
}

#endif
#endif
