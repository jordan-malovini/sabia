#include "Agenda.h"

namespace sabia {

static const uint16_t DIAS_ANTES_DO_MES[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
static bool bissexto(uint16_t ano) { return (ano % 4 == 0 && ano % 100 != 0) || ano % 400 == 0; }

uint32_t diasDesde2000(uint16_t ano, uint8_t mes, uint8_t dia) {
  if (ano < 2000) ano = 2000;
  if (mes < 1) mes = 1;
  if (mes > 12) mes = 12;
  if (dia < 1) dia = 1;
  uint32_t dias = 0;
  for (uint16_t a = 2000; a < ano; a++) dias += bissexto(a) ? 366 : 365;
  dias += DIAS_ANTES_DO_MES[mes - 1];
  if (mes > 2 && bissexto(ano)) dias += 1;
  return dias + dia - 1;
}

// 1º de janeiro de 2000 foi um sábado.
uint8_t diaDaSemana(uint32_t dias) { return static_cast<uint8_t>((dias + 6) % 7); }

uint8_t diaDoMes(uint32_t dias) {
  uint16_t ano = 2000;
  while (true) {
    uint32_t noAno = bissexto(ano) ? 366 : 365;
    if (dias < noAno) break;
    dias -= noAno;
    ano++;
  }
  for (uint8_t mes = 12; mes >= 1; mes--) {
    uint32_t antes = DIAS_ANTES_DO_MES[mes - 1] + ((mes > 2 && bissexto(ano)) ? 1 : 0);
    if (dias >= antes) return static_cast<uint8_t>(dias - antes + 1);
  }
  return 1;
}

static uint32_t minutoAbsoluto(const Momento& m) {
  return diasDesde2000(m.ano, m.mes, m.dia) * 1440UL + m.hora * 60UL + m.minuto;
}

bool diaPermitido(const Programa& p, uint32_t dias) {
  switch (p.modo) {
    case ModoDias::Semana: return (p.diasSemana >> diaDaSemana(dias)) & 1;
    case ModoDias::Impares: return diaDoMes(dias) % 2 == 1;
    case ModoDias::Pares: return diaDoMes(dias) % 2 == 0;
    case ModoDias::Intervalo: {
      uint8_t n = p.intervaloDias < 1 ? 1 : p.intervaloDias;
      if (dias < p.diaReferencia) return false;
      return (dias - p.diaReferencia) % n == 0;
    }
  }
  return false;
}

uint32_t Agenda::duracaoSetorS(const Programa& p, uint8_t setor) const {
  uint32_t minutos = p.minutosSetor[setor];
  if (minutos > MINUTOS_MAX_SETOR) minutos = MINUTOS_MAX_SETOR;
  uint8_t pct = p.ajustePct < AJUSTE_MIN ? AJUSTE_MIN : (p.ajustePct > AJUSTE_MAX ? AJUSTE_MAX : p.ajustePct);
  return minutos * 60UL * pct / 100UL;
}

uint32_t Agenda::duracaoTotalS(uint8_t i) const {
  if (i >= NUM_PROGRAMAS) return 0;
  uint32_t total = 0;
  for (uint8_t s = 0; s < NUM_SETORES; s++) total += duracaoSetorS(programas_[i], s);
  return total;
}

void Agenda::tick(const Momento& agora) {
  uint32_t m = minutoAbsoluto(agora);
  if (!iniciada_) {
    iniciada_ = true;
    ultimoMinuto_ = m;  // nada do passado dispara ao ligar
  }
  if (m < ultimoMinuto_ || m - ultimoMinuto_ > 10) {
    ultimoMinuto_ = m;  // o relógio foi ajustado: não dispara regas atrasadas
  }
  for (uint32_t t = ultimoMinuto_ + 1; t <= m; t++) verificarMinuto(t);
  ultimoMinuto_ = m;
  liberarPendente();
}

void Agenda::verificarMinuto(uint32_t minutoAbs) {
  uint32_t dia = minutoAbs / 1440UL;
  uint16_t minutoDoDia = static_cast<uint16_t>(minutoAbs % 1440UL);
  for (uint8_t i = 0; i < NUM_PROGRAMAS; i++) {
    const Programa& p = programas_[i];
    if (!p.ativo || !diaPermitido(p, dia)) continue;
    for (uint8_t k = 0; k < INICIOS_POR_PROGRAMA; k++) {
      if (p.inicios[k] != minutoDoDia) continue;
      if (dia < suspensoAteDia_) puladosPorChuva_++;
      else empilhar(i);
      break;  // dois inícios iguais no mesmo programa contam uma vez
    }
  }
}

void Agenda::empilhar(uint8_t i) {
  if (nPendentes_ < MAX_PENDENTES) pendentes_[nPendentes_++] = i;
}

void Agenda::liberarPendente() {
  if (nPendentes_ == 0) return;
  if (controlador_.estado() != Estado::Ocioso || controlador_.tamanhoFila() != 0) return;
  uint8_t i = pendentes_[0];
  for (uint8_t k = 1; k < nPendentes_; k++) pendentes_[k - 1] = pendentes_[k];
  nPendentes_--;
  const Programa& p = programas_[i];
  for (uint8_t s = 0; s < NUM_SETORES; s++) {
    uint32_t duracao = duracaoSetorS(p, s);
    if (duracao > 0) controlador_.pedir(s + 1, duracao);
  }
}

bool Agenda::iniciarPrograma(uint8_t i) {
  if (i >= NUM_PROGRAMAS || duracaoTotalS(i) == 0 || nPendentes_ >= MAX_PENDENTES) return false;
  empilhar(i);
  liberarPendente();
  return true;
}

void Agenda::suspender(uint8_t dias, const Momento& agora) {
  if (dias > SUSPENSAO_MAX_DIAS) dias = SUSPENSAO_MAX_DIAS;
  suspensoAteDia_ = dias == 0 ? 0 : diasDesde2000(agora.ano, agora.mes, agora.dia) + dias;
  if (dias > 0) nPendentes_ = 0;
}

uint8_t Agenda::diasSuspensos(const Momento& agora) const {
  uint32_t hoje = diasDesde2000(agora.ano, agora.mes, agora.dia);
  return suspensoAteDia_ > hoje ? static_cast<uint8_t>(suspensoAteDia_ - hoje) : 0;
}

bool Agenda::proximoInicio(const Momento& agora, uint8_t& programa, uint16_t& minutoDoDia, uint32_t& faltamMin) const {
  uint32_t m = minutoAbsoluto(agora);
  uint32_t hoje = m / 1440UL;
  for (uint32_t dia = hoje; dia <= hoje + 31; dia++) {
    if (dia < suspensoAteDia_) continue;
    bool achou = false;
    uint32_t melhor = 0;
    for (uint8_t i = 0; i < NUM_PROGRAMAS; i++) {
      const Programa& p = programas_[i];
      if (!p.ativo || !diaPermitido(p, dia) || duracaoTotalS(i) == 0) continue;
      for (uint8_t k = 0; k < INICIOS_POR_PROGRAMA; k++) {
        if (p.inicios[k] == SEM_HORARIO || p.inicios[k] >= 1440) continue;
        uint32_t t = dia * 1440UL + p.inicios[k];
        if (t <= m) continue;
        if (!achou || t < melhor) {
          achou = true;
          melhor = t;
          programa = i;
          minutoDoDia = p.inicios[k];
        }
      }
    }
    if (achou) {
      faltamMin = melhor - m;
      return true;
    }
  }
  return false;
}

}  // namespace sabia
