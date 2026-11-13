#include "Balanco.h"

#include <math.h>

namespace sabia {

static float limitar(float v, float minimo, float maximo) { return v < minimo ? minimo : (v > maximo ? maximo : v); }

uint16_t diaDoAno(uint32_t dias) {
  uint16_t ano = 2000;
  while (true) {
    bool bissexto = (ano % 4 == 0 && ano % 100 != 0) || ano % 400 == 0;
    uint32_t noAno = bissexto ? 366 : 365;
    if (dias < noAno) break;
    dias -= noAno;
    ano++;
  }
  return static_cast<uint16_t>(dias + 1);
}

float radiacaoExtraterrestreMm(float latitudeGraus, uint16_t dia) {
  if (isnan(latitudeGraus) || latitudeGraus < -90 || latitudeGraus > 90) return NAN;
  const float PI_F = 3.14159265f;
  float fi = latitudeGraus * PI_F / 180.0f;
  float angulo = 2.0f * PI_F * dia / 365.0f;
  float dr = 1.0f + 0.033f * cosf(angulo);            // distância relativa Terra-Sol
  float delta = 0.409f * sinf(angulo - 1.39f);        // declinação solar
  float x = limitar(-tanf(fi) * tanf(delta), -1.0f, 1.0f);
  float ws = acosf(x);                                // ângulo do pôr do sol
  float raMj = (24.0f * 60.0f / PI_F) * 0.0820f * dr * (ws * sinf(fi) * sinf(delta) + cosf(fi) * cosf(delta) * sinf(ws));
  if (raMj < 0) raMj = 0;
  return raMj * 0.408f;  // MJ/m2/dia -> mm/dia
}

float et0Hargreaves(float tmax, float tmin, float raMm) {
  if (isnan(tmax) || isnan(tmin) || isnan(raMm) || tmax < tmin) return NAN;
  float et0 = 0.0023f * ((tmax + tmin) / 2.0f + 17.8f) * sqrtf(tmax - tmin) * raMm;
  return et0 < 0 ? 0 : et0;
}

BalancoHidrico::BalancoHidrico() {
  for (float& s : saldo_) s = solo_.capacidadeMm;  // começa com o solo cheio
}

void BalancoHidrico::definir(const ParametrosSetor setores[NUM_SETORES], const ParametrosSolo& solo) {
  solo_ = solo;
  solo_.capacidadeMm = limitar(solo.capacidadeMm, 5.0f, 300.0f);
  if (solo_.chuvaEfetivaMinMm < 0) solo_.chuvaEfetivaMinMm = 0;
  for (uint8_t i = 0; i < NUM_SETORES; i++) {
    setores_[i] = setores[i];
    setores_[i].kc = limitar(setores[i].kc, 0.1f, 2.0f);
    setores_[i].taxaMmH = limitar(setores[i].taxaMmH, 0.1f, 100.0f);
    setores_[i].eficiencia = limitar(setores[i].eficiencia, 0.1f, 1.0f);
    setores_[i].deficitMinMm = limitar(setores[i].deficitMinMm, 0.0f, 100.0f);
    setores_[i].fator = limitar(setores[i].fator, 0.1f, 3.0f);
    saldo_[i] = limitar(saldo_[i], 0.0f, solo_.capacidadeMm);
  }
}

void BalancoHidrico::rega(uint8_t setor, uint32_t segundos) {
  if (setor < 1 || setor > NUM_SETORES) return;
  const ParametrosSetor& p = setores_[setor - 1];
  float mm = p.taxaMmH * (segundos / 3600.0f) * p.eficiencia;
  saldo_[setor - 1] = limitar(saldo_[setor - 1] + mm, 0.0f, solo_.capacidadeMm);
}

void BalancoHidrico::fecharDia(float chuvaMm, float et0Mm) {
  float efetiva = (isnan(chuvaMm) || chuvaMm < solo_.chuvaEfetivaMinMm) ? 0 : chuvaMm;
  confiavel_ = !isnan(et0Mm);
  for (uint8_t i = 0; i < NUM_SETORES; i++) {
    float sai = confiavel_ ? et0Mm * setores_[i].kc : 0;
    saldo_[i] = limitar(saldo_[i] + efetiva - sai, 0.0f, solo_.capacidadeMm);
  }
}

float BalancoHidrico::saldoMm(uint8_t setor) const { return setor >= 1 && setor <= NUM_SETORES ? saldo_[setor - 1] : 0; }

float BalancoHidrico::deficitMm(uint8_t setor) const {
  return setor >= 1 && setor <= NUM_SETORES ? solo_.capacidadeMm - saldo_[setor - 1] : 0;
}

uint8_t BalancoHidrico::saldoPct(uint8_t setor) const {
  return static_cast<uint8_t>(limitar(saldoMm(setor) * 100.0f / solo_.capacidadeMm + 0.5f, 0.0f, 100.0f));
}

uint32_t BalancoHidrico::tempoNecessarioS(uint8_t setor) const {
  if (setor < 1 || setor > NUM_SETORES) return 0;
  const ParametrosSetor& p = setores_[setor - 1];
  float deficit = deficitMm(setor);
  if (deficit <= 0 || deficit < p.deficitMinMm) return 0;
  float horas = deficit / (p.taxaMmH * p.eficiencia) * p.fator;
  return static_cast<uint32_t>(horas * 3600.0f + 0.5f);
}

uint32_t BalancoHidrico::ajustar(uint8_t setor, uint32_t programadoS) const {
  if (!confiavel_) return programadoS;
  uint32_t necessario = tempoNecessarioS(setor);
  return necessario < programadoS ? necessario : programadoS;
}

BalancoHidrico::Salvo BalancoHidrico::salvar() const {
  Salvo s;
  s.confiavel = confiavel_;
  for (uint8_t i = 0; i < NUM_SETORES; i++) s.saldoMm[i] = saldo_[i];
  return s;
}

void BalancoHidrico::recuperar(const Salvo& s) {
  if (s.versao != 1) return;
  confiavel_ = s.confiavel;
  for (uint8_t i = 0; i < NUM_SETORES; i++) {
    saldo_[i] = isnan(s.saldoMm[i]) ? solo_.capacidadeMm : limitar(s.saldoMm[i], 0.0f, solo_.capacidadeMm);
  }
}

void Chuva24h::somar(uint32_t horaAbsoluta, float mm) {
  if (isnan(mm) || mm <= 0) return;
  uint8_t i = horaAbsoluta % 24;
  if (!usada_[i] || hora_[i] != horaAbsoluta) {
    mm_[i] = 0;
    hora_[i] = horaAbsoluta;
    usada_[i] = true;
  }
  mm_[i] += mm;
}

float Chuva24h::total(uint32_t horaAbsoluta) const {
  float soma = 0;
  for (uint8_t i = 0; i < 24; i++) {
    if (usada_[i] && hora_[i] <= horaAbsoluta && horaAbsoluta - hora_[i] < 24) soma += mm_[i];
  }
  return soma;
}

}  // namespace sabia
