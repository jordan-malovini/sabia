#include "ApiJson.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace sabia {

Ajustes::Ajustes() : latitude(NAN) {}

static const char* const MODOS[] = {"semana", "impares", "pares", "intervalo"};
static char erro_[112];

static bool inteiro(JsonVariantConst v, long minimo, long maximo, long& valor) {
  if (!v.is<long>()) return false;
  valor = v.as<long>();
  return valor >= minimo && valor <= maximo;
}

static bool decimal(JsonVariantConst v, float minimo, float maximo) {
  if (!v.is<float>()) return false;
  float valor = v.as<float>();
  return valor >= minimo && valor <= maximo;
}

static bool texto(JsonVariantConst v, size_t minimo, size_t maximo) {
  if (!v.is<const char*>()) return false;
  size_t n = strlen(v.as<const char*>());
  return n >= minimo && n <= maximo;
}

static bool doisDigitos(const char* t, int& valor) {
  if (t[0] < '0' || t[0] > '9' || t[1] < '0' || t[1] > '9') return false;
  valor = (t[0] - '0') * 10 + (t[1] - '0');
  return true;
}

bool horaDeTexto(const char* t, uint16_t& minutoDoDia) {
  int h = 0, m = 0;
  if (t == nullptr || strlen(t) != 5 || t[2] != ':') return false;
  if (!doisDigitos(t, h) || !doisDigitos(t + 3, m) || h > 23 || m > 59) return false;
  minutoDoDia = static_cast<uint16_t>(h * 60 + m);
  return true;
}

void textoDeHora(uint16_t minutoDoDia, char saida[6]) {
  snprintf(saida, 6, "%02u:%02u", static_cast<unsigned>(minutoDoDia / 60 % 24), static_cast<unsigned>(minutoDoDia % 60));
}

static bool bissexto(unsigned ano) { return (ano % 4 == 0 && ano % 100 != 0) || ano % 400 == 0; }
static unsigned diasNoMes(unsigned ano, unsigned mes) {
  static const uint8_t DIAS[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return mes == 2 && bissexto(ano) ? 29 : DIAS[mes - 1];
}

bool diasDeData(const char* t, uint32_t& dias) {
  if (t == nullptr || strlen(t) != 10 || t[4] != '-' || t[7] != '-') return false;
  int seculo = 0, anoCurto = 0, mes = 0, dia = 0;
  if (!doisDigitos(t, seculo) || !doisDigitos(t + 2, anoCurto) || !doisDigitos(t + 5, mes) || !doisDigitos(t + 8, dia)) return false;
  unsigned ano = static_cast<unsigned>(seculo * 100 + anoCurto);
  if (ano < 2000 || ano > 2099 || mes < 1 || mes > 12 || dia < 1 || static_cast<unsigned>(dia) > diasNoMes(ano, mes)) return false;
  dias = diasDesde2000(static_cast<uint16_t>(ano), static_cast<uint8_t>(mes), static_cast<uint8_t>(dia));
  return true;
}

void dataDeDias(uint32_t dias, char saida[11]) {
  unsigned ano = 2000, mes = 1;
  while (ano < 2099 && dias >= (bissexto(ano) ? 366u : 365u)) {
    dias -= bissexto(ano) ? 366 : 365;
    ano++;
  }
  while (mes < 12 && dias >= diasNoMes(ano, mes)) {
    dias -= diasNoMes(ano, mes);
    mes++;
  }
  unsigned dia = dias + 1 > 31 ? 31 : dias + 1;
  snprintf(saida, 11, "%04u-%02u-%02u", ano, mes, dia);
}

// ---------------------------------------------------------------------------
// Programas
// ---------------------------------------------------------------------------
void programaParaJson(const Programa& p, JsonObject o) {
  o["ativo"] = p.ativo;
  JsonArray inicios = o["inicios"].to<JsonArray>();
  for (uint16_t ini : p.inicios) {
    if (ini == SEM_HORARIO || ini >= 1440) {
      inicios.add(static_cast<const char*>(nullptr));
    } else {
      char hora[6];
      textoDeHora(ini, hora);
      inicios.add(hora);
    }
  }
  uint8_t modo = static_cast<uint8_t>(p.modo);
  o["modo"] = MODOS[modo < 4 ? modo : 0];
  JsonArray dias = o["dias"].to<JsonArray>();
  for (uint8_t d = 0; d < 7; d++) dias.add((p.diasSemana >> d) & 1);
  o["intervaloDias"] = p.intervaloDias < 1 ? 1 : p.intervaloDias;
  if (p.modo == ModoDias::Intervalo) {
    char data[11];
    dataDeDias(p.diaReferencia, data);
    o["inicioIntervalo"] = data;
  } else {
    o["inicioIntervalo"] = static_cast<const char*>(nullptr);
  }
  JsonArray minutos = o["minutos"].to<JsonArray>();
  for (uint16_t m : p.minutosSetor) minutos.add(m);
  o["ajustePct"] = p.ajustePct;
}

const char* programaDeJson(JsonVariantConst e, Programa& p) {
  Programa t = p;
  long n = 0;

  if (!e["ativo"].is<bool>()) return "falta dizer se o programa está ligado";
  t.ativo = e["ativo"].as<bool>();

  JsonArrayConst inicios = e["inicios"].as<JsonArrayConst>();
  if (inicios.size() != INICIOS_POR_PROGRAMA) return "os horários de início devem ser 4";
  for (uint8_t k = 0; k < INICIOS_POR_PROGRAMA; k++) {
    JsonVariantConst v = inicios[k];
    if (v.isNull()) t.inicios[k] = SEM_HORARIO;
    else if (!horaDeTexto(v.as<const char*>(), t.inicios[k])) return "horário de início inválido";
  }

  const char* modo = e["modo"].as<const char*>();
  int indiceModo = -1;
  for (int k = 0; k < 4; k++) if (modo != nullptr && strcmp(modo, MODOS[k]) == 0) indiceModo = k;
  if (indiceModo < 0) return "modo de dias inválido";
  t.modo = static_cast<ModoDias>(indiceModo);

  JsonArrayConst dias = e["dias"].as<JsonArrayConst>();
  if (dias.size() != 7) return "os dias da semana devem ser 7";
  t.diasSemana = 0;
  for (uint8_t d = 0; d < 7; d++) if (dias[d].as<bool>()) t.diasSemana |= static_cast<uint8_t>(1 << d);

  if (!inteiro(e["intervaloDias"], 1, 30, n)) return "o intervalo deve ser de 1 a 30 dias";
  t.intervaloDias = static_cast<uint8_t>(n);
  JsonVariantConst inicioIntervalo = e["inicioIntervalo"];
  if (!inicioIntervalo.isNull() && !diasDeData(inicioIntervalo.as<const char*>(), t.diaReferencia))
    return "data de início do intervalo inválida";

  JsonArrayConst minutos = e["minutos"].as<JsonArrayConst>();
  if (minutos.size() != NUM_SETORES) return "os tempos devem ser 8, um por setor";
  for (uint8_t s = 0; s < NUM_SETORES; s++) {
    if (!inteiro(minutos[s], 0, MINUTOS_MAX_SETOR, n)) return "tempo de setor fora da faixa (0 a 180 min)";
    t.minutosSetor[s] = static_cast<uint16_t>(n);
  }

  if (!inteiro(e["ajustePct"], AJUSTE_MIN, AJUSTE_MAX, n)) return "o ajuste deve ser de 5 a 200 %";
  t.ajustePct = static_cast<uint8_t>(n);

  p = t;
  return nullptr;
}

// ---------------------------------------------------------------------------
// Configuração
// ---------------------------------------------------------------------------
void configPadrao(JsonDocument& doc) {
  doc.clear();
  doc["balancoHidrico"] = true;
  doc["adiarPorChuva"] = true;
  doc["chuvaAdiarMm"] = 5;

  JsonObject retomada = doc["retomada"].to<JsonObject>();
  retomada["ativa"] = true;
  retomada["esperaMin"] = 10;
  retomada["limiteMin"] = 120;

  JsonObject bomba = doc["bomba"].to<JsonObject>();
  bomba["usar"] = true;
  bomba["atrasoLigarS"] = 2;
  bomba["atrasoFecharS"] = 1;
  bomba["sobreposicaoS"] = 2;
  bomba["pausaEntreSetoresS"] = 3;

  JsonArray setores = doc["setores"].to<JsonArray>();
  for (uint8_t i = 1; i <= NUM_SETORES; i++) {
    JsonObject s = setores.add<JsonObject>();
    char nome[12];
    snprintf(nome, sizeof(nome), "Setor %u", i);
    s["nome"] = nome;
    s["ativo"] = true;
    s["tipo"] = "gotejo";
    s["duracaoMin"] = 25;
    s["kc"] = 1.0;
    s["taxaMmH"] = 2.5;
    s["eficienciaPct"] = 90;
    s["deficitMinMm"] = 3;
    s["tempoMaxMin"] = 60;
    JsonArray janela = s["janela"].to<JsonArray>();
    janela.add("00:00");
    janela.add("23:59");
    s["fator"] = 1.0;
  }

  JsonObject estacao = doc["estacao"].to<JsonObject>();
  estacao["mmPorBasculada"] = 0.2;
  estacao["chuvaEfetivaMinMm"] = 2;
  estacao["capacidadeSoloMm"] = 40;
  estacao["latitude"] = static_cast<const char*>(nullptr);
  estacao["ajusteTempC"] = 0;

  JsonObject relogio = doc["relogio"].to<JsonObject>();
  relogio["ntp"] = false;
  relogio["fuso"] = "America/Sao_Paulo";

  JsonObject rede = doc["rede"].to<JsonObject>();
  rede["wifiNome"] = "";
  rede["wifiSenha"] = "";
  rede["apNome"] = "SABIA";
  rede["apSenha"] = "";
  rede["apSempre"] = false;

  JsonObject registro = doc["registro"].to<JsonObject>();
  registro["drive"] = false;
  registro["driveUrl"] = "";
  registro["envioHoras"] = 24;
  registro["diasNoCartao"] = 7;

  JsonObject display = doc["display"].to<JsonObject>();
  display["luzSegundos"] = 60;
  display["travarComandos"] = false;

  doc["seguranca"]["sessaoMin"] = 30;
}

void mesclar(JsonVariant destino, JsonVariantConst origem) {
  if (!origem.is<JsonObjectConst>() || !destino.is<JsonObject>()) {
    destino.set(origem);
    return;
  }
  for (JsonPairConst par : origem.as<JsonObjectConst>()) {
    JsonVariant alvo = destino[par.key()];  // sem vínculo se a chave ainda não existe
    if (alvo.is<JsonObject>() && par.value().is<JsonObjectConst>()) mesclar(alvo, par.value());
    else destino[par.key()].set(par.value());
  }
}

static const char* erroSetor(unsigned setor, const char* motivo) {
  snprintf(erro_, sizeof(erro_), "Setor %u: %s", setor, motivo);
  return erro_;
}

const char* validarConfig(JsonVariantConst c) {
  long n = 0;
  long tempoMax[NUM_SETORES] = {};

  JsonArrayConst setores = c["setores"].as<JsonArrayConst>();
  if (setores.size() != NUM_SETORES) return "A configuração deve ter 8 setores";
  for (uint8_t i = 0; i < NUM_SETORES; i++) {
    JsonVariantConst s = setores[i];
    long duracao = 0;
    if (!texto(s["nome"], 1, 60)) return erroSetor(i + 1, "nome vazio ou longo demais");
    if (!s["ativo"].is<bool>()) return erroSetor(i + 1, "falta dizer se está ativo");
    if (!inteiro(s["tempoMaxMin"], 1, 240, tempoMax[i])) return erroSetor(i + 1, "tempo máximo fora da faixa (1 a 240 min)");
    if (!inteiro(s["duracaoMin"], 1, 240, duracao)) return erroSetor(i + 1, "duração fora da faixa (1 a 240 min)");
    if (duracao > tempoMax[i]) return erroSetor(i + 1, "a duração passa do tempo máximo");
    if (!decimal(s["kc"], 0.1f, 2.0f)) return erroSetor(i + 1, "Kc fora da faixa (0,1 a 2)");
    if (!decimal(s["taxaMmH"], 0.1f, 100.0f)) return erroSetor(i + 1, "taxa de aplicação fora da faixa (0,1 a 100 mm/h)");
    if (!decimal(s["eficienciaPct"], 10.0f, 100.0f)) return erroSetor(i + 1, "eficiência fora da faixa (10 a 100 %)");
    if (!decimal(s["deficitMinMm"], 0.0f, 100.0f)) return erroSetor(i + 1, "déficit mínimo fora da faixa (0 a 100 mm)");
    if (!decimal(s["fator"], 0.1f, 3.0f)) return erroSetor(i + 1, "fator de ajuste fora da faixa (0,1 a 3)");
  }

  JsonArrayConst programas = c["programas"].as<JsonArrayConst>();
  if (programas.size() != NUM_PROGRAMAS) return "A configuração deve ter 3 programas";
  for (uint8_t i = 0; i < NUM_PROGRAMAS; i++) {
    Programa p;
    const char* motivo = programaDeJson(programas[i], p);
    if (motivo != nullptr) {
      snprintf(erro_, sizeof(erro_), "Programa %c: %s", 'A' + i, motivo);
      return erro_;
    }
    for (uint8_t s = 0; s < NUM_SETORES; s++) {
      if (p.minutosSetor[s] > tempoMax[s]) {
        snprintf(erro_, sizeof(erro_), "Programa %c: o setor %u passa do tempo máximo dele (%ld min)", 'A' + i, s + 1, tempoMax[s]);
        return erro_;
      }
    }
  }

  JsonVariantConst bomba = c["bomba"];
  if (!bomba["usar"].is<bool>()) return "Bomba: falta dizer se ela é usada";
  if (!inteiro(bomba["atrasoLigarS"], 0, 30, n)) return "Bomba: atraso para ligar fora da faixa (0 a 30 s)";
  if (!inteiro(bomba["atrasoFecharS"], 0, 30, n)) return "Bomba: atraso para fechar fora da faixa (0 a 30 s)";
  if (!inteiro(bomba["sobreposicaoS"], 0, 30, n)) return "Bomba: sobreposição fora da faixa (0 a 30 s)";
  if (!inteiro(bomba["pausaEntreSetoresS"], 0, 120, n)) return "Bomba: pausa fora da faixa (0 a 120 s)";

  JsonVariantConst retomada = c["retomada"];
  if (!retomada["ativa"].is<bool>()) return "Queda de energia: falta dizer se a retomada está ligada";
  if (!inteiro(retomada["esperaMin"], 1, 60, n)) return "Queda de energia: espera fora da faixa (1 a 60 min)";
  if (!inteiro(retomada["limiteMin"], 10, 720, n)) return "Queda de energia: limite fora da faixa (10 a 720 min)";

  JsonVariantConst rede = c["rede"];
  if (!texto(rede["wifiNome"], 0, 32)) return "Rede: nome do Wi-Fi longo demais";
  if (!texto(rede["wifiSenha"], 0, 63)) return "Rede: senha do Wi-Fi longa demais";
  if (!texto(rede["apNome"], 1, 32)) return "Rede: o nome da rede própria deve ter de 1 a 32 caracteres";
  if (!texto(rede["apSenha"], 0, 63) || (strlen(rede["apSenha"].as<const char*>()) > 0 && strlen(rede["apSenha"].as<const char*>()) < 8))
    return "Rede: a senha da rede própria deve ter de 8 a 63 caracteres";
  if (!rede["apSempre"].is<bool>()) return "Rede: falta dizer se a rede própria fica sempre ligada";

  if (!c["balancoHidrico"].is<bool>() || !c["adiarPorChuva"].is<bool>()) return "Clima: falta dizer se o balanço e o adiamento por chuva estão ligados";
  if (!decimal(c["chuvaAdiarMm"], 0.0f, 100.0f)) return "Clima: chuva que adia a rega fora da faixa (0 a 100 mm)";

  JsonVariantConst estacao = c["estacao"];
  if (!decimal(estacao["mmPorBasculada"], 0.01f, 2.0f)) return "Estação: chuva por basculada fora da faixa (0,01 a 2 mm)";
  if (!decimal(estacao["chuvaEfetivaMinMm"], 0.0f, 20.0f)) return "Estação: chuva mínima fora da faixa (0 a 20 mm)";
  if (!decimal(estacao["capacidadeSoloMm"], 5.0f, 300.0f)) return "Estação: capacidade do solo fora da faixa (5 a 300 mm)";
  if (!estacao["latitude"].isNull() && !decimal(estacao["latitude"], -90.0f, 90.0f)) return "Estação: latitude fora da faixa (-90 a 90)";
  if (!decimal(estacao["ajusteTempC"], -10.0f, 10.0f)) return "Estação: correção da temperatura fora da faixa (-10 a 10)";

  JsonVariantConst registro = c["registro"];
  if (!inteiro(registro["diasNoCartao"], 1, 60, n)) return "Registro: dias no cartão fora da faixa (1 a 60)";
  if (!inteiro(registro["envioHoras"], 1, 168, n)) return "Registro: intervalo de envio fora da faixa (1 a 168 h)";
  if (!registro["drive"].is<bool>() || !texto(registro["driveUrl"], 0, TAMANHO_URL - 1)) return "Registro: endereço do Apps Script longo demais";
  if (registro["drive"].as<bool>() && strncmp(registro["driveUrl"].as<const char*>(), "https://", 8) != 0)
    return "Registro: para enviar ao Drive, informe o endereço do Apps Script (começa com https://)";

  JsonVariantConst seguranca = c["seguranca"];
  if (!inteiro(seguranca["sessaoMin"], 5, 1440, n)) return "Segurança: tempo de sessão fora da faixa (5 a 1440 min)";
  if (!seguranca["novaSenha"].isNull() && !texto(seguranca["novaSenha"], SENHA_MIN, SENHA_MAX))
    return "Segurança: a nova senha deve ter de 6 a 64 caracteres";

  return nullptr;
}

void lerAjustes(JsonVariantConst c, Ajustes& a) {
  a = Ajustes();
  JsonVariantConst bomba = c["bomba"];
  a.usarBomba = bomba["usar"] | true;
  a.controlador.atrasoBombaMs = (bomba["atrasoLigarS"] | 2UL) * 1000UL;
  a.controlador.atrasoFechamentoMs = (bomba["atrasoFecharS"] | 1UL) * 1000UL;
  a.controlador.sobreposicaoMs = (bomba["sobreposicaoS"] | 2UL) * 1000UL;
  a.controlador.pausaEntreSetoresMs = (bomba["pausaEntreSetoresS"] | 3UL) * 1000UL;

  uint32_t maior = 0;
  JsonArrayConst setores = c["setores"].as<JsonArrayConst>();
  for (uint8_t i = 0; i < NUM_SETORES; i++) {
    JsonVariantConst s = setores[i];
    uint32_t maxMin = s["tempoMaxMin"] | 60UL;
    if (maxMin < 1) maxMin = 1;
    if (maxMin > 240) maxMin = 240;
    uint32_t duracao = s["duracaoMin"] | 25UL;
    if (duracao < 1) duracao = 1;
    if (duracao > maxMin) duracao = maxMin;
    a.controlador.tempoMaximoSetorS[i] = maxMin * 60UL;
    a.controlador.setorDesativado[i] = !(s["ativo"] | true);
    a.duracaoPadraoMin[i] = static_cast<uint16_t>(duracao);
    a.setores[i].kc = s["kc"] | 1.0f;
    a.setores[i].taxaMmH = s["taxaMmH"] | 2.5f;
    a.setores[i].eficiencia = (s["eficienciaPct"] | 90.0f) / 100.0f;
    a.setores[i].deficitMinMm = s["deficitMinMm"] | 3.0f;
    a.setores[i].fator = s["fator"] | 1.0f;
    if (maxMin * 60UL > maior) maior = maxMin * 60UL;
  }
  a.controlador.tempoMaximoS = maior;

  JsonVariantConst retomada = c["retomada"];
  a.retomada.ativa = retomada["ativa"] | true;
  a.retomada.esperaS = (retomada["esperaMin"] | 10UL) * 60UL;
  a.retomada.limiteMin = retomada["limiteMin"] | 120UL;

  a.sessaoMin = c["seguranca"]["sessaoMin"] | 30;
  a.mmPorBasculada = c["estacao"]["mmPorBasculada"] | 0.2f;
  if (a.mmPorBasculada < 0.01f || a.mmPorBasculada > 2.0f) a.mmPorBasculada = 0.2f;
  a.ajusteTempC = c["estacao"]["ajusteTempC"] | 0.0f;
  if (a.ajusteTempC < -10.0f || a.ajusteTempC > 10.0f) a.ajusteTempC = 0;
  a.balancoHidrico = c["balancoHidrico"] | true;
  a.adiarPorChuva = c["adiarPorChuva"] | true;
  a.chuvaAdiarMm = c["chuvaAdiarMm"] | 5.0f;
  a.latitude = c["estacao"]["latitude"].is<float>() ? c["estacao"]["latitude"].as<float>() : NAN;
  a.solo.capacidadeMm = c["estacao"]["capacidadeSoloMm"] | 40.0f;
  a.solo.chuvaEfetivaMinMm = c["estacao"]["chuvaEfetivaMinMm"] | 2.0f;
  a.drive = c["registro"]["drive"] | false;
  snprintf(a.driveUrl, sizeof(a.driveUrl), "%s", c["registro"]["driveUrl"] | "");
  if (strncmp(a.driveUrl, "https://", 8) != 0) a.drive = false;
  a.envioHoras = c["registro"]["envioHoras"] | 24;
  if (a.envioHoras < 1) a.envioHoras = 1;
  a.diasNoCartao = c["registro"]["diasNoCartao"] | 7;
  if (a.diasNoCartao < 1) a.diasNoCartao = 1;
  if (a.diasNoCartao > 60) a.diasNoCartao = 60;
}

// ---------------------------------------------------------------------------
// Estado
// ---------------------------------------------------------------------------
static const char* nomeFase(Estado e) {
  switch (e) {
    case Estado::AbrindoValvula: return "abrindo";
    case Estado::Regando: return "regando";
    case Estado::Trocando: return "trocando";
    case Estado::FechandoValvula: return "fechando";
    case Estado::Pausa: return "pausa";
    case Estado::Ocioso: break;
  }
  return "parado";
}

void regaParaJson(JsonObject o, const Controlador& c, const Agenda& a, bool temHora, const Momento& agora) {
  if (c.setorAtivo() != 0) {
    JsonObject atual = o["atual"].to<JsonObject>();
    atual["setor"] = c.setorAtivo();
    atual["fase"] = nomeFase(c.estado());
    atual["restanteS"] = c.restanteS();
    atual["duracaoS"] = c.duracaoAtualS();
    atual["bomba"] = c.estado() == Estado::Regando || c.estado() == Estado::Trocando;
  } else {
    o["atual"] = static_cast<const char*>(nullptr);
  }

  JsonArray fila = o["fila"].to<JsonArray>();
  if (c.proximoSetor() != 0) {  // durante a troca, o próximo já saiu da fila
    JsonObject item = fila.add<JsonObject>();
    item["setor"] = c.proximoPedido().setor;
    item["duracaoS"] = c.proximoPedido().duracaoS;
  }
  for (size_t i = 0; i < c.tamanhoFila(); i++) {
    JsonObject item = fila.add<JsonObject>();
    item["setor"] = c.itemFila(i).setor;
    item["duracaoS"] = c.itemFila(i).duracaoS;
  }

  uint8_t programa = 0;
  uint16_t minutoDoDia = 0;
  uint32_t faltam = 0;
  if (temHora && a.proximoInicio(agora, programa, minutoDoDia, faltam)) {
    JsonObject proxima = o["proximaRega"].to<JsonObject>();
    char letra[2] = {static_cast<char>('A' + programa), 0};
    char hora[6];
    textoDeHora(minutoDoDia, hora);
    proxima["programa"] = letra;
    proxima["hora"] = hora;
    static const char* const DIAS[7] = {"dom", "seg", "ter", "qua", "qui", "sex", "sáb"};
    uint32_t hoje = diasDesde2000(agora.ano, agora.mes, agora.dia);
    uint32_t dia = (minutoAbsoluto(agora) + faltam) / 1440UL;
    proxima["quando"] = dia == hoje ? "hoje" : dia == hoje + 1 ? "amanhã" : DIAS[diaDaSemana(dia)];
    proxima["faltamMin"] = faltam;
  } else {
    o["proximaRega"] = static_cast<const char*>(nullptr);
  }
  o["pausaChuvaDias"] = temHora ? a.diasSuspensos(agora) : 0;
}

// ---------------------------------------------------------------------------
// Histórico
// ---------------------------------------------------------------------------
static void medida(JsonArray lista, float v) {
  if (isnan(v)) lista.add(static_cast<const char*>(nullptr));
  else lista.add(roundf(v * 10.0f) / 10.0f);
}

static void eventosParaJson(JsonObject o, const RegistroDia& hoje) {
  JsonArray eventos = o["eventos"].to<JsonArray>();
  for (uint8_t i = 0; i < hoje.totalEventos(); i++) {
    const Evento& e = hoje.eventoRecente(i);
    JsonArray par = eventos.add<JsonArray>();
    char hora[6];
    textoDeHora(e.minutoDoDia, hora);
    par.add(hora);
    par.add(e.texto);
  }
}

static void regasParaJson(JsonObject o, const uint32_t vezes[NUM_SETORES], const uint32_t minutos[NUM_SETORES],
                          const char* const nomes[NUM_SETORES]) {
  JsonArray regas = o["regas"].to<JsonArray>();
  for (uint8_t s = 0; s < NUM_SETORES; s++) {
    if (vezes[s] == 0) continue;
    JsonObject r = regas.add<JsonObject>();
    r["setor"] = s + 1;
    r["nome"] = nomes[s];
    r["vezes"] = vezes[s];
    r["minutos"] = minutos[s];
  }
}

void historicoHojeParaJson(JsonObject o, const RegistroDia& hoje, const char* const nomes[NUM_SETORES]) {
  JsonArray rotulos = o["rotulos"].to<JsonArray>();
  JsonArray chuva = o["chuvaMm"].to<JsonArray>();
  JsonArray temp = o["tempMaxC"].to<JsonArray>();
  for (uint8_t h = 0; h < 24; h++) {
    char rotulo[5];
    snprintf(rotulo, sizeof(rotulo), "%uh", h);
    rotulos.add(rotulo);
    medida(chuva, hoje.chuvaHoraMm(h));
    medida(temp, hoje.tempHoraC(h));
  }
  o["tempMinC"] = static_cast<const char*>(nullptr);
  o["et0Mm"] = static_cast<const char*>(nullptr);
  uint32_t vezes[NUM_SETORES], minutos[NUM_SETORES];
  for (uint8_t s = 0; s < NUM_SETORES; s++) {
    vezes[s] = hoje.vezes(s + 1);
    minutos[s] = (hoje.segundos(s + 1) + 30) / 60;
  }
  regasParaJson(o, vezes, minutos, nomes);
  eventosParaJson(o, hoje);
}

void historicoDiasParaJson(JsonObject o, uint16_t nDias, const ResumoDia* resumos, size_t nResumos, const RegistroDia& hoje,
                           float et0HojeMm, const char* const nomes[NUM_SETORES]) {
  static const char* const DIAS[7] = {"dom", "seg", "ter", "qua", "qui", "sex", "sáb"};
  JsonArray rotulos = o["rotulos"].to<JsonArray>();
  JsonArray chuva = o["chuvaMm"].to<JsonArray>();
  JsonArray maxima = o["tempMaxC"].to<JsonArray>();
  JsonArray minima = o["tempMinC"].to<JsonArray>();
  JsonArray et0 = o["et0Mm"].to<JsonArray>();
  uint32_t vezes[NUM_SETORES] = {}, minutos[NUM_SETORES] = {};
  ResumoDia resumoHoje = hoje.resumo(et0HojeMm);
  if (nDias < 1) nDias = 1;

  for (uint16_t i = 0; i < nDias; i++) {
    uint32_t atras = static_cast<uint32_t>(nDias - 1 - i);
    uint32_t dia = hoje.dia() >= atras ? hoje.dia() - atras : 0;
    const ResumoDia* r = dia == hoje.dia() ? &resumoHoje : nullptr;
    for (size_t k = 0; r == nullptr && k < nResumos; k++) if (resumos[k].dia == dia) r = &resumos[k];

    if (nDias <= 7) {
      rotulos.add(DIAS[diaDaSemana(dia)]);
    } else {
      char rotulo[4];
      snprintf(rotulo, sizeof(rotulo), "%u", diaDoMes(dia));
      rotulos.add(rotulo);
    }
    if (r == nullptr) {  // dia sem registro
      medida(chuva, 0);
      medida(maxima, NAN);
      medida(minima, NAN);
      medida(et0, NAN);
      continue;
    }
    medida(chuva, r->chuvaMm);
    medida(maxima, r->tempMaxC);
    medida(minima, r->tempMinC);
    medida(et0, r->et0Mm);
    for (uint8_t s = 0; s < NUM_SETORES; s++) {
      vezes[s] += r->vezes[s];
      minutos[s] += r->minutos[s];
    }
  }
  regasParaJson(o, vezes, minutos, nomes);
  eventosParaJson(o, hoje);
}

}  // namespace sabia
