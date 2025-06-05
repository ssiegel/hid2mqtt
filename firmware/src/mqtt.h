// SPDX-FileCopyrightText: © 2025 Stefan Siegel <ssiegel@sdas.net>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t mqtt_publish(const char *msg);
esp_err_t mqtt_set_config(const char *uri, const char *topic);
void mqtt_app_start(void);

#ifdef __cplusplus
}
#endif
