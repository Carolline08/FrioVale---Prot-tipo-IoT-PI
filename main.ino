#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
 
// ==========================
// CONFIGURAÇÕES DE REDE (Wi-Fi)
// ==========================
const char* ssid = "SENAC-Mesh";          // Substitua pelo nome do seu Wi-Fi
const char* password = "********";     // Substitua pela senha do seu Wi-Fi
 
// ==========================
// CONFIGURAÇÕES MQTT THINGSPEAK
// ==========================
const char* mqtt_server = "mqtt3.thingspeak.com";
const long  channelID   = 3513340; // Ex: 1234567 (Cole seu Channel ID numérico)
const char* mqttClientID = "HxsXBxMVDQEbDhYhOgU4Gwo";    // Cole seu Client ID fornecido pelo ThingSpeak
const char* mqttUser     = "HxsXBxMVDQEbDhYhOgU4Gwo";     // Cole seu Username fornecido pelo ThingSpeak
const char* mqttPass     = "t+ztAq6H1lIhG7KPP9eh5+VL";     // Cole seu Password fornecido pelo ThingSpeak
 
WiFiClient espClient;
PubSubClient client(espClient);
 
// ==========================
// PINOS E SENSORES
// ==========================
#define DHT_PIN 4
#define DHT_TYPE DHT11
 
#define LED_VERMELHO 5
#define LED_VERDE 6
 
// ==========================
// LIMITES DE TEMPERATURA
// ==========================
#define TEMP_MIN 7.0
#define TEMP_MAX 10.0
 
// ==========================
// VARIÁVEIS DE TEMPO (MILLIS)
// ==========================
unsigned long tempoAnterior = 0;
const unsigned long intervaloLeitura = 15000; // ThingSpeak recomenda intervalo mínimo de 15 segundos entre envios na conta gratuita
 
DHT dht(DHT_PIN, DHT_TYPE);
 
// ==========================
// CONEXÃO WI-FI
// ==========================
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando a: ");
  Serial.println(ssid);
 
  WiFi.begin(ssid, password);
 
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
 
  Serial.println("\nWi-Fi conectado!");
  Serial.print("Endereço IP: ");
  Serial.println(WiFi.localIP());
}
 
// ==========================
// RECONEXÃO MQTT
// ==========================
void reconnect() {
  while (!client.connected()) {
    Serial.print("Tentando conexão MQTT com ThingSpeak...");
   
    // Conecta passando ClientID, Username e Password
    if (client.connect(mqttClientID, mqttUser, mqttPass)) {
      Serial.println("Conectado com sucesso!");
    } else {
      Serial.print("Falha, rc=");
      Serial.print(client.state());
      Serial.println(" Tentando novamente em 5 segundos...");
      delay(5000);
    }
  }
}
 
// ==========================
// SETUP
// ==========================
void setup() {
  Serial.begin(115200);
 
  pinMode(LED_VERMELHO, OUTPUT);
  pinMode(LED_VERDE, OUTPUT);
 
  dht.begin();
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_VERMELHO, LOW);
 
  setup_wifi();
  client.setServer(mqtt_server, 1883);
 
  Serial.println("================================");
  Serial.println("        FRIOVALE - MQTT");
  Serial.println("   Monitoramento ThingSpeak");
  Serial.println("================================");
  Serial.println();
}
 
// ==========================
// LOOP
// ==========================
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
 
  unsigned long tempoAtual = millis();
 
  // Envia dados respeitando o intervalo definido
  if (tempoAtual - tempoAnterior >= intervaloLeitura) {
    tempoAnterior = tempoAtual;
 
    float temperatura = dht.readTemperature();
    float umidade = dht.readHumidity();
 
    if (isnan(temperatura) || isnan(umidade)) {
      Serial.println("ERRO AO LER DHT11!");
      digitalWrite(LED_VERDE, LOW);
      digitalWrite(LED_VERMELHO, HIGH);
      return;
    }
 
    // Exibição local no Serial
    Serial.println("----------------------------");
    Serial.print("Temperatura: ");
    Serial.print(temperatura, 1);
    Serial.println(" C");
    Serial.print("Umidade: ");
    Serial.print(umidade, 1);
    Serial.println(" %");
 
    // Lógica dos LEDs locais
    if (temperatura >= TEMP_MIN && temperatura <= TEMP_MAX) {
      digitalWrite(LED_VERDE, HIGH);
      digitalWrite(LED_VERMELHO, LOW);
      Serial.println("Status: NORMAL");
    } else {
      digitalWrite(LED_VERDE, LOW);
      digitalWrite(LED_VERMELHO, HIGH);
      Serial.println("Status: ALERTA");
    }
 
    // Montando a string de dados para o ThingSpeak
    // O formato aceito para múltiplos campos é: field1=<val1>&field2=<val2>
    String payload = "field1=" + String(temperatura, 1) + "&field2=" + String(umidade, 1);
   
    // Monta o tópico MQTT exigido pelo ThingSpeak
    String topico = "channels/" + String(channelID) + "/publish";
 
    // Publica no ThingSpeak
    if (client.publish(topico.c_str(), payload.c_str())) {
      Serial.println("Dados enviados ao ThingSpeak com sucesso!");
    } else {
      Serial.println("Falha ao enviar dados via MQTT.");
    }
   
    Serial.println("----------------------------\n");
  }
}
