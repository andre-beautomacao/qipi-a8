/***************************************************************
 * Includes e Defines
 ***************************************************************/
#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <ArduinoOTA.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <ETH.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <AsyncMqttClient.h>
#include "ESPAsyncWebServer.h"
#include "LittleFS.h"
#include "PCF8574.h"
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include <ElegantOTA.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "network_mqtt.h"
#include "ethernet_config.h"

#include "web_server.h"
/***************************************************************
 *  Definições de hardware ethernet
 ***************************************************************/
// Ethernet PHY parameters moved to network_mqtt.h

// ================== DEFINES: Wi-Fi, MQTT, Sistema ==================
#define WIFI_DEFAULT_SSID        "Q1AP"
#define WIFI_DEFAULT_PASS        "q1ambiental"
#define AP_DEFAULT_SSID_PREF     "QIPI"
#define AP_DEFAULT_PASS          "q1ambiental"
#define HOST_DEFAULT             "ESP32-QIPI"
#define HTTP_USER_DEFAULT        "admin"
#define HTTP_PASS_DEFAULT        "qipi123"

// ========== DEFINES: Hardware e Sensores ==========
#define LCD_COLS                 20
#define LCD_ROWS                 4
#define ADC1_PIN                 36  // pH
#define ADC2_PIN                 34  // ORP
#define ONEWIRE_BUS1             32
//#define ONEWIRE_BUS2           33   // Removido: apenas um sensor de temperatura
//#define ONEWIRE_BUS3           14   // Descomente se usar 3 sensores

// ========== DEFINES: PCF8574 ==========
#define PCF8574_R1_ADDR          0x24
#define PCF8574_R2_ADDR          0x25
#define PCF8574_I1_ADDR          0x22
#define PCF8574_SDA              4
#define PCF8574_SCL              5

// ========== DEFINES: MQTT ==========
#define MQTT_SERVER_DEFAULT      "srv812575.hstgr.cloud"
#define MQTT_PORT_DEFAULT        1883
#define MQTT_USER_DEFAULT        "hostinger"
#define MQTT_PASS_DEFAULT        "lsdyg9t$Adf"
#define MQTT_FIXED_IP_DEFAULT    "82.25.74.168"
#define MQTT_QOS_DEFAULT         1
#define MQTT_RETAIN_DEFAULT      0
#define MQTT_PUBLISH_INTERVAL_DEFAULT 1800

// ========== DEFINES: Intervalos das Tasks ==========
#define STARTUP_DELAY_SENSORS_DEFAULT     60    // 5 minutos (seg)
#define STARTUP_DELAY_PUBLISH_DEFAULT     600    // 10 minutos (seg)
#define STARTUP_DELAY_OTA_DEFAULT         60     // 1 minuto (seg)
#define SENSOR_INTERVAL_DEFAULT           10     // seg
#define PUBLISH_INTERVAL_DEFAULT          1800   // seg

// ========== DEFINES: EEPROM ==========
#define EEPROM_SIZE               224   // Ajuste se adicionar mais campos

#define EE_WIFI_SSID_ADDR         0     // 20 bytes
#define EE_WIFI_SSID_LEN          20

#define EE_WIFI_PASS_ADDR         20    // 20 bytes
#define EE_WIFI_PASS_LEN          20

#define EE_MQTT_SERVER_ADDR       40    // 32 bytes
#define EE_MQTT_SERVER_LEN        32

#define EE_MQTT_PORT_ADDR         72    // 6 bytes
#define EE_MQTT_PORT_LEN          6

#define EE_MQTT_USER_ADDR         78    // 32 bytes
#define EE_MQTT_USER_LEN          32

#define EE_MQTT_PASS_ADDR         110   // 32 bytes
#define EE_MQTT_PASS_LEN          32

#define EE_SENSOR_INTERVAL_ADDR   142   // 8 bytes
#define EE_SENSOR_INTERVAL_LEN    8

#define EE_PUBLISH_INTERVAL_ADDR  150   // 8 bytes
#define EE_PUBLISH_INTERVAL_LEN   8

#define EE_GRUPO2_STATUS_ADDR     158   // 1 byte
#define EE_GRUPO2_STATUS_LEN      1

#define EE_MQTT_QOS_ADDR          159   // 1 byte
#define EE_MQTT_QOS_LEN           1

#define EE_MQTT_RETAIN_ADDR       160   // 1 byte
#define EE_MQTT_RETAIN_LEN        1

#define EE_MQTT_FIXED_IP_ADDR     161   // 16 bytes
#define EE_MQTT_FIXED_IP_LEN      16

#define EE_WIFI_STA_ENABLE_ADDR   177   // 1 byte
#define EE_WIFI_STA_ENABLE_LEN    1

// ========== DEFINES: OTA ==========
#define REMOTEHOST                 "raw.githubusercontent.com"
#define REMOTEPATH                 "/andre-beautomacao/qipi-a8/main/qipi-a8.bin"
#define REMOTEPORT                 443
#define FILE_NAME                  "qipi-a8.bin"

// ========== DEFINES: Outras ==========
#define NUM_SAMPLES_ANALOG_DEFAULT 30
#define TEMP_THRESHOLD_DEFAULT     1.0f

/***************************************************************
 * Variáveis Globais
 ***************************************************************/

// ----------- Identidade e Rede -----------
String apSSIDprefix = AP_DEFAULT_SSID_PREF;
const char* ap_password = AP_DEFAULT_PASS;
String host           = HOST_DEFAULT;
String wifiSSID       = WIFI_DEFAULT_SSID;
String wifiPass       = WIFI_DEFAULT_PASS;
String mqtt_server    = MQTT_SERVER_DEFAULT;
uint16_t mqtt_port    = MQTT_PORT_DEFAULT;
String mqtt_username  = MQTT_USER_DEFAULT;
String mqtt_password  = MQTT_PASS_DEFAULT;
String mqttClientId;
String mqtt_fixed_ip  = MQTT_FIXED_IP_DEFAULT;
uint8_t mqtt_qos       = MQTT_QOS_DEFAULT;
bool mqtt_retain       = MQTT_RETAIN_DEFAULT;
unsigned long sensorInterval  = SENSOR_INTERVAL_DEFAULT;
unsigned long publishInterval = PUBLISH_INTERVAL_DEFAULT;
uint8_t wifi_sta_enable       = 0; // 0 = só AP, 1 = AP+STA

const char* http_username = HTTP_USER_DEFAULT;
const char* http_password = HTTP_PASS_DEFAULT;


char cmac_ap[20];
char cMacSTA[20];
char cMacETH[20];

volatile bool wifi_connected = false;
volatile bool eth_connected  = false;
volatile bool mqtt_connected = false;
volatile bool dns_forcado_eth = false;
volatile uint8_t mqttPreferencia = 1; // 1=WiFi, 2=Ethernet
volatile uint8_t mqtt_interface = 0; // 0 = desconectado, 1 = Wi-Fi, 2 = Ethernet

const unsigned long MAX_RECONNECT_INTERVAL = 60000;

// ----------- LCD/I2C/PCF8574 -----------
TwoWire I2Cone = TwoWire(0);
PCF8574 pcf8574_R1(&I2Cone, PCF8574_R1_ADDR, PCF8574_SDA, PCF8574_SCL);
PCF8574 pcf8574_R2(&I2Cone, PCF8574_R2_ADDR, PCF8574_SDA, PCF8574_SCL);
PCF8574 pcf8574_I1(&I2Cone, PCF8574_I1_ADDR, PCF8574_SDA, PCF8574_SCL);
LiquidCrystal_I2C lcd(0x27, LCD_COLS, LCD_ROWS);

// ----------- Sensores -----------
OneWire oneWire(ONEWIRE_BUS1);
DallasTemperature sensors(&oneWire);
// Para 3 sensores, descomente
//OneWire oneWire3(ONEWIRE_BUS3);
//DallasTemperature sensors3(&oneWire3);

// 0 = pH, 1 = ORP para cada entrada analógica
uint8_t tipo_adc1 = 0;
uint8_t tipo_adc2 = 1;

// Entradas analógicas
const int adc1 = 36;  // 4-20mA p/ leitura (p.ex., pH)
const int adc2 = 34;  // ORP

// Valores lidos (um para cada)
int adc1_val, adc2_val;
// Valores calculados (um para cada)
float ph1 = 0, orp1 = 0;



// ----------- GPIOs Relés/Entradas -----------
int relayState[16] = {0};
int diState[16]    = {0};
int last_diState[16] = {0};

// Relés (16)
int relay1state  = 0, relay2state  = 0, relay3state  = 0, relay4state  = 0,
    relay5state  = 0, relay6state  = 0, relay7state  = 0, relay8state  = 0,
    relay9state  = 0, relay10state = 0, relay11state = 0, relay12state = 0,
    relay13state = 0, relay14state = 0, relay15state = 0, relay16state = 0;

// Entradas digitais (1-8)
int di01State = 0, di02State = 0, di03State = 0, di04State = 0,
    di05State = 0, di06State = 0, di07State = 0, di08State = 0;
int last_di01State = 0, last_di02State = 0, last_di03State = 0, last_di04State = 0,
    last_di05State = 0, last_di06State = 0, last_di07State = 0, last_di08State = 0;

// ----------- Leituras Analógicas/Digitais -----------
float ph, orp;
float temperatura1;

float last_ph_sent = 0.0f, last_orp_sent = 0.0f;
float last_temp1 = 0.0f;

bool grupo1Online = false;
bool grupo1OfflineAlertSent = false;

const float limite_ph = 0.1f;
const float limite_orp = 10.0f;
const float tempThreshold = TEMP_THRESHOLD_DEFAULT;

static const int numSamples = NUM_SAMPLES_ANALOG_DEFAULT;
int phSamples[numSamples], orpSamples[numSamples];

// ----------- Servidor HTTP/MQTT -----------
AsyncMqttClient mqttClient;

// ----------- MQTT Topics -----------
char cOutTopic[40] = "";
char cInTopic[40]  = "";

// ----------- OTA/Download -----------
unsigned long ota_progress_millis = 0;

// ----------- IP público -----------
extern String url = "http://api.ipify.org/";
String ip, last_ip;

// ----------- Delay/reboot/Tasks -----------
unsigned long startupDelaySeconds     = 60;
unsigned long systemStartSeconds      = 0;
unsigned long startupDelaySensors     = STARTUP_DELAY_SENSORS_DEFAULT;
unsigned long startupDelayPublish     = STARTUP_DELAY_PUBLISH_DEFAULT;
unsigned long startupDelayOTA         = STARTUP_DELAY_OTA_DEFAULT;
unsigned long startupDelayGetIP       = 60; // 1 min

// ----------- Contadores/sistema -----------
int ciclosLeitura    = 0;
const int ciclosIgnorados = 3;
int confirmacoesEstado[8] = { 0 };

// ----------- Funções utilitárias/log -----------
// ========== DEFINE - FUNÇÃO DE LOG (SERIAL+WEB) ==========
#define LOG_BUFFER_SIZE 40
String logBuffer[LOG_BUFFER_SIZE];
int logWriteIndex = 0;

// Adiciona ao buffer de logs e imprime na Serial
void addLog(const String& msg) {
  logBuffer[logWriteIndex] = msg;
  logWriteIndex = (logWriteIndex + 1) % LOG_BUFFER_SIZE;
  Serial.println(msg); // Imprime na Serial também
}

// Função para substituir Serial.print (sem quebra de linha)
void addLogRaw(const String& msg) {
  if (logBuffer[logWriteIndex].length() == 0) {
    logBuffer[logWriteIndex] = msg;
  } else {
    logBuffer[logWriteIndex] += msg;
  }
  Serial.print(msg); // Sem quebra de linha
}

// Para log formatado tipo printf
void logf(const char* fmt, ...) {
  char buf[128];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  addLog(String(buf));
}

// ---- SÓ AGORA DEFINA AS MACROS: ----
#define LOG_PRINT(msg)      addLogRaw(String(msg))
#define LOG_PRINTLN(msg)    addLog(String(msg))
#define LOG_PRINTF(...)     logf(__VA_ARGS__)

// Categorized log helpers
#define LOG_MQTT(msg)       LOG_PRINTLN(String("[MQTT] ") + msg)
#define LOG_NETWORK(msg)    LOG_PRINTLN(String("[NETWORK] ") + msg)
#define LOG_EEPROM(msg)     LOG_PRINTLN(String("[EEPROM] ") + msg)
#define LOG_READINGS(msg)   LOG_PRINTLN(String("[READINGS] ") + msg)
#define LOG_OTA(msg)        LOG_PRINTLN(String("[OTA] ") + msg)

bool canPublish() {
  return (millis() / 1000UL - systemStartSeconds) >= startupDelayPublish;
}


/***************************************************************
 * Segue o restante do seu código (setup, loop, tasks, etc)
 * ...
 ***************************************************************/
/***************************************************************
 *  Declarações de funções
 ***************************************************************/
void sortArray(int array[], int size);
void verificarConexoes();
void WiFiEvent(WiFiEvent_t event);
void publishDigitalGroup1();
void publishAnalogGroup1();
void getFileFromServer();
void performOTAUpdateFromLittleFS();
void onOTAStart();
void onOTAProgress(size_t current, size_t final);
void onOTAEnd(bool success);
void setupADC();
void setupPCF8574();
void readDigitalInputs();
void readAnalogInputs();
void readInputs();
void checkSerialCommands();

// Funções de gerenciamento da EEPROM e atualização de parâmetros
void setupEEPROM();
void updateWifiSSIDInEEPROM(const char* novoSSID);
void updateWifiPassInEEPROM(const char* novaSenha);
void updateMqttServerInEEPROM(const char* newServer);
void updateMqttPortInEEPROM(const char* newPortStr);
void updateMqttUsernameInEEPROM(const char* newUsername);
void updateMqttPasswordInEEPROM(const char* newPassword);
void updateSensorIntervalInEEPROM(const char* newIntervalStr);
void updatePublishIntervalInEEPROM(const char* newIntervalStr);
void updateMqttQoSInEEPROM(uint8_t qos);
void updateMqttRetainInEEPROM(bool retain);
void updateWifiStaEnableInEEPROM(bool enable);

/***************************************************************
 *  Implementação das funções de EEPROM e atualização
 ***************************************************************/
void setupEEPROM() {
  EEPROM.begin(EEPROM_SIZE);

  // ===================== WiFi SSID =====================
  char storedSSID[EE_WIFI_SSID_LEN + 1];
  for (int i = 0; i < EE_WIFI_SSID_LEN; i++) {
    storedSSID[i] = EEPROM.read(EE_WIFI_SSID_ADDR + i);
  }
  storedSSID[EE_WIFI_SSID_LEN] = '\0';
  if (storedSSID[0] == 0xFF || storedSSID[0] == '\0') {
    strncpy(storedSSID, WIFI_DEFAULT_SSID, EE_WIFI_SSID_LEN);
    storedSSID[EE_WIFI_SSID_LEN] = '\0';
    for (int i = 0; i < EE_WIFI_SSID_LEN; i++) {
      EEPROM.write(EE_WIFI_SSID_ADDR + i, storedSSID[i]);
    }
    LOG_EEPROM("EEPROM vazia. Gravando wifiSSID como default.");
  } else {
    LOG_EEPROM("wifiSSID lido da EEPROM: " + String(storedSSID));
  }
  wifiSSID = String(storedSSID);

  // ===================== WiFi Senha =====================
  char storedPass[EE_WIFI_PASS_LEN + 1];
  for (int i = 0; i < EE_WIFI_PASS_LEN; i++) {
    storedPass[i] = EEPROM.read(EE_WIFI_PASS_ADDR + i);
  }
  storedPass[EE_WIFI_PASS_LEN] = '\0';
  if (storedPass[0] == 0xFF || storedPass[0] == '\0') {
    strncpy(storedPass, WIFI_DEFAULT_PASS, EE_WIFI_PASS_LEN);
    storedPass[EE_WIFI_PASS_LEN] = '\0';
    for (int i = 0; i < EE_WIFI_PASS_LEN; i++) {
      EEPROM.write(EE_WIFI_PASS_ADDR + i, storedPass[i]);
    }
    LOG_EEPROM("EEPROM vazia. Gravando wifiPass como default.");
  } else {
    LOG_EEPROM("wifiPass lido da EEPROM: " + String(storedPass));
  }
  wifiPass = String(storedPass);

  // ===================== MQTT Server =====================
  char storedMqttServer[EE_MQTT_SERVER_LEN + 1];
  for (int i = 0; i < EE_MQTT_SERVER_LEN; i++) {
    storedMqttServer[i] = EEPROM.read(EE_MQTT_SERVER_ADDR + i);
  }
  storedMqttServer[EE_MQTT_SERVER_LEN] = '\0';
  if (storedMqttServer[0] == 0xFF || storedMqttServer[0] == '\0') {
    strncpy(storedMqttServer, MQTT_SERVER_DEFAULT, EE_MQTT_SERVER_LEN);
    storedMqttServer[EE_MQTT_SERVER_LEN] = '\0';
    for (int i = 0; i < EE_MQTT_SERVER_LEN; i++) {
      EEPROM.write(EE_MQTT_SERVER_ADDR + i, storedMqttServer[i]);
    }
    LOG_EEPROM("EEPROM vazia. Gravando mqtt_server como default.");
  } else {
    LOG_EEPROM("mqtt_server lido da EEPROM: " + String(storedMqttServer));
  }
  mqtt_server = String(storedMqttServer);

  // ===================== MQTT Port =====================
  char storedMqttPort[EE_MQTT_PORT_LEN + 1];
  for (int i = 0; i < EE_MQTT_PORT_LEN; i++) {
    storedMqttPort[i] = EEPROM.read(EE_MQTT_PORT_ADDR + i);
  }
  storedMqttPort[EE_MQTT_PORT_LEN] = '\0';
  if (storedMqttPort[0] == 0xFF || storedMqttPort[0] == '\0') {
    strncpy(storedMqttPort, "1883", EE_MQTT_PORT_LEN);
    storedMqttPort[EE_MQTT_PORT_LEN] = '\0';
    for (int i = 0; i < EE_MQTT_PORT_LEN; i++) {
      EEPROM.write(EE_MQTT_PORT_ADDR + i, storedMqttPort[i]);
    }
    LOG_EEPROM("EEPROM vazia. Gravando mqtt_port como 1883");
  } else {
    LOG_EEPROM("mqtt_port lido da EEPROM: " + String(storedMqttPort));
  }
  mqtt_port = String(storedMqttPort).toInt();

  // ===================== MQTT Username =====================
  char storedMqttUsername[EE_MQTT_USER_LEN + 1];
  for (int i = 0; i < EE_MQTT_USER_LEN; i++) {
    storedMqttUsername[i] = EEPROM.read(EE_MQTT_USER_ADDR + i);
  }
  storedMqttUsername[EE_MQTT_USER_LEN] = '\0';
  if (storedMqttUsername[0] == 0xFF || storedMqttUsername[0] == '\0') {
    strncpy(storedMqttUsername, MQTT_USER_DEFAULT, EE_MQTT_USER_LEN);
    storedMqttUsername[EE_MQTT_USER_LEN] = '\0';
    for (int i = 0; i < EE_MQTT_USER_LEN; i++) {
      EEPROM.write(EE_MQTT_USER_ADDR + i, storedMqttUsername[i]);
    }
    LOG_EEPROM("EEPROM vazia. Gravando mqtt_username como default.");
  } else {
    LOG_EEPROM("mqtt_username lido da EEPROM: " + String(storedMqttUsername));
  }
  mqtt_username = String(storedMqttUsername);

  // ===================== MQTT Password =====================
  char storedMqttPassword[EE_MQTT_PASS_LEN + 1];
  for (int i = 0; i < EE_MQTT_PASS_LEN; i++) {
    storedMqttPassword[i] = EEPROM.read(EE_MQTT_PASS_ADDR + i);
  }
  storedMqttPassword[EE_MQTT_PASS_LEN] = '\0';
  if (storedMqttPassword[0] == 0xFF || storedMqttPassword[0] == '\0') {
    strncpy(storedMqttPassword, MQTT_PASS_DEFAULT, EE_MQTT_PASS_LEN);
    storedMqttPassword[EE_MQTT_PASS_LEN] = '\0';
    for (int i = 0; i < EE_MQTT_PASS_LEN; i++) {
      EEPROM.write(EE_MQTT_PASS_ADDR + i, storedMqttPassword[i]);
    }
    LOG_EEPROM("EEPROM vazia. Gravando mqtt_password como default.");
  } else {
    LOG_EEPROM("mqtt_password lido da EEPROM: " + String(storedMqttPassword));
  }
  mqtt_password = String(storedMqttPassword);

  // ===================== Sensor Interval =====================
  char storedSensorInterval[EE_SENSOR_INTERVAL_LEN + 1];
  for (int i = 0; i < EE_SENSOR_INTERVAL_LEN; i++) {
    storedSensorInterval[i] = EEPROM.read(EE_SENSOR_INTERVAL_ADDR + i);
  }
  storedSensorInterval[EE_SENSOR_INTERVAL_LEN] = '\0';
  if (storedSensorInterval[0] == 0xFF || storedSensorInterval[0] == '\0') {
    strncpy(storedSensorInterval, "10", EE_SENSOR_INTERVAL_LEN);
    storedSensorInterval[EE_SENSOR_INTERVAL_LEN] = '\0';
    for (int i = 0; i < EE_SENSOR_INTERVAL_LEN; i++) {
      EEPROM.write(EE_SENSOR_INTERVAL_ADDR + i, storedSensorInterval[i]);
    }
    LOG_EEPROM("EEPROM vazia. Gravando sensor_interval como 10");
  } else {
    LOG_EEPROM("sensor_interval lido da EEPROM: " + String(storedSensorInterval));
  }
  sensorInterval = String(storedSensorInterval).toInt();

  // ===================== Publish Interval =====================
  char storedPublishInterval[EE_PUBLISH_INTERVAL_LEN + 1];
  for (int i = 0; i < EE_PUBLISH_INTERVAL_LEN; i++) {
    storedPublishInterval[i] = EEPROM.read(EE_PUBLISH_INTERVAL_ADDR + i);
  }
  storedPublishInterval[EE_PUBLISH_INTERVAL_LEN] = '\0';
  if (storedPublishInterval[0] == 0xFF || storedPublishInterval[0] == '\0') {
    strncpy(storedPublishInterval, "1800", EE_PUBLISH_INTERVAL_LEN);
    storedPublishInterval[EE_PUBLISH_INTERVAL_LEN] = '\0';
    for (int i = 0; i < EE_PUBLISH_INTERVAL_LEN; i++) {
      EEPROM.write(EE_PUBLISH_INTERVAL_ADDR + i, storedPublishInterval[i]);
    }
    LOG_EEPROM("EEPROM vazia. Gravando publish_interval como 1800");
  } else {
    LOG_EEPROM("publish_interval lido da EEPROM: " + String(storedPublishInterval));
  }
  publishInterval = String(storedPublishInterval).toInt();



  // ===================== MQTT QoS =====================
  uint8_t storedQoS = EEPROM.read(EE_MQTT_QOS_ADDR);
  if (storedQoS == 0xFF || storedQoS > 2) {
    mqtt_qos = MQTT_QOS_DEFAULT;
    EEPROM.write(EE_MQTT_QOS_ADDR, mqtt_qos);
    LOG_EEPROM("EEPROM vazia. Gravando mqtt_qos como default.");
  } else {
    mqtt_qos = storedQoS;
    LOG_EEPROM("mqtt_qos lido da EEPROM: " + String(mqtt_qos));
  }

  // ===================== MQTT Retain =====================
  uint8_t storedRetain = EEPROM.read(EE_MQTT_RETAIN_ADDR);
  if (storedRetain == 0xFF || storedRetain > 1) {
    mqtt_retain = MQTT_RETAIN_DEFAULT;
    EEPROM.write(EE_MQTT_RETAIN_ADDR, mqtt_retain);
    LOG_EEPROM("EEPROM vazia. Gravando mqtt_retain como default.");
  } else {
    mqtt_retain = storedRetain ? true : false;
    LOG_EEPROM("mqtt_retain lido da EEPROM: " + String(mqtt_retain));
  }

  // ===================== MQTT Fixed IP =====================
  char storedFixedIp[EE_MQTT_FIXED_IP_LEN + 1];
  for (int i = 0; i < EE_MQTT_FIXED_IP_LEN; i++) {
    storedFixedIp[i] = EEPROM.read(EE_MQTT_FIXED_IP_ADDR + i);
  }
  storedFixedIp[EE_MQTT_FIXED_IP_LEN] = '\0';
  if (storedFixedIp[0] == 0xFF || storedFixedIp[0] == '\0') {
    strncpy(storedFixedIp, MQTT_FIXED_IP_DEFAULT, EE_MQTT_FIXED_IP_LEN);
    storedFixedIp[EE_MQTT_FIXED_IP_LEN] = '\0';
    for (int i = 0; i < EE_MQTT_FIXED_IP_LEN; i++) {
      EEPROM.write(EE_MQTT_FIXED_IP_ADDR + i, storedFixedIp[i]);
    }
    LOG_EEPROM("EEPROM vazia. Gravando mqtt_fixed_ip como default.");
  } else {
    LOG_EEPROM("mqtt_fixed_ip lido da EEPROM: " + String(storedFixedIp));
  }
  mqtt_fixed_ip = String(storedFixedIp);

  // ===================== WiFi STA Enable =====================
  uint8_t storedWifiStaEnable = EEPROM.read(EE_WIFI_STA_ENABLE_ADDR);
  if (storedWifiStaEnable == 0xFF || (storedWifiStaEnable != 0 && storedWifiStaEnable != 1)) {
    wifi_sta_enable = 0;
    EEPROM.write(EE_WIFI_STA_ENABLE_ADDR, wifi_sta_enable);
    LOG_EEPROM("EEPROM vazia. Gravando wifi_sta_enable como 0 (desabilitado)");
  } else {
    wifi_sta_enable = storedWifiStaEnable;
    LOG_EEPROM("wifi_sta_enable lido da EEPROM: " + String(wifi_sta_enable));
  }

  EEPROM.commit();
}
// ---- UPDATE: WiFi SSID ----
void updateWifiSSIDInEEPROM(const char* novoSSID) {
  char newSSID[EE_WIFI_SSID_LEN + 1];
  strncpy(newSSID, novoSSID, EE_WIFI_SSID_LEN);
  newSSID[EE_WIFI_SSID_LEN] = '\0';
  for (int i = 0; i < EE_WIFI_SSID_LEN; i++) {
    EEPROM.write(EE_WIFI_SSID_ADDR + i, newSSID[i]);
  }
  EEPROM.commit();
  wifiSSID = String(newSSID);
  LOG_EEPROM("wifiSSID atualizado para: " + wifiSSID);
}

// ---- UPDATE: WiFi Password ----
void updateWifiPassInEEPROM(const char* novaSenha) {
  char newPass[EE_WIFI_PASS_LEN + 1];
  strncpy(newPass, novaSenha, EE_WIFI_PASS_LEN);
  newPass[EE_WIFI_PASS_LEN] = '\0';
  for (int i = 0; i < EE_WIFI_PASS_LEN; i++) {
    EEPROM.write(EE_WIFI_PASS_ADDR + i, newPass[i]);
  }
  EEPROM.commit();
  wifiPass = String(newPass);
  LOG_EEPROM("wifiPass atualizado para: " + wifiPass);
}

// ---- UPDATE: MQTT Server ----
void updateMqttServerInEEPROM(const char* newServer) {
  char newServerBuffer[EE_MQTT_SERVER_LEN + 1];
  strncpy(newServerBuffer, newServer, EE_MQTT_SERVER_LEN);
  newServerBuffer[EE_MQTT_SERVER_LEN] = '\0';
  for (int i = 0; i < EE_MQTT_SERVER_LEN; i++) {
    EEPROM.write(EE_MQTT_SERVER_ADDR + i, newServerBuffer[i]);
  }
  EEPROM.commit();
  mqtt_server = String(newServerBuffer);
  LOG_EEPROM("mqtt_server atualizado para: " + mqtt_server);
}

// ---- UPDATE: MQTT Port ----
void updateMqttPortInEEPROM(const char* newPortStr) {
  char newPortBuffer[EE_MQTT_PORT_LEN + 1];
  strncpy(newPortBuffer, newPortStr, EE_MQTT_PORT_LEN);
  newPortBuffer[EE_MQTT_PORT_LEN] = '\0';
  for (int i = 0; i < EE_MQTT_PORT_LEN; i++) {
    EEPROM.write(EE_MQTT_PORT_ADDR + i, newPortBuffer[i]);
  }
  EEPROM.commit();
  mqtt_port = String(newPortBuffer).toInt();
  LOG_EEPROM("mqtt_port atualizado para: " + String(mqtt_port));
}

// ---- UPDATE: MQTT Username ----
void updateMqttUsernameInEEPROM(const char* newUsername) {
  char newUsernameBuffer[EE_MQTT_USER_LEN + 1];
  strncpy(newUsernameBuffer, newUsername, EE_MQTT_USER_LEN);
  newUsernameBuffer[EE_MQTT_USER_LEN] = '\0';
  for (int i = 0; i < EE_MQTT_USER_LEN; i++) {
    EEPROM.write(EE_MQTT_USER_ADDR + i, newUsernameBuffer[i]);
  }
  EEPROM.commit();
  mqtt_username = String(newUsernameBuffer);
  LOG_EEPROM("mqtt_username atualizado para: " + mqtt_username);
}

// ---- UPDATE: MQTT Password ----
void updateMqttPasswordInEEPROM(const char* newPassword) {
  char newPasswordBuffer[EE_MQTT_PASS_LEN + 1];
  strncpy(newPasswordBuffer, newPassword, EE_MQTT_PASS_LEN);
  newPasswordBuffer[EE_MQTT_PASS_LEN] = '\0';
  for (int i = 0; i < EE_MQTT_PASS_LEN; i++) {
    EEPROM.write(EE_MQTT_PASS_ADDR + i, newPasswordBuffer[i]);
  }
  EEPROM.commit();
  mqtt_password = String(newPasswordBuffer);
  LOG_EEPROM("mqtt_password atualizado para: " + mqtt_password);
}

// ---- UPDATE: Sensor Interval ----
void updateSensorIntervalInEEPROM(const char* newIntervalStr) {
  char newIntervalBuffer[EE_SENSOR_INTERVAL_LEN + 1];
  strncpy(newIntervalBuffer, newIntervalStr, EE_SENSOR_INTERVAL_LEN);
  newIntervalBuffer[EE_SENSOR_INTERVAL_LEN] = '\0';
  for (int i = 0; i < EE_SENSOR_INTERVAL_LEN; i++) {
    EEPROM.write(EE_SENSOR_INTERVAL_ADDR + i, newIntervalBuffer[i]);
  }
  EEPROM.commit();
  sensorInterval = String(newIntervalBuffer).toInt();
  LOG_EEPROM("sensor_interval atualizado para: " + String(sensorInterval) + " segundos");
}

// ---- UPDATE: Publish Interval ----
void updatePublishIntervalInEEPROM(const char* newIntervalStr) {
  char newIntervalBuffer[EE_PUBLISH_INTERVAL_LEN + 1];
  strncpy(newIntervalBuffer, newIntervalStr, EE_PUBLISH_INTERVAL_LEN);
  newIntervalBuffer[EE_PUBLISH_INTERVAL_LEN] = '\0';
  for (int i = 0; i < EE_PUBLISH_INTERVAL_LEN; i++) {
    EEPROM.write(EE_PUBLISH_INTERVAL_ADDR + i, newIntervalBuffer[i]);
  }
  EEPROM.commit();
  publishInterval = String(newIntervalBuffer).toInt();
  LOG_EEPROM("publish_interval atualizado para: " + String(publishInterval) + " segundos");
}


// ---- UPDATE: MQTT QoS ----
void updateMqttQoSInEEPROM(uint8_t qos) {
  if (qos > 2) qos = MQTT_QOS_DEFAULT;
  EEPROM.write(EE_MQTT_QOS_ADDR, qos);
  EEPROM.commit();
  mqtt_qos = qos;
  LOG_EEPROM("mqtt_qos atualizado para: " + String(mqtt_qos));
}

// ---- UPDATE: MQTT Retain ----
void updateMqttRetainInEEPROM(bool retain) {
  EEPROM.write(EE_MQTT_RETAIN_ADDR, retain ? 1 : 0);
  EEPROM.commit();
  mqtt_retain = retain;
  LOG_EEPROM("mqtt_retain atualizado para: " + String(mqtt_retain));
}

// ---- UPDATE: MQTT Fixed IP ----
void updateMqttFixedIpInEEPROM(const char* novoIp) {
  char newIp[EE_MQTT_FIXED_IP_LEN + 1];
  strncpy(newIp, novoIp, EE_MQTT_FIXED_IP_LEN);
  newIp[EE_MQTT_FIXED_IP_LEN] = '\0';
  for (int i = 0; i < EE_MQTT_FIXED_IP_LEN; i++) {
    EEPROM.write(EE_MQTT_FIXED_IP_ADDR + i, newIp[i]);
  }
  EEPROM.commit();
  mqtt_fixed_ip = String(newIp);
  LOG_EEPROM("mqtt_fixed_ip atualizado para: " + mqtt_fixed_ip);
}

// ---- UPDATE: WiFi STA Enable ----
void updateWifiStaEnableInEEPROM(bool enable) {
  EEPROM.write(EE_WIFI_STA_ENABLE_ADDR, enable ? 1 : 0);
  EEPROM.commit();
  wifi_sta_enable = enable ? 1 : 0;
  LOG_EEPROM("wifi_sta_enable atualizado para: " + String(wifi_sta_enable));
  ESP.restart();
}

/***************************************************************
 *  Outras funções auxiliares
 ***************************************************************/
void sortArray(int array[], int size) {
  for (int i = 0; i < size - 1; i++) {
    int minIndex = i;
    for (int j = i + 1; j < size; j++) {
      if (array[j] < array[minIndex]) {
        minIndex = j;
      }
    }
    if (minIndex != i) {
      int temp = array[i];
      array[i] = array[minIndex];
      array[minIndex] = temp;
    }
  }
}

/***************************************************************
 *  Funções de Publicação unificada
***************************************************************/
void publishAllInputs1() {
  if (!canPublish()) return;
  if (!mqttClient.connected()) {
  LOG_MQTT("MQTT não conectado. Temperaturas não enviadas.");
    return;
  }
  DynamicJsonDocument doc(512);
  doc["mac"] = cmac_ap;
  doc["ph"] = roundf(ph * 100.0f) / 100.0f;
  doc["orp"] = roundf(orp);
  doc["adc1"] = adc1_val;
  doc["adc2"] = adc2_val;
  doc["temp"] = temperatura1;
  doc["di01"] = di01State;
  doc["di02"] = di02State;
  doc["di03"] = di03State;
  doc["di04"] = di04State;
  doc["di05"] = di05State;
  doc["di06"] = di06State;
  doc["di07"] = di07State;
  doc["di08"] = di08State;

  char mqtt_message[512];
  serializeJson(doc, mqtt_message);

  String topic = "qipi/allinputs";
  topic.toCharArray(cOutTopic, 40);

  if (mqttClient.connected()) {
    mqttClient.publish(cOutTopic, mqtt_qos, mqtt_retain, mqtt_message);
    LOG_READINGS("Grupo 1 publicado em " + topic + ": " + String(mqtt_message));
  } else {
    LOG_MQTT("Erro: MQTT desconectado.");
  }
}

/***************************************************************
 *  Funções de Publicação Separada
 ***************************************************************/
void publishDigitalGroup1() {
  if (!canPublish()) return;
  if (!mqttClient.connected()) {
  LOG_MQTT("MQTT não conectado. Temperaturas não enviadas.");
    return;
  }
  DynamicJsonDocument doc(256);
  doc["mac"] = cmac_ap;  // usa cmac_ap para identificar o primeiro dispositivo
  // Entradas digitais 1 a 8
  doc["di01"] = di01State;
  doc["di02"] = di02State;
  doc["di03"] = di03State;
  doc["di04"] = di04State;
  doc["di05"] = di05State;
  doc["di06"] = di06State;
  doc["di07"] = di07State;
  doc["di08"] = di08State;

  char buffer[256];
  serializeJson(doc, buffer);
  String topic = "qipi/digital_inputs";  // mesmo tópico
  mqttClient.publish(topic.c_str(), mqtt_qos, mqtt_retain, buffer);
  LOG_READINGS("Group1 Digital published: " + String(buffer));
}

void publishAnalogGroup1() {
  if (!canPublish()) return;
  if (!mqttClient.connected()) {
  LOG_MQTT("MQTT não conectado. Temperaturas não enviadas.");
    return;
  }
  DynamicJsonDocument doc(256);
  doc["mac"] = cmac_ap;
  // Dados analógicos para dispositivo 1
  doc["ph"] = roundf(ph * 100.0f) / 100.0f;
  doc["orp"] = roundf(orp);
  doc["adc1"] = adc1_val;
  doc["adc2"] = adc2_val;
  char buffer[256];
  serializeJson(doc, buffer);
  String topic = "qipi/analog_inputs";
  mqttClient.publish(topic.c_str(), mqtt_qos, mqtt_retain, buffer);
  LOG_READINGS("Group1 Analog published: " + String(buffer));
}

void publishTemperatures() {
  if (!canPublish()) return;
  if (!mqttClient.connected()) {
  LOG_MQTT("MQTT não conectado. Temperaturas não enviadas.");
    return;
  }

  DynamicJsonDocument doc(256);
  doc["mac"] = cmac_ap;
  doc["t1"] = temperatura1;
  //doc["t3"] = temperatura3;

  char buffer[256];
  serializeJson(doc, buffer);

  String topic = "qipi/temperatures";
  mqttClient.publish(topic.c_str(), mqtt_qos, mqtt_retain, buffer);
  LOG_READINGS("Temperaturas publicadas: " + String(buffer));
}
/***************************************************************
 *  OTA e Download
 ***************************************************************/
void getFileFromServer() {
  WiFiClientSecure client;
  client.setInsecure();
  // Remove any previous firmware file to free space for the new download
  LittleFS.remove("/" + String(FILE_NAME));
  if (client.connect(REMOTEHOST, REMOTEPORT)) {
    LOG_OTA("Connected to server");
    client.print("GET " + String(REMOTEPATH) + " HTTP/1.1\r\n");
    client.print("Host: " + String(REMOTEHOST) + "\r\n");
    client.println("Connection: close\r\n");
    client.println();
    File file = LittleFS.open("/" + String(FILE_NAME), FILE_WRITE);
    if (!file) {
      LOG_OTA("Failed to open file for writing");
      return;
    }
    bool endOfHeaders = false;
    String headers = "";
    String http_response_code = "error";
    const size_t bufferSize = 1024;
    uint8_t buffer[bufferSize];
    while (client.connected() && !endOfHeaders) {
      if (client.available()) {
        char c = client.read();
        headers += c;
        if (headers.startsWith("HTTP/1.1")) {
          http_response_code = headers.substring(9, 12);
        }
        if (headers.endsWith("\r\n\r\n")) {
          endOfHeaders = true;
        }
      }
    }
    LOG_OTA("HTTP response code: " + http_response_code);
    while (client.connected()) {
      if (client.available()) {
        size_t bytesRead = client.readBytes(buffer, bufferSize);
        file.write(buffer, bytesRead);
      }
    }
    file.close();
    client.stop();
    LOG_OTA("File saved successfully");
  } else {
    LOG_OTA("Failed to connect to server");
  }
}

void performOTAUpdateFromLittleFS() {
  File file = LittleFS.open("/" + String(FILE_NAME), FILE_READ);
  if (!file) {
    LOG_OTA("Failed to open file for reading");
    return;
  }
  LOG_OTA("Starting update..");
  size_t fileSize = file.size();
  LOG_OTA(String(fileSize));
  if (!Update.begin(fileSize, U_FLASH)) {
    LOG_OTA("Cannot do the update");
    file.close();
    return;
  }
  Update.writeStream(file);
  if (Update.end()) {
    LOG_OTA("Successful update");
    file.close();
    // Delete the firmware file to free space after a successful update
    LittleFS.remove("/" + String(FILE_NAME));
  } else {
    LOG_OTA("Error Occurred: " + String(Update.getError()));
    file.close();
    return;
  }
  LOG_OTA("Reset in 4 seconds....");
  delay(4000);
  ETH.end();  // libera o driver
  delay(100);
  ESP.restart();
}

void onOTAStart() {
  LOG_OTA("OTA update started!");
}
void onOTAProgress(size_t current, size_t final) {
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    LOG_PRINTF("[OTA] Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}
void onOTAEnd(bool success) {
  if (success) {
    LOG_OTA("OTA update finished successfully!");
  } else {
    LOG_OTA("There was an error during OTA update!");
  }
}

/***************************************************************
 *  Auxiliares de IO
 ***************************************************************/
void setupADC() {
  pinMode(adc1, INPUT);
  pinMode(adc2, INPUT);
}

void setupPCF8574() {
  for (int pin = P0; pin <= P7; pin++) {
    pcf8574_I1.pinMode(pin, INPUT);
    pcf8574_R1.pinMode(pin, OUTPUT);
    pcf8574_R2.pinMode(pin, OUTPUT);
    pcf8574_R1.digitalWrite(pin, HIGH);
    pcf8574_R2.digitalWrite(pin, HIGH);
  }
  LOG_READINGS(pcf8574_I1.begin() ? "pcf8574_I1 OK" : "pcf8574_I1 KO");
}

/***************************************************************
 *  Funções de Leitura Separadas
 ***************************************************************/
void readTemperatures() {
  sensors.requestTemperatures();

  temperatura1 = sensors.getTempCByIndex(0);

  if (temperatura1 == -127.0f) temperatura1 = 0;
  //if (temperatura3 == -127.0f) temperatura3 = 0;

  LOG_READINGS("Temp1: " + String(temperatura1));
  /*LOG_PRINT("Temp3: "); LOG_PRINTLN(temperatura3);*/

  bool mudou1 = abs(temperatura1 - last_temp1) >= 1.0f;

  if (mudou1) {
    publishTemperatures();
    last_temp1 = temperatura1;
  }
}

void readDigitalInputs() {
  bool group1Changed = false;

  // Grupo 1
  int state01 = !pcf8574_I1.digitalRead(P0);
  int state02 = !pcf8574_I1.digitalRead(P1);
  int state03 = !pcf8574_I1.digitalRead(P2);
  int state04 = !pcf8574_I1.digitalRead(P3);
  int state05 = !pcf8574_I1.digitalRead(P4);
  int state06 = !pcf8574_I1.digitalRead(P5);
  int state07 = !pcf8574_I1.digitalRead(P6);
  int state08 = !pcf8574_I1.digitalRead(P7);

  if (ciclosLeitura < ciclosIgnorados) {
    di01State = last_di01State = state01;
    di02State = last_di02State = state02;
    di03State = last_di03State = state03;
    di04State = last_di04State = state04;
    di05State = last_di05State = state05;
    di06State = last_di06State = state06;
    di07State = last_di07State = state07;
    di08State = last_di08State = state08;
  } else {
    if (state01 != di01State && ++confirmacoesEstado[0] >= 2) {
      di01State = state01;
      group1Changed = true;
      confirmacoesEstado[0] = 0;
    } else if (state01 == di01State) confirmacoesEstado[0] = 0;
    if (state02 != di02State && ++confirmacoesEstado[1] >= 2) {
      di02State = state02;
      group1Changed = true;
      confirmacoesEstado[1] = 0;
    } else if (state02 == di02State) confirmacoesEstado[1] = 0;
    if (state03 != di03State && ++confirmacoesEstado[2] >= 2) {
      di03State = state03;
      group1Changed = true;
      confirmacoesEstado[2] = 0;
    } else if (state03 == di03State) confirmacoesEstado[2] = 0;
    if (state04 != di04State && ++confirmacoesEstado[3] >= 2) {
      di04State = state04;
      group1Changed = true;
      confirmacoesEstado[3] = 0;
    } else if (state04 == di04State) confirmacoesEstado[3] = 0;
    if (state05 != di05State && ++confirmacoesEstado[4] >= 2) {
      di05State = state05;
      group1Changed = true;
      confirmacoesEstado[4] = 0;
    } else if (state05 == di05State) confirmacoesEstado[4] = 0;
    if (state06 != di06State && ++confirmacoesEstado[5] >= 2) {
      di06State = state06;
      group1Changed = true;
      confirmacoesEstado[5] = 0;
    } else if (state06 == di06State) confirmacoesEstado[5] = 0;
    if (state07 != di07State && ++confirmacoesEstado[6] >= 2) {
      di07State = state07;
      group1Changed = true;
      confirmacoesEstado[6] = 0;
    } else if (state07 == di07State) confirmacoesEstado[6] = 0;
    if (state08 != di08State && ++confirmacoesEstado[7] >= 2) {
      di08State = state08;
      group1Changed = true;
      confirmacoesEstado[7] = 0;
    } else if (state08 == di08State) confirmacoesEstado[7] = 0;
  }

  /*LOG_PRINTLN("Grupo 1:");
  LOG_PRINT("Entrada 01: "); LOG_PRINTLN(state01);
  LOG_PRINT("Entrada 02: "); LOG_PRINTLN(state02);
  LOG_PRINT("Entrada 03: "); LOG_PRINTLN(state03);
  LOG_PRINT("Entrada 04: "); LOG_PRINTLN(state04);
  LOG_PRINT("Entrada 05: "); LOG_PRINTLN(state05);
  LOG_PRINT("Entrada 06: "); LOG_PRINTLN(state06);
  LOG_PRINT("Entrada 07: "); LOG_PRINTLN(state07);
  LOG_PRINT("Entrada 08: "); LOG_PRINTLN(state08);
  */

  // Leitura de Grupo 2 removida (apenas 8 entradas digitais)

  ciclosLeitura++;

  if (group1Changed) {
    publishDigitalGroup1();
    publishAllInputs1();
    LOG_READINGS("Digital Group 1 changed.");
  }

}

void readAnalogInputs() {
  // Grupo 1 - Leitura e processamento
  for (int i = 0; i < numSamples; i++) {
    delay(10);
    phSamples[i]  = analogRead(adc1);
    orpSamples[i] = analogRead(adc2);
  }
  sortArray(phSamples, numSamples);
  sortArray(orpSamples, numSamples);
  adc1_val = phSamples[numSamples / 2];
  adc2_val = orpSamples[numSamples / 2];

  ph  = (0.0036f * adc1_val) + 0.684f;
  orp = (0.7658f * adc2_val) - 1312.3f;

  LOG_READINGS("pH: " + String(ph));
  LOG_READINGS("ORP: " + String(orp));

  // Comunicação grupo 1
  grupo1Online = ph > 2.0f || orp > -500.0f;
  if (!grupo1Online && !grupo1OfflineAlertSent) {
    sendDeviceMessage("Controladora 1 sem comunicação");
    LOG_READINGS("Controladora 1 sem comunicação - alerta enviado");
    grupo1OfflineAlertSent = true;
  } else if (grupo1Online) {
    grupo1OfflineAlertSent = false;
  }

  // Publicar se mudou
  if (abs(ph - last_ph_sent) > limite_ph || abs(orp - last_orp_sent) > limite_orp) {
    publishAnalogGroup1();
    publishAllInputs1();
    last_ph_sent  = ph;
    last_orp_sent = orp;
    LOG_READINGS("Mudança significativa - Controladora 1.");
  }


}


// Função que chama ambas as leituras
void readInputs() {
  // Também solicita as temperaturas
  readTemperatures();
  readDigitalInputs();
  readAnalogInputs();
}

void TaskSensors(void* pvParameters) {
  (void)pvParameters;
  unsigned long startTime = millis();

  for (;;) {
    if ((millis() - startTime) > (startupDelaySensors * 1000UL)) {
      readInputs();
    }
    vTaskDelay((sensorInterval * 1000) / portTICK_PERIOD_MS);  // sensorInterval já é em segundos
  }
}


void TaskPublish(void* pvParameters) {
  (void)pvParameters;
  vTaskDelay((startupDelayPublish * 1000) / portTICK_PERIOD_MS); // 10 min

  while (ciclosLeitura < ciclosIgnorados) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }

  readInputs();

  for (;;) {
    if (mqtt_connected) {
      publishDigitalGroup1();
      publishAnalogGroup1();
      publishTemperatures();
      publishAllInputs1();

    } else {
      LOG_PRINTLN("[MQTT] Não conectado, pulando publish.");
    }
    vTaskDelay((publishInterval * 1000) / portTICK_PERIOD_MS);
  }
}


void TaskOTA(void* pvParameters) {
  (void)pvParameters;
  vTaskDelay((startupDelayOTA * 1000) / portTICK_PERIOD_MS); // 1 min

  for (;;) {
    ElegantOTA.loop();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void resetConfig() {
  updateWifiSSIDInEEPROM("Q1-LAB");
  updateWifiPassInEEPROM("q1ambiental");
  updateMqttServerInEEPROM("srv812575.hstgr.cloud");
  updateMqttPortInEEPROM("1883");
  updateMqttUsernameInEEPROM("hostinger");
  updateMqttPasswordInEEPROM("lsdyg9t$Adf");
  updateSensorIntervalInEEPROM("5");
  updatePublishIntervalInEEPROM("1800");


  // QoS e Retain padrão
  updateMqttQoSInEEPROM(1);
  updateMqttRetainInEEPROM(false);

  // IP fixo MQTT padrão
  updateMqttFixedIpInEEPROM("82.25.74.168");

  //WiFi STA padrão
  updateWifiStaEnableInEEPROM(false);

  LOG_EEPROM("Configurações resetadas para os valores padrão.");
  ETH.end();  // libera o driver
  delay(100);
  ESP.restart();
}

void checkSerialCommands() {
  /* para atualizar algum parametro via serial, usar o formato:
  {
    "mac": "2CBCBB65A52C",
    "mqtt_fixed_ip": "82.25.74.168"
  }
  */
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.equalsIgnoreCase("RESET")) {
      resetConfig();
      ETH.end();  // libera o driver
      delay(100);
      ESP.restart();
      return;
    }
    if (input.equalsIgnoreCase("RESTART")) {
      ETH.end();  // libera o driver
      delay(100);
      ESP.restart();
      return;
    }
    // Tenta interpretar como JSON
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, input);
    if (error) {
      LOG_EEPROM("Erro ao interpretar JSON da serial: " + String(error.f_str()));
      return;
    }

    if (!doc.containsKey("mac")) {
      LOG_EEPROM("Parâmetro 'mac' ausente no JSON serial.");
      return;
    }

    const char* macReceived = doc["mac"];
    if (strcmp(macReceived, cmac_ap) == 0) {
      if (doc.containsKey("ssid")) updateWifiSSIDInEEPROM(String(doc["ssid"]).c_str());
      if (doc.containsKey("senha")) updateWifiPassInEEPROM(String(doc["senha"]).c_str());
      if (doc.containsKey("mqtt_server")) updateMqttServerInEEPROM(String(doc["mqtt_server"]).c_str());
      if (doc.containsKey("mqtt_port")) updateMqttPortInEEPROM(String(doc["mqtt_port"]).c_str());
      if (doc.containsKey("mqtt_username")) updateMqttUsernameInEEPROM(String(doc["mqtt_username"]).c_str());
      if (doc.containsKey("mqtt_password")) updateMqttPasswordInEEPROM(String(doc["mqtt_password"]).c_str());
      if (doc.containsKey("sensor_interval")) updateSensorIntervalInEEPROM(String(doc["sensor_interval"]).c_str());
      if (doc.containsKey("publish_interval")) updatePublishIntervalInEEPROM(String(doc["publish_interval"]).c_str());
      if (doc.containsKey("mqtt_fixed_ip")) updateMqttFixedIpInEEPROM(String(doc["mqtt_fixed_ip"]).c_str());
      if (doc.containsKey("mqtt_qos")) updateMqttQoSInEEPROM(doc["mqtt_qos"]);
      if (doc.containsKey("mqtt_retain")) updateMqttRetainInEEPROM(doc["mqtt_retain"]);
      if (doc.containsKey("wifi_sta_enable")) updateWifiStaEnableInEEPROM(doc["wifi_sta_enable"]);


      LOG_EEPROM("Configurações aplicadas via Serial com sucesso!");
    } else {
      LOG_EEPROM("MAC recebido não corresponde ao dispositivo.");
    }
  }
}

/***************************************************************
 *  Setup e Loop (RTOS)
 ***************************************************************/
void setup() {
  //systemStartMillis = millis();
  Serial.begin(115200);
  setupEEPROM();
  if (!LittleFS.begin(true)) {
    LOG_OTA("Erro ao montar LittleFS");
  }

  beginConnection();
  delay(5000);
  setupADC();
  setupPCF8574();
  sensors.begin();
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onMessage(onMqttMessage);

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else {  // U_SPIFFS
        type = "filesystem";
      }

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      LOG_OTA("Start updating " + type);
    })
    .onEnd([]() {
      LOG_OTA("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      LOG_PRINTF("[OTA] Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      LOG_PRINTF("[OTA] Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
        LOG_OTA("Auth Failed");
      } else if (error == OTA_BEGIN_ERROR) {
        LOG_OTA("Begin Failed");
      } else if (error == OTA_CONNECT_ERROR) {
        LOG_OTA("Connect Failed");
      } else if (error == OTA_RECEIVE_ERROR) {
        LOG_OTA("Receive Failed");
      } else if (error == OTA_END_ERROR) {
        LOG_OTA("End Failed");
      }
    });

  ArduinoOTA.begin();

  serverRoutes();
  server.begin();

  // Indicador de boot nos relés
  pcf8574_R1.digitalWrite(P0, 0);
  delay(1000);
  //pcf8574_R1.digitalWrite(P0, 1);
  // Criação das tasks RTOS
  xTaskCreate(TaskNetwork, "NetworkTask", 4096, NULL, 3, NULL);
  xTaskCreate(TaskSensors, "SensorTask", 4096, NULL, 2, NULL);
  xTaskCreate(TaskPublish, "PublishTask", 4096, NULL, 2, NULL);
}

void loop() {
  ArduinoOTA.handle();
  checkSerialCommands();
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}
