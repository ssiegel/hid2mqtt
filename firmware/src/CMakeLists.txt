# SPDX-FileCopyrightText: © 2025 Stefan Siegel <ssiegel@sdas.net>
#
# SPDX-License-Identifier: GPL-3.0-or-later

idf_component_register(
    SRCS
        config_lock.c
        main.c
        mqtt.c
        ota.c
        qr_provisioning.c
        usb_hid.c
        wifi.c
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        esp_netif
        esp_event
        esp_wifi
        mqtt
        usb
        usb_host_hid
        log
)
