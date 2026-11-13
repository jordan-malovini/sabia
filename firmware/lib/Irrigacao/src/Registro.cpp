#include "Registro.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Agenda.h"

namespace sabia {

Leitura::Leitura() : tempC(NAN), umidadePct(NAN), pressaoHpa(NAN), lux(NAN) {}
HoraClima::HoraClima() : tempMaxC(NAN), tempMinC(NAN), umidadePct(NAN), pressaoHpa(NAN), lux(NAN) {}
ResumoDia::ResumoDia() : tempMaxC(NAN), tempMinC(NAN), et0Mm(NAN) {}

static float maior(float a, float b) { return isnan(a) ? b : (isnan(b) ? a : (a > b ? a : b)); }
static float menor(float a, float b) { return isnan(a) ? b : (isnan(b) ? a : (a < b ? a : b)); }

// Escreve um número com `casas` decimais, ou nada se for NAN.
static size_t numero(char* saida, size_t tamanho, float v, int casas) {
  if (tamanho == 0) return 0;
  if (isnan(v)) {
    saida[0] = 0;
    return 0;
  }
  int n = snprintf(saida, tamanho, "%.*f", casas, static_cast<double>(v));
  return n < 0 ? 0 : (static_cast<size_t>(n) >= tamanho ? tamanho - 1 : static_cast<size_t>(n));
}

// Monta uma linha a partir de campos numéricos separados por ';'.
class Montador {
 public:
  Montador(char* linha, const char* inicio) : linha_(linha) {
    usado_ = static_cast<size_t>(snprintf(linha_, TAMANHO_LINHA, "%s", inicio));
    if (usado_ >= TAMANHO_LINHA) usado_ = TAMANHO_LINHA - 1;
  }
  void campo(float v, int casas) {
    separador();
    usado_ += numero(linha_ + usado_, TAMANHO_LINHA - usado_, v, casas);
  }
  void inteiro(unsigned long v) {
    separador();
    int n = snprintf(linha_ + usado_, TAMANHO_LINHA - usado_, "%lu", v);
    if (n > 0) usado_ += static_cast<size_t>(n);
    if (usado_ >= TAMANHO_LINHA) usado_ = TAMANHO_LINHA - 1;
  }

 private:
  void separador() {
    if (usado_ + 1 < TAMANHO_LINHA) {
      linha_[usado_++] = ';';
      linha_[usado_] = 0;
    }
  }
  char* linha_;
  size_t usado_;
};

static void prefixo(char* saida, char tipo, uint16_t minutoDoDia) {
  snprintf(saida, 10, "%c;%02u:%02u", tipo, static_cast<unsigned>(minutoDoDia / 60 % 24), static_cast<unsigned>(minutoDoDia % 60));
}

// Separa a linha em campos por ';'. O último campo fica com o resto do texto
// quando `maximo` é atingido (o texto de um evento pode ter ';').
static uint8_t separar(char* texto, char* campos[], uint8_t maximo) {
  uint8_t n = 0;
  campos[n++] = texto;
  for (char* p = texto; *p != 0 && n < maximo; p++) {
    if (*p == ';') {
      *p = 0;
      campos[n++] = p + 1;
    }
  }
  return n;
}

static float valor(const char* campo) { return campo == nullptr || campo[0] == 0 ? NAN : static_cast<float>(atof(campo)); }

static bool lerHora(const char* campo, uint16_t& minutoDoDia) {
  if (campo == nullptr || strlen(campo) != 5 || campo[2] != ':') return false;
  int h = atoi(campo), m = atoi(campo + 3);
  if (h < 0 || h > 23 || m < 0 || m > 59) return false;
  minutoDoDia = static_cast<uint16_t>(h * 60 + m);
  return true;
}

void RegistroDia::iniciar(uint32_t dia) {
  dia_ = dia;
  for (uint8_t h = 0; h < 24; h++) {
    chuvaHora_[h] = 0;
    tempMaxHora_[h] = tempMinHora_[h] = NAN;
  }
  for (uint8_t s = 0; s < NUM_SETORES; s++) {
    vezes_[s] = 0;
    segundos_[s] = 0;
  }
  nEventos_ = proximoEvento_ = 0;
}

void RegistroDia::chuva(uint16_t minutoDoDia, float mm, char* linha) {
  if (mm <= 0 || isnan(mm)) {
    if (linha != nullptr) linha[0] = 0;
    return;
  }
  chuvaHora_[minutoDoDia / 60 % 24] += mm;
  if (linha != nullptr) {
    char inicio[10];
    prefixo(inicio, 'C', minutoDoDia);
    Montador(linha, inicio).campo(mm, 1);
  }
}

void RegistroDia::temperatura(uint16_t minutoDoDia, float tempC) {
  if (isnan(tempC)) return;
  uint8_t h = minutoDoDia / 60 % 24;
  tempMaxHora_[h] = maior(tempMaxHora_[h], tempC);
  tempMinHora_[h] = menor(tempMinHora_[h], tempC);
}

void RegistroDia::clima(uint16_t minutoDoDia, const HoraClima& hora, char* linha) {
  temperatura(minutoDoDia, hora.tempMaxC);
  temperatura(minutoDoDia, hora.tempMinC);
  if (linha != nullptr) {
    char inicio[10];
    prefixo(inicio, 'T', minutoDoDia);
    Montador m(linha, inicio);
    m.campo(hora.tempMaxC, 1);
    m.campo(hora.tempMinC, 1);
    m.campo(hora.umidadePct, 0);
    m.campo(hora.pressaoHpa, 1);
    m.campo(hora.lux, 0);
  }
}

void RegistroDia::rega(uint16_t minutoDoDia, uint8_t setor, uint32_t segundos, char* linha) {
  if (linha != nullptr) linha[0] = 0;
  if (setor < 1 || setor > NUM_SETORES || segundos == 0) return;
  vezes_[setor - 1]++;
  segundos_[setor - 1] += segundos;
  char texto[TAMANHO_EVENTO];
  if (segundos >= 60) snprintf(texto, sizeof(texto), "Setor %u regou %lu min", setor, static_cast<unsigned long>((segundos + 30) / 60));
  else snprintf(texto, sizeof(texto), "Setor %u regou %lu s", setor, static_cast<unsigned long>(segundos));
  guardarEvento(minutoDoDia, texto);
  if (linha != nullptr) {
    char inicio[10];
    prefixo(inicio, 'R', minutoDoDia);
    Montador m(linha, inicio);
    m.inteiro(setor);
    m.inteiro(segundos);
  }
}

void RegistroDia::evento(uint16_t minutoDoDia, const char* texto, char* linha) {
  guardarEvento(minutoDoDia, texto);
  if (linha != nullptr) {
    char inicio[10];
    prefixo(inicio, 'E', minutoDoDia);
    snprintf(linha, TAMANHO_LINHA, "%s;%.*s", inicio, static_cast<int>(TAMANHO_EVENTO - 1), texto);
    for (char* p = linha; *p != 0; p++) if (*p == '\n' || *p == '\r') *p = ' ';
  }
}

void RegistroDia::guardarEvento(uint16_t minutoDoDia, const char* texto) {
  Evento& e = eventos_[proximoEvento_];
  e.minutoDoDia = minutoDoDia;
  snprintf(e.texto, sizeof(e.texto), "%s", texto);
  proximoEvento_ = static_cast<uint8_t>((proximoEvento_ + 1) % MAX_EVENTOS);
  if (nEventos_ < MAX_EVENTOS) nEventos_++;
}

const Evento& RegistroDia::eventoRecente(uint8_t i) const {
  static const Evento vazio;
  if (i >= nEventos_) return vazio;
  return eventos_[(proximoEvento_ + MAX_EVENTOS - 1 - i) % MAX_EVENTOS];
}

bool RegistroDia::aplicarLinha(const char* linha) {
  char copia[TAMANHO_LINHA];
  snprintf(copia, sizeof(copia), "%s", linha);
  size_t n = strlen(copia);
  while (n > 0 && (copia[n - 1] == '\n' || copia[n - 1] == '\r')) copia[--n] = 0;
  if (n < 8 || copia[1] != ';') return false;

  char tipo = copia[0];
  char* campos[8] = {};
  uint8_t total = separar(copia, campos, tipo == 'E' ? 3 : 8);
  uint16_t minuto = 0;
  if (total < 3 || !lerHora(campos[1], minuto)) return false;

  switch (tipo) {
    case 'C':
      chuva(minuto, valor(campos[2]));
      return true;
    case 'T': {
      HoraClima h;
      h.tempMaxC = valor(campos[2]);
      h.tempMinC = total > 3 ? valor(campos[3]) : NAN;
      clima(minuto, h);
      return true;
    }
    case 'R':
      if (total < 4) return false;
      rega(minuto, static_cast<uint8_t>(atoi(campos[2])), static_cast<uint32_t>(strtoul(campos[3], nullptr, 10)));
      return true;
    case 'E':
      guardarEvento(minuto, campos[2]);
      return true;
    default:
      return false;
  }
}

float RegistroDia::chuvaMm() const {
  float total = 0;
  for (float mm : chuvaHora_) total += mm;
  return total;
}

float RegistroDia::tempHoraC(uint8_t hora) const { return hora < 24 ? tempMaxHora_[hora] : NAN; }

float RegistroDia::tempMaxC() const {
  float v = NAN;
  for (float t : tempMaxHora_) v = maior(v, t);
  return v;
}

float RegistroDia::tempMinC() const {
  float v = NAN;
  for (float t : tempMinHora_) v = menor(v, t);
  return v;
}

ResumoDia RegistroDia::resumo(float et0Mm) const {
  ResumoDia r;
  r.dia = dia_;
  r.chuvaMm = chuvaMm();
  r.tempMaxC = tempMaxC();
  r.tempMinC = tempMinC();
  r.et0Mm = et0Mm;
  for (uint8_t s = 0; s < NUM_SETORES; s++) {
    r.vezes[s] = vezes_[s];
    r.minutos[s] = static_cast<uint16_t>((segundos_[s] + 30) / 60);
  }
  return r;
}

// ---------------------------------------------------------------------------
// Resumo
// ---------------------------------------------------------------------------
static bool bissexto(unsigned ano) { return (ano % 4 == 0 && ano % 100 != 0) || ano % 400 == 0; }

static void dataDoDia(uint32_t dias, unsigned& ano, unsigned& mes, unsigned& dia) {
  static const uint8_t DIAS[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  ano = 2000;
  mes = 1;
  while (dias >= (bissexto(ano) ? 366u : 365u)) {
    dias -= bissexto(ano) ? 366 : 365;
    ano++;
  }
  while (mes < 12 && dias >= (mes == 2 && bissexto(ano) ? 29u : DIAS[mes - 1])) {
    dias -= mes == 2 && bissexto(ano) ? 29 : DIAS[mes - 1];
    mes++;
  }
  dia = dias + 1;
}

void formatarResumo(const ResumoDia& r, char linha[TAMANHO_LINHA]) {
  unsigned ano, mes, dia;
  char inicio[12];
  dataDoDia(r.dia, ano, mes, dia);
  snprintf(inicio, sizeof(inicio), "%04u-%02u-%02u", ano, mes, dia);
  Montador m(linha, inicio);
  m.campo(r.chuvaMm, 1);
  m.campo(r.tempMaxC, 1);
  m.campo(r.tempMinC, 1);
  m.campo(r.et0Mm, 1);
  for (uint8_t s = 0; s < NUM_SETORES; s++) {
    m.inteiro(r.vezes[s]);
    m.inteiro(r.minutos[s]);
  }
}

bool lerResumo(const char* linha, ResumoDia& r) {
  char copia[TAMANHO_LINHA];
  snprintf(copia, sizeof(copia), "%s", linha);
  size_t n = strlen(copia);
  while (n > 0 && (copia[n - 1] == '\n' || copia[n - 1] == '\r')) copia[--n] = 0;
  char* campos[5 + 2 * NUM_SETORES] = {};
  if (separar(copia, campos, 5 + 2 * NUM_SETORES) != 5 + 2 * NUM_SETORES) return false;
  int ano = 0, mes = 0, dia = 0;
  if (sscanf(campos[0], "%d-%d-%d", &ano, &mes, &dia) != 3 || ano < 2000 || ano > 2099 || mes < 1 || mes > 12 || dia < 1 || dia > 31) return false;
  ResumoDia t;
  t.dia = diasDesde2000(static_cast<uint16_t>(ano), static_cast<uint8_t>(mes), static_cast<uint8_t>(dia));
  t.chuvaMm = isnan(valor(campos[1])) ? 0 : valor(campos[1]);
  t.tempMaxC = valor(campos[2]);
  t.tempMinC = valor(campos[3]);
  t.et0Mm = valor(campos[4]);
  for (uint8_t s = 0; s < NUM_SETORES; s++) {
    t.vezes[s] = static_cast<uint16_t>(atoi(campos[5 + 2 * s]));
    t.minutos[s] = static_cast<uint16_t>(atoi(campos[6 + 2 * s]));
  }
  r = t;
  return true;
}

void acumular(HoraClima& hora, const Leitura& l, uint16_t& n) {
  hora.tempMaxC = maior(hora.tempMaxC, l.tempC);
  hora.tempMinC = menor(hora.tempMinC, l.tempC);
  // Umidade, pressão e luz: média corrida das leituras da hora.
  auto media = [n](float atual, float novo) { return isnan(novo) ? atual : (isnan(atual) ? novo : (atual * n + novo) / (n + 1)); };
  hora.umidadePct = media(hora.umidadePct, l.umidadePct);
  hora.pressaoHpa = media(hora.pressaoHpa, l.pressaoHpa);
  hora.lux = media(hora.lux, l.lux);
  n++;
}

float luxDoVeml7700(uint16_t bruto) {
  // Resolução = 0,0036 x (800 / 25 ms) x (2 / (1/8)) = 1,8432 lux por contagem,
  // o que cobre até cerca de 120 mil lux (sol pleno). Sem correção de não
  // linearidade: atrás da cúpula o valor já pede calibração própria.
  return bruto * 1.8432f;
}

}  // namespace sabia
