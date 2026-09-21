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

/*
 * Blackbox storage in the unused part of the MCU's own flash, for boards that
 * have neither a dataflash chip nor an SD card slot.
 *
 * STM32F405: sectors 9 and 10 (0x080A0000..0x080DFFFF, 2 x 128 KB) sit between
 * the end of the firmware and the config sector (11). They are presented to
 * flashfs as a 256 KB device (NAND semantics, so a closed log is flushed and
 * the next one starts on a fresh page), so the stock blackbox, MSP dataflash
 * download (Configurator "Save flash to file"), MSC and CLI flash_* commands
 * work unchanged.
 *
 * On the F4 the CPU stalls while its flash is being programmed or erased.
 * Programming is done in small pages (32 bytes, ~0.15 ms) so the flight loop
 * only ever sees short hiccups. Erasing a 128 KB sector stalls for ~1-2 s and
 * is therefore only ever done from the ground (CLI flash_erase / Configurator
 * "Erase flash"); the log simply stops when the device is full.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "platform.h"

#if defined(USE_FLASH_INTERNAL) && defined(STM32F4)

#include "drivers/flash.h"
#include "drivers/flash_stm32f4_internal.h"
#include "build/build_config.h"
#include "common/utils.h"
#include "drivers/system.h"

#define INTFLASH_BASE           0x080A0000UL
#define INTFLASH_FIRST_SECTOR   FLASH_Sector_9
#define INTFLASH_SECTOR_SIZE    (128 * 1024)
#define INTFLASH_SECTORS        2
#define INTFLASH_PAGE_SIZE      32
#define INTFLASH_TOTAL_SIZE     (INTFLASH_SECTOR_SIZE * INTFLASH_SECTORS)

// F4 sector numbers are encoded as (n * 8) in the StdPeriph API.
#define INTFLASH_SECTOR_ID(idx) ((uint16_t)(INTFLASH_FIRST_SECTOR + ((idx) * 8)))

// Load-address end of the firmware image (.data is the last section the
// linker places in flash), provided by the linker script.
extern uint8_t _sidata;
extern uint8_t _sdata;
extern uint8_t _edata;

static flashGeometry_t geometry = {
    .sectors = INTFLASH_SECTORS,
    .pageSize = INTFLASH_PAGE_SIZE,
    .sectorSize = INTFLASH_SECTOR_SIZE,
    .totalSize = INTFLASH_TOTAL_SIZE,
    .pagesPerSector = INTFLASH_SECTOR_SIZE / INTFLASH_PAGE_SIZE,
    // NAND semantics make flashfs flush the pending word and start the next
    // log on a fresh page when a log is closed.
    .flashType = FLASH_TYPE_NAND,
};

static bool intflashAvailable = false;

// A flash word may only be programmed once between erases, but flashfs hands
// over runs of arbitrary length and alignment. Bytes that do not complete a
// word are parked here until the run continues or the log is closed.
static uint32_t pendingAddress;     // device offset of the word being assembled
static uint8_t pendingBytes[4];
static uint8_t pendingCount;

static uint32_t firmwareEndAddress(void)
{
    return (uint32_t)&_sidata + ((uint32_t)&_edata - (uint32_t)&_sdata);
}

bool stm32f4intflash_init(int flashNumToUse)
{
    UNUSED(flashNumToUse);

    // Refuse to run if the firmware image has grown into the log sectors:
    // logging would then overwrite code. The linker script keeps this from
    // happening at build time, this is the runtime belt to that suspender.
    intflashAvailable = firmwareEndAddress() <= INTFLASH_BASE;
    return intflashAvailable;
}

bool stm32f4intflash_isReady(void)
{
    // Every operation completes before returning, so the device is always idle.
    return intflashAvailable;
}

bool stm32f4intflash_waitForReady(uint32_t timeoutMillis)
{
    UNUSED(timeoutMillis);
    return intflashAvailable;
}

static void intflashUnlock(void)
{
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                    FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
}

void stm32f4intflash_eraseSector(uint32_t address)
{
    if (!intflashAvailable || address >= INTFLASH_TOTAL_SIZE) {
        return;
    }

    pendingCount = 0;
    intflashUnlock();
    FLASH_EraseSector(INTFLASH_SECTOR_ID(address / INTFLASH_SECTOR_SIZE), VoltageRange_3);
    FLASH_Lock();
}

void stm32f4intflash_eraseCompletely(void)
{
    for (unsigned i = 0; i < INTFLASH_SECTORS; i++) {
        stm32f4intflash_eraseSector(i * INTFLASH_SECTOR_SIZE);
    }
}

static void intflashProgramPending(void)
{
    if (pendingCount == 0) {
        return;
    }
    uint32_t word = 0xFFFFFFFFUL;
    memcpy(&word, pendingBytes, pendingCount);
    FLASH_ProgramWord(INTFLASH_BASE + pendingAddress, word);
    pendingCount = 0;
}

uint32_t stm32f4intflash_pageProgram(uint32_t address, const uint8_t *data, int length)
{
    if (!intflashAvailable || length <= 0 || address + length > INTFLASH_TOTAL_SIZE) {
        return address;
    }

    intflashUnlock();

    if (pendingCount && (address != pendingAddress + pendingCount)) {
        // The run did not continue where the last one stopped; seal the old word.
        intflashProgramPending();
    }

    uint32_t offset = address;
    while (length > 0) {
        if (pendingCount == 0 && (offset % 4) == 0 && length >= 4) {
            uint32_t word;
            memcpy(&word, data, 4);
            if (FLASH_ProgramWord(INTFLASH_BASE + offset, word) != FLASH_COMPLETE) {
                break;
            }
            data += 4;
            offset += 4;
            length -= 4;
            continue;
        }

        if (pendingCount == 0) {
            pendingAddress = offset & ~3UL;
            pendingCount = offset % 4;
            memset(pendingBytes, 0xFF, sizeof(pendingBytes));
        }
        pendingBytes[pendingCount++] = *data++;
        offset++;
        length--;
        if (pendingCount == 4) {
            intflashProgramPending();
        }
    }

    FLASH_Lock();

    return offset;
}

void stm32f4intflash_flush(void)
{
    if (!intflashAvailable || pendingCount == 0) {
        return;
    }
    intflashUnlock();
    intflashProgramPending();
    FLASH_Lock();
}

int stm32f4intflash_readBytes(uint32_t address, uint8_t *buffer, int length)
{
    if (!intflashAvailable || address >= INTFLASH_TOTAL_SIZE) {
        return 0;
    }
    if ((uint32_t)length > INTFLASH_TOTAL_SIZE - address) {
        length = INTFLASH_TOTAL_SIZE - address;
    }
    memcpy(buffer, (const uint8_t *)(INTFLASH_BASE + address), length);
    return length;
}

const flashGeometry_t *stm32f4intflash_getGeometry(void)
{
    static const flashGeometry_t none = {0};
    return intflashAvailable ? &geometry : &none;
}

#endif
