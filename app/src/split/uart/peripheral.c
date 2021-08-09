/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <devicetree.h>
#include <zephyr/types.h>
#include <sys/util.h>
#include <init.h>

#include <logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <drivers/uart.h>
#include <zmk/matrix.h>

#define POS_STATE_LEN 16

static uint8_t position_state[POS_STATE_LEN];
K_SEM_DEFINE(tx_done, 0, 1);

const struct device* uart = NULL;

K_THREAD_STACK_DEFINE(service_q_stack, CONFIG_ZMK_SPLIT_UART_PERIPHERAL_STACK_SIZE);

struct k_work_q service_work_q;

K_MSGQ_DEFINE(position_state_msgq, sizeof(char[POS_STATE_LEN]),
              CONFIG_ZMK_SPLIT_UART_PERIPHERAL_POSITION_QUEUE_SIZE, 4);

void send_position_state_callback(struct k_work *work) {
  uint8_t state[POS_STATE_LEN];
  //int i = 0;

  while (k_msgq_get(&position_state_msgq, &state, K_NO_WAIT) == 0) {
    uart_tx(uart, state, sizeof(state), 0);
    k_sem_take(&tx_done, K_FOREVER);
    /*for (i = 0; i < 16; i++)
    {
      uart_poll_out(uart, i);
      k_msleep(10);
    }*/
  }
};

K_WORK_DEFINE(service_position_notify_work, send_position_state_callback);

int send_position_state() {
    int err = k_msgq_put(&position_state_msgq, position_state, K_MSEC(100));
    if (err) {
        switch (err) {
        case -EAGAIN: {
            LOG_WRN("Position state message queue full, popping first message and queueing again");
            uint8_t discarded_state[POS_STATE_LEN];
            k_msgq_get(&position_state_msgq, &discarded_state, K_NO_WAIT);
            return send_position_state();
        }
        default:
            LOG_WRN("Failed to queue position state to send (%d)", err);
            return err;
        }
    }

    k_work_submit_to_queue(&service_work_q, &service_position_notify_work);

    return 0;
}

int zmk_split_uart_position_pressed(uint8_t position) {
    WRITE_BIT(position_state[position / 8], position % 8, true);
    return send_position_state();
}

int zmk_split_uart_position_released(uint8_t position) {
    WRITE_BIT(position_state[position / 8], position % 8, false);
    return send_position_state();
}

static void uart_callback(const struct device* dev,
  struct uart_event* evt, void* user_data)
{
  if (evt->type == UART_TX_DONE)
  {
    LOG_DBG("TX done");
    k_sem_give(&tx_done);
  }
}

int service_init(const struct device *_arg) {
    k_work_q_start(&service_work_q, service_q_stack, K_THREAD_STACK_SIZEOF(service_q_stack),
      CONFIG_ZMK_SPLIT_UART_PERIPHERAL_PRIORITY);

    uart = device_get_binding(DT_LABEL(DT_CHOSEN(zmk_split_uart)));

    uart_callback_set(uart, uart_callback, NULL);

    return 0;
}

SYS_INIT(service_init, APPLICATION, CONFIG_ZMK_UART_INIT_PRIORITY);
