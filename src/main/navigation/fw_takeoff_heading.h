/*
 * This file is part of INAV.
 *
 * INAV is free software. You can redistribute this software
 * and/or modify this software under the terms of the
 * GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

// Heading hold after a catapult/hand launch flown in ANGLE, without NAV LAUNCH
typedef enum {
    FW_TAKEOFF_HEADING_WAIT = 0,
    FW_TAKEOFF_HEADING_DETECT,
    FW_TAKEOFF_HEADING_HOLD,
    FW_TAKEOFF_HEADING_DONE,
} fwTakeoffHeadingState_e;

typedef struct fwTakeoffHeadingParams_s {
    bool enabled;
    float accelThresh;      // cm/s/s along the fuselage
    uint16_t detectMs;
    uint16_t holdMs;
    uint8_t maxRoll;        // deg
    uint8_t abortDeadband;  // rcCommand points of the roll stick
} fwTakeoffHeadingParams_t;

typedef struct fwTakeoffHeadingInput_s {
    bool armed;
    bool angleMode;
    bool navOwnsRoll;
    bool launchMode;
    bool throttleLow;
    bool level;
    float accelX;           // cm/s/s
    int16_t yaw;            // decidegrees
    int16_t rollStick;      // pilot rcCommand[ROLL]
    uint32_t nowMs;
} fwTakeoffHeadingInput_t;

typedef struct fwTakeoffHeading_s {
    uint8_t state;
    uint32_t sinceMs;
    int16_t target;         // decidegrees
    int16_t last;           // decidegrees
    uint16_t detected;      // throws detected since boot
    uint16_t aborted;       // holds ended by the roll stick
    int16_t maxError;       // largest heading error of the last hold, decidegrees
} fwTakeoffHeading_t;

// Bank (deg, + = right wing down) to add to the pilot's ANGLE command
float fwTakeoffHeadingUpdate(fwTakeoffHeading_t *s, const fwTakeoffHeadingParams_t *p, const fwTakeoffHeadingInput_t *in);

#ifndef FW_TAKEOFF_HEADING_TEST
#include "common/time.h"
void fwTakeoffHeadingApply(timeUs_t currentTimeUs);
const fwTakeoffHeading_t *fwTakeoffHeadingStatus(void);
#endif
