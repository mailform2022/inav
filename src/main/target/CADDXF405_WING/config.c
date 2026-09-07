/*
 * This file is part of INAV Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Alternatively, the contents of this file may be used under the terms
 * of the GNU General Public License Version 3, as described below:
 *
 * This file is free software: you may copy, redistribute and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see http://www.gnu.org/licenses/.
 */

#include <stdbool.h>
#include <stdint.h>

#include "platform.h"

#include "common/maths.h"
#include "fc/config.h"
#include "fc/fc_msp_box.h"
#include "fc/rc_modes.h"
#include "io/serial.h"
#include "io/piniobox.h"
#include "flight/servos.h"
#include "flight/mixer_profile.h"

#define MOLNIYA_USER4_AUX_INDEX 5   // AUX6 == CH10
#define MOLNIYA_S7_SERVO_INDEX 5    // pad S7, PB14

void targetConfiguration(void)
{
    serialConfigMutable()->portConfigs[findSerialPortIndexByIdentifier(SERIAL_PORT_USART1)].functionMask = FUNCTION_RX_SERIAL;
    serialConfigMutable()->portConfigs[findSerialPortIndexByIdentifier(SERIAL_PORT_USART3)].functionMask = FUNCTION_GPS;
    serialConfigMutable()->portConfigs[findSerialPortIndexByIdentifier(SERIAL_PORT_USART6)].functionMask = FUNCTION_MSP;

    pinioBoxConfigMutable()->permanentId[0] = BOX_PERMANENT_ID_USER1;
    // USER2 drives PB10 (pad S9) high, which releases the arming relay.
    pinioBoxConfigMutable()->permanentId[1] = BOX_PERMANENT_ID_USER2;

    // USER4 on CH10, the same switch that throws the S7 servo between its end points.
    modeActivationConditionsMutable(0)->modeId = BOXUSER4;
    modeActivationConditionsMutable(0)->auxChannelIndex = MOLNIYA_USER4_AUX_INDEX;
    modeActivationConditionsMutable(0)->range.startStep = CHANNEL_VALUE_TO_STEP(1700);
    modeActivationConditionsMutable(0)->range.endStep = CHANNEL_VALUE_TO_STEP(2100);

    // S7 is a plain servo output driven straight from CH10, so a two-position
    // switch throws it between the two end points set with the servo command.
    customServoMixersMutable(0)->targetChannel = MOLNIYA_S7_SERVO_INDEX;
    customServoMixersMutable(0)->inputSource = INPUT_RC_CH10;
    customServoMixersMutable(0)->rate = 100;
    customServoMixersMutable(0)->speed = 0;
#ifdef USE_PROGRAMMING_FRAMEWORK
    customServoMixersMutable(0)->conditionId = -1;
#endif

    servoParamsMutable(MOLNIYA_S7_SERVO_INDEX)->min = 1000;
    servoParamsMutable(MOLNIYA_S7_SERVO_INDEX)->max = 2000;
    servoParamsMutable(MOLNIYA_S7_SERVO_INDEX)->middle = 1500;
    servoParamsMutable(MOLNIYA_S7_SERVO_INDEX)->rate = 100;
}
