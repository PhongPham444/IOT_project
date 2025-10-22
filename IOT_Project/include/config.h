#pragma once

// hardware pins
#define PIN_LED_BLINK        GPIO_NUM_48
#define PIN_NEOPIXEL         8
#define NEO_COUNT            4
#define PIN_RELAY            6
#define FAN_PIN              GPIO_NUM_10 

// default STA WiFi (leave empty to skip auto STA)
#define DEFAULT_WLAN_SSID    ""
#define DEFAULT_WLAN_PASS    ""

// AP credentials
#define AP_SSID              "YoloUNO_AP"
#define AP_PASS              ""

// CoreIOT MQTT defaults (please replace with your device credentials)
static const char* COREIOT_MQTT_HOST = "app.coreiot.io";
static const uint16_t COREIOT_MQTT_PORT = 1883;
static const char* COREIOT_CLIENT_ID = "";
static const char* COREIOT_MQTT_USER = "";
static const char* COREIOT_MQTT_PASS = "";