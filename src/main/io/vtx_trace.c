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

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "platform.h"

#if defined(USE_VTX_TRACE)

#include "build/build_config.h"

#include "drivers/time.h"

#include "io/vtx_trace.h"

#define VTX_TRACE_BUF_SIZE      16384
#define VTX_TRACE_RX_GAP_MS     20
#define VTX_TRACE_HDR_SIZE      8

// Ring of variable-length records: [len u8][flags u8][repeats u16][timeMs u32][data len]
// flags: bit0 = dir, bits1..3 = proto
static EXTENDED_FASTRAM uint8_t traceBuf[VTX_TRACE_BUF_SIZE];
static uint32_t traceHead;          // next write offset
static uint32_t traceTail;          // oldest record offset
static uint32_t traceUsed;          // bytes in use
static uint32_t lastRecordOffset;   // offset of the newest record, or UINT32_MAX
static vtxTraceStats_t stats;

static struct {
    uint8_t  buf[VTX_TRACE_MAX_PAYLOAD];
    uint8_t  len;
    uint8_t  proto;
    uint32_t startMs;
    uint32_t lastByteMs;
} rxPending;

static uint8_t bufRead(uint32_t off)
{
    return traceBuf[off % VTX_TRACE_BUF_SIZE];
}

static void bufWrite(uint32_t off, uint8_t v)
{
    traceBuf[off % VTX_TRACE_BUF_SIZE] = v;
}

static uint8_t recordTotalLen(uint32_t off)
{
    return VTX_TRACE_HDR_SIZE + bufRead(off);
}

static void evictOldest(void)
{
    const uint8_t total = recordTotalLen(traceTail);
    traceTail = (traceTail + total) % VTX_TRACE_BUF_SIZE;
    traceUsed -= total;
    stats.records--;
    stats.dropped++;
    if (stats.records == 0) {
        lastRecordOffset = UINT32_MAX;
    }
}

static bool lastRecordMatches(uint8_t flags, const uint8_t *data, uint8_t len)
{
    if (lastRecordOffset == UINT32_MAX) {
        return false;
    }
    if (bufRead(lastRecordOffset) != len || bufRead(lastRecordOffset + 1) != flags) {
        return false;
    }
    for (uint8_t i = 0; i < len; i++) {
        if (bufRead(lastRecordOffset + VTX_TRACE_HDR_SIZE + i) != data[i]) {
            return false;
        }
    }
    return true;
}

static void commitRecord(uint32_t timeMs, uint8_t proto, uint8_t dir, const uint8_t *data, uint8_t len)
{
    if (!stats.enabled) {
        return;
    }
    if (len > VTX_TRACE_MAX_PAYLOAD) {
        len = VTX_TRACE_MAX_PAYLOAD;
    }
    const uint8_t flags = (dir & 1) | ((proto & 7) << 1);

    if (lastRecordMatches(flags, data, len)) {
        // Same frame again: bump the repeat counter instead of storing a copy
        uint16_t rep = bufRead(lastRecordOffset + 2) | (bufRead(lastRecordOffset + 3) << 8);
        if (rep < UINT16_MAX) {
            rep++;
        }
        bufWrite(lastRecordOffset + 2, rep & 0xFF);
        bufWrite(lastRecordOffset + 3, rep >> 8);
        return;
    }

    const uint32_t total = VTX_TRACE_HDR_SIZE + len;
    while (stats.records && (VTX_TRACE_BUF_SIZE - traceUsed) < total) {
        evictOldest();
    }

    const uint32_t off = traceHead;
    bufWrite(off + 0, len);
    bufWrite(off + 1, flags);
    bufWrite(off + 2, 0);
    bufWrite(off + 3, 0);
    bufWrite(off + 4, timeMs & 0xFF);
    bufWrite(off + 5, (timeMs >> 8) & 0xFF);
    bufWrite(off + 6, (timeMs >> 16) & 0xFF);
    bufWrite(off + 7, (timeMs >> 24) & 0xFF);
    for (uint8_t i = 0; i < len; i++) {
        bufWrite(off + VTX_TRACE_HDR_SIZE + i, data[i]);
    }

    traceHead = (traceHead + total) % VTX_TRACE_BUF_SIZE;
    traceUsed += total;
    stats.records++;
    lastRecordOffset = off;
}

static void flushRxPending(void)
{
    if (rxPending.len) {
        commitRecord(rxPending.startMs, rxPending.proto, VTX_TRACE_DIR_RX, rxPending.buf, rxPending.len);
        rxPending.len = 0;
    }
}

void vtxTraceInit(void)
{
    vtxTraceClear();
    stats.enabled = true;
}

void vtxTraceSetEnabled(bool enabled)
{
    if (!enabled) {
        flushRxPending();
    }
    stats.enabled = enabled;
}

void vtxTraceClear(void)
{
    const bool enabled = stats.enabled;
    memset(&stats, 0, sizeof(stats));
    stats.enabled = enabled;
    traceHead = traceTail = traceUsed = 0;
    lastRecordOffset = UINT32_MAX;
    rxPending.len = 0;
}

void vtxTraceTx(vtxTraceProto_e proto, const uint8_t *buf, uint8_t len)
{
    if (!stats.enabled) {
        return;
    }
    flushRxPending();
    stats.txBytes += len;
    commitRecord(millis(), proto, VTX_TRACE_DIR_TX, buf, len);
}

void vtxTraceRxByte(vtxTraceProto_e proto, uint8_t byte)
{
    if (!stats.enabled) {
        return;
    }
    const uint32_t now = millis();
    stats.rxBytes++;

    if (rxPending.len && (rxPending.proto != proto ||
                          (now - rxPending.lastByteMs) >= VTX_TRACE_RX_GAP_MS ||
                          rxPending.len >= VTX_TRACE_MAX_PAYLOAD)) {
        flushRxPending();
    }
    if (rxPending.len == 0) {
        rxPending.proto = proto;
        rxPending.startMs = now;
    }
    rxPending.buf[rxPending.len++] = byte;
    rxPending.lastByteMs = now;
}

void vtxTraceUpdate(uint32_t nowMs)
{
    if (rxPending.len && (nowMs - rxPending.lastByteMs) >= VTX_TRACE_RX_GAP_MS) {
        flushRxPending();
    }
}

const vtxTraceStats_t *vtxTraceStats(void)
{
    return &stats;
}

bool vtxTraceGet(uint32_t idx, vtxTraceRecord_t *out)
{
    if (idx >= stats.records) {
        return false;
    }
    uint32_t off = traceTail;
    for (uint32_t i = 0; i < idx; i++) {
        off = (off + recordTotalLen(off)) % VTX_TRACE_BUF_SIZE;
    }
    out->len = bufRead(off);
    const uint8_t flags = bufRead(off + 1);
    out->dir = flags & 1;
    out->proto = (flags >> 1) & 7;
    out->repeats = bufRead(off + 2) | (bufRead(off + 3) << 8);
    out->timeMs = bufRead(off + 4) | (bufRead(off + 5) << 8) | (bufRead(off + 6) << 16) | ((uint32_t)bufRead(off + 7) << 24);
    for (uint8_t i = 0; i < out->len; i++) {
        out->data[i] = bufRead(off + VTX_TRACE_HDR_SIZE + i);
    }
    return true;
}

#endif
