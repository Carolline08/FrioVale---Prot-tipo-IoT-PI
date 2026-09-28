#include <DHT.h>
 
// ==========================
// PINOS
// ==========================
 
#define DHT_PIN 4
#define DHT_TYPE DHT11 // Alterado para DHT11
 
#define LED_VERMELHO 5
#define LED_VERDE 6
 
// ==========================
// LIMITES DE TEMPERATURA
// ==========================
 
#define TEMP_MIN 15.0
#define TEMP_MAX 25.0
 
// ==========================
// VARIÁVEIS DE TEMPO (MILLIS)
// ==========================
 
unsigned long tempoAnterior = 0;
const unsigned long intervaloLeitura = 2000; // Intervalo de 2 segundos
 
// ==========================
// OBJETO DHT11
// ==========================
 
DHT dht(DHT_PIN, DHT_TYPE);
 
// ==========================
// SETUP
// ==========================
 
void setup() {
 
  // Inicia comunicação Serial
  Serial.begin(115200);
 
  // Configura LEDs
  pinMode(LED_VERMELHO, OUTPUT);
  pinMode(LED_VERDE, OUTPUT);
 
  // Inicia o sensor DHT11
  dht.begin();
 
  // Estado inicial dos LEDs
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_VERMELHO, LOW);
 
  // Mensagem inicial
  Serial.println("================================");
  Serial.println("            FRIOVALE");
  Serial.println("   Monitoramento de Camara Fria");
  Serial.println("================================");
  Serial.println("Sistema iniciando...");
  Serial.println();
 
  delay(2000);
}
 
 
// ==========================
// LOOP
// ==========================
 
void loop() {
 
  unsigned long tempoAtual = millis();
 
  // Verifica se já passaram 2 segundos desde a última leitura
  if (tempoAtual - tempoAnterior >= intervaloLeitura) {
    tempoAnterior = tempoAtual;
 
    // ==========================
    // LEITURA DOS SENSORES
    // ==========================
   
    float temperatura = dht.readTemperature();
    float umidade = dht.readHumidity();
   
    // ==========================
    // VERIFICAÇÃO DE ERRO
    // ==========================
   
    if (isnan(temperatura) || isnan(umidade)) {
   
      Serial.println("================================");
      Serial.println("ERRO AO LER DHT11!");
      Serial.println("Verifique a conexao do sensor.");
      Serial.println("================================");
      Serial.println();
   
      // Liga LED vermelho
      digitalWrite(LED_VERDE, LOW);
      digitalWrite(LED_VERMELHO, HIGH);
   
      return;
    }
   
    // ==========================
    // EXIBIÇÃO DOS DADOS
    // ==========================
   
    Serial.println("----------------------------");
   
    Serial.print("Temperatura: ");
    Serial.print(temperatura, 1);
    Serial.println(" C");
   
    Serial.print("Umidade: ");
    Serial.print(umidade, 1);
    Serial.println(" %");
   
    // ==========================
    // TEMPERATURA NORMAL
    // ==========================
   
    if (temperatura >= TEMP_MIN && temperatura <= TEMP_MAX) {
   
      // LED verde ligado
      digitalWrite(LED_VERDE, HIGH);
      digitalWrite(LED_VERMELHO, LOW);
   
      Serial.println("Status: NORMAL");
      Serial.println("Temperatura dentro do limite.");
    }
   
    // ==========================
    // TEMPERATURA ALTA
    // ==========================
   
    else if (temperatura > TEMP_MAX) {
   
      // LED vermelho ligado
      digitalWrite(LED_VERDE, LOW);
      digitalWrite(LED_VERMELHO, HIGH);
   
      Serial.println("Status: ALERTA");
      Serial.println("ALERTA: TEMPERATURA ALTA!");
    }
   
    // ==========================
    // TEMPERATURA BAIXA
    // ==========================
   
    else {
   
      // LED vermelho ligado
      digitalWrite(LED_VERDE, LOW);
      digitalWrite(LED_VERMELHO, HIGH);
   
      Serial.println("Status: ALERTA");
      Serial.println("ALERTA: TEMPERATURA BAIXA!");
    }
   
    Serial.println("----------------------------");
    Serial.println();
  }
}
