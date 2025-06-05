// SPDX-FileCopyrightText: © 2025 Stefan Siegel <ssiegel@sdas.net>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "config_lock.h"
#include "mqtt.h"
#include "ota.h"
#include "qr_provisioning.h"
#include "usb_hid.h"
#include "wifi.h"

#include <string.h>

#include <freertos/FreeRTOS.h>

#include <esp_log.h>
#include <esp_timer.h>
#include <nvs_flash.h>
#include <rtc_wdt.h>

static const char *TAG = "app";

void configure_watchdog(unsigned int timeout_ms) {
    rtc_wdt_protect_off();
    rtc_wdt_disable();
    rtc_wdt_set_length_of_reset_signal(RTC_WDT_SYS_RESET_SIG,
                                       RTC_WDT_LENGTH_3_2us);
    rtc_wdt_set_stage(RTC_WDT_STAGE0, RTC_WDT_STAGE_ACTION_RESET_SYSTEM);
    rtc_wdt_set_time(RTC_WDT_STAGE0, timeout_ms);
    rtc_wdt_enable();
    rtc_wdt_protect_on();
}

#define KEY_TIMEOUT_MQTT_SUBMIT 50000 // 50ms
static char collected_keys[2048] = "";
static char *current_key = collected_keys;
static int64_t key_timestamp = 0;

void key_char_submit() {
    if (current_key > collected_keys) {
        if (current_key >= collected_keys + sizeof(collected_keys)) {
            ESP_LOGW(TAG, "key_char_submit buffer full, truncating");
            current_key = collected_keys + sizeof(collected_keys) - 1;
        }
        *current_key = 0;
        ESP_LOGI(TAG, "key_char_submit with string: %s", collected_keys);

        const char *aim_stripped = collected_keys;
        if (strncmp(aim_stripped, "]Q1", 3) == 0) {
            aim_stripped += 3;
        }

        bool publish = false;
        if (is_config_locked()) {
            if (strncmp(aim_stripped, "UNLOCK:", 7) == 0) {
                ESP_LOGI(TAG, "attempting unlock config");
                publish = config_unlock(aim_stripped + 7) != ESP_OK;
            } else {
                publish = true;
            }
        } else {
            if (strncmp(aim_stripped, "LOCK:", 5) == 0) {
                ESP_LOGI(TAG, "lock config");
                config_lock(aim_stripped + 5);
            } else if (strncmp(aim_stripped, "OTA:", 4) == 0) {
                ESP_LOGI(TAG, "attempting OTA");
                configure_watchdog(300000);
                update_firmware(aim_stripped + 4);
            } else if (strncmp(aim_stripped, "WIFI:", 5) == 0) {
                ESP_LOGI(TAG, "provision wifi");
                provision_wifi_qr(aim_stripped);
            } else if (strncmp(aim_stripped, "MQTT:", 5) == 0) {
                ESP_LOGI(TAG, "provision mqtt");
                provision_mqtt_qr(aim_stripped);
            } else {
                publish = true;
            }
        }
        if (publish) {
            ESP_LOGI(TAG, "publishing to mqtt");
            mqtt_publish(collected_keys);
        }
        current_key = collected_keys;
    }
}

void key_char_callback(char c) {
    if ('\t' == c) {
        key_char_submit();
    } else if (c) {
        if (current_key >= collected_keys + sizeof(collected_keys) - 1) {
            ESP_LOGW(TAG, "key_char_submit buffer full, submitting before collecting more keys");
            key_char_submit();
        }
        *current_key = c;
        ++current_key;
    }
    key_timestamp = esp_timer_get_time();
}

void app_main(void) {
    configure_watchdog(5000);

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    rtc_wdt_feed();
    config_lock_start();

    rtc_wdt_feed();
    wifi_init_sta();

    for (int i = 0; i < 50; ++i) {
        rtc_wdt_feed();
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    rtc_wdt_feed();
    mqtt_app_start();

    // provision_wifi_qr("WIFI:T:WPA;S:example;P:secret;H:false;;");
    // provision_mqtt_qr("MQTT:U:mqtt://mqtt.example.com;T:hid2mqtt;;");

    rtc_wdt_feed();
    usb_hid_start(key_char_callback);

    while (true) {
        rtc_wdt_feed();
        usb_hid_handle_events();
        if (key_timestamp + KEY_TIMEOUT_MQTT_SUBMIT < esp_timer_get_time()) {
            key_char_submit();
            key_timestamp = esp_timer_get_time();
        }
    }
}
