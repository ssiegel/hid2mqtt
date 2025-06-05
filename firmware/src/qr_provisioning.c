// SPDX-FileCopyrightText: © 2025 Stefan Siegel <ssiegel@sdas.net>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "qr_provisioning.h"
#include "mqtt.h"

#include <string.h>
#include <strings.h>

#include <esp_check.h>
#include <esp_log.h>
#include <esp_wifi.h>

static const char *TAG = "qr_provisioning";

/**
 * Parse a Wi-Fi QR code string of the form:
 *   WIFI:T:<auth>;S:<ssid>;P:<pass>;H:<hidden>;;
 * and store it to NVS
 * Returns ESP_OK on success, error code otherwise.
 */
esp_err_t provision_wifi_qr(const char *qr) {
    if (!qr || strncmp(qr, "WIFI:", 5) != 0) {
        return ESP_ERR_INVALID_ARG;
    }

    wifi_config_t conf = {.sta = {.btm_enabled = 1,
                                  .rm_enabled = 1,
                                  .mbo_enabled = 1,
                                  .ft_enabled = 1}};

    // Skip "WIFI:" and duplicate the rest for destructive tokenization
    char *payload = strdup(qr + 5);
    if (!payload)
        return ESP_ERR_NO_MEM;

    char *token = strtok(payload, ";");
    while (token) {
        if (strncmp(token, "T:", 2) == 0) {
            const char *auth = token + 2;
            if (strcasecmp(auth, "WPA") == 0 ||
                strcasecmp(auth, "WPA/WPA2") == 0) {
                conf.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
            } else if (strcasecmp(auth, "WEP") == 0) {
                conf.sta.threshold.authmode = WIFI_AUTH_WEP;
            } else if (strcasecmp(auth, "nopass") == 0) {
                conf.sta.threshold.authmode = WIFI_AUTH_OPEN;
            } else {
                ESP_LOGW(TAG, "Unknown auth mode '%s', defaulting to OPEN",
                         auth);
                conf.sta.threshold.authmode = WIFI_AUTH_OPEN;
            }
        } else if (strncmp(token, "S:", 2) == 0) {
            strncpy((char *)conf.sta.ssid, token + 2,
                    sizeof(conf.sta.ssid) - 1);
            conf.sta.ssid[sizeof(conf.sta.ssid) - 1] = '\0';
        } else if (strncmp(token, "P:", 2) == 0) {
            strncpy((char *)conf.sta.password, token + 2,
                    sizeof(conf.sta.password) - 1);
            conf.sta.password[sizeof(conf.sta.password) - 1] = '\0';
        } else if (strncmp(token, "H:", 2) == 0) {
            const char *hidden = token + 2;
            conf.sta.bssid_set = false;
            if (strcasecmp(hidden, "true") == 0) {
                conf.sta.scan_method = WIFI_FAST_SCAN;
                conf.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
            }
        }
        token = strtok(NULL, ";");
    }

    free(payload);

    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &conf), TAG,
                        "failed to set wifi config");

    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_wifi_connect();

    return ESP_OK;
}

/**
 * Parse a MQTT QR code string of the form:
 *   MQTT:U:<uri>;T:<topic>;;
 * and store it to NVS
 * Returns ESP_OK on success, error code otherwise.
 */
esp_err_t provision_mqtt_qr(const char *qr) {
    if (!qr || strncmp(qr, "MQTT:", 5) != 0)
        return ESP_ERR_INVALID_ARG;

    // Skip "MQTT:" and duplicate the rest for destructive tokenization
    char *payload = strdup(qr + 5);
    if (!payload)
        return ESP_ERR_NO_MEM;

    const char *uri = NULL;
    const char *topic = NULL;

    char *token = strtok(payload, ";");
    while (token) {
        if (strncmp(token, "U:", 2) == 0)
            uri = token + 2;
        else if (strncmp(token, "T:", 2) == 0)
            topic = token + 2;
        token = strtok(NULL, ";");
    }

    esp_err_t ret = ESP_ERR_INVALID_ARG;
    if (uri && topic)
        ret = mqtt_set_config(uri, topic);

    free(payload);

    return ret;
}
