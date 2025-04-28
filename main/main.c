#include <stdio.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_system.h"
#include "mqtt_client.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"


// Definiciones de WiFi
#define WIFI_SSID      // SSID
#define WIFI_PASS      // Password
#define MAX_RETRY      5

// Definición de MQTT
static const char *TAG = "WiFi";
static int retry_num = 0;
static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static const char *TAG_MQTT = "MQTT";

// Declaración de certificados embebidos
extern const uint8_t device_cert_pem_start[]   asm("_binary_Node1certificate_crt_start");
extern const uint8_t device_cert_pem_end[]     asm("_binary_Node1certificate_crt_end");
extern const uint8_t private_key_pem_start[]   asm("_binary_Node1private_key_start");
extern const uint8_t private_key_pem_end[]     asm("_binary_Node1private_key_end");
extern const uint8_t aws_root_ca_pem_start[]   asm("_binary_AmazonRootCA1_pem_start");
extern const uint8_t aws_root_ca_pem_end[]     asm("_binary_AmazonRootCA1_pem_end");

// Configuración de GPIO
#define TRIG_SENSOR_1 GPIO_NUM_0
#define ECHO_SENSOR_1 GPIO_NUM_1

#define TRIG_SENSOR_2 GPIO_NUM_2
#define ECHO_SENSOR_2 GPIO_NUM_3

// Callback para eventos Wi-Fi
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT)
    {
        if (event_id == WIFI_EVENT_STA_START){
            esp_wifi_connect();
        }
        else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
        {
            if (retry_num < MAX_RETRY)
            {
                esp_wifi_connect();
                retry_num++;
                ESP_LOGI(TAG, "Reintentando conexión al AP");
            }
            else
            {
                xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
            }
            ESP_LOGI(TAG,"Conexión al AP fallida");
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Obtenida IP:" IPSTR, IP2STR(&event->ip_info.ip));
        retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// Inicializa y conecta al WiFi
void wifi_init_sta(void)
{
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            /* Indicamos que no queremos modo PMF (opcional) */
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );

    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta terminado.");

    /* Esperamos hasta que se establezca la conexión (WIFI_CONNECTED_BIT) */
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
            WIFI_CONNECTED_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "Conectado al AP SSID:%s", WIFI_SSID);
    }
    else
    {
        ESP_LOGI(TAG, "No se pudo conectar al AP SSID:%s", WIFI_SSID);
    }
}

// Inicializa y conecta a un sensor ultrasonico
void init_ultrasonic_sensor(gpio_num_t trig_pin, gpio_num_t echo_pin)
{
    gpio_set_direction(trig_pin, GPIO_MODE_OUTPUT);
    gpio_set_direction(echo_pin, GPIO_MODE_INPUT);
}

// Obtiene el estado de parqueo
bool parking_status(gpio_num_t trig_pin, gpio_num_t echo_pin)
{
    int64_t pulse_start = 0, pulse_end = 0, pulse_duration = 0;
    int64_t timeout = 0;

    // Enviar pulso de 10 µs en TRIG
    gpio_set_level(trig_pin, 0);
    esp_rom_delay_us(2);
    gpio_set_level(trig_pin, 1);
    esp_rom_delay_us(10);
    gpio_set_level(trig_pin, 0);

    // Esperar a que ECHO se ponga alto
    timeout = esp_timer_get_time();
    while (gpio_get_level(echo_pin) == 0)
    {
        pulse_start = esp_timer_get_time();
        if ((pulse_start - timeout) > 30000) // Timeout de 30 ms
        {
            ESP_LOGW("SENSOR", "Timeout esperando pulso de inicio");
            return -1.0;
        }
    }

    // Esperar a que ECHO se ponga bajo
    timeout = esp_timer_get_time();
    while (gpio_get_level(echo_pin) == 1)
    {
        pulse_end = esp_timer_get_time();
        if ((pulse_end - timeout) > 30000) // Timeout de 30 ms
        {
            ESP_LOGW("SENSOR", "Timeout esperando pulso de fin");
            return -1.0;
        }
    }

    pulse_duration = pulse_end - pulse_start; // Duración del pulso en microsegundos

    // Calcular distancia en centímetros
    float distance_cm = (pulse_duration * 0.0343) / 2.0;

    if (distance_cm < 10)
    {
        return false; // Parqueado
    } else
    {
        return true; // Libre
    }
}

// Tarea para publicar mensajes MQTT
void mqtt_publish_task(void *pvParameters)
{
    esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t)pvParameters;
    char payload[100];

    while (1)
    {
        bool parking1 = parking_status(TRIG_SENSOR_1, ECHO_SENSOR_1);
        vTaskDelay(pdMS_TO_TICKS(500)); // Esperar 500ms entre medidas
        bool parking2 = parking_status(TRIG_SENSOR_2, ECHO_SENSOR_2);

        // Formatear el mensaje JSON
        snprintf(payload, sizeof(payload), "{\"Estacionamiento1\": %d, \"Estacionamiento2\": %d}", parking1, parking2);

        // Publicar el mensaje
        int msg_id = esp_mqtt_client_publish(client, "smartparking/nodo1", payload, 0, 1, 0);
        if (msg_id == -1)
        {
            ESP_LOGE(TAG_MQTT, "Error al publicar el mensaje");
        }
        else
        {
            ESP_LOGI(TAG_MQTT, "Mensaje publicado, ID: %d, Payload: %s", msg_id, payload);
        }

        vTaskDelay(pdMS_TO_TICKS(5000)); // Esperar 5 segundos entre publicaciones
    }
}

// Callback para eventos MQTT
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    switch (event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_CONNECTED");
        if (client == NULL)
        {
            ESP_LOGE(TAG_MQTT, "El handle del cliente MQTT es NULL");
            break;
        }
        xTaskCreatePinnedToCore(mqtt_publish_task, "mqtt_publish_task", 4096, (void *)client, 5, NULL, 0);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG_MQTT, "MQTT_EVENT_DISCONNECTED");
        break;
    default:
        ESP_LOGI(TAG_MQTT, "Other event id:%d", event->event_id);
        break;
    }
}

// Inicializa y conecta al MQTT
void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtts://a1uud8twxwkyab-ats.iot.us-east-1.amazonaws.com",
        .broker.address.port = 8883,

        // Configuración de TLS
        .broker.verification.certificate = (const char *)aws_root_ca_pem_start,

        // Certificados del cliente
        .credentials.authentication.certificate = (const char *)device_cert_pem_start,
        .credentials.authentication.key = (const char *)private_key_pem_start,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    if (client == NULL)
    {
        ESP_LOGE(TAG_MQTT, "Error al inicializar el cliente MQTT");
        return;
    }
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}








void app_main(void)
{
    // Inicializa NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Iniciar WiFi
    wifi_init_sta();

    // Inicializar sensores ultrasónicos
    init_ultrasonic_sensor(TRIG_SENSOR_1, ECHO_SENSOR_1);
    init_ultrasonic_sensor(TRIG_SENSOR_2, ECHO_SENSOR_2);

    // Iniciar MQTT
    mqtt_app_start();
}
