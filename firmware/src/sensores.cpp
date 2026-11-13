// SABIÁ - sensores de clima no barramento I2C.
//
// BME280 (temperatura, umidade, pressão) pela biblioteca da Adafruit e
// VEML7700 (luz) direto pelo Wire, que são só dois registradores.
// Sem sensor, a leitura fica vazia (NAN) e o resto do firmware segue normal;
// a presença é conferida de novo a cada minuto, para aceitar um sensor
// ligado depois.

#include <Adafruit_BME280.h>
#include <Arduino.h>
#include <Wire.h>

#include "sistema.h"

namespace sabia {

constexpr uint8_t ENDERECO_BME280 = 0x76;
constexpr uint8_t ENDERECO_VEML7700 = 0x10;
constexpr uint32_t INTERVALO_LEITURA_MS = 10000;
constexpr uint32_t INTERVALO_PROCURA_MS = 60000;

static Adafruit_BME280 bme;
static bool temBme = false, temVeml = false;
static Leitura ultima;
static uint32_t ultimaLeituraMs = 0, ultimaProcuraMs = 0;

static bool iniciarBme() {
  if (!bme.begin(ENDERECO_BME280, &Wire)) return false;
  // Modo de estação: uma medida por vez, sem filtro, para o sensor não se aquecer.
  bme.setSampling(Adafruit_BME280::MODE_FORCED, Adafruit_BME280::SAMPLING_X1, Adafruit_BME280::SAMPLING_X1,
                  Adafruit_BME280::SAMPLING_X1, Adafruit_BME280::FILTER_OFF);
  return true;
}

static bool iniciarVeml() {
  // Registrador 0 (configuração): ganho 1/8, integração de 25 ms, ligado.
  Wire.beginTransmission(ENDERECO_VEML7700);
  Wire.write(0x00);
  Wire.write(0x00);
  Wire.write(0x13);
  return Wire.endTransmission() == 0;
}

static float lerVeml() {
  Wire.beginTransmission(ENDERECO_VEML7700);
  Wire.write(0x04);  // registrador da luz ambiente
  if (Wire.endTransmission(false) != 0) return NAN;
  if (Wire.requestFrom(ENDERECO_VEML7700, static_cast<uint8_t>(2)) != 2) return NAN;
  uint16_t bruto = Wire.read();
  bruto |= static_cast<uint16_t>(Wire.read()) << 8;
  return luxDoVeml7700(bruto);
}

static void procurar() {
  ultimaProcuraMs = millis();
  if (!temBme) temBme = iniciarBme();
  if (!temVeml) temVeml = iniciarVeml();
}

void sensoresIniciar() {
  procurar();
  Serial.printf("Sensores: BME280 %s, VEML7700 %s\r\n", temBme ? "ok" : "nao encontrado", temVeml ? "ok" : "nao encontrado");
}

bool sensoresTick(uint32_t agoraMs) {
  if ((!temBme || !temVeml) && agoraMs - ultimaProcuraMs >= INTERVALO_PROCURA_MS) procurar();
  if (ultimaLeituraMs != 0 && agoraMs - ultimaLeituraMs < INTERVALO_LEITURA_MS) return false;
  ultimaLeituraMs = agoraMs == 0 ? 1 : agoraMs;

  Leitura l;
  if (temBme && bme.takeForcedMeasurement()) {
    float t = bme.readTemperature(), u = bme.readHumidity(), p = bme.readPressure();
    // Fora da faixa do sensor é falha de leitura, não clima.
    if (!isnan(t) && t > -40 && t < 85) l.tempC = t + ajustes.ajusteTempC;
    if (!isnan(u) && u >= 0 && u <= 100) l.umidadePct = u;
    if (!isnan(p) && p > 30000 && p < 110000) l.pressaoHpa = p / 100.0f;
    if (isnan(l.tempC)) temBme = false;  // parou de responder: procura de novo depois
  }
  if (temVeml) {
    l.lux = lerVeml();
    if (isnan(l.lux)) temVeml = false;
  }
  ultima = l;
  return true;
}

const Leitura& sensoresLeitura() { return ultima; }

}  // namespace sabia
