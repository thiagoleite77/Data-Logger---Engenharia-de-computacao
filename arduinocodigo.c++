

============================================================
DATA LOGGER AMBIENTAL - v1.0
MCU: ATmega328P (Arduino Uno R3)
Monitora temperatura, umidade relativa e luminosidade,
registra na EEPROM interna com timestamp (RTC) e gera
alertas visuais (LEDs) e sonoros (buzzer).
============================================================
Bibliotecas: LiquidCrystal I2C, RTClib, DHT sensor library

PINAGEM
D2 DHT11 (dados) A0 LDR (divisor com 10k)
D4 LED verde (normal) A4 SDA (LCD + RTC)
D5 LED amarelo (gravacao/erro sensor) A5 SCL (LCD + RTC)
D6 LED vermelho (alerta)
D8 Buzzer
D9 Tecla UP D10 Tecla DOWN D11 Tecla OK (INPUT_PULLUP)
/
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <EEPROM.h>
#include <DHT.h>
// ------------------------- PINOS ---------------------------
#define DHT_PIN 2
// O Wokwi so tem a peca DHT22. Na simulacao use 1; no hardware real (DHT11) use 0.

#define DHT_TYPE DHT11
#define LDR_PIN A0
#define LED_VERDE 4
#define LED_AMARELO 5
#define LED_VERMELHO 6
#define BUZZER_PIN 8
#define BTN_UP_PIN 9
#define BTN_DOWN_PIN 10
#define BTN_OK_PIN 11
#define LDR_INVERTIDO 0 // 1 se mais luz => leitura analogica menor
#define SERIAL_DEBUG 1 // 1 = imprime leituras no monitor serial
// ------------------------- GATILHOS ------------------------
// Faixa NORMAL (intervalos abertos, conforme especificacao)
const float T_MIN = 15.0, T_MAX = 30.0; // Temperatura (C)
const float U_MIN = 10.0, U_MAX = 50.0; // Umidade (%)
const int L_MIN = 70, L_MAX = 100; // Luminosidade (%)
// Bits da mascara de alerta
#define ALERTA_TEMP 1
#define ALERTA_UMID 2
#define ALERTA_LUZ 4
// ------------------------- EEPROM --------------------------
// Layout (ATmega328P = 1024 bytes):
// 0..15 -> configuracoes do usuario (struct Config)
// 16..975 -> 120 registros de 8 bytes (buffer circular)
struct Config {
uint8_t magic; // 0xA5 = configuracao valida
int8_t utc; // fuso horario (-10 a -2)
uint8_t fahrenheit; // 0 = Celsius, 1 = Fahrenheit
uint8_t dataUS; // 0 = DD/MM/AAAA, 1 = MM/DD/AAAA
uint8_t logIdx; // indice do intervalo de log
uint8_t mudo; // 1 = buzzer silenciado
};
struct Registro { // 8 bytes
uint32_t ts; // timestamp UTC (segundos, epoch Unix)
int16_t temp10; // temperatura em decimos de grau C
uint8_t umid; // umidade %
uint8_t luz; // luminosidade %
};
#define CFG_ADDR 0
#define REC_BASE 16
#define MAX_REC 120
#define CFG_MAGIC 0xA5
#define UTC_MIN -10
#define UTC_MAX -2
Config cfg;
uint8_t proxIdx = 0; // proxima posicao de escrita
uint8_t totalReg = 0; // registros validos
const uint16_t INTERVALOS_LOG[4] = {10, 60, 300, 900}; // segundos
const char* const ROTULO_LOG[4] = {"10 s", "1 min", "5 min", "15 min"};
// ------------------------- OBJETOS -------------------------
LiquidCrystal_I2C lcd(0x27, 16, 2); // endereco 0x27 (ou 0x3F)
RTC_DS1307 rtc;
DHT dht(DHT_PIN, DHT_TYPE);
uint8_t grau[8] = {0x06, 0x09, 0x09, 0x06, 0x00, 0x00, 0x00, 0x00}; // simbolo de grau
// ------------------------- ESTADO --------------------------
float temperatura = 0, umidade = 0;
uint8_t luzPct = 0;
bool dhtOk = false, dhtJaLeu = false, dhtTentou = false;
uint8_t alertaMask = 0, alertaAnt = 0;
unsigned long tAmostra = 0, tLog = 0, tTela = 0, tBuzzer = 0, tPulsoLog = 0;
bool pulsoAtivo = false;
const unsigned long INTERVALO_AMOSTRA = 2000; // DHT11: minimo 1 s
enum Estado { E_TELA, E_MENU, E_LOG, E_APAGAR, E_RELOGIO };
Estado estado = E_TELA;
uint8_t tela = 0; // 0 = medidas, 1 = data/hora, 2 = status
#define NUM_TELAS 3
uint8_t itemMenu = 0;
uint8_t logPos = 0; // 0 = registro mais recente
bool redraw = true;
// Editor de relogio
uint8_t campoRel = 0;
int edV[5]; // hora, minuto, dia, mes, ano
const int edMin[5] = {0, 0, 1, 1, 2024};
const int edMax[5] = {23, 59, 31, 12, 2099};
const char* const edNome[5] = {"Hora", "Minuto", "Dia", "Mes", "Ano"};
// Menu
#define NUM_ITENS 10
const char* const TITULOS[NUM_ITENS] = {
"Ver log", "Exportar serial", "Apagar log", "Fuso horario", "Unidade temp.",
"Formato data", "Intervalo log", "Ajustar relogio", "Buzzer", "Voltar"
};
#define B_UP 0
#define B_DOWN 1
#define B_OK 2
// ============================================================
// FUNCOES AUXILIARES
// ============================================================
// Escreve uma linha do LCD (16 colunas) so se o texto mudou (evita flicker)
char cacheLcd[2][17];
void lcdLinha(uint8_t row, const char* txt) {
char buf[17];
uint8_t i = 0;
for (; i < 16 && txt[i]; i++) buf[i] = txt[i];
for (; i < 16; i++) buf[i] = ' ';
buf[16] = 0;
if (memcmp(buf, cacheLcd[row], 17) != 0) {
memcpy(cacheLcd[row], buf, 17);
lcd.setCursor(0, row);
lcd.print(buf);
}
}
// Le botoes com debounce; retorna B_UP/B_DOWN/B_OK ou -1
int8_t lerBotao() {
static const uint8_t pinos[3] = {BTN_UP_PIN, BTN_DOWN_PIN, BTN_OK_PIN};
static bool ant[3] = {HIGH, HIGH, HIGH};
static unsigned long t[3] = {0, 0, 0};
for (uint8_t i = 0; i < 3; i++) {
bool s = digitalRead(pinos[i]);
if (s != ant[i] && millis() - t[i] > 40) {
t[i] = millis();
ant[i] = s;
if (s == LOW) return i; // borda de descida = tecla pressionada
}
}
return -1;
}
void beepTecla() { if (!cfg.mudo) tone(BUZZER_PIN, 2500, 20); }
// Hora local = RTC (UTC) + fuso configurado
DateTime agoraLocal() {
return DateTime(rtc.now().unixtime() + (int32_t)cfg.utc * 3600L);
}
float paraUnidade(float c) { return cfg.fahrenheit ? c * 9.0 / 5.0 + 32.0 : c; }
// Formata temperatura com 1 casa decimal (sem usar float no snprintf)
void fmtTemp(char* out, float tc) {
int v = (int)lround(paraUnidade(tc) * 10.0);
snprintf(out, 10, "%s%d.%d", v < 0 ? "-" : "", abs(v) / 10, abs(v) % 10);
}
uint8_t diasNoMes(int m, int a) {
static const uint8_t d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
if (m == 2 && ((a % 4 == 0 && a % 100 != 0) || a % 400 == 0)) return 29;
return d[m - 1];
}
// ============================================================
// CONFIG / EEPROM
// ============================================================
void salvarConfig() { EEPROM.put(CFG_ADDR, cfg); } // put() so grava se mudou
void carregarConfig() {
EEPROM.get(CFG_ADDR, cfg);
if (cfg.magic != CFG_MAGIC || cfg.utc < UTC_MIN || cfg.utc > UTC_MAX || cfg.logIdx > 3) {
cfg.magic = CFG_MAGIC;
cfg.utc = -3; // padrao: Brasilia
cfg.fahrenheit = 0;
cfg.dataUS = 0;
cfg.logIdx = 1; // 1 min
cfg.mudo = 0;
salvarConfig();
}
}
int recAddr(uint8_t i) { return REC_BASE + i * sizeof(Registro); }
// Varre a EEPROM no boot: acha o registro mais novo (maior timestamp)
// e continua a gravacao logo apos ele (buffer circular sem desgaste extra).
void localizarProximoRegistro() {
uint32_t maior = 0;
int idx = -1;
totalReg = 0;
for (uint8_t i = 0; i < MAX_REC; i++) {
Registro r;
EEPROM.get(recAddr(i), r);
if (r.ts != 0xFFFFFFFF) { // 0xFF... = posicao vazia
totalReg++;
if (idx < 0 || r.ts >= maior) { maior = r.ts; idx = i; }
}
}
proxIdx = (idx < 0) ? 0 : (idx + 1) % MAX_REC;
}
void salvarRegistro() {
Registro r;
r.ts = rtc.now().unixtime(); // armazenado em UTC
r.temp10 = (int16_t)lround(temperatura * 10.0);
r.umid = (uint8_t)constrain((int)lround(umidade), 0, 100);
r.luz = luzPct;
EEPROM.put(recAddr(proxIdx), r);
proxIdx = (proxIdx + 1) % MAX_REC;
if (totalReg < MAX_REC) totalReg++;
pulsoAtivo = true; // pisca LED amarelo
tPulsoLog = millis();
}
// k = 0 => mais recente
uint8_t idxDeK(uint8_t k) { return (proxIdx + 2 * MAX_REC - 1 - k) % MAX_REC; }
void apagarLog() {
lcdLinha(0, "Apagando...");
lcdLinha(1, "");
for (int i = 0; i < MAX_REC * (int)sizeof(Registro); i++) EEPROM.update(REC_BASE + i, 0xFF);
proxIdx = 0;
totalReg = 0;
}
void exportarSerial() {
Serial.println(F("timestamp_local,temperatura_C,umidade_%,luminosidade_%"));
for (int j = totalReg - 1; j >= 0; j--) { // do mais antigo ao mais novo
Registro r;
EEPROM.get(recAddr(idxDeK(j)), r);
DateTime d(r.ts + (int32_t)cfg.utc * 3600L);
char b[24];
snprintf(b, sizeof b, "%04d-%02d-%02d %02d:%02d:%02d,", d.year(), d.month(), d.day(),
d.hour(), d.minute(), d.second());
Serial.print(b);
Serial.print(r.temp10 / 10.0, 1);
Serial.print(',');
Serial.print(r.umid);
Serial.print(',');
Serial.println(r.luz);
}
}
// ============================================================
// SENSORES E ALERTAS
// ============================================================
void lerSensores() {
float h = dht.readHumidity();
float t = dht.readTemperature();
dhtTentou = true;
if (isnan(h) || isnan(t)) {
dhtOk = false; // mantem ultimo valor valido
} else {
dhtOk = true;
dhtJaLeu = true;
temperatura = t;
umidade = h;
}
long soma = 0;
for (uint8_t i = 0; i < 8; i++) soma += analogRead(LDR_PIN); // media de 8 leituras
int raw = soma / 8;
int pct = map(raw, 0, 1023, 0, 100);
luzPct = LDR_INVERTIDO ? 100 - pct : pct;
// Avalia gatilhos (faixa normal = intervalo aberto)
alertaMask = 0;
if (dhtJaLeu) {
if (!(temperatura > T_MIN && temperatura < T_MAX)) alertaMask |= ALERTA_TEMP;
if (!(umidade > U_MIN && umidade < U_MAX)) alertaMask |= ALERTA_UMID;
}
if (!(luzPct > L_MIN && luzPct < L_MAX)) alertaMask |= ALERTA_LUZ;
#if SERIAL_DEBUG
Serial.print(F("T=")); Serial.print(temperatura, 1);
Serial.print(F("C U=")); Serial.print(umidade, 0);
Serial.print(F("% L=")); Serial.print(luzPct);
Serial.print(F("% alerta=")); Serial.println(alertaMask);
#endif
}
void atualizarSaidas(unsigned long agora) {
if (pulsoAtivo && agora - tPulsoLog >= 300) pulsoAtivo = false;
digitalWrite(LED_VERDE, alertaMask == 0);
digitalWrite(LED_VERMELHO, alertaMask != 0 && ((agora / 300) % 2 == 0)); // pisca
// Amarelo: pisca ao gravar; fica aceso se o DHT falhar
digitalWrite(LED_AMARELO, pulsoAtivo || (dhtTentou && !dhtOk));
// Buzzer: bipe a cada 2 s enquanto houver alerta (se nao estiver mudo)
if (alertaMask && !cfg.mudo && agora - tBuzzer >= 2000) {
tBuzzer = agora;
tone(BUZZER_PIN, 1000, 200);
}
}
// ============================================================
// INTERFACE
// ============================================================
void desenharTela() {
char b[24], ts[10];
if (estado == E_TELA) {
DateTime loc = agoraLocal();
if (tela == 0) { // ---- medidas
if (dhtJaLeu) {
fmtTemp(ts, temperatura);
snprintf(b, sizeof b, "T:%s%c%c U:%d%%", ts, (char)1, cfg.fahrenheit ? 'F' : 'C',
(int)lround(umidade));
} else {
snprintf(b, sizeof b, "T:--.- U:--%%");
}
if (alertaMask) { // marca '!' na coluna 16
size_t n = strlen(b);
while (n < 15) b[n++] = ' ';
b[15] = '!';
b[16] = 0;
}
lcdLinha(0, b);
snprintf(b, sizeof b, "L:%d%% %02d:%02d:%02d", (int)luzPct, loc.hour(), loc.minute(), loc.second());
lcdLinha(1, b);
} else if (tela == 1) { // ---- data/hora
if (cfg.dataUS) snprintf(b, sizeof b, "DATA %02d/%02d/%04d", loc.month(), loc.day(), loc.year());
else snprintf(b, sizeof b, "DATA %02d/%02d/%04d", loc.day(), loc.month(), loc.year());
lcdLinha(0, b);
snprintf(b, sizeof b, "%02d:%02d:%02d UTC%+d", loc.hour(), loc.minute(), loc.second(), cfg.utc);
lcdLinha(1, b);
} else { // ---- status
lcdLinha(0, alertaMask ? "STATUS: ALERTA" : "STATUS: NORMAL");
if (alertaMask) {
b[0] = 0;
if (alertaMask & ALERTA_TEMP) strcat(b, "TEMP ");
if (alertaMask & ALERTA_UMID) strcat(b, "UMID ");
if (alertaMask & ALERTA_LUZ) strcat(b, "LUZ");
lcdLinha(1, b);
} else {
snprintf(b, sizeof b, "Log: %d/%d", totalReg, MAX_REC);
lcdLinha(1, b);
}
}
return;
}
if (estado == E_MENU) {
snprintf(b, sizeof b, ">%s", TITULOS[itemMenu]);
lcdLinha(0, b);
switch (itemMenu) {
case 3: snprintf(b, sizeof b, "UTC%+d OK=mudar", cfg.utc); break;
case 4: snprintf(b, sizeof b, "%c%c OK=mudar", (char)1, cfg.fahrenheit ? 'F' : 'C'); break;
case 5: snprintf(b, sizeof b, "%s OK=mudar", cfg.dataUS ? "MM/DD" : "DD/MM"); break;
case 6: snprintf(b, sizeof b, "%s OK=mudar", ROTULO_LOG[cfg.logIdx]); break;
case 8: snprintf(b, sizeof b, "%s OK=mudar", cfg.mudo ? "Mudo" : "Ligado"); break;
case 9: snprintf(b, sizeof b, "OK=sair"); break;
default: snprintf(b, sizeof b, "OK=executar"); break;
}
lcdLinha(1, b);
return;
}
if (estado == E_LOG) {
Registro r;
EEPROM.get(recAddr(idxDeK(logPos)), r);
DateTime d(r.ts + (int32_t)cfg.utc * 3600L);
if (cfg.dataUS) snprintf(b, sizeof b, "#%03d %02d/%02d %02d:%02d", logPos + 1, d.month(), d.day(), d.hour(), d.minute());
else snprintf(b, sizeof b, "#%03d %02d/%02d %02d:%02d", logPos + 1, d.day(), d.month(), d.hour(), d.minute());
lcdLinha(0, b);
int tv = (int)lround(paraUnidade(r.temp10 / 10.0));
snprintf(b, sizeof b, "T%d%c U%d%% L%d%%", tv, cfg.fahrenheit ? 'F' : 'C', r.umid, r.luz);
lcdLinha(1, b);
return;
}
if (estado == E_APAGAR) {
lcdLinha(0, "Apagar o log?");
lcdLinha(1, "OK=Sim UP=Nao");
return;
}
if (estado == E_RELOGIO) {
lcdLinha(0, "Ajustar relogio");
snprintf(b, sizeof b, "%s: %d %s", edNome[campoRel], edV[campoRel], campoRel == 4 ? "OK=salvar" : "OK=prox");
lcdLinha(1, b);
}
}
void aviso(const char* l1, const char* l2) {
lcdLinha(0, l1);
lcdLinha(1, l2);
delay(1200);
}
void executarItem() {
switch (itemMenu) {
case 0: // Ver log
if (totalReg == 0) aviso("Log vazio", "");
else { estado = E_LOG; logPos = 0; }
break;
case 1: // Exportar serial
exportarSerial();
aviso("Enviado para", "Serial 9600");
break;
case 2: estado = E_APAGAR; break;
case 3: // Fuso
cfg.utc++;
if (cfg.utc > UTC_MAX) cfg.utc = UTC_MIN;
salvarConfig();
break;
case 4: cfg.fahrenheit ^= 1; salvarConfig(); break;
case 5: cfg.dataUS ^= 1; salvarConfig(); break;
case 6: cfg.logIdx = (cfg.logIdx + 1) % 4; tLog = millis(); salvarConfig(); break;
case 7: { // Ajustar relogio (hora local)
DateTime l = agoraLocal();
edV[0] = l.hour(); edV[1] = l.minute(); edV[2] = l.day();
edV[3] = l.month(); edV[4] = l.year();
campoRel = 0;
estado = E_RELOGIO;
break;
}
case 8: cfg.mudo ^= 1; salvarConfig(); break;
case 9: estado = E_TELA; break;
}
}
void salvarRelogio() {
int dia = min(edV[2], (int)diasNoMes(edV[3], edV[4]));
DateTime loc(edV[4], edV[3], dia, edV[0], edV[1], 0);
rtc.adjust(DateTime(loc.unixtime() - (int32_t)cfg.utc * 3600L)); // RTC guarda UTC
}
void tratarBotoes() {
int8_t b = lerBotao();
if (b < 0) return;
redraw = true;
beepTecla();
switch (estado) {
case E_TELA:
if (b == B_UP) tela = (tela + NUM_TELAS - 1) % NUM_TELAS;
else if (b == B_DOWN) tela = (tela + 1) % NUM_TELAS;
else { estado = E_MENU; itemMenu = 0; }
break;
case E_MENU:
if (b == B_UP) itemMenu = (itemMenu + NUM_ITENS - 1) % NUM_ITENS;
else if (b == B_DOWN) itemMenu = (itemMenu + 1) % NUM_ITENS;
else executarItem();
break;
case E_LOG:
if (b == B_UP) { if (logPos > 0) logPos--; }
else if (b == B_DOWN) { if (logPos + 1 < totalReg) logPos++; }
else estado = E_MENU;
break;
case E_APAGAR:
if (b == B_OK) apagarLog(); // qualquer outra tecla cancela
estado = E_MENU;
break;
case E_RELOGIO:
if (b == B_UP) { if (++edV[campoRel] > edMax[campoRel]) edV[campoRel] = edMin[campoRel]; }
if (b == B_DOWN) { if (--edV[campoRel] < edMin[campoRel]) edV[campoRel] = edMax[campoRel]; }
if (b == B_OK) {
if (++campoRel > 4) { salvarRelogio(); estado = E_MENU; }
}
break;
}
}
// ============================================================
// SETUP / LOOP
// ============================================================
// ============================================================
// ABERTURA "ITAJ" (letras grandes)
// ============================================================
// Cada letra ocupa as 2 linhas do LCD, montada com meios-blocos.
byte blocoSup[8] = {31, 31, 31, 31, 0, 0, 0, 0}; // meio bloco superior
byte blocoInf[8] = {0, 0, 0, 0, 31, 31, 31, 31}; // meio bloco inferior
#define BLK_SUP 0 // caractere custom 0
#define BLK_INF 1 // caractere custom 1
#define BLK_CHEIO 255 // bloco cheio (ja existe na ROM do LCD)
#define BLK_VAZIO 32 // espaco
#define BMO_LARG 13
#define BMO_COL0 1 // centraliza 13 colunas nas 16 do LCD
const uint8_t BMO[2][BMO_LARG] = {
// B . M . O
{ 255, 0, 1, 32, 255, 0, 1, 0, 255, 32, 255, 0, 255 }, // linha de cima
{ 255, 1, 255, 32, 255, 32, 32, 32, 255, 32, 255, 1, 255 } // linha de baixo
};
// Caracteres da carinha feliz (substituem os slots 0..3 depois da dissolucao)
byte faceOlho[8] = {0x00, 0x0E, 0x1F, 0x1F, 0x1F, 0x0E, 0x00, 0x00};
byte faceSorrE[8] = {0x00, 0x00, 0x10, 0x08, 0x07, 0x00, 0x00, 0x00}; // canto esquerdo
byte faceSorrM[8] = {0x00, 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}; // meio
byte faceSorrD[8] = {0x00, 0x00, 0x01, 0x02, 0x1C, 0x00, 0x00, 0x00}; // canto direito
void carinhaFeliz() {
lcd.createChar(0, faceOlho);
lcd.createChar(1, faceSorrE);
lcd.createChar(2, faceSorrM);
lcd.createChar(3, faceSorrD);
lcd.clear();
// olhos na linha de cima
lcd.setCursor(5, 0); lcd.write((uint8_t)0);
lcd.setCursor(10, 0); lcd.write((uint8_t)0);
// sorriso na linha de baixo
lcd.setCursor(5, 1); lcd.write((uint8_t)1);
for (uint8_t c = 6; c <= 9; c++) { lcd.setCursor(c, 1); lcd.write((uint8_t)2); }
lcd.setCursor(10, 1); lcd.write((uint8_t)3);
tone(BUZZER_PIN, 2000, 80); // bip alegre
delay(1200);
// piscadinha
lcd.setCursor(5, 0); lcd.write('-');
lcd.setCursor(10, 0); lcd.write('-');
delay(180);
lcd.setCursor(5, 0); lcd.write((uint8_t)0);
lcd.setCursor(10, 0); lcd.write((uint8_t)0);
delay(1000);
lcd.clear();
}
void aberturaBMO() {
lcd.createChar(BLK_SUP, blocoSup);
lcd.createChar(BLK_INF, blocoInf);
lcd.clear();
// Desenha "BMO" e guarda a posicao das celulas preenchidas
uint8_t celulas[32];
uint8_t n = 0;
for (uint8_t r = 0; r < 2; r++) {
for (uint8_t c = 0; c < BMO_LARG; c++) {
uint8_t cod = BMO[r][c];
lcd.setCursor(c + BMO_COL0, r);
lcd.write(cod);
if (cod != BLK_VAZIO) celulas[n++] = r * 16 + c + BMO_COL0;
}
}
// Teste rapido dos LEDs e do buzzer enquanto o nome aparece
digitalWrite(LED_VERDE, HIGH);
digitalWrite(LED_AMARELO, HIGH);
digitalWrite(LED_VERMELHO, HIGH);
tone(BUZZER_PIN, 1500, 150);
delay(2000); // nome parado na tela
digitalWrite(LED_VERDE, LOW);
digitalWrite(LED_AMARELO, LOW);
digitalWrite(LED_VERMELHO, LOW);
// Dissolve: apaga as celulas uma a uma, em ordem aleatoria
randomSeed(analogRead(A1)); // A1 livre = semente aleatoria
for (int8_t i = n - 1; i > 0; i--) { // embaralha (Fisher-Yates)
uint8_t j = random(i + 1);
uint8_t tmp = celulas[i]; celulas[i] = celulas[j]; celulas[j] = tmp;
}
for (uint8_t i = 0; i < n; i++) {
lcd.setCursor(celulas[i] % 16, celulas[i] / 16);
lcd.write((uint8_t)BLK_VAZIO);
delay(140);
}
delay(300);
carinhaFeliz(); // depois do nome, a carinha
}
void setup() {
Serial.begin(9600);
pinMode(LED_VERDE, OUTPUT);
pinMode(LED_AMARELO, OUTPUT);
pinMode(LED_VERMELHO, OUTPUT);
pinMode(BUZZER_PIN, OUTPUT);
pinMode(BTN_UP_PIN, INPUT_PULLUP);
pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
pinMode(BTN_OK_PIN, INPUT_PULLUP);
dht.begin();
lcd.init();
lcd.backlight();
if (!rtc.begin()) {
lcd.setCursor(0, 0);
lcd.print("RTC nao achado!");
while (1);
}
// Ajusta pela hora de compilacao SOMENTE se o RTC estiver parado.
// Depois, acerte o relogio pelo menu (Ajustar relogio).
if (!rtc.isrunning()) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
carregarConfig();
localizarProximoRegistro();
// Abertura: nome ITAJ em letras grandes que se desfaz devagar
aberturaBMO();
lcd.createChar(1, grau); // recarrega o simbolo de grau (slot 1) para as telas
// Dica rapida de uso e depois o projeto entra em funcionamento
lcdLinha(0, "UP/DOWN: telas");
lcdLinha(1, "OK: menu");
delay(1500);
lerSensores();
tAmostra = tLog = millis();
}
void loop() {
unsigned long agora = millis();
// 1) Amostragem periodica
if (agora - tAmostra >= INTERVALO_AMOSTRA) {
tAmostra = agora;
lerSensores();
// Ao entrar em alerta, grava imediatamente um registro do evento
if (alertaMask && !alertaAnt && dhtOk) salvarRegistro();
alertaAnt = alertaMask;
}
// 2) Registro periodico (intervalo configuravel)
if (dhtOk && agora - tLog >= (unsigned long)INTERVALOS_LOG[cfg.logIdx] * 1000UL) {
tLog = agora;
salvarRegistro();
}
// 3) Teclas, LEDs/buzzer e display
tratarBotoes();
atualizarSaidas(agora);
if (redraw || agora - tTela >= 200) {
tTela = agora;
redraw = false;
desenharTela();
}
}