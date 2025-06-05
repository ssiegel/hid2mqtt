// SPDX-FileCopyrightText: © 2025 Stefan Siegel <ssiegel@sdas.net>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdbool.h>

#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

bool is_config_locked();
esp_err_t config_lock(const char *key);
esp_err_t config_unlock(const char *key);
void config_lock_start(void);

#ifdef __cplusplus
}
#endif
