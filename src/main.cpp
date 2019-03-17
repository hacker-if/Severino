#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include <BLAKE2s.h>
#include <base64.hpp>

#include <TimeLib.h>
#include <NtpClientLib.h>

#include <SPI.h>
#include <MFRC522.h>

// Pinos do ISP
#define SS_PIN  15    // D8
#define RST_PIN 0     // D3

// Definições de pinos
#define BUTTON_PIN  16  // D0
#define OPEN_PIN    5   // D1
#define LED_PIN     4   // D2

// definições de parâmetros de tempo
#define T_DESTRAVADO 60000L
#define T_ABERTO 1000
#define T_ADCIONAR 30000L

// definições para as mensagens
#define SEP '$'


//---------------------------------------------//
//            VARIÁVEIS GLOBAIS
//---------------------------------------------//
// variáveis do controle da porta
unsigned long t_destravado, t_aberto, t_adcionar;
bool destravado, aberto, adcionar_uid;
unsigned int user_id;

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
byte sig_key[] = "c4d18dd0141c3d40cbdb4ed8f1373b8f";
byte key_len = sizeof(sig_key) - 1;
const byte sig_len = 16;
const byte b64_len = (sig_len + 2) / 3 * 4;

// Leitor NFC MFRC522
MFRC522 mfrc522(SS_PIN, RST_PIN); // Instance of the class


//---------------------------------------------//
//            FUNÇÕES
//---------------------------------------------//
void sign(byte* hash, byte* msg, byte msg_len) {
  blake.reset(sig_key, key_len, sig_len);
  blake.update(msg, msg_len);
  blake.finalize(hash, sig_len);
}

bool check_payload(byte* msg, byte msg_len, byte* sig) {
  byte test[sig_len];
  sign(test, msg, msg_len);
  byte b64[b64_len];
  encode_base64(test, sig_len, b64);
  for (int i = 0; i < b64_len; i++) {
    if (b64[i] != sig[i]) return false;
  }
  return true;
}

void mqtt_publish(const char* topic, const char* msg) {
  // char* payload = (char*)malloc(l + 12 + b64_len);
  char payload[40 + b64_len];
  sprintf(payload, "%s.%lu", msg, NTP.getTime());

  byte hash[sig_len];
  byte msg_len = (byte)strlen(payload);
  Serial.println(msg_len);

  sign(hash, (byte *)payload, msg_len);
  unsigned char hash2[b64_len];
  encode_base64(hash, sig_len, hash2);
  strcat(payload, "$");
  strcat(payload, reinterpret_cast<const char *>(hash2));

  mqtt_client.publish(topic, payload);
}

void destravar_porta() {
  t_destravado = millis();
  destravado = true;
  digitalWrite(LED_PIN, HIGH);
  Serial.println("Porta destravada");
  mqtt_client.publish(mqtt_outTopic, "Porta destravada");
}

void travar_porta() {
  destravado = false;
  digitalWrite(LED_PIN, LOW);
  Serial.println("Porta travada");
  // mqtt_client.publish(mqtt_outTopic, "Porta travada");
}

void abre_porta() {
  aberto = true;
  digitalWrite(OPEN_PIN, HIGH);
  Serial.println("Porta aberta");
  travar_porta();
  mqtt_publish(mqtt_outTopic, "aberto");
}

void reconnectWifi() {
  Serial.print("Conectado-se a rede ");
  Serial.print(ssid);
  Serial.println("...");
  WiFi.begin(ssid, pass);

  if (WiFi.waitForConnectResult() != WL_CONNECTED)
    return;

  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Iniciando NTP
  NTP.begin();

  randomSeed(micros());
}

bool reconnectMQTT() {
  Serial.println("Conectando-se ao MQTT...");
  String client_id = "clientid_";
  client_id += String(random(0xffff), HEX);
  if (mqtt_client.connect(client_id.c_str(), mqtt_username, mqtt_passwd)) {
    // Once connected, publish an announcement...
    mqtt_client.publish(mqtt_outTopic, "hello world", true);
    // ... and resubscribe
    mqtt_client.subscribe(mqtt_inTopic);
    Serial.print("Conectado ao broker: ");
    Serial.println(mqtt_server);
  } else {
    Serial.print("falha, rc=");
    Serial.print(mqtt_client.state());
    Serial.print(" tentando novamente em ");
    Serial.print(mqtt_rcinterval/1000);
    Serial.println(" segundos");
  }
  return mqtt_client.connected();
}

// callback que lida com as mensagem recebidas
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  // encontrando o tamanho da mensagem, limitada por SEP
  byte msg_len = 0;
  for (; msg_len < length && payload[msg_len] != SEP; msg_len++);
  // tamanho da assinatura
  byte sig_size = length - msg_len - 1;

  // ignora msg se a assinatura não tiver o tamanho correto
  if (sig_size != b64_len) {
    Serial.println("msg. mal formatada");
    return;
  }

  // guarda msg como string
  char* msg = (char*)malloc(msg_len + 1);
  memcpy(msg, (char*)payload, msg_len);
  msg[msg_len + 1] = '\0';

  // guarda assinatura como lista de bytes
  byte* sig = (byte*)malloc(sig_size);
  memcpy(sig, payload + msg_len + 1, sig_size);

  // testa assinatura
  bool check = check_payload((byte*)msg, msg_len, sig);

  // ignora msg se a assinatura forneceda é incorreta
  if (!check) {
    Serial.println("ass. invalida");
    return;
  }

  // agora que a msg foi autenticada execute o que foi pedido
  Serial.println("ass. autenticada");

  char* data = strtok(msg, ".");     // comando
  long temp = atol(strtok(NULL, "\0")); // timestamp do envio da msg

  if ( abs(temp - NTP.getTime()) > 10) {
    Serial.println("timestamp expirado");
    return;
  }

  char* command = strtok(data, ":");

  if(strcmp(command, "liberar") == 0) {
    destravar_porta();
  } else if (strcmp(command, "id") == 0) {
    adcionar_uid = true;
    t_adcionar = millis();
    user_id = atol(strtok(NULL, ","));
  } else if (strcmp(command, "abrir") == 0) {
      abre_porta();
      t_aberto = millis();
  } else {
    Serial.println("comando invalido");
  }
}

void rfid_loop() {
  if (!mfrc522.PICC_IsNewCardPresent()) {
    delay(50);
    return;
  }
  if (!mfrc522.PICC_ReadCardSerial()) {
    delay(50);
    return;
  }
  mfrc522.PICC_HaltA();

  char msg[20];
  byte msg_len;
  if (adcionar_uid) {
    sprintf(msg, "id:%d,uid:", user_id);
    adcionar_uid = false;
    msg_len = strlen(msg);
  } else {
    strcpy(msg, "uid:");
    msg_len = 4;
  }

  for (int i = 0; i < mfrc522.uid.size; ++i) {
    sprintf(&msg[msg_len + i*2], "%02x", mfrc522.uid.uidByte[i]);
  }

  Serial.println(msg);
  mqtt_publish(mqtt_outTopic, msg);
}

//---------------------------------------------//
//                  SETUP
//---------------------------------------------//
void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  //configuração dos pinos
  pinMode(OPEN_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

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

  if (adcionar_uid && millis() - t_adcionar > T_ADCIONAR) {
    adcionar_uid = false;
  }

  // Controle da porta
  // Se a porta estiver liberada para abertura...
  if (destravado) {
  // abre a porta se o botão for pressionado
    if (digitalRead(BUTTON_PIN) == LOW) {
      abre_porta();
      t_aberto = millis();
    }

    // retira a liberação da porta depois de 'T_LIBERADO' ms
    if (millis() - t_destravado > T_DESTRAVADO) {
      travar_porta();
    }
  }

  // Mantém a porta aberta por "T_ABERTO" ms e para
  if (aberto && millis() - t_aberto > T_ABERTO) {
    digitalWrite(OPEN_PIN, LOW);
    aberto = false;
  }

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

  // executa loop do MQTT
  if (mqtt_client.connected()) mqtt_client.loop();
}
