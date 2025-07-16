#ifndef NETWORK_MQTT_H
#define NETWORK_MQTT_H

#include <Arduino.h>
#include <AsyncMqttClient.h>
void beginConnection();
void exibirDadosRede(bool isEthernet);
void checarDnsEsp32();
void connectToMqtt();
void onMqttConnect(bool sessionPresent);
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);
void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
void getIPdata();
void sendDeviceMessage(const String& mensagem);
void atualizarLedMQTT();
void TaskNetwork(void* pvParameters);

#endif // NETWORK_MQTT_H
