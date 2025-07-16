#include "network_mqtt.h"
#include "ethernet_config.h"

#include <WiFi.h>
#include <ETH.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include "PCF8574.h"

// Extern globals from main sketch
extern String wifiSSID;
extern String wifiPass;
extern uint8_t wifi_sta_enable;
extern String apSSIDprefix;
extern const char* ap_password;
extern String mqtt_server;
extern uint16_t mqtt_port;
extern String mqtt_username;
extern String mqtt_password;
extern String mqttClientId;
extern String mqtt_fixed_ip;
extern volatile bool wifi_connected;
extern volatile bool eth_connected;
extern volatile bool mqtt_connected;
extern volatile uint8_t mqttPreferencia;
extern volatile uint8_t mqtt_interface;

extern char cmac_ap[20];

extern char cMacETH[];
extern AsyncMqttClient mqttClient;
extern PCF8574 pcf8574_R1;
extern String url;
extern String ip;
extern String last_ip;

// Functions defined elsewhere
extern void publishDigitalGroup1();
extern void publishAnalogGroup1();
extern void updateFirmwareFromServer();
extern void updateWifiSSIDInEEPROM(const char* novoSSID);
extern void updateWifiPassInEEPROM(const char* novaSenha);
extern void updateMqttServerInEEPROM(const char* newServer);
extern void updateMqttPortInEEPROM(const char* newPortStr);
extern void updateMqttUsernameInEEPROM(const char* newUsername);
extern void updateMqttPasswordInEEPROM(const char* newPassword);
extern void updateSensorIntervalInEEPROM(const char* newIntervalStr);
extern void updatePublishIntervalInEEPROM(const char* newIntervalStr);
extern void updateMqttQoSInEEPROM(uint8_t qos);
extern void updateMqttRetainInEEPROM(bool retain);
extern void updateMqttFixedIpInEEPROM(const char* novoIp);
extern void updateWifiStaEnableInEEPROM(bool enable);

extern void addLog(const String& msg);
extern void addLogRaw(const String& msg);
extern void logf(const char* fmt, ...);

#define LOG_PRINT(msg)      addLogRaw(String(msg))
#define LOG_PRINTLN(msg)    addLog(String(msg))
#define LOG_PRINTF(...)     logf(__VA_ARGS__)

// Categorized log helpers
#define LOG_MQTT(msg)       LOG_PRINTLN(String("[MQTT] ") + msg)
#define LOG_NETWORK(msg)    LOG_PRINTLN(String("[NETWORK] ") + msg)
#define LOG_EEPROM(msg)     LOG_PRINTLN(String("[EEPROM] ") + msg)
#define LOG_READINGS(msg)   LOG_PRINTLN(String("[READINGS] ") + msg)
#define LOG_OTA(msg)        LOG_PRINTLN(String("[OTA] ") + msg)

void getIPdata() {
  LOG_NETWORK("Obtendo IP Publico...");
  HTTPClient http;
  http.begin(url);
  int httpResponseCode = http.GET();
  if (httpResponseCode > 0) {
    ip = http.getString();
    if (ip != last_ip) {
      LOG_NETWORK("New public IP: " + ip);
      last_ip = ip;
    }
    sendDeviceMessage("Dispositivo reiniciado");
  } else {
    LOG_NETWORK(String("Não foi possível obter o endereço IP. Erro: ") + httpResponseCode);
  }
  http.end();
}

void sendDeviceMessage(const String& mensagem) {
  if (!mqttClient.connected()) {
    LOG_MQTT("MQTT não conectado. Mensagem não enviada.");
    return;
  }

  DynamicJsonDocument doc(256);
  doc["mac"] = cmac_ap;
  doc["ip"] = ip;
  doc["mensagem"] = mensagem;

  char mqtt_message[256];
  serializeJson(doc, mqtt_message);

  mqttClient.publish("qipi/devices", 1, false, mqtt_message);
  LOG_MQTT("Mensagem enviada para 'qipi/devices': " + mensagem);
}

void beginConnection() {
  ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER, ETH_CLK_MODE);
  delay(5000);
  unsigned long start = millis();
  while (((uint32_t)ETH.localIP() == 0) && (millis() - start < 10000)) {
    delay(100);
  }
  if ((uint32_t)ETH.localIP() != 0) {
    LOG_NETWORK(String("Ethernet IP: ") + ETH.localIP().toString());
  } else {
    LOG_NETWORK("Ethernet IP não obtido (timeout).");
  }

  String mac_eth = ETH.macAddress();
  mac_eth.replace(":", "");
  mac_eth.toCharArray(cMacETH, 20);

  if (wifi_sta_enable == 1) {
    WiFi.mode(WIFI_AP_STA);
    LOG_NETWORK("Wi-Fi em modo AP+STA.");
    WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());
  } else {
    WiFi.mode(WIFI_AP);
    LOG_NETWORK("Wi-Fi em modo AP.");
  }


  String apSSID = apSSIDprefix;    // Exemplo: "QIPI"
  String mac_ap;

  // 1. Crie AP temporário (ou apenas inicialize a interface)
  WiFi.softAP("TEMP", ap_password); // Qualquer SSID temporário

  // 2. Aguarde a interface inicializar e obtenha o MAC real
  delay(100);  // Pequeno delay para garantir inicialização
  mac_ap = WiFi.softAPmacAddress(); // Ex: "14:33:5C:45:08:7D"
  mac_ap.replace(":", "");          // Ex: "14335C45087D"
  mac_ap.toCharArray(cmac_ap, sizeof(cmac_ap));

  // 3. Monte o SSID definitivo
  apSSID += "_" + mac_ap;
  mqttClientId = apSSID;

  // 4. Desconecte o AP temporário e crie o AP definitivo
  WiFi.softAPdisconnect(true);
  delay(100);
  WiFi.softAP(apSSID.c_str(), ap_password);
  LOG_NETWORK("AP criado com SSID: " + apSSID);


  LOG_NETWORK("----- DADOS DE REDE -----");
  LOG_NETWORK("MAC Wi-Fi (AP):   " + WiFi.softAPmacAddress());
  LOG_NETWORK("-------------------");
  LOG_NETWORK("IP Wi-Fi:         " + WiFi.localIP().toString());
  LOG_NETWORK("MAC Wi-Fi (STA):  " + WiFi.macAddress());
  LOG_NETWORK("Gateway Wi-Fi:    " + WiFi.gatewayIP().toString());
  LOG_NETWORK("DNS Wi-Fi:        " + WiFi.dnsIP().toString());
  LOG_NETWORK("-------------------");
  LOG_NETWORK("MAC ETH:          " + mac_eth);
  LOG_NETWORK("IP ETH:           " + ETH.localIP().toString());
  LOG_NETWORK("Gateway ETH:      " + ETH.gatewayIP().toString());
  LOG_NETWORK("DNS ETH:          " + ETH.dnsIP().toString());
  LOG_NETWORK("-------------------");
  LOG_NETWORK("MAC AP p/ MQTT:   " + mac_ap);
}

void exibirDadosRede(bool isEthernet) {
  LOG_NETWORK("----- DADOS DE REDE -----");
  LOG_NETWORK("MAC Wi-Fi (AP):   " + WiFi.softAPmacAddress());
  LOG_NETWORK("-------------------");
  LOG_NETWORK("IP Wi-Fi:         " + WiFi.localIP().toString());
  LOG_NETWORK("MAC Wi-Fi:        " + WiFi.macAddress());
  LOG_NETWORK("Gateway Wi-Fi:    " + WiFi.gatewayIP().toString());
  LOG_NETWORK("DNS Wi-Fi:        " + WiFi.dnsIP().toString());
  LOG_NETWORK("------------------------");
  LOG_NETWORK("IP Ethernet:      " + ETH.localIP().toString());
  LOG_NETWORK("MAC Ethernet:     " + ETH.macAddress());
  LOG_NETWORK("Gateway Ethernet: " + ETH.gatewayIP().toString());
  LOG_NETWORK("DNS Ethernet:     " + ETH.dnsIP().toString());
  LOG_NETWORK("------------------------");
  LOG_NETWORK("MQTT:             " + String(mqtt_connected ? "Conectado" : "Desconectado"));
  LOG_NETWORK("Preferência MQTT: " + String(mqttPreferencia == 1 ? "Wi-Fi" : (mqttPreferencia == 2 ? "Ethernet" : "Nenhuma")));
  LOG_NETWORK("------------------------");
}

void checarDnsEsp32() {
  IPAddress dns = ETH.dnsIP();
  LOG_PRINTLN("[NETWORK] ETH.dnsIP(): " + dns.toString());

  WiFiClient testClient;
  int result = testClient.connect("google.com", 80);
  if (result) {
    LOG_PRINTLN("[NETWORK] DNS FUNCIONAL! Conseguiu conectar em google.com:80");
    testClient.stop();
  } else {
    LOG_PRINTLN("[NETWORK] Falha ao resolver domínio! DNS possivelmente NÃO funcional.");
  }
}

void connectToMqtt() {
  LOG_MQTT("Chamando connectToMqtt()");
  LOG_MQTT("eth_connected: " + String(eth_connected));
  LOG_MQTT("wifi_connected: " + String(wifi_connected));
  LOG_MQTT("[mqttPreferencia: " + String(mqttPreferencia));
  checarDnsEsp32();

  if (!(eth_connected || wifi_connected)) {
    LOG_MQTT("Sem conexão Ethernet/Wi-Fi. Não conectou MQTT.");
    return;
  }
  if (mqttClient.connected()) {
    LOG_MQTT("Já conectado ao broker MQTT");
    return;
  }

  mqttClient.setClientId(mqttClientId.c_str());
  mqttClient.setCredentials(mqtt_username.c_str(), mqtt_password.c_str());

  bool mqttConnectSuccess = false;

  if (mqtt_server.length() > 0) {
    LOG_PRINTLN("[MQTT] Tentando conectar via hostname: " + mqtt_server);
    mqttClient.setServer(mqtt_server.c_str(), mqtt_port);
    mqttClient.connect();
    delay(1800);
    if (mqttClient.connected()) {
      mqttConnectSuccess = true;
      LOG_PRINTLN("[MQTT] Conectado ao MQTT via hostname!");
    }
  }

  if (!mqttConnectSuccess && mqtt_fixed_ip.length() > 0) {
    IPAddress brokerIP;
    if (brokerIP.fromString(mqtt_fixed_ip)) {
      LOG_PRINTLN("[MQTT] Tentando conectar via IP fixo: " + mqtt_fixed_ip);
      mqttClient.setServer(brokerIP, mqtt_port);
      mqttClient.connect();
      delay(1800);
      if (mqttClient.connected()) {
        mqttConnectSuccess = true;
        LOG_PRINTLN("[MQTT] Conectado ao MQTT via IP fixo da EEPROM!");
      }
    } else {
      LOG_PRINTLN("[MQTT] IP fixo da EEPROM inválido: " + mqtt_fixed_ip);
    }
  }

  if (!mqttConnectSuccess) {
    IPAddress brokerIP(82, 25, 74, 168);
    LOG_PRINTLN("[MQTT] Tentando conectar via IP hardcoded: 82.25.74.168");
    mqttClient.setServer(brokerIP, mqtt_port);
    mqttClient.connect();
    delay(1800);
    if (mqttClient.connected()) {
      mqttConnectSuccess = true;
      LOG_PRINTLN("[MQTT] Conectado ao MQTT via IP hardcoded!");
    }
  }

  if (!mqttConnectSuccess) {
    LOG_PRINTLN("[MQTT] Falha ao conectar ao broker MQTT por todos os métodos.");
  }
}

void onMqttConnect(bool sessionPresent) {
  LOG_MQTT("Conectado ao MQTT!");
  getIPdata();
  mqtt_connected = true;
  atualizarLedMQTT();

  mqttClient.subscribe("qipi/input", 1);
  mqttClient.subscribe("qipi/update", 1);
  mqttClient.subscribe("qipi/restart", 1);
  mqttClient.subscribe("qipi/request", 1);
  mqttClient.subscribe("qipi/setConfig", 1);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  LOG_MQTT(String("MQTT desconectado. Razão: ") + String((uint8_t)reason));
  mqtt_connected = false;
  atualizarLedMQTT();
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  String incomingMessage;
  incomingMessage.reserve(len);
  for (size_t i = 0; i < len; i++) {
    incomingMessage += payload[i];
  }

  if (strcmp(topic, "qipi/setConfig") == 0) {
    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, incomingMessage);
    if (error) {
      LOG_PRINTLN(String("Erro ao deserializar JSON em setConfig: ") + error.f_str());
      return;
    }
    if (!doc.containsKey("mac")) {
      LOG_PRINTLN("Parâmetro mac ausente em setConfig.");
      return;
    }
    const char* macReceived = doc["mac"];
    if (strcmp(macReceived, cmac_ap) == 0) {
      if (doc.containsKey("ssid")) updateWifiSSIDInEEPROM(doc["ssid"]);
      if (doc.containsKey("senha")) updateWifiPassInEEPROM(doc["senha"]);
      if (doc.containsKey("mqtt_server")) updateMqttServerInEEPROM(doc["mqtt_server"]);
      if (doc.containsKey("mqtt_port")) updateMqttPortInEEPROM(doc["mqtt_port"]);
      if (doc.containsKey("mqtt_username")) updateMqttUsernameInEEPROM(doc["mqtt_username"]);
      if (doc.containsKey("mqtt_password")) updateMqttPasswordInEEPROM(doc["mqtt_password"]);
      if (doc.containsKey("sensor_interval")) updateSensorIntervalInEEPROM(doc["sensor_interval"]);
      if (doc.containsKey("publish_interval")) updatePublishIntervalInEEPROM(doc["publish_interval"]);
      if (doc.containsKey("mqtt_qos")) updateMqttQoSInEEPROM(doc["mqtt_qos"]);
      if (doc.containsKey("mqtt_retain")) updateMqttRetainInEEPROM(doc["mqtt_retain"]);
      if (doc.containsKey("mqtt_fixed_ip")) updateMqttFixedIpInEEPROM(doc["mqtt_fixed_ip"]);
      if (doc.containsKey("wifi_sta_enable")) updateWifiStaEnableInEEPROM(doc["wifi_sta_enable"]);
    } else {
      LOG_MQTT("Mac não confere. Ignorando atualização de configuração.");
    }
    return;
  }

  LOG_MQTT(String("Message arrived [") + topic + "]: " + incomingMessage);
  StaticJsonDocument<200> doc;
  DeserializationError err = deserializeJson(doc, incomingMessage);
  if (err) {
    LOG_MQTT(String("deserializeJson() failed: ") + err.f_str());
    return;
  }
  if (!doc.containsKey("mac")) {
    LOG_MQTT("Campo 'mac' ausente no JSON.");
    return;
  }
  const char* macReceived = doc["mac"];
  if (strcmp(macReceived, cmac_ap) == 0) {
    if (strcmp(topic, "qipi/update") == 0) {
      LOG_OTA("Nova atualização disponível");
      delay(2000);
      LOG_OTA("Baixando e atualizando...");
      updateFirmwareFromServer();
    } else if (strcmp(topic, "qipi/restart") == 0) {
      LOG_OTA("Reiniciando dispositivo...");
      ETH.end();
      delay(100);
      ESP.restart();
    } else if (strcmp(topic, "qipi/request") == 0) {
      LOG_MQTT("Solicitação de leituras recebida. Enviando dados...");
      publishDigitalGroup1();
      publishAnalogGroup1();
    }
  } else {
    LOG_MQTT("Endereço MAC não corresponde a este dispositivo.");
  }
}

void atualizarLedMQTT() {
  pcf8574_R1.digitalWrite(P3, mqtt_connected ? LOW : HIGH);
}

void TaskNetwork(void* pvParameters) {
  (void)pvParameters;

  for (;;) {
    bool ethStatus  = ETH.linkUp();
    bool wifiStatus = (WiFi.status() == WL_CONNECTED);

    if (eth_connected != ethStatus) {
      eth_connected = ethStatus;
      LOG_PRINTLN("[NETWORK] eth_connected atualizado: " + String(eth_connected ? "1 (conectado)" : "0 (desconectado)"));
      exibirDadosRede(false);
    }
    if (wifi_connected != wifiStatus) {
      wifi_connected = wifiStatus;
      LOG_PRINTLN("[NETWORK] wifi_connected atualizado: " + String(wifi_connected ? "1 (conectado)" : "0 (desconectado)"));
      exibirDadosRede(true);
    }

    if (wifi_connected && WiFi.dnsIP().toString() != "0.0.0.0") {
      if (!mqtt_connected || mqtt_interface != 1) {
        LOG_PRINTLN("[MQTT] Conectando pelo Wi-Fi...");
        mqttClient.disconnect(true);
        delay(100);
        mqtt_interface = 1;
        connectToMqtt();
      }
    } else if (eth_connected && ETH.dnsIP().toString() != "0.0.0.0") {
      if (!mqtt_connected || mqtt_interface != 2) {
        LOG_PRINTLN("[MQTT] Conectando pelo Ethernet (fallback)...");
        mqttClient.disconnect(true);
        delay(100);
        mqtt_interface = 2;
        connectToMqtt();
      }
    } else {
      if (mqtt_connected) {
        mqttClient.disconnect(true);
        mqtt_connected = false;
        mqtt_interface = 0;
      }
    }

    atualizarLedMQTT();

    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

