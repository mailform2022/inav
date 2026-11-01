/*
 * This file is part of INAV.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <stdbool.h>

// ESC protocol AUTO: after the first arm with an unknown result each motor
// spins alone on DSHOT600 and the current rise shows whether its ESC runs.
void escAutoUpdate(void);
bool escAutoTestRunning(void);
