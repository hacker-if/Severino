#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <BLAKE2s.h>
#include <Base64.h>
#include <TimeLib.h>
#include <NtpClientLib.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Door.h>

// Abilita msg. por serial
#define DEBUG

// Pinos do ISP
#define SS_PIN  15    // D8
#define RST_PIN 0     // D3

// Definições de pinos
#define BUTTON_PIN  2   // D4
#define OPEN_PIN    5   // D1
#define LED_PIN     4   // D2

//---------------------------------------------//
//            VARIÁVEIS GLOBAIS
//---------------------------------------------//
// variáveis do controle da porta
Door porta(OPEN_PIN, BUTTON_PIN, LED_PIN);
unsigned int user_id;
byte last_mode = 0;

// variáveis do wifi
const char* ssid = "IFnet";
const char* pass = "";
const unsigned long wifi_rcinterval = 1000; // Intervalo entre reconexões
unsigned long wifi_lastrc;
WiFiClient wclient;

//variável do cliente MQTT
// const char* mqtt_server = "broker.mqttdashboard.com";
const char* mqtt_server = "18.231.106.101";
const char* mqtt_inTopic = "fechadura";   // nome do tópico de publicação
const char* mqtt_outTopic = "server";  // nome do tópico de inscrição
const char* mqtt_username = "fechadura";
const char* mqtt_passwd = "1234";
const unsigned long mqtt_rcinterval = 3000;     // Intervalo entre reconexões
unsigned long mqtt_lastrc;
PubSubClient mqtt_client(wclient);

// Variáveis para autenticação de msgs
BLAKE2s blake;
// byte sig_key[] = "you-will-never-guess-again";
const byte sig_key[] = "c4d18dd0141c3d40cbdb4ed8f1373b8f";
const byte key_len = sizeof(sig_key) - 1;
const byte sig_len = 16;
const byte b64_len = Base64.encodedLength(sig_len);

// Leitor NFC MFRC522
MFRC522 mfrc522(SS_PIN, RST_PIN);

//---------------------------------------------//
//            FUNÇÕES
//---------------------------------------------//
void sign(char* hash, char* msg, byte msg_len) {
  char sig[sig_len];
  blake.reset(sig_key, key_len, sig_len);
  blake.update(msg, msg_len);
  blake.finalize(sig, sig_len);
  Base64.encode(hash, sig, sig_len);
  sig[b64_len] = '\0';
}

bool check_payload(char* hash, char* msg, byte msg_len) {
  char test[b64_len + 1];
  sign(test, msg, msg_len);

  bool result = true;

  for (int i = 0; i < b64_len; i++)
    if (test[i] != hash[i]) result = false;

  return result;
}

void mqtt_publish(const char* topic, const char* msg) {
  char payload[40 + b64_len];
  sprintf(payload, "%s.%lu", msg, NTP.getTime());

  byte msg_len = (byte)strlen(payload);

  payload[msg_len] = '$';
  sign(&payload[msg_len + 1], payload, msg_len);

  mqtt_client.publish(topic, payload);
}

// callback que lida com as mensagem recebidas
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  // encontrando o tamanho da mensagem, limitada por SEP
  byte msg_len = 0;
  for (; msg_len < length && payload[msg_len] != '$'; msg_len++);
  // tamanho da assinatura
  byte sig_size = length - msg_len - 1;

  // ignora msg se a assinatura não tiver o tamanho correto
  if (sig_size != b64_len) {
    #ifdef DEBUG
    Serial.println("msg. mal formatada");
    #endif

    return;
  }

  // guarda msg como string
  char* msg = (char*)malloc(msg_len + 1);
  memcpy(msg, (char*)payload, msg_len);
  msg[msg_len] = '\0';

  // guarda assinatura como lista de bytes
  char sig[b64_len];
  memcpy(sig, payload + msg_len + 1, sig_size);

  // ignora msg se a assinatura forneceda é incorreta
  if (!check_payload(sig, msg, msg_len)) {
    #ifdef DEBUG
    Serial.println("ass. invalida");
    #endif

    return;
  }

  // agora que a msg foi autenticada execute o que foi pedido
  #ifdef DEBUG
  Serial.println("ass. autenticada");
  #endif

  char* data = strtok(msg, ".");     // comando
  long temp = atol(strtok(NULL, "")); // timestamp do envio da msg

  if (abs(temp - NTP.getTime()) > 10) {
    #ifdef DEBUG
    Serial.println("timestamp expirado");
    #endif

    return;
  }

  char* command = strtok(data, ":");

  if(strcmp(command, "liberar") == 0) {
    porta.unlock();
  } else if (strcmp(command, "id") == 0) {
    porta.add_card();
    user_id = atol(strtok(NULL, ","));
  } else if (strcmp(command, "abrir") == 0) {
    porta.open();
  }
  #ifdef DEBUG
  else {
    Serial.println("comando invalido");
  }
  #endif
}// definições para as mensagens
#define SEP '$'

void rfid_loop() {
  if (porta.mode == 1 || porta.mode == 2)
    return;

  if (!mfrc522.PICC_IsNewCardPresent())
    return;

  if (!mfrc522.PICC_ReadCardSerial())
    return;

  mfrc522.PICC_HaltA();

  char msg[20];
  byte msg_len;
  if (porta.mode == 3) {
    sprintf(msg, "id:%d,uid:", user_id);
    msg_len = strlen(msg);
    porta.lock();
  } else {
    strcpy(msg, "uid:");
    msg_len = 4;
  }

  for (int i = 0; i < mfrc522.uid.size; ++i) {
    sprintf(&msg[msg_len + i*2], "%02x", mfrc522.uid.uidByte[i]);
  }
  #ifdef DEBUG
  Serial.println(msg);
  #endif
  mqtt_publish(mqtt_outTopic, msg);
}

void reconnectWifi() {
  #ifdef DEBUG
  Serial.print("Conectado-se a rede ");
  Serial.print(ssid);
  Serial.println("...");
  #endif

  WiFi.begin(ssid, pass);

  if (WiFi.waitForConnectResult() != WL_CONNECTED)
    return;

  #ifdef DEBUG
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  #endif

  // Iniciando NTP
  NTP.begin();

  randomSeed(micros());
}

bool reconnectMQTT() {
  #ifdef DEBUG
  Serial.println("Conectando-se ao MQTT...");
  #endif
  String client_id = "clientid_";
  client_id += String(random(0xffff), HEX);
  if (mqtt_client.connect(client_id.c_str(), mqtt_username, mqtt_passwd)) {
    // Once connected, publish an announcement...
    mqtt_client.publish(mqtt_outTopic, "hello world", true);
    // ... and resubscribe
    mqtt_client.subscribe(mqtt_inTopic);

    #ifdef DEBUG
    Serial.print("Conectado ao broker: ");
    Serial.println(mqtt_server);
    #endif
  }
  #ifdef DEBUG
  else {
    Serial.print("falha, rc=");
    Serial.print(mqtt_client.state());
    Serial.print(" tentando novamente em ");
    Serial.print(mqtt_rcinterval/1000);
    Serial.println(" segundos");
  }
  #endif

  return mqtt_client.connected();
}

//---------------------------------------------//
//                  SETUP
//---------------------------------------------//
void setup() {
  #ifdef DEBUG
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  #endif

  // configuracao do wifi
  WiFi.mode(WIFI_STA);

  // configuração do MQTT
  mqtt_client.setServer(mqtt_server, 1883);
  mqtt_client.setCallback(mqtt_callback);

  // configura o NFC
  SPI.begin(); // Init SPI bus
  mfrc522.PCD_Init(); // Init MFRC522
}

//---------------------------------------------//
//                  LOOP
//---------------------------------------------//
void loop() {
  // Tentar ler cartao e adquirir seu serial
  rfid_loop();

  // loop o controle da porta
  porta.update();

  // enviar msg se a porta estiver aberta
  if (porta.mode != last_mode) {
    last_mode = porta.mode;
    if (porta.mode == 2) mqtt_publish(mqtt_outTopic, "aberto");

    #ifdef DEBUG
    if (porta.mode == 0) Serial.println("Porta travada");
    if (porta.mode == 1) Serial.println("Porta destravada");
    if (porta.mode == 2) Serial.println("Porta aberta");
    if (porta.mode == 3) Serial.println("Adcionando cartao");
    #endif
  }

  // executa loop do MQTT
  if (mqtt_client.connected()) mqtt_client.loop();

  // reconecta ao wifi
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long now = millis();
    // Desabilita NTP
    NTP.stop();
    if(now - wifi_lastrc > wifi_rcinterval) {
      reconnectWifi();
      wifi_lastrc = now;
    }
  }

  // reconecta ao servido MQTT
  if (WiFi.status() == WL_CONNECTED && !mqtt_client.connected()) {
    unsigned long now = millis();
    if (now - mqtt_lastrc > mqtt_rcinterval) {
      reconnectMQTT();
      mqtt_lastrc = now;
    }
  }
}
