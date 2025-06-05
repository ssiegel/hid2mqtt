// SPDX-FileCopyrightText: © 2025 Stefan Siegel <ssiegel@sdas.net>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mqtt.h"

#include <string.h>
#include <assert.h>

#include <esp_err.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <mqtt_client.h>

static const char *TAG = "mqtt";

static char *mqtt_uri;
static char *mqtt_topic;

static esp_mqtt_client_handle_t mqtt_client;
static nvs_handle_t nvs;

#ifdef LOG_TO_MQTT
static vprintf_like_t original_vprintf = NULL;
static char log_buffer[1024];
static char *log_buffer_end = log_buffer + sizeof(log_buffer);
static char *log_buffer_cur = log_buffer;
static int suppress_mqtt_logger = 0;

static int mqtt_logger(const char *fmt, va_list args) {
    int ret = 0;
    if (original_vprintf) {
        va_list args_copy;
        va_copy(args_copy, args);
        ret = original_vprintf(fmt, args_copy);
        va_end(args_copy);
    }
    if (suppress_mqtt_logger)
        return ret;

    int flush = 0;
    int len =
        vsnprintf(log_buffer_cur, log_buffer_end - log_buffer_cur, fmt, args);
    log_buffer_cur += len;
    if (log_buffer_cur >= log_buffer_end) {
        flush = 1;
        log_buffer_cur = log_buffer_end - 1;
        log_buffer_cur[0] = 0;
    } else if (log_buffer_cur > log_buffer &&
               (log_buffer_cur[-1] == '\n' || log_buffer_cur[-1] == '\r')) {
        flush = 1;
        while (log_buffer_cur > log_buffer &&
               (log_buffer_cur[-1] == '\n' || log_buffer_cur[-1] == '\r')) {
            --log_buffer_cur;
            log_buffer_cur[0] = 0;
        }
    }
    if (log_buffer_cur > log_buffer && flush) {
        ++suppress_mqtt_logger;
        esp_mqtt_client_publish(mqtt_client, mqtt_topic, log_buffer,
                                log_buffer_cur - log_buffer, 0, 0);
        --suppress_mqtt_logger;
        log_buffer_cur = log_buffer;
        log_buffer_cur[0] = 0;
    }
    return ret;
}
#endif

static void mqtt_event_handler_cb(void *handler_args, esp_event_base_t base,
                                  int32_t event_id, void *event_data) {
    (void)handler_args;
    (void)base;
    (void)event_id;
    esp_mqtt_event_handle_t event = event_data;
    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected");
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT disconnected");
        break;
    case MQTT_EVENT_PUBLISHED:
#ifdef LOG_TO_MQTT
        ++suppress_mqtt_logger;
#endif
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
#ifdef LOG_TO_MQTT
        --suppress_mqtt_logger;
#endif
        break;
    case MQTT_EVENT_ERROR:
#ifdef LOG_TO_MQTT
        ++suppress_mqtt_logger;
#endif
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            if (event->error_handle->esp_tls_last_esp_err)
                ESP_LOGE(TAG, "Last error reported from esp-tls: 0x%x",
                         event->error_handle->esp_tls_last_esp_err);
            if (event->error_handle->esp_tls_stack_err)
                ESP_LOGE(TAG, "Last error reported from tls stack: 0x%x",
                         event->error_handle->esp_tls_stack_err);
            if (event->error_handle->esp_transport_sock_errno)
                ESP_LOGE(
                    TAG,
                    "Last error captured as transport's socket errno: 0x%x",
                    event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)",
                     strerror(event->error_handle->esp_transport_sock_errno));
        }
#ifdef LOG_TO_MQTT
        --suppress_mqtt_logger;
#endif
        break;
    default:
        ESP_LOGI(TAG, "Other event id: %d", event->event_id);
        break;
    }
}

esp_err_t mqtt_publish(const char *msg) {
    const int msg_id =
        esp_mqtt_client_publish(mqtt_client, mqtt_topic, msg, 0, 2, 0);
    if (msg_id >= 0)
        return ESP_OK;
    else
        return ESP_FAIL;
}

esp_err_t mqtt_set_config(const char *uri, const char *topic) {
    char *new_uri = strdup(uri);
    char *new_topic = strdup(topic);

    if (!new_uri || !new_topic) {
        free(new_uri);
        free(new_topic);
        ESP_LOGE(TAG, "Failed to allocate memory for URI and/or topic");
        return ESP_ERR_NO_MEM;
    }

    free(mqtt_uri);
    mqtt_uri = new_uri;
    free(mqtt_topic);
    mqtt_topic = new_topic;

    ESP_ERROR_CHECK(nvs_set_str(nvs, "uri", mqtt_uri));
    ESP_ERROR_CHECK(nvs_set_str(nvs, "topic", mqtt_topic));
    ESP_ERROR_CHECK(nvs_commit(nvs));

    ESP_ERROR_CHECK(esp_mqtt_client_set_uri(mqtt_client, mqtt_uri));
    esp_mqtt_client_disconnect(mqtt_client);
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_mqtt_client_reconnect(mqtt_client);
    return ESP_OK;
}

void mqtt_app_start(void) {
    ESP_ERROR_CHECK(nvs_open("mqtt", NVS_READWRITE, &nvs));

    size_t required_size;

    if (nvs_get_str(nvs, "uri", NULL, &required_size) != ESP_OK ||
        nvs_get_str(nvs, "topic", NULL, &required_size) != ESP_OK) {
        ESP_LOGW(TAG, "No MQTT config found in NVS, initializing");
        ESP_ERROR_CHECK(nvs_set_str(nvs, "uri", "mqtt://127.0.0.1"));
        ESP_ERROR_CHECK(nvs_set_str(nvs, "topic", "hid2mqtt"));
        ESP_ERROR_CHECK(nvs_commit(nvs));
    }

    ESP_ERROR_CHECK(nvs_get_str(nvs, "uri", NULL, &required_size));
    mqtt_uri = malloc(required_size);
    assert(mqtt_uri);
    ESP_ERROR_CHECK(nvs_get_str(nvs, "uri", mqtt_uri, &required_size));

    ESP_ERROR_CHECK(nvs_get_str(nvs, "topic", NULL, &required_size));
    mqtt_topic = malloc(required_size);
    assert(mqtt_topic);
    ESP_ERROR_CHECK(nvs_get_str(nvs, "topic", mqtt_topic, &required_size));

    esp_mqtt_client_config_t cfg = {.broker.address.uri = mqtt_uri};
    mqtt_client = esp_mqtt_client_init(&cfg);
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(
        mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler_cb, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt_client));
#ifdef LOG_TO_MQTT
    original_vprintf = esp_log_set_vprintf(mqtt_logger);
#endif
}
