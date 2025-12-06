#pragma once

// hardware pins
#define PIN_LED_BLINK        45
#define LED_COUNT            1
#define PIN_NEOPIXEL         6
#define NEO_COUNT            4
#define PIN_RELAY            8

//define ports
#define PIN_LIGHT GPIO_NUM_1
#define PIN_MOISTURE GPIO_NUM_2
#define PIN_LED GPIO_NUM_48

// default STA WiFi (leave empty to skip auto STA)
#define DEFAULT_WLAN_SSID    ""
#define DEFAULT_WLAN_PASS    ""

// AP credentials
#define AP_SSID              "YoloUNO_AP"
#define AP_PASS              ""

// CoreIOT MQTT defaults (please replace with your device credentials)
static const char* COREIOT_MQTT_HOST = "app.coreiot.io";
static const uint16_t COREIOT_MQTT_PORT = 1883;
static const char* COREIOT_CLIENT_ID = "khgihpx87q85awm1ebmr";
static const char* COREIOT_MQTT_USER = "1r78wta7gnyzyc9r4aan";
static const char* COREIOT_MQTT_PASS = "4f0aesigc40kkv25374u";
static const double DEVICE_LONGITUDE = 106.65789107082472;
static const double DEVICE_LATITUDE  = 10.772175109674038;