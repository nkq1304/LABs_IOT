#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "DHT.h"
#include "driver/rtc_io.h"
#include "driver/adc.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_continuous.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc_cal.h"
#include <driver/gpio.h>
#include <string.h>
#include <freertos/timers.h>

#define DHT_PIN GPIO_NUM_12
#define LIGHT_SENSOR_PIN ADC1_CHANNEL_6
#define LED_PIN GPIO_NUM_18

#if defined(CONFIG_EXAMPLE_TYPE_DHT11)
#define SENSOR_TYPE DHT_TYPE_DHT11
#endif

SemaphoreHandle_t xSemaphore;

static void dht_task(void *param){
    float temperature, humidity;
    #ifdef CONFIG_EXAMPLE_INTERNAL_PULLUP
        gpio_set_pull_mode(dht_gpio, GPIO_PULLUP_ONLY);
    #endif
    esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t)param;

    while (1) {
        if (dht_read_float_data(DHT_TYPE_DHT11, DHT_PIN, &humidity, &temperature) == ESP_OK) {
            char temp_data[50];
            char hum_data[50];
            sprintf(temp_data, "%.1f", temperature);
            sprintf(hum_data, "%.1f", humidity);
            printf("Temperature: %.1f, Humidity: %.1f\n", temperature, humidity);
            esp_mqtt_client_publish(client, "kimquynh1304/feeds/cambien1", temp_data, 0, 1, 0);
            esp_mqtt_client_publish(client, "kimquynh1304/feeds/cambien3", hum_data, 0, 1, 0);
        } 
        else {
            printf("Could not read data from sensor\n");
        }
        vTaskDelay(30000 / portTICK_PERIOD_MS);  // Delay for 30 seconds
    }
}
static void light_sensor_task(void *pvParameters)
{
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(LIGHT_SENSOR_PIN, ADC_ATTEN_DB_0);
    esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t)pvParameters;
    while (1)
    {
        xSemaphoreTake(xSemaphore, portMAX_DELAY);
        uint16_t light_value = adc1_get_raw(LIGHT_SENSOR_PIN);

        char light_data[50];
        sprintf(light_data, "%d", light_value);
        printf("Light value: %d\n", light_value);
        esp_mqtt_client_publish(client, "kimquynh1304/feeds/cambien2", light_data, 0, 1, 0);

        xSemaphoreGive(xSemaphore);
        vTaskDelay(30000 / portTICK_PERIOD_MS); // Delay for 30 seconds
    }
    vTaskDelete(NULL);
}

static const char *TAG = "mqtt_example";

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // float temperature, humidity;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, "kimquynh1304/feeds/nutnhan1", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "kimquynh1304/feeds/nutnhan1", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        xTaskCreate(dht_task, "dht_task", 4096, (void *)client, 3, NULL);
        //call light task
        xTaskCreate(light_sensor_task, "light_sensor_task", 4096, (void *)client, 3, NULL);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        if(event->data[0] == '1'){
            gpio_set_level(LED_PIN, 1); // Turn on the LED
        }
        else{
            gpio_set_level(LED_PIN, 0); // Turn off the LED
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://io.adafruit.com:1883",
        .credentials.username = "kimquynh1304",
        .credentials.authentication.password = "aio_gJib69ZYqycZilN45hRtWsVBpI2l",
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

void app_main(void)
{
    xSemaphore = xSemaphoreCreateMutex();
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("mqtt_example", ESP_LOG_VERBOSE);
    esp_log_level_set("transport_base", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("transport", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());
    mqtt_app_start();


    //TODO: Add your code here
        // Configure DHT_PIN as an input initially
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << DHT_PIN),
        .mode = GPIO_MODE_INPUT,
        //set up pull up only
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io_conf);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 0);

}