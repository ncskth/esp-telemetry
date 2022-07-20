#include <mdns.h>
#include <sys/socket.h>
#include <netdb.h>
#include <esp_netif.h>
#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <hal/gpio_hal.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <driver/spi_slave.h>
#include <hal/spi_hal.h>
#include <driver/gpio.h>

#include "hardware.h"
#include "wifi.h"

void init();
void loop();

uint16_t rx = 0;
DMA_ATTR uint8_t spi_buf1[SPI_MAX_DMA_LEN + 1];
DMA_ATTR uint8_t spi_buf2[SPI_MAX_DMA_LEN + 1];

spi_slave_transaction_t spi_trans1 = {
    .rx_buffer = spi_buf1,
    .length = SPI_MAX_DMA_LEN * 8,
};
spi_slave_transaction_t spi_trans2 = {
    .rx_buffer = spi_buf2,
    .length = SPI_MAX_DMA_LEN * 8,
};

void app_main() {
    init();
}

IRAM_ATTR void spi_post_trans_cb(spi_slave_transaction_t *trans) {
    GPIO.out_w1tc.val = (1<<PIN_DEBUG); //make pin low
    spi_slave_get_trans_result(SPI2_HOST, &trans, 0);
    send_data(trans);
    spi_slave_queue_trans(SPI2_HOST, trans, 0);
    rx = trans->trans_len / 8;
}

IRAM_ATTR void spi_post_setup_cb(spi_slave_transaction_t *trans) {
    GPIO.out_w1ts.val = (1<<PIN_DEBUG); //make pin high
}

void init() {
    // misc
    gpio_set_direction(PIN_DEBUG, GPIO_MODE_OUTPUT);
    esp_event_loop_create_default();
    nvs_flash_init();
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    init_wifi();
    printf("chungus\n");

    // SPI
    spi_bus_config_t bus_conf = {
        .max_transfer_sz = SPI_MAX_DMA_LEN,
        .miso_io_num = PIN_MISO,
        .mosi_io_num = -1, //only one way
        .sclk_io_num = PIN_SCK,
        .quadhd_io_num = -1,
        .quadwp_io_num = -1,
    };
    spi_slave_interface_config_t slave_conf = {
        .post_trans_cb = spi_post_trans_cb,
        .post_setup_cb = spi_post_setup_cb,
        .queue_size = 2,
        .spics_io_num = PIN_CS
    };
    ESP_ERROR_CHECK(spi_slave_initialize(SPI2_HOST, &bus_conf, &slave_conf, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_slave_queue_trans(SPI2_HOST, &spi_trans1, 0));
    ESP_ERROR_CHECK(spi_slave_queue_trans(SPI2_HOST, &spi_trans2, 0));

    while (true) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        printf("rx %d\n", rx);
        rx = 0;
    }
}

void loop() {

}