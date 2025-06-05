// SPDX-FileCopyrightText: © 2025 Stefan Siegel <ssiegel@sdas.net>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ota.h"

#include <esp_crt_bundle.h>
#include <esp_https_ota.h>
#include <esp_log.h>

static const char *TAG = "ota";

void update_firmware(const char *url) {
    ESP_LOGI(TAG, "Attempting firmware update via %s", url);
    esp_http_client_config_t config = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };
    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK)
        ESP_LOGI(TAG, "OTA successful");
    else
        ESP_LOGE(TAG, "OTA failed");

    esp_restart();
}
