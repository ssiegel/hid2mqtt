// SPDX-FileCopyrightText: © 2025 Stefan Siegel <ssiegel@sdas.net>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "config_lock.h"

#include <string.h>
#include <assert.h>

#include <esp_log.h>
#include <nvs_flash.h>

static const char *TAG = "config_lock";

static char *lock_key;

static nvs_handle_t nvs;

bool is_config_locked() {
    return lock_key && *lock_key;
}

esp_err_t config_lock(const char *key) {
    char *new_key = strdup(key);

    if (!new_key) {
        ESP_LOGE(TAG, "Failed to allocate memory for config lock key");
        return ESP_ERR_NO_MEM;
    }

    free(lock_key);
    lock_key = new_key;

    if (is_config_locked())
        ESP_LOGI(TAG, "Setting new key '%s'", lock_key);
    else
        ESP_LOGI(TAG, "Removing key");

    ESP_ERROR_CHECK(nvs_set_str(nvs, "lock_key", lock_key));
    ESP_ERROR_CHECK(nvs_commit(nvs));
    return ESP_OK;
}

esp_err_t config_unlock(const char *key) {
    if (is_config_locked() && strcmp(key, lock_key) != 0) {
        ESP_LOGW(TAG, "Failed to unlock config, expected key '%s', got key '%s'", lock_key, key);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Unlock config, got correct key '%s'", key);
    return config_lock("");
}

void config_lock_start(void) {
    ESP_ERROR_CHECK(nvs_open("config_lock", NVS_READWRITE, &nvs));

    size_t required_size;

    if (nvs_get_str(nvs, "lock_key", NULL, &required_size) != ESP_OK) {
        ESP_LOGW(TAG, "No config lock key found in NVS, initializing");
        ESP_ERROR_CHECK(nvs_set_str(nvs, "lock_key", ""));
        ESP_ERROR_CHECK(nvs_commit(nvs));
    }

    ESP_ERROR_CHECK(nvs_get_str(nvs, "lock_key", NULL, &required_size));
    lock_key = malloc(required_size);
    assert(lock_key);
    ESP_ERROR_CHECK(nvs_get_str(nvs, "lock_key", lock_key, &required_size));
}
