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
static int16_t buf1[BUFSIZE];
static int16_t buf2[BUFSIZE];
SemaphoreHandle_t bufFilledSem;
SemaphoreHandle_t bufTransmittedSem;

void i2s_write_task(void *args)
{
    int16_t* bufToSend = buf1;
    while (true)
    {
        if(xSemaphoreTake(bufFilledSem, pdMS_TO_TICKS(1000)) == pdFALSE){
            printf("buffer not filled");
            break;
        }
        i2s_channel_write(tx_chan, bufToSend, BUFSIZE*sizeof(int16_t), NULL, 1000);
        xSemaphoreGive(bufTransmittedSem);
        bufToSend = (bufToSend == buf1) ? buf2 : buf1;
    }
    vTaskDelete(NULL);
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

    gpio_config_t btncfg = {
        .pin_bit_mask = BIT64(GPIO_NUM_16),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&btncfg);

    initEngineAudio();
    fillBufEngineSound(buf1, BUFSIZE, false);

    size_t w_bytes = BUFSIZE*sizeof(int16_t);
    /* (Optional) Preload the data before enabling the TX channel, so that the valid data can be transmitted immediately */
    while (w_bytes == BUFSIZE*sizeof(int16_t))
    {
        /* Here we load the target buffer repeatedly, until all the DMA buffers are preloaded */
        ESP_ERROR_CHECK(i2s_channel_preload_data(tx_chan, buf1, BUFSIZE*sizeof(int16_t), &w_bytes));
    }

    bufFilledSem = xSemaphoreCreateBinary();
    bufTransmittedSem = xSemaphoreCreateBinary();
    
    /* Enable the TX channel */
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));
    xTaskCreate(i2s_write_task, "i2s_write", 4096, NULL, 2, NULL);
    
    int16_t* bufToFill = buf2;
    xSemaphoreGive(bufFilledSem);
    while (1)
    {
        bool revUp = !gpio_get_level(GPIO_NUM_16);
        // printf("bufFilling \n");
        fillBufEngineSound(bufToFill, BUFSIZE, revUp);
        //wait for previous tx to finish 
        vTaskDelay(pdMS_TO_TICKS(10));
        if(xSemaphoreTake(bufTransmittedSem,pdMS_TO_TICKS(1000)) == pdFALSE){
            printf("buf wasnt transmitted");
            break;
        }
        //tell that new tx is available
        xSemaphoreGive(bufTransmittedSem);
        bufToFill = (bufToFill == buf1) ? buf2 : buf1;
    }
    vTaskDelete(NULL);
}
