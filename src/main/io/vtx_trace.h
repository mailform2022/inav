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

/*
 * Raw VTX serial trace: every byte sent to and received from the VTX, with a
 * millisecond timestamp, kept in a ring buffer in CCM RAM and dumped with the
 * CLI command `vtxtrace`. Identical back-to-back frames (status polls) are
 * collapsed into one record with a repeat counter so the buffer covers a
 * whole flight rather than a couple of minutes of polling.
 */

typedef enum {
    VTX_TRACE_PROTO_TRAMP = 0,
    VTX_TRACE_PROTO_SMARTAUDIO = 1,
} vtxTraceProto_e;

typedef enum {
    VTX_TRACE_DIR_TX = 0,
    VTX_TRACE_DIR_RX = 1,
} vtxTraceDir_e;

#define VTX_TRACE_MAX_PAYLOAD 40

typedef struct vtxTraceRecord_s {
    uint32_t timeMs;
    uint16_t repeats;       // extra identical frames merged into this record
    uint8_t  proto;         // vtxTraceProto_e
    uint8_t  dir;           // vtxTraceDir_e
    uint8_t  len;
    uint8_t  data[VTX_TRACE_MAX_PAYLOAD];
} vtxTraceRecord_t;

typedef struct vtxTraceStats_s {
    uint32_t records;       // records currently held
    uint32_t dropped;       // records evicted to make room
    uint32_t txBytes;
    uint32_t rxBytes;
    bool     enabled;
} vtxTraceStats_t;

void vtxTraceInit(void);
void vtxTraceSetEnabled(bool enabled);
void vtxTraceClear(void);

// Whole frame the FC has just sent
void vtxTraceTx(vtxTraceProto_e proto, const uint8_t *buf, uint8_t len);
// One byte pulled from the VTX serial port; bytes are grouped into a frame
// until a TX happens or the line goes quiet for VTX_TRACE_RX_GAP_MS
void vtxTraceRxByte(vtxTraceProto_e proto, uint8_t byte);
// Periodic housekeeping (closes an RX frame after the inter-frame gap)
void vtxTraceUpdate(uint32_t nowMs);

const vtxTraceStats_t *vtxTraceStats(void);
// Iterate oldest -> newest. Returns false when idx is past the end.
bool vtxTraceGet(uint32_t idx, vtxTraceRecord_t *out);
