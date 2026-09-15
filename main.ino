/*
 * ============================================================================
 *  FrioVale — Monitor IoT de Câmara Fria (ESP32 + DHT22 + LCD + LEDs)
 * ============================================================================
 *
 *  HARDWARE (conforme diagrama de montagem)
 *    ESP32 DevKit .......... placa principal
 *    DHT22 ................. GPIO 4   (temperatura e umidade)
 *    Reed switch ........... GPIO 5   (sensor de porta, INPUT_PULLUP)
 *    LED verde ............. GPIO 12  (temperatura dentro da faixa)
 *    LED vermelho .......... GPIO 13  (alerta)
 *    LCD 16x2 I2C .......... GPIO 21 SDA / GPIO 22 SCL (addr 0x27)
 *    Módulo 4G/LTE ......... UART2: TX GPIO 17 / RX GPIO 16, PWRKEY GPIO 27
 *    Fonte 5V .............. alimentação
 *
 *  PREMISSA MANTIDA DO PROJETO
 *    "Normal" não é uma faixa fixa. 8,7 °C é normal para goiaba Paluma de vez,
 *    é limítrofe para melão Honeydew e é dano térmico acumulado para Cantaloupe.
 *    O LED verde só acende em relação a um PerfilFruta selecionado; sem perfil
 *    o display avisa e o LED vermelho pisca.
 *
 *  LIMITAÇÃO DO DHT22 — LEIA ANTES DE USAR EM PRODUÇÃO
 *    O DHT22 tem exatidão de ±0,5 °C e ±2–5 % UR, e perde confiabilidade
 *    justamente acima de 90 % UR, que é a faixa exigida por goiaba e por
 *    melões do grupo reticulatus. Ele serve bem para o eixo de temperatura
 *    e para detectar tendência de umidade. Se a UR for critério de decisão
 *    comercial, troque por SHT31-D ou SHT45 (mesmo I2C do LCD, sem novo pino).
 *    Por isso o firmware trata a UR como INFORMATIVA e só gera alerta forte
 *    de umidade quando o desvio é grande o bastante para superar o erro do
 *    sensor (ver MARGEM_ERRO_UR).
 *
 *  Bibliotecas: DHT sensor library (Adafruit), LiquidCrystal_I2C,
 *               TinyGSM, PubSubClient, ArduinoJson, Preferences
 * ============================================================================
 */

#define TINY_GSM_MODEM_SIM7600     // ajuste p/ SIM800L, SIM7000, etc.

#include <Wire.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_task_wdt.h>

// ----------------------------------------------------------------------------
// 1. PINAGEM
// ----------------------------------------------------------------------------
#define PIN_DHT        4
#define PIN_REED       5
#define PIN_LED_VERDE  12
#define PIN_LED_VERM   13
#define PIN_MODEM_TX   17
#define PIN_MODEM_RX   16
#define PIN_MODEM_PWR  27

#define DHTTYPE        DHT22
#define LCD_ADDR       0x27

// ----------------------------------------------------------------------------
// 2. TEMPOS E TOLERÂNCIAS
// ----------------------------------------------------------------------------
const uint32_t INTERVALO_LEITURA_MS   = 2500UL;   // DHT22 exige >= 2 s
const uint32_t INTERVALO_TELA_MS      = 4000UL;   // rotação de telas do LCD
const uint32_t INTERVALO_ENVIO_MS     = 60000UL;
const uint32_t PORTA_ABERTA_LIMITE_MS = 120000UL; // 2 min de porta aberta
const uint32_t REED_DEBOUNCE_MS       = 800UL;

const uint8_t  LEITURAS_P_CONFIRMAR = 4;   // nº de leituras seguidas fora da
                                           // faixa antes de alarmar (evita
                                           // falso positivo na abertura)
const float    MARGEM_ERRO_UR       = 5.0; // % — erro do DHT22 em UR alta
const uint32_t CHILLING_ALARME_MIN  = 45;  // min acumulados abaixo do limiar
const uint16_t BUFFER_OFFLINE       = 240; // ~4 h de registros por minuto

// ----------------------------------------------------------------------------
// 3. PERFIS — a unidade de configuração
// ----------------------------------------------------------------------------
enum Estagio : uint8_t { EST_VERDE_MADURO, EST_DE_VEZ, EST_MADURO };
enum Transporte : uint8_t { TR_PRECOOL_LONGO, TR_PRECOOL_CURTO,
                            TR_SEM_PRECOOL, TR_CAMARA_ESTATICA };

struct PerfilFruta {
  const char* id;
  const char* especie;
  const char* grupo;
  const char* cultivar;
  const char* rotuloLcd;      // <= 16 caracteres, para o display
  Estagio     estagio;
  Transporte  transporte;
  float       tMin, tMax;     // faixa considerada "Normal"
  float       tChilling;      // abaixo disso: dano por frio acumulativo
  float       rhMin, rhMax;
  uint16_t    vidaUtilDias;
  const char* nota;
};

const PerfilFruta PERFIS[] = {
  { "melao_honeydew_maduro_maritimo", "Cucumis melo", "var. inodorus",
    "Honeydew", "Melao Honeydew", EST_MADURO, TR_PRECOOL_LONGO,
    7.0, 10.0, 6.0, 85.0, 90.0, 21,
    "7-10 C vale AQUI: inodorus maduro. Nao extrapolar." },

  { "melao_pele_sapo_devez_rodoviario", "Cucumis melo", "var. inodorus",
    "Pele de Sapo", "Melao P. Sapo", EST_DE_VEZ, TR_PRECOOL_CURTO,
    8.0, 10.0, 7.0, 85.0, 90.0, 14,
    "De vez tolera menos frio que fruto maduro." },

  { "melao_cantaloupe_devez_maritimo", "Cucumis melo", "var. reticulatus",
    "Cantaloupe", "Melao Cantalou", EST_DE_VEZ, TR_PRECOOL_LONGO,
    2.0, 5.0, 1.0, 90.0, 95.0, 15,
    "Climaterico: 7-10 C aqui significa perda rapida." },

  { "melao_charentais_maduro_curto", "Cucumis melo", "var. cantalupensis",
    "Charentais", "Melao Charent.", EST_MADURO, TR_PRECOOL_CURTO,
    2.0, 4.0, 1.0, 90.0, 95.0, 8,
    "Vida util curta e alta producao de etileno." },

  { "goiaba_paluma_verde_maritimo", "Psidium guajava", "vermelha",
    "Paluma", "Goiaba Paluma", EST_VERDE_MADURO, TR_PRECOOL_LONGO,
    9.0, 10.0, 8.0, 90.0, 95.0, 21,
    "Verde-maduro e MAIS sensivel a frio que maduro." },

  { "goiaba_paluma_devez_rodoviario", "Psidium guajava", "vermelha",
    "Paluma", "Goiaba Paluma", EST_DE_VEZ, TR_PRECOOL_CURTO,
    8.0, 10.0, 7.0, 90.0, 95.0, 12,
    "Casca escurecida = sinal tardio de chilling." },

  { "goiaba_seculo21_maduro_camara", "Psidium guajava", "branca",
    "Seculo XXI", "Goiaba Sec.XXI", EST_MADURO, TR_CAMARA_ESTATICA,
    5.0, 8.0, 4.0, 90.0, 95.0, 10,
    "Fruto maduro tolera piso mais baixo." },
};
const uint8_t N_PERFIS = sizeof(PERFIS) / sizeof(PERFIS[0]);

// ----------------------------------------------------------------------------
// 4. ESTADO
// ----------------------------------------------------------------------------
enum Status : uint8_t {
  ST_SEM_PERFIL, ST_FALHA_SENSOR, ST_CHILLING,
  ST_ALTA, ST_BAIXA, ST_NORMAL
};

struct Registro { uint32_t minuto; int16_t t10; uint8_t rh; uint8_t status; };

struct Estado {
  int8_t   perfilIdx = -1;
  float    t = NAN, rh = NAN;
  Status   status = ST_SEM_PERFIL;
  bool     portaAberta = false;
  bool     alertaPorta = false;
  uint32_t portaAbertaDesde = 0;
  uint8_t  contForaFaixa = 0;
  uint32_t minutosChilling = 0;
  uint32_t minutosForaFaixa = 0;
  uint32_t ultimoTickMinuto = 0;
  uint32_t ultimaLeituraOk = 0;
  uint32_t inicioViagem = 0;
  bool     redeOk = false;
  uint16_t bufCount = 0, bufHead = 0;
  Registro buffer[BUFFER_OFFLINE];
} est;

DHT dht(PIN_DHT, DHTTYPE);
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);
HardwareSerial modemSerial(2);
TinyGsm modem(modemSerial);
TinyGsmClient gsmClient(modem);
PubSubClient mqtt(gsmClient);
Preferences prefs;

const char* APN         = "sua.apn";
const char* APN_USER    = "";
const char* APN_PASS    = "";
const char* MQTT_HOST   = "broker.friovale.com.br";
const uint16_t MQTT_PORT = 1883;
const char* VEICULO     = "ABC-1234";
const char* TOP_TELEM   = "friovale/ABC-1234/telemetria";
const char* TOP_ALERTA  = "friovale/ABC-1234/alerta";
const char* TOP_CMD     = "friovale/ABC-1234/comando";

// ----------------------------------------------------------------------------
// 5. LCD — escrita sempre com 16 colunas preenchidas
// ----------------------------------------------------------------------------
void lcdLinha(uint8_t linha, const char* txt) {
  char buf[17];
  snprintf(buf, sizeof(buf), "%-16.16s", txt);
  lcd.setCursor(0, linha);
  lcd.print(buf);
}

void lcdDuasLinhas(const char* l0, const char* l1) {
  lcdLinha(0, l0);
  lcdLinha(1, l1);
}

// ----------------------------------------------------------------------------
// 6. PERFIL
// ----------------------------------------------------------------------------
int8_t buscarPerfil(const char* id) {
  for (uint8_t i = 0; i < N_PERFIS; i++)
    if (strcmp(PERFIS[i].id, id) == 0) return (int8_t)i;
  return -1;
}

bool selecionarPerfil(const char* id) {
  int8_t idx = buscarPerfil(id);
  if (idx < 0) { Serial.printf("[ERRO] Perfil '%s' inexistente.\n", id); return false; }

  est.perfilIdx = idx;
  est.contForaFaixa = 0;
  est.minutosChilling = 0;
  est.minutosForaFaixa = 0;
  est.inicioViagem = millis();
  prefs.putString("perfil", id);

  const PerfilFruta& p = PERFIS[idx];
  char l1[17];
  snprintf(l1, sizeof(l1), "Faixa %.0f-%.0f C", p.tMin, p.tMax);
  lcdDuasLinhas(p.rotuloLcd, l1);

  Serial.printf("\n=== PERFIL ===\n %s %s '%s'\n estagio %d | transporte %d\n"
                " faixa %.1f-%.1f C | chilling < %.1f C | UR %.0f-%.0f%%\n"
                " nota: %s\n\n",
                p.especie, p.grupo, p.cultivar, (int)p.estagio,
                (int)p.transporte, p.tMin, p.tMax, p.tChilling,
                p.rhMin, p.rhMax, p.nota);
  delay(2500);
  return true;
}

// ----------------------------------------------------------------------------
// 7. LEITURA DO DHT22 (mediana de 3 para filtrar ruído)
// ----------------------------------------------------------------------------
float mediana3(float a, float b, float c) {
  if ((a >= b && a <= c) || (a >= c && a <= b)) return a;
  if ((b >= a && b <= c) || (b >= c && b <= a)) return b;
  return c;
}

bool lerDHT() {
  static float histT[3] = {NAN, NAN, NAN};
  static float histH[3] = {NAN, NAN, NAN};
  static uint8_t i = 0;

  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (isnan(t) || isnan(h) || t < -40 || t > 80) return false;

  histT[i] = t; histH[i] = h; i = (i + 1) % 3;

  if (isnan(histT[0]) || isnan(histT[1]) || isnan(histT[2])) {
    est.t = t; est.rh = h;
  } else {
    est.t  = mediana3(histT[0], histT[1], histT[2]);
    est.rh = mediana3(histH[0], histH[1], histH[2]);
  }
  est.ultimaLeituraOk = millis();
  return true;
}

// ----------------------------------------------------------------------------
// 8. PORTA (reed switch com debounce)
// ----------------------------------------------------------------------------
void lerPorta() {
  static bool estavel = false;
  static bool ultimo = false;
  static uint32_t mudouEm = 0;

  bool aberta = (digitalRead(PIN_REED) == HIGH);  // ímã afastado = aberta
  if (aberta != ultimo) { ultimo = aberta; mudouEm = millis(); }

  if (millis() - mudouEm >= REED_DEBOUNCE_MS && estavel != ultimo) {
    estavel = ultimo;
    est.portaAberta = estavel;
    est.portaAbertaDesde = estavel ? millis() : 0;
    if (!estavel) est.alertaPorta = false;
  }

  if (est.portaAberta && !est.alertaPorta &&
      millis() - est.portaAbertaDesde >= PORTA_ABERTA_LIMITE_MS) {
    est.alertaPorta = true;
    publicarAlerta("porta_aberta", "Porta aberta por tempo excessivo");
  }
}

// ----------------------------------------------------------------------------
// 9. CLASSIFICAÇÃO — aqui o perfil vira decisão
// ----------------------------------------------------------------------------
void avaliar() {
  if (est.perfilIdx < 0) { est.status = ST_SEM_PERFIL; return; }
  if (millis() - est.ultimaLeituraOk > 30000UL) { est.status = ST_FALHA_SENSOR; return; }

  const PerfilFruta& p = PERFIS[est.perfilIdx];
  Status bruto;

  if (est.t < p.tChilling)      bruto = ST_CHILLING;
  else if (est.t > p.tMax)      bruto = ST_ALTA;
  else if (est.t < p.tMin)      bruto = ST_BAIXA;
  else                          bruto = ST_NORMAL;

  // Exige confirmação em leituras consecutivas: uma abertura de porta
  // não deve disparar alerta de carga comprometida.
  if (bruto == ST_NORMAL) {
    est.contForaFaixa = 0;
    est.status = ST_NORMAL;
  } else {
    if (est.contForaFaixa < 255) est.contForaFaixa++;
    est.status = (est.contForaFaixa >= LEITURAS_P_CONFIRMAR) ? bruto : est.status;
    if (est.status == ST_SEM_PERFIL) est.status = ST_NORMAL;
  }

  // Contadores acumulados por minuto
  if (millis() - est.ultimoTickMinuto >= 60000UL) {
    est.ultimoTickMinuto = millis();
    if (est.status == ST_CHILLING) {
      est.minutosChilling++;
      if (est.minutosChilling == CHILLING_ALARME_MIN) {
        char m[96];
        snprintf(m, sizeof(m), "Dano por frio: %s '%s' a %.1f C por %u min",
                 p.especie, p.cultivar, est.t, est.minutosChilling);
        publicarAlerta("chilling", m);
      }
    } else if (est.status == ST_ALTA || est.status == ST_BAIXA) {
      est.minutosForaFaixa++;
    } else if (est.minutosChilling > 0) {
      est.minutosChilling--;
    }
    registrarBuffer();
  }
}

// ----------------------------------------------------------------------------
// 10. LEDs — verde = dentro da faixa do perfil; vermelho = tudo mais
// ----------------------------------------------------------------------------
void atualizarLeds() {
  bool verde = false, verm = false;
  bool pisca = (millis() / 400) % 2;        // 1,25 Hz
  bool piscaLento = (millis() / 900) % 2;

  switch (est.status) {
    case ST_NORMAL:       verde = !est.alertaPorta; verm = est.alertaPorta && pisca; break;
    case ST_ALTA:
    case ST_BAIXA:        verm = true; break;
    case ST_CHILLING:     verm = pisca; break;              // pisca rápido
    case ST_FALHA_SENSOR: verm = piscaLento; verde = piscaLento; break;  // âmbar
    case ST_SEM_PERFIL:   verm = piscaLento; break;
  }
  digitalWrite(PIN_LED_VERDE, verde);
  digitalWrite(PIN_LED_VERM, verm);
}

// ----------------------------------------------------------------------------
// 11. TELAS DO LCD (rotação)
// ----------------------------------------------------------------------------
void atualizarLcd() {
  static uint8_t tela = 0;
  static uint32_t ultimo = 0;
  char l0[17], l1[17];

  // Condições críticas assumem o display inteiro, sem rotação.
  if (est.status == ST_SEM_PERFIL) {
    lcdDuasLinhas("Sem perfil!", "Defina a carga");
    return;
  }
  if (est.status == ST_FALHA_SENSOR) {
    lcdDuasLinhas("Falha sensor", "Verificar DHT22");
    return;
  }
  if (est.alertaPorta) {
    snprintf(l1, sizeof(l1), "Temp: %.1f C", est.t);
    lcdDuasLinhas("Porta Aberta!", l1);
    return;
  }

  if (millis() - ultimo < INTERVALO_TELA_MS) return;
  ultimo = millis();
  const PerfilFruta& p = PERFIS[est.perfilIdx];

  switch (tela) {
    case 0:
      snprintf(l0, sizeof(l0), "Temp: %.1f C", est.t);
      switch (est.status) {
        case ST_ALTA:     strcpy(l1, "Temp. Alta!");   break;
        case ST_BAIXA:    strcpy(l1, "Temp. Baixa!");  break;
        case ST_CHILLING: strcpy(l1, "RISCO DE FRIO!");break;
        default:          strcpy(l1, "Status: Normal");break;
      }
      break;
    case 1:
      snprintf(l0, sizeof(l0), "Umidade: %.0f%%", est.rh);
      if (est.rh < p.rhMin - MARGEM_ERRO_UR)      strcpy(l1, "UR baixa");
      else if (est.rh > p.rhMax + MARGEM_ERRO_UR) strcpy(l1, "UR alta");
      else                                        strcpy(l1, "UR ok");
      break;
    case 2:
      snprintf(l0, sizeof(l0), "%s", p.rotuloLcd);
      snprintf(l1, sizeof(l1), "Faixa %.0f-%.0f C", p.tMin, p.tMax);
      break;
    default:
      snprintf(l0, sizeof(l0), "Fora faixa:%lum", (unsigned long)est.minutosForaFaixa);
      snprintf(l1, sizeof(l1), "Rede: %s", est.redeOk ? "Online" : "Offline");
      break;
  }
  lcdDuasLinhas(l0, l1);
  tela = (tela + 1) % 4;
}

// ----------------------------------------------------------------------------
// 12. BUFFER OFFLINE + ENVIO
// ----------------------------------------------------------------------------
void registrarBuffer() {
  Registro& r = est.buffer[est.bufHead];
  r.minuto = (millis() - est.inicioViagem) / 60000UL;
  r.t10 = (int16_t)(est.t * 10);
  r.rh  = (uint8_t)est.rh;
  r.status = (uint8_t)est.status;
  est.bufHead = (est.bufHead + 1) % BUFFER_OFFLINE;
  if (est.bufCount < BUFFER_OFFLINE) est.bufCount++;
}

const char* nomeStatus(Status s) {
  switch (s) {
    case ST_NORMAL: return "normal";
    case ST_ALTA: return "alta";
    case ST_BAIXA: return "baixa";
    case ST_CHILLING: return "chilling";
    case ST_FALHA_SENSOR: return "falha_sensor";
    default: return "sem_perfil";
  }
}

void publicarAlerta(const char* tipo, const char* msg) {
  Serial.printf("[ALERTA:%s] %s\n", tipo, msg);
  if (!mqtt.connected()) return;
  StaticJsonDocument<256> d;
  d["veiculo"] = VEICULO;
  d["tipo"] = tipo;
  d["mensagem"] = msg;
  d["t"] = round(est.t * 10) / 10.0;
  if (est.perfilIdx >= 0) d["perfil"] = PERFIS[est.perfilIdx].id;
  char buf[256];
  mqtt.publish(TOP_ALERTA, buf, serializeJson(d, buf));
}

void publicarTelemetria() {
  if (!mqtt.connected()) return;
  StaticJsonDocument<448> d;
  d["veiculo"] = VEICULO;
  d["t"] = round(est.t * 10) / 10.0;
  d["ur"] = round(est.rh);
  d["status"] = nomeStatus(est.status);
  d["porta"] = est.portaAberta ? "aberta" : "fechada";
  d["min_fora_faixa"] = est.minutosForaFaixa;
  d["min_chilling"] = est.minutosChilling;
  d["horas_viagem"] = (millis() - est.inicioViagem) / 3600000UL;

  if (est.perfilIdx >= 0) {
    const PerfilFruta& p = PERFIS[est.perfilIdx];
    JsonObject perf = d.createNestedObject("perfil");
    perf["id"] = p.id;
    perf["especie"] = p.especie;
    perf["grupo"] = p.grupo;
    perf["cultivar"] = p.cultivar;
    perf["estagio"] = (int)p.estagio;
    perf["transporte"] = (int)p.transporte;
    perf["t_min"] = p.tMin;
    perf["t_max"] = p.tMax;
    perf["t_chilling"] = p.tChilling;
  }
  char buf[448];
  if (mqtt.publish(TOP_TELEM, buf, serializeJson(d, buf))) drenarBuffer();
}

// Envia o histórico acumulado enquanto esteve sem sinal (comum na estrada).
void drenarBuffer() {
  while (est.bufCount > 0 && mqtt.connected()) {
    uint16_t idx = (est.bufHead + BUFFER_OFFLINE - est.bufCount) % BUFFER_OFFLINE;
    Registro& r = est.buffer[idx];
    StaticJsonDocument<160> d;
    d["veiculo"] = VEICULO;
    d["min"] = r.minuto;
    d["t"] = r.t10 / 10.0;
    d["ur"] = r.rh;
    d["status"] = nomeStatus((Status)r.status);
    d["hist"] = true;
    char buf[160];
    if (!mqtt.publish(TOP_TELEM, buf, serializeJson(d, buf))) break;
    est.bufCount--;
  }
}

// ----------------------------------------------------------------------------
// 13. REDE 4G
// ----------------------------------------------------------------------------
void ligarModem() {
  pinMode(PIN_MODEM_PWR, OUTPUT);
  digitalWrite(PIN_MODEM_PWR, HIGH); delay(1200);
  digitalWrite(PIN_MODEM_PWR, LOW);
  modemSerial.begin(115200, SERIAL_8N1, PIN_MODEM_RX, PIN_MODEM_TX);
  delay(3000);
  modem.restart();
}

void manterRede() {
  static uint32_t ultimaTentativa = 0;
  if (mqtt.connected()) { est.redeOk = true; mqtt.loop(); return; }
  est.redeOk = false;
  if (millis() - ultimaTentativa < 20000UL) return;
  ultimaTentativa = millis();

  if (!modem.isNetworkConnected()) { modem.waitForNetwork(10000); return; }
  if (!modem.isGprsConnected() && !modem.gprsConnect(APN, APN_USER, APN_PASS)) return;

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  if (mqtt.connect(VEICULO)) { mqtt.subscribe(TOP_CMD); est.redeOk = true; }
}

void onMqtt(char* topico, byte* payload, unsigned int len) {
  StaticJsonDocument<192> d;
  if (deserializeJson(d, payload, len)) return;
  if (d.containsKey("perfil")) selecionarPerfil(d["perfil"]);
}

// ----------------------------------------------------------------------------
// 14. SETUP / LOOP
// ----------------------------------------------------------------------------
void listarPerfis() {
  Serial.println("\n--- PERFIS ---");
  for (uint8_t i = 0; i < N_PERFIS; i++) {
    const PerfilFruta& p = PERFIS[i];
    Serial.printf("%-34s | %-16s %-22s | %.1f-%.1f C | chill<%.1f\n",
                  p.id, p.especie, p.grupo, p.tMin, p.tMax, p.tChilling);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED_VERDE, OUTPUT);
  pinMode(PIN_LED_VERM, OUTPUT);
  pinMode(PIN_REED, INPUT_PULLUP);

  Wire.begin(21, 22);
  lcd.init(); lcd.backlight();
  lcdDuasLinhas("FrioVale", "Iniciando...");

  dht.begin();
  prefs.begin("friovale", false);
  mqtt.setCallback(onMqtt);
  ligarModem();

  esp_task_wdt_init(60, true);
  esp_task_wdt_add(NULL);

  listarPerfis();
  String salvo = prefs.getString("perfil", "");
  if (!salvo.length() || !selecionarPerfil(salvo.c_str())) {
    lcdDuasLinhas("Sem perfil!", "Defina a carga");
    Serial.println("\nNenhum perfil ativo. Sem especie, cultivar, estagio e");
    Serial.println("condicao de transporte nao ha faixa de referencia, entao");
    Serial.println("o LED verde nao pode significar nada. Use: perfil <id>");
  }
  est.ultimoTickMinuto = millis();
  est.inicioViagem = millis();
}

void loop() {
  esp_task_wdt_reset();
  manterRede();

  if (Serial.available()) {
    String l = Serial.readStringUntil('\n'); l.trim();
    if (l == "listar") listarPerfis();
    else if (l.startsWith("perfil ")) selecionarPerfil(l.substring(7).c_str());
    else if (l == "status")
      Serial.printf("T=%.1f UR=%.0f status=%s porta=%s\n", est.t, est.rh,
                    nomeStatus(est.status), est.portaAberta ? "aberta" : "fechada");
  }

  static uint32_t tLeitura = 0, tEnvio = 0;
  uint32_t agora = millis();

  if (agora - tLeitura >= INTERVALO_LEITURA_MS) {
    tLeitura = agora;
    lerDHT();
    lerPorta();
    avaliar();
  }

  atualizarLeds();     // fora do bloco temporizado: o pisca precisa ser fluido
  atualizarLcd();

  if (agora - tEnvio >= INTERVALO_ENVIO_MS) {
    tEnvio = agora;
    publicarTelemetria();
  }
}

