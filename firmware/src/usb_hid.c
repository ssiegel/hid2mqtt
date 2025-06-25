// SPDX-FileCopyrightText: © 2025 Stefan Siegel <ssiegel@sdas.net>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "usb_hid.h"

#include <stdbool.h>
#include <assert.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <esp_err.h>
#include <esp_log.h>

#include <usb/hid_host.h>
#include <usb/hid_usage_keyboard.h>
#include <usb/usb_host.h>

static const char *TAG = "usb_hid";

static QueueHandle_t app_event_queue = NULL;

static key_char_cb_t key_char_callback = NULL;

typedef enum {
    APP_EVENT_HID_HOST_DEVICE = 0,
    APP_EVENT_HID_HOST_INTERFACE
} app_event_group_t;

typedef struct {
    app_event_group_t event_group;
    hid_host_device_handle_t device_handle;
    hid_host_driver_event_t driver_event;
    hid_host_interface_event_t interface_event;
    void *arg;
} app_event_queue_t;

/*
 * Assume US English keyboard layout, configure the barcode scanner accordingly
 */
const uint8_t keycode2ascii[100][4] = {
    {0, 0, 0, 0},            // HID_KEY_NO_PRESS
    {0, 0, 0, 0},            // HID_KEY_ROLLOVER
    {0, 0, 0, 0},            // HID_KEY_POST_FAIL
    {0, 0, 0, 0},            // HID_KEY_ERROR_UNDEFINED
    {'a', 'A', 0x01, 0x01},  // HID_KEY_A
    {'b', 'B', 0x02, 0x02},  // HID_KEY_B
    {'c', 'C', 0x03, 0x03},  // HID_KEY_C
    {'d', 'D', 0x04, 0x04},  // HID_KEY_D
    {'e', 'E', 0x05, 0x05},  // HID_KEY_E
    {'f', 'F', 0x06, 0x06},  // HID_KEY_F
    {'g', 'G', 0x07, 0x07},  // HID_KEY_G
    {'h', 'H', 0x08, 0x08},  // HID_KEY_H
    {'i', 'I', 0x09, 0x09},  // HID_KEY_I
    {'j', 'J', 0x0a, 0x0a},  // HID_KEY_J
    {'k', 'K', 0x0b, 0x0b},  // HID_KEY_K
    {'l', 'L', 0x0c, 0x0c},  // HID_KEY_L
    {'m', 'M', 0x0d, 0x0d},  // HID_KEY_M
    {'n', 'N', 0x0e, 0x0e},  // HID_KEY_N
    {'o', 'O', 0x0f, 0x0f},  // HID_KEY_O
    {'p', 'P', 0x10, 0x10},  // HID_KEY_P
    {'q', 'Q', 0x11, 0x11},  // HID_KEY_Q
    {'r', 'R', 0x12, 0x12},  // HID_KEY_R
    {'s', 'S', 0x13, 0x13},  // HID_KEY_S
    {'t', 'T', 0x14, 0x14},  // HID_KEY_T
    {'u', 'U', 0x15, 0x15},  // HID_KEY_U
    {'v', 'V', 0x16, 0x16},  // HID_KEY_V
    {'w', 'W', 0x17, 0x17},  // HID_KEY_W
    {'x', 'X', 0x18, 0x18},  // HID_KEY_X
    {'y', 'Y', 0x19, 0x19},  // HID_KEY_Y
    {'z', 'Z', 0x1a, 0x1a},  // HID_KEY_Z
    {'1', '!', 0, 0},        // HID_KEY_1
    {'2', '@', 0x00, 0x00},  // HID_KEY_2
    {'3', '#', 0, 0},        // HID_KEY_3
    {'4', '$', 0, 0},        // HID_KEY_4
    {'5', '%', 0x1d, 0x1d},  // HID_KEY_5
    {'6', '^', 0x1e, 0x1e},  // HID_KEY_6
    {'7', '&', 0, 0},        // HID_KEY_7
    {'8', '*', 0x7f, 0x7f},  // HID_KEY_8
    {'9', '(', 0, 0},        // HID_KEY_9
    {'0', ')', 0, 0},        // HID_KEY_0
    {0x0d, 0x0d, 0, 0},      // HID_KEY_ENTER
    {0x1b, 0x1b, 0, 0},      // HID_KEY_ESC
    {0x08, 0x08, 0, 0},      // HID_KEY_DEL
    {0x09, 0, 0, 0},         // HID_KEY_TAB
    {' ', ' ', 0x00, 0x00},  // HID_KEY_SPACE
    {'-', '_', 0x1f, 0x1f},  // HID_KEY_MINUS
    {'=', '+', 0, 0},        // HID_KEY_EQUAL
    {'[', '{', 0x1b, 0x1b},  // HID_KEY_OPEN_BRACKET
    {']', '}', 0x1d, 0x1d},  // HID_KEY_CLOSE_BRACKET
    {'\\', '|', 0x1c, 0x1c}, // HID_KEY_BACK_SLASH
    {0, 0, 0, 0},            // HID_KEY_SHARP
    {';', ':', 0, 0},        // HID_KEY_COLON
    {'\'', '"', 0, 0},       // HID_KEY_QUOTE
    {'`', '~', 0x00, 0x1e},  // HID_KEY_TILDE
    {',', '<', 0, 0},        // HID_KEY_LESS
    {'.', '>', 0, 0},        // HID_KEY_GREATER
    {'/', '?', 0, 0},        // HID_KEY_SLASH
    {0, 0, 0, 0},            // HID_KEY_CAPS_LOCK
    {0, 0, 0, 0},            // HID_KEY_F1
    {0, 0, 0, 0},            // HID_KEY_F2
    {0, 0, 0, 0},            // HID_KEY_F3
    {0, 0, 0, 0},            // HID_KEY_F4
    {0, 0, 0, 0},            // HID_KEY_F5
    {0, 0, 0, 0},            // HID_KEY_F6
    {0, 0, 0, 0},            // HID_KEY_F7
    {0, 0, 0, 0},            // HID_KEY_F8
    {0, 0, 0, 0},            // HID_KEY_F9
    {0, 0, 0, 0},            // HID_KEY_F10
    {0, 0, 0, 0},            // HID_KEY_F11
    {0, 0, 0, 0},            // HID_KEY_F12
    {0, 0, 0, 0},            // HID_KEY_PRINT_SCREEN
    {0, 0, 0, 0},            // HID_KEY_SCROLL_LOCK
    {0, 0, 0, 0},            // HID_KEY_PAUSE
    {0, 0, 0, 0},            // HID_KEY_INSERT
    {0, 0, 0, 0},            // HID_KEY_HOME
    {0, 0, 0, 0},            // HID_KEY_PAGEUP
    {0x7f, 0x7f, 0, 0},      // HID_KEY_DELETE
    {0, 0, 0, 0},            // HID_KEY_END
    {0, 0, 0, 0},            // HID_KEY_PAGEDOWN
    {0, 0, 0, 0},            // HID_KEY_RIGHT
    {0, 0, 0, 0},            // HID_KEY_LEFT
    {0, 0, 0, 0},            // HID_KEY_DOWN
    {0, 0, 0, 0},            // HID_KEY_UP
    {0, 0, 0, 0},            // HID_KEY_NUM_LOCK
    {'/', '/', 0, 0},        // HID_KEY_KEYPAD_DIV
    {'*', '*', 0, 0},        // HID_KEY_KEYPAD_MUL
    {'-', '-', 0, 0},        // HID_KEY_KEYPAD_SUB
    {'+', '+', 0, 0},        // HID_KEY_KEYPAD_ADD
    {0x0d, 0x0d, 0, 0},      // HID_KEY_KEYPAD_ENTER
    {'1', '1', 0, 0},        // HID_KEY_KEYPAD_1
    {'2', '2', 0, 0},        // HID_KEY_KEYPAD_2
    {'3', '3', 0, 0},        // HID_KEY_KEYPAD_3
    {'4', '4', 0, 0},        // HID_KEY_KEYPAD_4
    {'5', '5', 0x1d, 0x1d},  // HID_KEY_KEYPAD_5
    {'6', '6', 0, 0},        // HID_KEY_KEYPAD_6
    {'7', '7', 0, 0},        // HID_KEY_KEYPAD_7
    {'8', '8', 0, 0},        // HID_KEY_KEYPAD_8
    {'9', '9', 0, 0},        // HID_KEY_KEYPAD_9
    {'0', '0', 0, 0},        // HID_KEY_KEYPAD_0
    {0, 0, 0, 0},            // HID_KEY_KEYPAD_DELETE
};

/**
 * @brief HID Keyboard get char symbol from key code
 *
 * @param[in] modifier  Keyboard modifier data
 * @param[in] key_code  Keyboard key code
 * @param[in] key_char  Pointer to key char data
 *
 * @return true  Key scancode converted successfully
 * @return false Key scancode unknown
 */
static inline bool hid_keyboard_get_char(uint8_t modifier, uint8_t key_code,
                                         unsigned char *key_char) {
    uint8_t mod = 0;
    if (modifier & (HID_LEFT_SHIFT | HID_RIGHT_SHIFT))
        mod |= 1;
    if (modifier & (HID_LEFT_CONTROL | HID_RIGHT_CONTROL))
        mod |= 2;

    uint8_t found = 0;
    if (key_code < (sizeof(keycode2ascii) / sizeof(keycode2ascii[0])))
        found = keycode2ascii[key_code][mod];
    if (found) {
        *key_char = found;
        return true;
    } else {
        return false;
    }
}

/**
 * @brief Key buffer scan code search.
 *
 * @param[in] src       Pointer to source buffer where to search
 * @param[in] key       Key scancode to search
 * @param[in] length    Size of the source buffer
 */
static inline bool key_found(const uint8_t *const src, uint8_t key,
                             unsigned int length) {
    for (unsigned int i = 0; i < length; i++) {
        if (src[i] == key) {
            return true;
        }
    }
    return false;
}

/**
 * @brief USB HID Host Keyboard Interface report callback handler
 *
 * @param[in] data    Pointer to input report data buffer
 * @param[in] length  Length of input report data buffer
 */
static void hid_host_keyboard_report_callback(const uint8_t *const data,
                                              const int length) {
    hid_keyboard_input_report_boot_t *kb_report =
        (hid_keyboard_input_report_boot_t *)data;

    if (length < sizeof(hid_keyboard_input_report_boot_t)) {
        return;
    }

    const bool alt_pressed =
        kb_report->modifier.val &&
        kb_report->modifier.val ==
            (kb_report->modifier.val & (HID_LEFT_ALT | HID_RIGHT_ALT));
    static uint32_t alt_code = 0;
    static uint8_t prev_keys[HID_KEYBOARD_KEY_MAX] = {0};
    unsigned char key_char;

    if (alt_code && !alt_pressed) {
        ESP_LOGI(TAG, "Alt-Code: Code %"PRIu32" (0x%"PRIx32") completed", alt_code, alt_code);
        if (key_char_callback) {
            // convert the alt code unicode code point into utf8 bytes
            if (alt_code <= 0x7f) {
                // 1-byte sequence: 0xxxxxxx
                key_char_callback((char)alt_code);
            } else if (alt_code <= 0x7ff) {
                // 2-byte sequence: 110xxxxx 10xxxxxx
                key_char_callback((char)(0xc0 | ((alt_code >>  6) & 0x1f)));
                key_char_callback((char)(0x80 | ( alt_code        & 0x3f)));
            } else if (alt_code <= 0xffff) {
                // 3-byte sequence: 1110xxxx 10xxxxxx 10xxxxxx
                key_char_callback((char)(0xe0 | ((alt_code >> 12) & 0x0f)));
                key_char_callback((char)(0x80 | ((alt_code >>  6) & 0x3f)));
                key_char_callback((char)(0x80 | ( alt_code        & 0x3f)));
            } else if (alt_code <= 0x10ffff) {
                // 4-byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
                key_char_callback((char)(0xf0 | ((alt_code >> 18) & 0x07)));
                key_char_callback((char)(0x80 | ((alt_code >> 12) & 0x3f)));
                key_char_callback((char)(0x80 | ((alt_code >>  6) & 0x3f)));
                key_char_callback((char)(0x80 | ( alt_code        & 0x3f)));
            } else {
                // Invalid code point, emit replacement character U+FFFD
                key_char_callback((char)0xef);
                key_char_callback((char)0xbf);
                key_char_callback((char)0xbd);
            }
        }
        alt_code = 0;
    }
    for (int i = 0; i < HID_KEYBOARD_KEY_MAX; i++) {
        // figure out which keys have been just pressed
        if (kb_report->key[i] > HID_KEY_ERROR_UNDEFINED &&
            !key_found(prev_keys, kb_report->key[i], HID_KEYBOARD_KEY_MAX)) {
            if (hid_keyboard_get_char(kb_report->modifier.val,
                                      kb_report->key[i], &key_char)) {
                if (alt_pressed) {
                    if (key_char >= '0' && key_char <= '9') {
                        ESP_LOGI(TAG, "Alt-Code: Key %d pressed -> ASCII %x",
                                 kb_report->key[i], key_char);
                        alt_code = 10 * alt_code + (key_char - '0');
                    } else {
                        ESP_LOGW(
                            TAG,
                            "Alt-Code: Key %d pressed -> ASCII %x (ignoring)",
                            kb_report->key[i], key_char);
                    }
                    if (key_char_callback)
                        key_char_callback(0);
                } else {
                    ESP_LOGI(TAG, "Key %d pressed -> ASCII %x",
                             kb_report->key[i], key_char);
                    if (key_char_callback)
                        key_char_callback(key_char);
                }
            } else {
                ESP_LOGI(TAG, "Key %d pressed -> no matching ASCII",
                         kb_report->key[i]);
            }
        }
    }

    memcpy(prev_keys, &kb_report->key, HID_KEYBOARD_KEY_MAX);
}

/**
 * @brief USB HID Host interface callback
 *
 * @param[in] hid_device_handle  HID Device handle
 * @param[in] event              HID Host interface event
 * @param[in] arg                Pointer to arguments, does not used
 */
void hid_host_interface_event(hid_host_device_handle_t hid_device_handle,
                              const hid_host_interface_event_t event,
                              void *arg) {
    uint8_t data[64] = {0};
    size_t data_length = 0;
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

    switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
        ESP_ERROR_CHECK(hid_host_device_get_raw_input_report_data(
            hid_device_handle, data, sizeof(data), &data_length));
        hid_host_keyboard_report_callback(data, data_length);
        break;
    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HID Device DISCONNECTED");
        ESP_ERROR_CHECK(hid_host_device_close(hid_device_handle));
        break;
    case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
        ESP_LOGI(TAG, "HID Device TRANSFER_ERROR");
        break;
    default:
        ESP_LOGE(TAG, "HID Device Unhandled event");
        break;
    }
}

void hid_host_interface_callback(hid_host_device_handle_t hid_device_handle,
                                 const hid_host_interface_event_t event,
                                 void *arg) {
    const app_event_queue_t evt_queue = {.event_group =
                                             APP_EVENT_HID_HOST_INTERFACE,
                                         .device_handle = hid_device_handle,
                                         .interface_event = event,
                                         .arg = arg};

    if (app_event_queue) {
        xQueueSend(app_event_queue, &evt_queue, 0);
    }
}

/**
 * @brief USB HID Host Device event
 *
 * @param[in] hid_device_handle  HID Device handle
 * @param[in] event              HID Host Device event
 * @param[in] arg                Pointer to arguments, does not used
 */
void hid_host_device_event(hid_host_device_handle_t hid_device_handle,
                           const hid_host_driver_event_t event, void *arg) {
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle, &dev_params));

    switch (event) {
    case HID_HOST_DRIVER_EVENT_CONNECTED:
        ESP_LOGI(TAG, "HID Device, protocol %d CONNECTED", dev_params.proto);
        if (dev_params.proto != HID_PROTOCOL_KEYBOARD) {
            ESP_LOGE(TAG, "Error: can only support HID_PROTOCOL_KEYBOARD");
            break;
        }
        if (dev_params.sub_class != HID_SUBCLASS_BOOT_INTERFACE) {
            ESP_LOGE(TAG,
                     "Error: can only support HID_SUBCLASS_BOOT_INTERFACE");
            break;
        }

        const hid_host_device_config_t dev_config = {
            .callback = hid_host_interface_callback, .callback_arg = NULL};

        ESP_ERROR_CHECK(hid_host_device_open(hid_device_handle, &dev_config));

        // At least the Tera HW0007 barcode reader needs this call to
        // actually report any keypresses later
        size_t dummy = 0;
        hid_host_get_report_descriptor(hid_device_handle, &dummy);

        ESP_ERROR_CHECK(hid_class_request_set_protocol(
            hid_device_handle, HID_REPORT_PROTOCOL_BOOT));
        ESP_ERROR_CHECK(hid_class_request_set_idle(hid_device_handle, 0, 0));
        ESP_ERROR_CHECK(hid_host_device_start(hid_device_handle));
        break;
    default:
        break;
    }
}

/**
 * @brief Start USB Host install and handle common USB host library events
 *
 * @param[in] arg  Not used
 */
static void usb_lib_task(void *arg) {
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };

    ESP_ERROR_CHECK(usb_host_install(&host_config));
    xTaskNotifyGive(arg);

    while (true) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
    }
}

/**
 * @brief HID Host Device callback
 *
 * Puts new HID Device event to the queue
 *
 * @param[in] hid_device_handle HID Device handle
 * @param[in] event             HID Device event
 * @param[in] arg               Not used
 */
void hid_host_device_callback(hid_host_device_handle_t hid_device_handle,
                              const hid_host_driver_event_t event, void *arg) {
    const app_event_queue_t evt_queue = {.event_group =
                                             APP_EVENT_HID_HOST_DEVICE,
                                         .device_handle = hid_device_handle,
                                         .driver_event = event,
                                         .arg = arg};

    if (app_event_queue) {
        xQueueSend(app_event_queue, &evt_queue, 0);
    }
}

void usb_hid_start(key_char_cb_t key_char_cb) {
    BaseType_t task_created;
    ESP_LOGI(TAG, "Keyboard HID Host");

    key_char_callback = key_char_cb;

    /*
     * Create usb_lib_task to:
     * - initialize USB Host library
     * - Handle USB Host events
     */
    task_created =
        xTaskCreatePinnedToCore(usb_lib_task, "usb_events", 4096,
                                xTaskGetCurrentTaskHandle(), 2, NULL, 0);
    assert(task_created == pdTRUE);

    // Wait for notification from usb_lib_task to proceed
    ulTaskNotifyTake(false, 1000);

    app_event_queue = xQueueCreate(10, sizeof(app_event_queue_t));

    /*
     * HID host driver configuration
     * - create background task for handling low level event inside the HID
     * driver
     * - provide the device callback to get new HID Device connection event
     */
    const hid_host_driver_config_t hid_host_driver_config = {
        .create_background_task = true,
        .task_priority = 5,
        .stack_size = 4096,
        .core_id = 0,
        .callback = hid_host_device_callback,
        // .callback = hid_host_device_event,
        .callback_arg = NULL};

    ESP_ERROR_CHECK(hid_host_install(&hid_host_driver_config));
}

void usb_hid_handle_events(void) {
    app_event_queue_t evt_queue;
    if (xQueueReceive(app_event_queue, &evt_queue, pdMS_TO_TICKS(10))) {
        if (APP_EVENT_HID_HOST_DEVICE == evt_queue.event_group) {
            hid_host_device_event(evt_queue.device_handle,
                                  evt_queue.driver_event, evt_queue.arg);
        } else if (APP_EVENT_HID_HOST_INTERFACE == evt_queue.event_group) {
            hid_host_interface_event(evt_queue.device_handle,
                                     evt_queue.interface_event, evt_queue.arg);
        }
    }
}
