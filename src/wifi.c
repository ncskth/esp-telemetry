#include <sys/socket.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <nvs.h>
#include <mdns.h>
#include <netdb.h>
#include <driver/spi_slave.h>
#include <errno.h>
#include <string.h>

#define DEFAULT_SSID "NCSpeople"
#define DEFAULT_PASSWORD "peopleNCS"

#define TCP_PORT 55671

// #define AP

uint8_t user_ssid[32];
uint8_t user_password[64];

uint16_t udp_port = -1;
int udp_socket = -1;
int tcp_socket = -1;

int udp_enabled = false;
bool wifi_connected = false;
volatile struct sockaddr_in udp_addr;
TaskHandle_t udp_sender_handle;
QueueHandle_t udp_queue;
spi_slave_transaction_t *trans_to_send;

wifi_config_t wifi_config = {
    .sta = {
        .ssid = DEFAULT_SSID,
        .password = DEFAULT_PASSWORD,
        .scan_method = WIFI_ALL_CHANNEL_SCAN,
        .bssid_set = 0,
        .channel = 0,
        .listen_interval = 0,
        .pmf_cfg = {
            .required = 0
        },
        .rm_enabled = 1,
        .btm_enabled = 1,
    },
};



wifi_config_t wifi_config_ap = {
    .ap = {
        .ssid = "bajskorvimunnen",
        .ssid_len = strlen("bajskorvimunnen"),
        .password = "bajskorvimunnen",
        .authmode = WIFI_AUTH_WPA_WPA2_PSK,
        .max_connection = 10,
    },
};

void scan_wifi() {
    esp_wifi_scan_start(NULL, true);
    uint16_t number = 64;
    uint16_t ap_count;
    wifi_ap_record_t ap_info[64];

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    printf("networks found %d\n", ap_count);
    for (int i = 0; i < ap_count; i++) {
        printf("-----------------------------------------\n");
        printf("SSID:\t%s RSSI:\t%d channel:\t%d\n", ap_info[i].ssid, ap_info[i].rssi, ap_info[i].primary);
    }
}

IRAM_ATTR void send_data(spi_slave_transaction_t *trans) {
    int has_awaken;
    // xQueueSendFromISR(udp_queue, trans, &has_awaken);
    vTaskNotifyGiveFromISR(udp_sender_handle, &has_awaken);
    trans_to_send = trans;
    portYIELD_FROM_ISR();
}

// returns 1 if error
bool yielding_read(int client, uint8_t *buf, uint8_t len) {
    uint8_t received = 0;
    while (received != len) {
        int bytes_received = recv(client, buf + received, len - received, MSG_DONTWAIT);

        if (bytes_received < 0 ) {
            if (errno == ERR_WOULDBLOCK) {
                vTaskDelay(10);
                continue;
            }
            if (errno == ENOTCONN) {
                return 1;
                break;
            }

            vTaskDelay(10);
            continue;
        }
        if (bytes_received > 0) {
            received += bytes_received;
            printf("got %d bytes\n", bytes_received);
        }
    }
    return 0;
}

void udp_sender_task() {
    while (true) {
        uint8_t buf[4096] = "LOLCAT123";
        spi_slave_transaction_t trans;
        // xQueueReceive(udp_queue, &trans, portMAX_DELAY);
        // xTaskNotifyWait(0, 0, NULL, portMAX_DELAY);
        vTaskDelay(1);
        trans = *trans_to_send;
        if (udp_enabled && wifi_connected) {
            // int e = sendto(udp_socket, trans.rx_buffer, trans.trans_len / 8, MSG_DONTWAIT, &udp_addr, sizeof(udp_addr))
            sendto(udp_socket, buf, sizeof(buf), MSG_DONTWAIT, &udp_addr, sizeof(udp_addr));
            // printf("sent %d %d %d %dbytes\n", trans.length, trans.user, e, errno);
        }
    }
}

void tcp_server_task(void *user) {
    struct sockaddr_in remote_addr;
	unsigned int socklen;
	socklen = sizeof(remote_addr);
    uint8_t rx_buf[128];
    uint8_t tx_buf[32];

    while (1) {
        vTaskDelay(10);
        if (!wifi_connected) {
            // printf("no wifi\n");
            vTaskDelay(100);
            continue;
        }
        int client_socket = -1;
        client_socket = accept(tcp_socket,(struct sockaddr *)&remote_addr, &socklen);

        if (client_socket < 0) {
            // printf("no client\n");
            vTaskDelay(100);
            continue;
        }
        fcntl(tcp_socket, F_SETFL, O_NONBLOCK);
        printf("got client\n");
        // handle client loop
        while (1) {
            uint8_t id;
            if (yielding_read(client_socket, &id, 1)) {
                break;
            }

            printf("got id %d\n", id);
            if (id == 0) { // handshake
                if (yielding_read(client_socket, rx_buf, 96)) {
                    break;
                }
                uint8_t response = 42;
                send(client_socket, &response, 1, 0);
            }

            if (id == 1) { // update wifi config
                if (yielding_read(client_socket, rx_buf, 96)) {
                    break;
                }
                memcpy(user_ssid, rx_buf, 32);
                memcpy(user_password, rx_buf + 32, 64);
                nvs_handle_t nvs;
                nvs_open("storage", NVS_READWRITE, &nvs);
                nvs_set_blob(nvs, "user_ssid", user_ssid, sizeof(user_ssid));
                nvs_set_blob(nvs, "user_password", user_password, sizeof(user_password));
                uint8_t ack = 1;
                send(client_socket, &ack, 1, 0);
            }

            if (id == 32) { // start streaming
                if (yielding_read(client_socket, rx_buf, 6)) {
                    break;
                }
                uint32_t addr;
                uint16_t port;
                memcpy(&addr, rx_buf, 4); // get ip, network order
                memcpy(&port, rx_buf + 4, 2); // get port, network order
                udp_addr.sin_addr.s_addr = addr;
                udp_addr.sin_port = port;
                udp_addr.sin_family = AF_INET;
                udp_enabled = 1;
                char* char_addr = inet_ntoa(udp_addr);
                uint8_t ack = 1;
                send(client_socket, &ack, 1, 0);
            }

            vTaskDelay(100);
        }// handle client loop
    }// accept client loop
}

void got_ip_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data) {
    printf("wifi connected\n");
    struct sockaddr_in tcpServerAddr = {
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_family = AF_INET,
        .sin_port = htons(TCP_PORT)
    };
    mdns_init();
    char hostname[16];
    sprintf(hostname, "esp_telemetry");
    mdns_hostname_set(hostname);

    tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    bind(tcp_socket, (struct sockaddr *)&tcpServerAddr, sizeof(tcpServerAddr));
    listen(tcp_socket, 1);
    fcntl(tcp_socket, F_SETFL, O_NONBLOCK);

    udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    fcntl(udp_socket, F_SETFL, O_NONBLOCK);
    wifi_connected = 1;
}

void wifi_disconnected_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data) {
    printf("wifi disconnected\n");
    if (wifi_connected) {
        close(tcp_socket);
        close(udp_socket);
        mdns_free();
    }
    wifi_connected = 0;

    static uint8_t ping = 0;
    if (ping) {
        memcpy(wifi_config.sta.ssid, user_ssid, sizeof(user_ssid));
        memcpy(wifi_config.sta.password, user_password, sizeof(user_password));
        ping = 0;
    } else {
        memcpy(wifi_config.sta.ssid, DEFAULT_SSID, sizeof(DEFAULT_SSID));
        memcpy(wifi_config.sta.password, DEFAULT_PASSWORD, sizeof(DEFAULT_PASSWORD));
        ping = 1;
    }

    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_connect(); // a failed connect sends a disconnected event
}

void init_wifi () {
    esp_netif_init();

    #ifdef AP
    esp_netif_create_default_wifi_ap();
    #else
    esp_netif_create_default_wifi_sta();
    #endif
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
    nvs_handle_t nvs;
    uint err, len;
    len = 255;
    err = nvs_open("storage", NVS_READWRITE, &nvs);
    err |= nvs_get_blob(nvs, "user_ssid", &user_ssid, &len);
    err |= nvs_get_blob(nvs, "user_password", &user_password, &len);
    if (err == ESP_ERR_NVS_NOT_FOUND || true) {
        nvs_set_blob(nvs, "user_ssid", DEFAULT_SSID, sizeof(DEFAULT_SSID));
        nvs_set_blob(nvs, "user_password", DEFAULT_PASSWORD, sizeof(DEFAULT_PASSWORD));
        memcpy(user_ssid, DEFAULT_SSID, sizeof(DEFAULT_SSID));
        memcpy(user_password, DEFAULT_PASSWORD, sizeof(DEFAULT_PASSWORD));
    }
    memcpy(wifi_config.sta.ssid, user_ssid, sizeof(user_ssid));
    memcpy(wifi_config.sta.password, user_password, sizeof(user_password));

    ESP_ERROR_CHECK(esp_wifi_set_country_code("SE", true));

    #ifdef AP
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config_ap));
    #else
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    #endif
    ESP_ERROR_CHECK(esp_wifi_start());
    scan_wifi();
    #ifdef AP
    printf("%s %s\n", wifi_config_ap.ap.ssid, wifi_config_ap.ap.password);
    got_ip_handler(NULL, NULL, NULL, NULL);
    #else
    ESP_ERROR_CHECK(esp_wifi_connect());
    #endif
    esp_wifi_set_ps(WIFI_PS_NONE);

    udp_queue = xQueueCreate(2, sizeof(spi_slave_transaction_t));
    esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, wifi_disconnected_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, got_ip_handler, NULL, NULL);
    xTaskCreate(tcp_server_task, "tcp_server", 5000, NULL, 2, NULL);
    xTaskCreate(udp_sender_task, "udp sender", 5000, NULL, 3, &udp_sender_handle);
}
