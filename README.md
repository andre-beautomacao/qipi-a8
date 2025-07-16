# Changelog

## 2025-07-16 16:50 BRT
- Removida `TaskOTA` e a biblioteca `ElegantOTA` por não utilizar mais OTA via web.



## 2025-07-16 16:42 BRT
- Criada a task `TaskFirmwareUpdate` para baixar e aplicar o firmware em segundo plano quando recebido `qipi/update`.



## 2025-07-16 15:10 BRT
- Firmware OTA agora é baixado diretamente para a flash sem usar LittleFS, evitando erros de espaço insuficiente.


## 2025-07-16 14:53 BRT
- Limpeza de todos os arquivos do LittleFS antes do download OTA para evitar falta de espaço.

## 2025-07-16 14:27 BRT
- Removido o arquivo de firmware do LittleFS antes do download e apos uma atualizacao OTA bem-sucedida para evitar falta de espaco.

## 2025-07-16 13:59 BRT
- Corrigida declaracao de `cmac_ap` com tamanho fixo para uso de `sizeof()`.


## 2025-07-16 13:51 BRT
- Renomeada variável `cMacAP` para `cmac_ap` e atualizada em todo o código.
- `cmac_ap` agora recebe o MAC do AP em `beginConnection()`.


## 2025-07-04 18:23 UTC
- Corrigido caminho remoto do OTA para o usuário `andre-beautomacao`.
- Arquivo binário permanece como `qipi-a8.bin`.

## 2025-07-04 18:18 UTC
- Atualizado diretório remoto para OTA.
- Arquivo binário agora se chama `qipi-a8.bin`.

## 2025-07-04 14:03 UTC
- Criação do arquivo `README.md` com o histórico de versões.

## 2025-07-04 17:29 UTC
- Simplificado para 8 entradas digitais, 2 analógicas e 1 sensor de temperatura.
- Removidas funções e rotas referentes ao segundo grupo de entradas.

## 2025-07-04 14:01 UTC
- Padronização das mensagens de log.
- Inclusão de categorias nas mensagens de log.

## 2025-07-03 14:39 UTC
- Adicionado "external linkage" para URL.
- Incluso cabeçalho ETH no servidor web.

## 2025-07-02 20:17 UTC
- URL de IP público acessível globalmente.
- Adicionado arquivo de configuração Ethernet.
- Definidos sensores globais como `extern`.
- Refatoração do código em módulos (rede, MQTT e servidor web).
- Inclusos cabeçalhos WiFi e Ethernet.
- Definições Ethernet PHY movidas para cabeçalho.

## 2025-07-02 14:50 UTC
- Versão inicial carregada para o repositório.
