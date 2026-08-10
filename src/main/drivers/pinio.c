/*
 * This file is part of Cleanflight, Betaflight and INAV
 *
 * Cleanflight, Betaflight and INAV are free software. You can 
 * redistribute this software and/or modify this software under 
 * the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, 
 * or (at your option) any later version.
 *
 * Cleanflight, Betaflight and INAV are distributed in the hope that 
 * they will be useful, but WITHOUT ANY WARRANTY; without even the 
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.
 *
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>

#include "platform.h"

#ifdef USE_PINIO

#include "build/debug.h"
#include "common/memory.h"
#include "drivers/io.h"
#include "drivers/pinio.h"

/*** Hardware definitions ***/
const pinioHardware_t pinioHardware[] = {
#if defined(PINIO1_PIN)
#if !defined(PINIO1_FLAGS)
#define PINIO1_FLAGS 0
#endif
    { .ioTag = IO_TAG(PINIO1_PIN), .ioMode = IOCFG_OUT_PP, .flags = PINIO1_FLAGS },
#endif

#if defined(PINIO2_PIN)
#if !defined(PINIO2_FLAGS)
#define PINIO2_FLAGS 0
#endif
    { .ioTag = IO_TAG(PINIO2_PIN), .ioMode = IOCFG_OUT_PP, .flags = PINIO2_FLAGS },
#endif

#if defined(PINIO3_PIN)
#if !defined(PINIO3_FLAGS)
#define PINIO3_FLAGS 0
#endif
    { .ioTag = IO_TAG(PINIO3_PIN), .ioMode = IOCFG_OUT_PP, .flags = PINIO3_FLAGS },
#endif

#if defined(PINIO4_PIN)
#if !defined(PINIO4_FLAGS)
#define PINIO4_FLAGS 0
#endif
    { .ioTag = IO_TAG(PINIO4_PIN), .ioMode = IOCFG_OUT_PP, .flags = PINIO4_FLAGS },
#endif
};

const int pinioHardwareCount = ARRAYLEN(pinioHardware);

/*** Runtime configuration ***/
typedef struct pinioRuntime_s {
    IO_t io;
    ioConfig_t ioMode;
    bool inverted;
    bool hizIdle;
    bool state;
} pinioRuntime_t;

static pinioRuntime_t pinioRuntime[PINIO_COUNT];

void pinioInit(void)
{
    if (pinioHardwareCount == 0) {
        return;
    }

    for (int i = 0; i < pinioHardwareCount; i++) {
        IO_t io = IOGetByTag(pinioHardware[i].ioTag);

        if (!io) {
            continue;
        }

        pinioRuntime[i].inverted = (pinioHardware[i].flags & PINIO_FLAGS_INVERTED) != 0;
        pinioRuntime[i].hizIdle = (pinioHardware[i].flags & PINIO_FLAGS_HIZ_IDLE) != 0;
        pinioRuntime[i].ioMode = pinioHardware[i].ioMode;

        if (pinioRuntime[i].hizIdle) {
            IOInit(io, OWNER_PINIO, RESOURCE_INPUT, RESOURCE_INDEX(i));
            IOConfigGPIO(io, IOCFG_IN_FLOATING);
        } else {
            IOInit(io, OWNER_PINIO, RESOURCE_OUTPUT, RESOURCE_INDEX(i));
            IOConfigGPIO(io, pinioHardware[i].ioMode);
            IOWrite(io, pinioRuntime[i].inverted);
        }

        pinioRuntime[i].io = io;
        pinioRuntime[i].state = false;
    }
}

void pinioSet(int index, bool on)
{
    const bool newState = on ^ pinioRuntime[index].inverted;

    if (!pinioRuntime[index].io) {
        return;
    }

    if (newState == pinioRuntime[index].state) {
        return;
    }

    if (pinioRuntime[index].hizIdle) {
        // "off" means released rather than driven, so the pin swaps direction
        // instead of just changing level.
        if (on) {
            IOConfigGPIO(pinioRuntime[index].io, pinioRuntime[index].ioMode);
            IOWrite(pinioRuntime[index].io, newState);
        } else {
            IOConfigGPIO(pinioRuntime[index].io, IOCFG_IN_FLOATING);
        }
    } else {
        IOWrite(pinioRuntime[index].io, newState);
    }

    pinioRuntime[index].state = newState;
}
#endif
