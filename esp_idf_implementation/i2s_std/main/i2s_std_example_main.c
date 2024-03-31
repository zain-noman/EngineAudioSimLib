/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdint.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "sdkconfig.h"

// screw header files
void fillBufEngineSound(int16_t *buf, size_t size, bool revUp);
void initEngineAudio();

static i2s_chan_handle_t tx_chan; // I2S tx channel handler

#define BUFSIZE 1024

IRAM_ATTR bool i2s_send_isr(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx)
{
    QueueHandle_t i2s_done_queue = *((QueueHandle_t *)user_ctx);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(i2s_done_queue, event, &xHigherPriorityTaskWoken);
    return xHigherPriorityTaskWoken == pdTRUE;
}

void app_main(void)
{
    i2s_chan_config_t tx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&tx_chan_cfg, &tx_chan, NULL));
    i2s_std_config_t tx_std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED, // some codecs may require mclk signal, this example doesn't need it
            .bclk = GPIO_NUM_16,
            .ws = GPIO_NUM_17,
            .dout = GPIO_NUM_15,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &tx_std_cfg));
    i2s_event_callbacks_t i2s_callbacks = {
        .on_recv = NULL,
        .on_recv_q_ovf = NULL,
        .on_sent = i2s_send_isr,
        .on_send_q_ovf = NULL,
    };
    QueueHandle_t i2s_done_queue = xQueueCreate(10, sizeof(i2s_event_data_t));
    i2s_channel_register_event_callback(tx_chan, &i2s_callbacks, &i2s_done_queue);

    gpio_config_t btncfg = {
        .pin_bit_mask = BIT64(GPIO_NUM_16),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&btncfg);

    int16_t *w_buf = (int16_t *)calloc(sizeof(int16_t), BUFSIZE);
    assert(w_buf); // Check if w_buf allocation success
    initEngineAudio();
    fillBufEngineSound(w_buf, BUFSIZE,false);
    
    size_t w_bytes = BUFSIZE;
    /* (Optional) Preload the data before enabling the TX channel, so that the valid data can be transmitted immediately */
    while (w_bytes == BUFSIZE)
    {
        /* Here we load the target buffer repeatedly, until all the DMA buffers are preloaded */
        ESP_ERROR_CHECK(i2s_channel_preload_data(tx_chan, w_buf, BUFSIZE, &w_bytes));
    }

    /* Enable the TX channel */
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));

    while (1)
    {
        i2s_event_data_t evnt;
        if (xQueueReceive(i2s_done_queue, &evnt, pdMS_TO_TICKS(1000)) == pdFALSE)
        {
            printf("Write Task: i2s write failed\n");
            continue;
        }
        bool revUp = !gpio_get_level(GPIO_NUM_16);
        fillBufEngineSound(evnt.data, evnt.size,revUp);
        
        if (uxQueueMessagesWaiting(i2s_done_queue) != 0){
            printf("underflow \n");
        }
    }
    free(w_buf);
    vTaskDelete(NULL);
}
