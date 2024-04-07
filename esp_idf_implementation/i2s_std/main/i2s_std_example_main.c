/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdint.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_pdm.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "sdkconfig.h"

// screw header files
void fillBufEngineSound(int16_t *buf, size_t size, bool revUp);
void initEngineAudio();

static i2s_chan_handle_t tx_chan; // I2S tx channel handler
#define BUFSIZE 1024
static int16_t* buf1;
static int16_t* buf2;
QueueHandle_t bufFilledQueue;
SemaphoreHandle_t bufTransmittedSem;

void i2s_write_task(void *args)
{
    int16_t* bufToSend = buf1;
    while (true)
    {
        fflush(stdout);
        if(xQueueReceive(bufFilledQueue, &bufToSend ,pdMS_TO_TICKS(1000)) == pdFALSE){
            printf("buffer not filled");
            break;
        }
        fflush(stdout);
        i2s_channel_write(tx_chan, bufToSend, BUFSIZE*sizeof(int16_t), NULL, 1000);
        printf("buffer tx ed \n");
        xSemaphoreGive(bufTransmittedSem);
        bufToSend = (bufToSend == buf1) ? buf2 : buf1;
    }
    vTaskDelete(NULL);
}

void app_main(void)
{
    i2s_chan_config_t tx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&tx_chan_cfg, &tx_chan, NULL));
    i2s_pdm_tx_config_t tx_cfg = {
        .clk_cfg = I2S_PDM_TX_CLK_DAC_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_PDM_TX_SLOT_DAC_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = GPIO_NUM_16,
            .dout = GPIO_NUM_15,
            .invert_flags = {
                .clk_inv = false,
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_pdm_tx_mode(tx_chan, &tx_cfg));

    gpio_config_t btncfg = {
        .pin_bit_mask = BIT64(GPIO_NUM_40),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&btncfg);

    buf1 = heap_caps_malloc(BUFSIZE*sizeof(int16_t), MALLOC_CAP_DMA);
    buf2 = heap_caps_malloc(BUFSIZE*sizeof(int16_t), MALLOC_CAP_DMA);

    initEngineAudio();
    fillBufEngineSound(buf1, BUFSIZE, false);

    size_t w_bytes = BUFSIZE*sizeof(int16_t);
    /* (Optional) Preload the data before enabling the TX channel, so that the valid data can be transmitted immediately */
    while (w_bytes == BUFSIZE*sizeof(int16_t))
    {
        /* Here we load the target buffer repeatedly, until all the DMA buffers are preloaded */
        ESP_ERROR_CHECK(i2s_channel_preload_data(tx_chan, buf1, BUFSIZE*sizeof(int16_t), &w_bytes));
    }

    bufFilledQueue = xQueueCreate(10,sizeof(int16_t*));
    bufTransmittedSem = xSemaphoreCreateBinary();

    /* Enable the TX channel */
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));
    xTaskCreate(i2s_write_task, "i2s_write", 4096, NULL, 2, NULL);
    
    int16_t* bufToFill = buf2;
    xQueueSend(bufFilledQueue, &buf1, pdMS_TO_TICKS(1000));
    fflush(stdout);
    while (1)
    {
        bool revUp = !gpio_get_level(GPIO_NUM_40);
        // printf("bufFilling \n");
        fillBufEngineSound(bufToFill, BUFSIZE, revUp);
        //wait for previous tx to finish 
        vTaskDelay(pdMS_TO_TICKS(10));
        if(xSemaphoreTake(bufTransmittedSem,pdMS_TO_TICKS(1000)) == pdFALSE){
            printf("buf wasnt transmitted");
            break;
        }
        //tell that new tx is available
        xQueueSend(bufFilledQueue, &bufToFill, pdMS_TO_TICKS(1000));
        printf("buf sent for transmission\n");
        if (bufToFill == buf1)
            bufToFill = buf2;
        else
            bufToFill = buf1;
    }
    vTaskDelete(NULL);
}
