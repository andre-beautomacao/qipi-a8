#include "web_server.h"

#include <Update.h>
#include <ArduinoJson.h>
#include <ETH.h>
#include "network_mqtt.h"
#include "PCF8574.h"

// Extern globals from main sketch
extern const char* http_username;
extern const char* http_password;
extern String wifiSSID;
extern String wifiPass;
extern uint8_t wifi_sta_enable;
extern String mqtt_server;
extern uint16_t mqtt_port;
extern String mqtt_username;
extern String mqtt_password;
extern String mqttClientId;
extern String mqtt_fixed_ip;
extern uint8_t mqtt_qos;
extern bool mqtt_retain;
extern unsigned long sensorInterval;
extern unsigned long publishInterval;
extern volatile bool mqtt_connected;
extern char cmac_ap[];

// Sensor and I/O values
extern float ph, orp, temperatura1;
extern int adc1_val, adc2_val;
extern int di01State, di02State, di03State, di04State,
           di05State, di06State, di07State, di08State;

extern String logBuffer[];
extern int logWriteIndex;

extern void addLog(const String& msg);
extern void addLogRaw(const String& msg);
extern void logf(const char* fmt, ...);
extern void connectToMqtt();
extern void resetConfig();
extern void updateWifiSSIDInEEPROM(const char* novoSSID);
extern void updateWifiPassInEEPROM(const char* novaSenha);
extern void updateMqttServerInEEPROM(const char* newServer);
extern void updateMqttPortInEEPROM(const char* newPortStr);
extern void updateMqttUsernameInEEPROM(const char* newUsername);
extern void updateMqttPasswordInEEPROM(const char* newPassword);
extern void updateSensorIntervalInEEPROM(const char* newIntervalStr);
extern void updatePublishIntervalInEEPROM(const char* newIntervalStr);
extern void updateMqttFixedIpInEEPROM(const char* novoIp);
extern void updateMqttQoSInEEPROM(uint8_t qos);
extern void updateMqttRetainInEEPROM(bool retain);
extern void updateWifiStaEnableInEEPROM(bool enable);
extern void updateGrupo2AtivoInEEPROM(bool ativo);

#define LOG_PRINT(msg)      addLogRaw(String(msg))
#define LOG_PRINTLN(msg)    addLog(String(msg))
#define LOG_PRINTF(...)     logf(__VA_ARGS__)

// Categorized log helpers
#define LOG_MQTT(msg)       LOG_PRINTLN(String("[MQTT] ") + msg)
#define LOG_NETWORK(msg)    LOG_PRINTLN(String("[NETWORK] ") + msg)
#define LOG_EEPROM(msg)     LOG_PRINTLN(String("[EEPROM] ") + msg)
#define LOG_READINGS(msg)   LOG_PRINTLN(String("[READINGS] ") + msg)
#define LOG_OTA(msg)        LOG_PRINTLN(String("[OTA] ") + msg)
#define LOG_BUFFER_SIZE 40

AsyncWebServer server(80);


void serverRoutes() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!request->authenticate(http_username, http_password)) {
      return request->requestAuthentication();
    }

    String extraTabBtn = "";

    String page = R"rawliteral(
      <!DOCTYPE html>
      <html lang="pt-br">
      <head>
        <meta charset="UTF-8">
        <title>QIPI - Interface</title>
        <style>
          body { font-family: Arial; background: #f4f4f4; margin: 0; padding: 20px; }
          .tabs { display: flex; margin-bottom: 18px; }
          .tab-btn { flex:1; padding: 12px 0; background: #eee; border: none; cursor: pointer; font-size: 16px; }
          .tab-btn.active { background: #2196F3; color: #fff; font-weight: bold; }
          .tab-content { display: none; background: #fff; margin-top: 0; padding: 18px 8px 10px 8px; border-radius: 0 0 8px 8px; }
          .tab-content.active { display: block; }
          label { display: block; margin-top: 8px; font-weight: bold; }
          input[type="text"], input[type="number"], input[type="password"], select {
            width: 280px; padding: 6px; margin-top: 4px; margin-bottom: 8px; font-size: 15px;
          }
          button { margin-top: 10px; padding: 8px 18px; background: #2196F3; color: #fff; border: none; cursor: pointer; border-radius: 4px;}
          #mqttStatus { margin-left: 15px; font-weight: bold;}
          .status-ok { color: #218838; }
          .status-err { color: #c00; }
          .config-section { margin-bottom:28px; border-bottom:1px solid #eee; padding-bottom: 12px;}
          .config-section:last-child { border-bottom:none; }
          #logsContent { background:#222;color:#b5fa88;padding:12px 6px;height:280px;overflow:auto; font-size:15px;}
          #otaProgressBar {width:100%;background:#ccc;display:none;margin-top:12px;}
          #otaProgress {background:#2196F3;width:0;height:10px;transition:width .2s;}
        </style>
      </head>
      <body>
        <div class="tabs">
          <button class="tab-btn active" onclick="showTab(0)">Status</button>
          <button class="tab-btn" onclick="showTab(1)">Configurações</button>
          <button class="tab-btn" onclick="showTab(2)">Logs</button>
          <button class="tab-btn" onclick="showTab(3)">OTA / Firmware</button>"
    )rawliteral";
    page += extraTabBtn;
    page += R"rawliteral(
        </div>
        <div id="tab0" class="tab-content active">
          <h2>Status Atual</h2>
          <div id="status-dados">Carregando...</div>
        </div>
        <div id="tab1" class="tab-content">
          <h2>Configurações</h2>
          <div class="config-section">
            <h3>
              MQTT 
              <span id="mqttStatus" style="font-size: 16px; margin-left: 12px;"></span>
            </h3>
            <form id="mqttForm">
              <label>Host:</label><input type="text" id="mqtt_server">
              <label>Porta:</label><input type="number" id="mqtt_port">
              <label>Usuário:</label><input type="text" id="mqtt_username">
              <label>Senha:</label><input type="text" id="mqtt_password">
              <label>ClientId:</label><input type="text" id="mqtt_clientid" readonly>
              <label>QoS:</label><input type="number" id="mqtt_qos" min="0" max="2">
              <label>Retain:</label><input type="number" id="mqtt_retain" min="0" max="1">
              <label>IP fixo (Broker):</label><input type="text" id="mqtt_fixed_ip">
              <label>Intervalo de publicação (s):</label><input type="number" id="publish_interval">
              <div style="margin-top:14px;">
                <button type="button" onclick="salvarMQTT()">Salvar MQTT</button>
                <button type="button" onclick="conectarMQTT()" style="background:#1db014;margin-left:10px;">Conectar MQTT</button>
              </div>
            </form>
          </div>
          <div class="config-section">
            <h3>Rede</h3>
            <form id="redeForm">
              <fieldset style="margin-bottom:16px; border:1px solid #ddd; padding:10px;">
                <legend><b>Wi-Fi</b></legend>
                <label>SSID:</label><input type="text" id="wifi_ssid">
                <label>Senha Wi-Fi:</label><input type="text" id="wifi_pass">
                <label>IP:</label><input type="text" id="ip_wifi" readonly>
                <label>Máscara:</label><input type="text" id="mask_wifi" readonly>
                <label>Gateway:</label><input type="text" id="gw_wifi" readonly>
                <label>DNS:</label><input type="text" id="dns_wifi" readonly>
                <label>MAC Wi-Fi:</label><input type="text" id="mac_wifi" readonly>
                <div style="margin-bottom:12px;">
                  <label>Wi-Fi STA habilitado:</label>
                  <input type="checkbox" id="wifiStaEnable">
                  <span id="wifiStaStatus"></span>
                </div>
                <div style="margin-top:14px;">
                  <button type="button" onclick="salvarRede()">Salvar Wi-Fi</button>
                </div>
              </fieldset>
              <fieldset style="margin-bottom:16px; border:1px solid #ddd; padding:10px;">
                <legend><b>Ethernet</b></legend>
                <label>IP:</label><input type="text" id="ip_eth" readonly>
                <label>Máscara:</label><input type="text" id="mask_eth" readonly>
                <label>Gateway:</label><input type="text" id="gw_eth" readonly>
                <label>DNS:</label><input type="text" id="dns_eth" readonly>
                <label>MAC ETH:</label><input type="text" id="mac_eth" readonly>
              </fieldset>
            </form>

          </div>
        </div>
        <div id="tab2" class="tab-content">
          <h2>Logs do Sistema</h2>
          <button onclick="carregarLogs()" style="margin-bottom:12px;">Atualizar</button>
          <pre id="logsContent"></pre>
        </div>
        <div id="tab3" class="tab-content">
          <h2>OTA (Atualização de Firmware)</h2>
          <form id="otaForm">
            <input type="file" id="firmware" name="firmware" accept=".bin">
            <button type="submit">Enviar Firmware</button>
          </form>
          <div id="otaMsg" style="margin-top:12px; color: #2196F3;"></div>
          <div id="otaProgressBar">
            <div id="otaProgress"></div>
          </div>
          <div style="margin-top:20px">
            <button style="background:#F68C20;margin-right:14px;" onclick="resetConfig()">Reset Configurações</button>
            <button style="background:#c00;" onclick="rebootDevice()">Reiniciar Dispositivo</button>
          </div>
        </div>
        <div id="tab4" class="tab-content" style="display:none;">
          <h2>Status 2 (Grupo 2)</h2>
          <div id="status2-dados">Carregando...</div>
        </div>
        <script>
          function showTab(idx) {
            let btns = document.querySelectorAll('.tab-btn');
            let tabs = document.querySelectorAll('.tab-content');
            btns.forEach((b,i) => b.classList.toggle('active', i==idx));
            tabs.forEach((t,i) => t.classList.toggle('active', i==idx));
            if(idx==0) { loadStatus(); }
            else if(idx==1) { loadMQTT(); loadRede(); }
            else if(idx==2) carregarLogs();
          }
          async function loadStatus() {
            const res = await fetch('/statusGrupo1');
            const data = await res.json();
            let html = '';
            for(let k in data){
              html += `<b>${k}:</b> ${data[k]}<br>`;
            }
            document.getElementById('status-dados').innerHTML = html;
          }
          async function loadMQTT() {
            const res = await fetch('/getConfig');
            const data = await res.json();
            document.getElementById('mqtt_server').value = data.mqtt_server;
            document.getElementById('mqtt_port').value = data.mqtt_port;
            document.getElementById('mqtt_username').value = data.mqtt_username;
            document.getElementById('mqtt_password').value = data.mqtt_password;
            document.getElementById('mqtt_clientid').value = data.nome || "";
            document.getElementById('mqtt_qos').value = data.mqtt_qos;
            document.getElementById('mqtt_retain').value = data.mqtt_retain;
            document.getElementById('mqtt_fixed_ip').value = data.mqtt_fixed_ip;
            document.getElementById('publish_interval').value = data.publish_interval;
            let s = document.getElementById('mqttStatus');
            if (data.mqtt_connected) {
              s.innerText = "Conectado";
              s.className = "status-ok";
            } else {
              s.innerText = "Desconectado";
              s.className = "status-err";
            }
          }
          async function salvarMQTT() {
            let body = {
              mac: "",
              mqtt_server: document.getElementById('mqtt_server').value,
              mqtt_port: parseInt(document.getElementById('mqtt_port').value),
              mqtt_username: document.getElementById('mqtt_username').value,
              mqtt_password: document.getElementById('mqtt_password').value,
              mqtt_clientid: document.getElementById('mqtt_clientid').value,
              mqtt_qos: parseInt(document.getElementById('mqtt_qos').value),
              mqtt_retain: parseInt(document.getElementById('mqtt_retain').value),
              mqtt_fixed_ip: document.getElementById('mqtt_fixed_ip').value,
              publish_interval: parseInt(document.getElementById('publish_interval').value)
            };
            await fetch('/setConfig', {
              method: 'POST',
              headers: { 'Content-Type': 'application/json' },
              body: JSON.stringify(body)
            });
            alert("Configurações MQTT salvas.");
            loadMQTT();
          }
          async function conectarMQTT() {
            let btn = event.target;
            btn.disabled = true;
            btn.innerText = "Conectando...";
            const res = await fetch('/connectMqtt');
            const txt = await res.text();
            alert(txt || "Comando enviado.");
            loadMQTT();
            btn.disabled = false;
            btn.innerText = "Conectar MQTT";
          }
          async function loadRede() {
            const res = await fetch('/getConfig');
            const data = await res.json();
            // Carrega Wi-Fi STA enable
            document.getElementById('wifiStaEnable').checked = !!data.wifi_sta_enable;
            document.getElementById('wifiStaStatus').innerText = data.wifi_sta_enable ? "Habilitado" : "Desabilitado";

            // Wi-Fi (editáveis)
            document.getElementById('wifi_ssid').value = data.ssid || "";
            document.getElementById('wifi_pass').value = data.senha || "";

            // Wi-Fi (readonly)
            document.getElementById('ip_wifi').value = data.ip_wifi || "";
            document.getElementById('mask_wifi').value = data.mask_wifi || "";
            document.getElementById('gw_wifi').value = data.gw_wifi || "";
            document.getElementById('dns_wifi').value = data.dns_wifi || "";
            document.getElementById('mac_wifi').value = data.mac_wifi || "";

            // Ethernet (readonly)
            document.getElementById('ip_eth').value = data.ip_eth || "";
            document.getElementById('mask_eth').value = data.mask_eth || "";
            document.getElementById('gw_eth').value = data.gw_eth || "";
            document.getElementById('dns_eth').value = data.dns_eth || "";
            document.getElementById('mac_eth').value = data.mac_eth || "";
          }


          async function salvarRede() {
            let body = {
              mac: "",
              wifi_ssid: document.getElementById('wifi_ssid').value,
              wifi_pass: document.getElementById('wifi_pass').value,
              wifi_sta_enable: document.getElementById('wifiStaEnable').checked ? 1 : 0,
            };
            await fetch('/setConfig', {
              method: 'POST',
              headers: { 'Content-Type': 'application/json' },
              body: JSON.stringify(body)
            });
            alert("Configurações Wi-Fi salvas.");
            loadRede();
          }
          async function carregarLogs() {
            const res = await fetch('/log');
            const txt = await res.text();
            document.getElementById('logsContent').innerText = txt || 'Sem logs.';
          }
          document.addEventListener('DOMContentLoaded', function() {
            // Controle do toggle Wi-Fi STA habilitado
            var wifiStaEnableElem = document.getElementById('wifiStaEnable');
            var wifiStaStatusElem = document.getElementById('wifiStaStatus');
            if (wifiStaEnableElem && wifiStaStatusElem) {
              wifiStaEnableElem.addEventListener('change', function() {
                wifiStaStatusElem.innerText = this.checked ? "Habilitado" : "Desabilitado";
              });
            }
          });

          document.getElementById("otaForm").onsubmit = function(e){
            e.preventDefault();
            const otaMsg = document.getElementById('otaMsg');
            const otaBar = document.getElementById('otaProgressBar');
            const otaProgress = document.getElementById('otaProgress');
            otaBar.style.display = "block";
            otaProgress.style.width = "0";
            otaMsg.innerText = "Enviando firmware...";
            let fileInput = document.getElementById('firmware');
            let file = fileInput.files[0];
            if (!file) { otaMsg.innerText = "Selecione um arquivo .bin."; otaBar.style.display = "none"; return; }
            let form = new FormData();
            form.append("firmware", file);

            let xhr = new XMLHttpRequest();
            xhr.open("POST", "/update", true);
            xhr.upload.onprogress = function(e) {
              if (e.lengthComputable) {
                let percent = Math.round((e.loaded / e.total) * 100);
                otaProgress.style.width = percent + "%";
                otaMsg.innerText = `Enviando firmware... (${percent}%)`;
              }
            };
            xhr.onload = function() {
              otaMsg.innerText = xhr.responseText;
              otaProgress.style.width = "100%";
              if (xhr.responseText.toLowerCase().includes('reiniciando')) setTimeout(()=>location.reload(), 5000);
              setTimeout(() => { otaBar.style.display = "none"; }, 5000);
            };
            xhr.onerror = function() {
              otaMsg.innerText = "Falha ao enviar firmware!";
              otaBar.style.display = "none";
            };
            xhr.send(form);
          };
          async function resetConfig() {
            if(confirm("Tem certeza que deseja resetar TODAS as configurações?")) {
              let r = await fetch('/resetConfig', {method:'POST'});
              alert(await r.text());
              location.reload();
            }
          }
          async function rebootDevice() {
            if(confirm("Deseja reiniciar o dispositivo agora?")) {
              let r = await fetch('/reboot', {method:'POST'});
              alert(await r.text());
              setTimeout(()=>location.reload(),3000);
            }
          }
          loadStatus();
        </script>
      </body>
      </html>
    )rawliteral";
    request->send(200, "text/html", page);
    });

  server.on("/getConfig", HTTP_GET, [](AsyncWebServerRequest* request) {
    DynamicJsonDocument doc(768);  // aumenta um pouco o buffer

    // Campos antigos (já existentes)
    doc["mac_wifi"] = cmac_ap;
    doc["nome"] = mqttClientId;
    doc["ssid"] = wifiSSID;
    doc["senha"] = wifiPass;
    doc["mqtt_server"] = mqtt_server;
    doc["mqtt_port"] = mqtt_port;
    doc["mqtt_username"] = mqtt_username;
    doc["mqtt_password"] = mqtt_password;
    doc["mqtt_qos"] = mqtt_qos;
    doc["mqtt_retain"] = mqtt_retain ? 1 : 0;
    doc["mqtt_connected"] = mqtt_connected;
    doc["sensor_interval"] = sensorInterval;
    doc["publish_interval"] = publishInterval;
    doc["mqtt_fixed_ip"] = mqtt_fixed_ip;
    doc["wifi_sta_enable"] = wifi_sta_enable;

    // ============ Dados de REDE (Wi-Fi e ETH) ============
    // Wi-Fi
    if (WiFi.status() == WL_CONNECTED) {
        doc["ip_wifi"]   = WiFi.localIP().toString();
        doc["mask_wifi"] = WiFi.subnetMask().toString();
        doc["gw_wifi"]   = WiFi.gatewayIP().toString();
        doc["dns_wifi"]  = WiFi.dnsIP().toString();
    } else {
        doc["ip_wifi"]   = "";
        doc["mask_wifi"] = "";
        doc["gw_wifi"]   = "";
        doc["dns_wifi"]  = "";
    }
    // Ethernet
    if (ETH.linkUp()) {
        doc["ip_eth"]   = ETH.localIP().toString();
        doc["mask_eth"] = ETH.subnetMask().toString();
        doc["gw_eth"]   = ETH.gatewayIP().toString();
        doc["dns_eth"]  = ETH.dnsIP().toString();
    } else {
        doc["ip_eth"]   = "";
        doc["mask_eth"] = "";
        doc["gw_eth"]   = "";
        doc["dns_eth"]  = "";
    }

    // ================== "Genéricos" (retrocompatibilidade) ==================
    // Prioriza Wi-Fi, se não tiver, pega Ethernet
    if (WiFi.status() == WL_CONNECTED) {
        doc["ip"]   = WiFi.localIP().toString();
        doc["mask"] = WiFi.subnetMask().toString();
        doc["gw"]   = WiFi.gatewayIP().toString();
        doc["dns"]  = WiFi.dnsIP().toString();
    } else if (ETH.linkUp()) {
        doc["ip"]   = ETH.localIP().toString();
        doc["mask"] = ETH.subnetMask().toString();
        doc["gw"]   = ETH.gatewayIP().toString();
        doc["dns"]  = ETH.dnsIP().toString();
    } else {
        doc["ip"]   = "";
        doc["mask"] = "";
        doc["gw"]   = "";
        doc["dns"]  = "";
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });


  server.on("/setConfig", HTTP_POST, [](AsyncWebServerRequest* request) {}, NULL,
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      String body;
      for (size_t i = 0; i < len; i++) body += (char)data[i];
      StaticJsonDocument<256> doc;
      DeserializationError error = deserializeJson(doc, body);
      if (error) {
        request->send(400, "application/json", "{\"error\":\"JSON inválido\"}");
        return;
      }
      // Comentei a verificação do MAC para evitar erro ao enviar em branco!
      // const char* macReceived = doc["mac"];
      // if (strcmp(macReceived, cmac_ap) != 0 && strcmp(macReceived, cmac_ap2) != 0) {
      //   request->send(403, "application/json", "{\"error\":\"MAC não corresponde\"}");
      //   return;
      // }
      if (doc.containsKey("wifi_ssid")) updateWifiSSIDInEEPROM(String(doc["wifi_ssid"]).c_str());
      if (doc.containsKey("wifi_pass")) updateWifiPassInEEPROM(String(doc["wifi_pass"]).c_str());
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

      request->send(200, "application/json", "{\"status\":\"Configurações atualizadas com sucesso\"}");
    });


  server.on("/statusGrupo1", HTTP_GET, [](AsyncWebServerRequest* request) {
    DynamicJsonDocument doc(512);
    doc["ph"] = ph;
    doc["orp"] = orp;
    doc["adc1"] = adc1_val;
    doc["adc2"] = adc2_val;
    doc["temp1"] = temperatura1;
    doc["di01"] = di01State;
    doc["di02"] = di02State;
    doc["di03"] = di03State;
    doc["di04"] = di04State;
    doc["di05"] = di05State;
    doc["di06"] = di06State;
    doc["di07"] = di07State;
    doc["di08"] = di08State;
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
    });


  server.on("/statusRede", HTTP_GET, [](AsyncWebServerRequest* request) {
    DynamicJsonDocument doc(512);
    doc["mac_wifi"] = cmac_ap;
    doc["ssid"] = wifiSSID;
    doc["senha"] = wifiPass;
    if (WiFi.status() == WL_CONNECTED) {
      doc["ip_wifi"] = WiFi.localIP().toString();
      doc["mask_wifi"] = WiFi.subnetMask().toString();
      doc["gw_wifi"] = WiFi.gatewayIP().toString();
    } else {
      doc["ip_wifi"] = "";
      doc["mask_wifi"] = "";
      doc["gw_wifi"] = "";
    }
    if (ETH.linkUp()) {
      doc["ip_eth"] = ETH.localIP().toString();
      doc["mask_eth"] = ETH.subnetMask().toString();
      doc["gw_eth"] = ETH.gatewayIP().toString();
    } else {
      doc["ip_eth"] = "";
      doc["mask_eth"] = "";
      doc["gw_eth"] = "";
    }
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
    });

  server.on("/resetConfig", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!request->authenticate(http_username, http_password)) {
      return request->requestAuthentication();
    }
    request->send(200, "text/plain", "Resetando configurações...");
    delay(500);
    resetConfig();
    });

  
    server.on("/log", HTTP_GET, [](AsyncWebServerRequest* request) {
    String logText = "";
    int idx = logWriteIndex;
    for (int i = 0; i < LOG_BUFFER_SIZE; i++) {
      int curIdx = (idx + i) % LOG_BUFFER_SIZE;
      if (logBuffer[curIdx].length() > 0)
        logText += logBuffer[curIdx] + "\n";
    }
    request->send(200, "text/plain", logText);
    });

  server.on("/connectMqtt", HTTP_GET, [](AsyncWebServerRequest* request) {
    connectToMqtt();
    request->send(200, "text/plain", "Comando para conectar MQTT enviado.");
    });

  server.on("/ota", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->redirect("/");
    });
  server.on("/update", HTTP_POST,
    [](AsyncWebServerRequest *request) {
      bool updateOK = !Update.hasError();
      String msg = updateOK ? "Update feito! Reiniciando..." : "Erro no update!";
      request->send(200, "text/plain", msg);
      if (updateOK) {
        delay(1000); ESP.restart();
      }
    },
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      if (!index) {
        LOG_PRINTF("[OTA] Recebendo: %s\n", filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
      }
      if (Update.write(data, len) != len) Update.printError(Serial);
      if (final) {
        if (Update.end(true)) LOG_PRINTF("[OTA] Sucesso: %u bytes\n", index + len);
        else Update.printError(Serial);
      }
    }
    );
  server.on("/reboot", HTTP_POST, [](AsyncWebServerRequest* req){
    req->send(200, "text/plain", "Reiniciando...");
    delay(300);
    ESP.restart();
    });// Outras rotas...
}

