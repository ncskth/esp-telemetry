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

int udp_socket = -1;

void init();
void loop();

void init_wifi();

void app_main() {
    init();
    xTaskCreate(loop, "loop", 40000, NULL, 5, NULL);
}

void init() {
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
    spi_bus_config_t conf;
    spi_bus_initialize(SPI2_HOST, &conf, SPI_DMA_CH_AUTO);
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "esp_telemetry",
            .password = "esp_telemetry",
            .ssid_len = strlen("esp_telemetry"),
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .max_connection = 4,
            .channel = 7,
        }
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_ps(WIFI_PS_NONE);

    udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    fcntl(udp_socket, F_SETFL, O_NONBLOCK);
}

void loop() {
    uint8_t buf[3048];
    uint8_t counter = 0;
    while (true) {
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = inet_addr("192.168.4.2");
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(1338);
        buf[0] = counter++;
        // sendto(udp_socket, buf, 1000, MSG_DONTWAIT, &dest_addr, sizeof(dest_addr));
        sendto(udp_socket, buf, 1500, MSG_DONTWAIT, &dest_addr, sizeof(dest_addr));
        sendto(udp_socket, buf, 1500, MSG_DONTWAIT, &dest_addr, sizeof(dest_addr));
        if (sendto(udp_socket, buf, 1500, MSG_DONTWAIT, &dest_addr, sizeof(dest_addr) == 0)) {
            printf("sent\n");
        } else {
            printf("didn't send %d\n", errno);
        }
        vTaskDelay(1);
    }
}