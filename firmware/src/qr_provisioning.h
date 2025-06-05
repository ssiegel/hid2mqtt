// SPDX-FileCopyrightText: © 2025 Stefan Siegel <ssiegel@sdas.net>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t provision_wifi_qr(const char *qr);
esp_err_t provision_mqtt_qr(const char *qr);

#ifdef __cplusplus
}
#endif
