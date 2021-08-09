/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/types.h>

#include <drivers/uart.h>
#include <logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <init.h>

#define POSITION_STATE_DATA_LEN 16
uint8_t position_state[POSITION_STATE_DATA_LEN];

const struct device* uart = NULL;

K_MSGQ_DEFINE(peripheral_event_msgq, sizeof(struct zmk_position_state_changed),
  CONFIG_ZMK_SPLIT_UART_CENTRAL_POSITION_QUEUE_SIZE, 4);

void peripheral_event_work_callback(struct k_work *work) {
  struct zmk_position_state_changed ev;
  while (k_msgq_get(&peripheral_event_msgq, &ev, K_NO_WAIT) == 0) {
    LOG_DBG("Trigger key position state change for %d", ev.position);
    ZMK_EVENT_RAISE(new_zmk_position_state_changed(ev));
  }
}

K_WORK_DEFINE(peripheral_event_work, peripheral_event_work_callback);

static void peripheral_event_process(void)
{
  static uint8_t last_position_state[POSITION_STATE_DATA_LEN] = {};
  uint8_t changed_positions[POSITION_STATE_DATA_LEN];

  for (int i = 0; i < POSITION_STATE_DATA_LEN; i++) {
    changed_positions[i] = position_state[i] ^ last_position_state[i];
    last_position_state[i] = position_state[i];
    LOG_DBG("received: %d", position_state[i]);
  }

  for (int i = 0; i < POSITION_STATE_DATA_LEN; i++) {
    for (int j = 0; j < 8; j++) {
      if (changed_positions[i] & BIT(j)) {
        LOG_DBG("changed %d %d", i, j);
        uint32_t position = (i * 8) + j;
        bool pressed = position_state[i] & BIT(j);
        struct zmk_position_state_changed ev = {
            .position = position, .state = pressed, .timestamp = k_uptime_get()};

        k_msgq_put(&peripheral_event_msgq, &ev, K_NO_WAIT);
        k_work_submit(&peripheral_event_work);
      }
    }
  }
}

static void uart_callback(const struct device* dev,
  struct uart_event* evt, void* user_data)
{
  if (evt->type == UART_RX_DISABLED)
  {
    peripheral_event_process();
    uart_rx_enable(dev, position_state, POSITION_STATE_DATA_LEN, 0);
  }
}

int zmk_split_uart_central_init(const struct device* _arg)
{
  LOG_DBG("");
  uart = device_get_binding(DT_LABEL(DT_CHOSEN(zmk_split_uart)));

  uart_callback_set(uart, uart_callback, NULL);
  uart_rx_enable(uart, position_state, POSITION_STATE_DATA_LEN, 0);

  return 0;
}

SYS_INIT(zmk_split_uart_central_init, APPLICATION, CONFIG_ZMK_UART_INIT_PRIORITY);
