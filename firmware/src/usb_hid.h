// SPDX-FileCopyrightText: © 2025 Stefan Siegel <ssiegel@sdas.net>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*key_char_cb_t)(char);
void usb_hid_start(key_char_cb_t);
void usb_hid_handle_events(void);

#ifdef __cplusplus
}
#endif
