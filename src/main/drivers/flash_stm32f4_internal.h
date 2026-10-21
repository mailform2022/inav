/*
 * This file is part of INAV.
 *
 * INAV is free software. You can redistribute
 * this software and/or modify this software under the terms of the
 * GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * INAV is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.
 *
 * If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "drivers/flash.h"

bool stm32f4intflash_init(int flashNumToUse);
bool stm32f4intflash_isReady(void);
bool stm32f4intflash_waitForReady(uint32_t timeoutMillis);
void stm32f4intflash_eraseSector(uint32_t address);
void stm32f4intflash_eraseCompletely(void);
uint32_t stm32f4intflash_pageProgram(uint32_t address, const uint8_t *data, int length);
void stm32f4intflash_flush(void);
int stm32f4intflash_readBytes(uint32_t address, uint8_t *buffer, int length);
const flashGeometry_t *stm32f4intflash_getGeometry(void);
