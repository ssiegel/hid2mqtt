// SPDX-FileCopyrightText: © 2025 Stefan Siegel <ssiegel@sdas.net>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void update_firmware(const char *url) __attribute__((__noreturn__));

#ifdef __cplusplus
}
#endif
