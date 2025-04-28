# SmartParking Node - AWS IoT Core Integration

## Descripción

Este proyecto corresponde al firmware de un nodo de SmartParking.
Cada nodo administra múltiples sensores de estacionamiento y se conecta de manera segura a AWS IoT Core usando MQTT para reportar en tiempo real el estado de ocupación de los espacios que tiene a su cargo.

## Características principales

* Monitoreo de sensores de ultrasonido para detección de vehículos.
* Publicación periódica del estado de cada estacionamiento a un tema MQTT en AWS IoT Core.
* Gestión de reconexión automática en caso de pérdida de red.
* Bajo consumo de energía y alta confiabilidad en la comunicación.

## Requerimientos

* Hardware:
  * ESP32-c3.
  * Módulos de sensores de ultrasonido HC-SR04 por cada espacio de estacionamiento.

* Software:
  * ESP-IDF v5.1.
  * AWS IoT Core configurado (certificados, endpoint, temas MQTT).

* Librerías:
  * MQTT (incluida en ESP-IDF).
  * WiFi (incluida en ESP-IDF).

## Configuración

Antes de compilar y cargar el firmware:

1. Configurar el archivo de conexión WiFi:

        // Definiciones de WiFi
        #define WIFI_SSID      "KylaRueda 2.4GHZ"
        #define WIFI_PASS      "KaylaRM4424@"

2. Configurar el endpoint y certificados de AWS IoT Core:

        // Declaración de certificados embebidos
        extern const uint8_t device_cert_pem_start[]   asm("_binary_Node1certificate_crt_start");
        extern const uint8_t device_cert_pem_end[]     asm("_binary_Node1certificate_crt_end");
        extern const uint8_t private_key_pem_start[]   asm("_binary_Node1private_key_start");
        extern const uint8_t private_key_pem_end[]     asm("_binary_Node1private_key_end");
        extern const uint8_t aws_root_ca_pem_start[]   asm("_binary_AmazonRootCA1_pem_start");
        extern const uint8_t aws_root_ca_pem_end[]     asm("_binary_AmazonRootCA1_pem_end");

3. Configurar los tópicos MQTT por cada estacionamiento:

        // Formatear el mensaje JSON
        snprintf(payload, sizeof(payload), "{\"Estacionamiento1\": %d, \"Estacionamiento2\": %d}", parking1, parking2);

4. Configurar tiempos de publicación y validación de sensores:

        // Formatear el mensaje JSON
        snprintf(payload, sizeof(payload), "{\"Estacionamiento1\": %d, \"Estacionamiento2\": %d}", parking1, parking2);
