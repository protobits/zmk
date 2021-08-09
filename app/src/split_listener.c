/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_split_listener

#include <device.h>
#include <power/reboot.h>
#include <logging/log.h>

#if defined(CONFIG_ZMK_SPLIT_BLE)
#include <zmk/split/bluetooth/service.h>
#elif defined(CONFIG_ZMK_SPLIT_UART)
#include <zmk/split/uart/peripheral.h>
#endif

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/hid.h>
#include <zmk/endpoints.h>

int split_listener(const zmk_event_t *eh) {
    LOG_DBG("");
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev != NULL) {
        if (ev->state) {
#if defined(CONFIG_ZMK_SPLIT_BLE)
            return zmk_split_bt_position_pressed(ev->position);
        } else {
            return zmk_split_bt_position_released(ev->position);
#elif defined(CONFIG_ZMK_SPLIT_UART)
            return zmk_split_uart_position_pressed(ev->position);
        } else {
            return zmk_split_uart_position_released(ev->position);
#endif
        }
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(split_listener, split_listener);
ZMK_SUBSCRIPTION(split_listener, zmk_position_state_changed);