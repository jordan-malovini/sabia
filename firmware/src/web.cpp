// SABIÁ - Wi-Fi e servidor da página web.
//
// Entrega a página (embutida no firmware, ver embutir_pagina.py) e responde
// aos endereços de docs/api.md. Roda numa tarefa própria: se a rede travar ou
// um cliente demorar, o loop das válvulas e da bomba continua no ritmo dele.
// Cada pedido fecha a trava só enquanto lê ou altera o estado; a resposta é
// enviada com a trava já aberta.
//
// Rede:
//  - tenta o Wi-Fi da casa. Sem Wi-Fi configurado, ou depois de 20 s sem
//    conseguir conectar, liga a rede própria (AP), com senha (RF16);
//  - a rede própria desliga 1 min depois de o Wi-Fi da casa voltar, a não ser
//    que esteja marcada para ficar sempre ligada.
//
// Segurança (RF16): ler é livre; alterar exige sessão. A senha da página é
// guardada só como hash com sal, e a sessão é um cookie HttpOnly.

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <math.h>
#include <time.h>
#include <mbedtls/md.h>
#if __has_include(<esp_random.h>)
#include <esp_random.h>
#endif

#include "pagina_gz.h"
#include "pinos.h"
#include "sistema.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef WIFI_SSID
#ifdef SABIA_SIMULADOR
#define WIFI_SSID "Wokwi-GUEST"  // rede do simulador, sem senha
#else
#define WIFI_SSID ""
#endif
#define WIFI_PASSWORD ""
#endif
#ifndef AP_PASSWORD
#define AP_PASSWORD "sabia1234"
#endif
#ifndef WEB_PASSWORD
#define WEB_PASSWORD "sabia1234"
#endif

namespace sabia {

static const char* const TIPO_JSON = "application/json";
static const char* const TIPO_TEXTO = "text/plain; charset=utf-8";

static WebServer servidor(80);

// ---------------------------------------------------------------------------
// Rede
// ---------------------------------------------------------------------------
static volatile bool reconfigurarRede = false;
static bool apLigado = false, apSempre = false, temWifiDaCasa = false;
static bool estavaConectado = false;
static uint32_t semConexaoDesdeMs = 0, conectadoDesdeMs = 0, ultimaTentativaMs = 0;
static String nomeWifi, nomeAp;
static uint32_t ultimoNtpMs = 0;  // relógio pela internet (ver acertarPelaInternet)
static bool ntpFeito = false;

static void ligarAp() {
  if (apLigado) return;
  String senha;
  {
    Trava t;
    senha = memoria.getString("apSenha", AP_PASSWORD);
  }
  if (senha.length() < 8) senha = AP_PASSWORD;
  WiFi.mode(temWifiDaCasa ? WIFI_AP_STA : WIFI_AP);
  apLigado = WiFi.softAP(nomeAp.c_str(), senha.c_str());
  if (apLigado) {
    Serial.printf("\r\nRede propria \"%s\" ligada. Pagina: http://%s\r\n", nomeAp.c_str(), WiFi.softAPIP().toString().c_str());
  }
}

static void desligarAp() {
  if (!apLigado) return;
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  apLigado = false;
}

static void redeConfigurar() {
  String senha;
  {
    Trava t;
    JsonDocument doc;
    configCarregar(doc);
    nomeWifi = doc["rede"]["wifiNome"] | "";
    nomeAp = doc["rede"]["apNome"] | "SABIA";
    apSempre = doc["rede"]["apSempre"] | false;
    if (nomeWifi.length() == 0) {  // nada gravado: usa o que veio do secrets.h, se houver
      nomeWifi = WIFI_SSID;
      senha = WIFI_PASSWORD;
    } else {
      senha = memoria.getString("wifiSenha", "");
    }
  }
  if (nomeAp.length() == 0) nomeAp = "SABIA";
  temWifiDaCasa = nomeWifi.length() > 0;

  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);  // as novas tentativas são feitas em redeTick
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true);
  apLigado = false;
  estavaConectado = false;
  semConexaoDesdeMs = ultimaTentativaMs = millis();

  if (temWifiDaCasa) {
    WiFi.mode(WIFI_STA);
    WiFi.setHostname("sabia");
#ifdef SABIA_SIMULADOR
    WiFi.begin(nomeWifi.c_str(), senha.c_str(), 6);  // no Wokwi, dizer o canal acelera a conexão
#else
    WiFi.begin(nomeWifi.c_str(), senha.c_str());
#endif
  }
  if (apSempre || !temWifiDaCasa) ligarAp();
}

static void redeTick() {
  static uint32_t ultimaVezMs = 0;
  if (reconfigurarRede) {
    reconfigurarRede = false;
    delay(300);  // deixa a resposta do pedido sair antes de derrubar a rede
    redeConfigurar();
    return;
  }
  if (millis() - ultimaVezMs < 1000) return;
  ultimaVezMs = millis();
  if (!temWifiDaCasa) return;

  bool conectado = WiFi.status() == WL_CONNECTED;
  if (conectado && !estavaConectado) {
    conectadoDesdeMs = millis();
    ultimoNtpMs = 0;  // acabou de conectar: confere o relógio logo
    MDNS.end();
    if (MDNS.begin("sabia")) MDNS.addService("http", "tcp", 80);
    Serial.printf("\r\nWi-Fi \"%s\" conectado. Pagina: http://%s ou http://sabia.local\r\n", nomeWifi.c_str(),
                  WiFi.localIP().toString().c_str());
  }
  if (!conectado && estavaConectado) semConexaoDesdeMs = ultimaTentativaMs = millis();
  estavaConectado = conectado;

  if (conectado) {
    if (apLigado && !apSempre && millis() - conectadoDesdeMs > 60000UL) desligarAp();
    return;
  }
  if (!apLigado && millis() - semConexaoDesdeMs > 20000UL) ligarAp();
  // Com a rede própria ligada, procurar o Wi-Fi da casa atrapalha quem está
  // conectado nela; por isso as tentativas são espaçadas.
  if (millis() - ultimaTentativaMs > (apLigado ? 60000UL : 10000UL)) {
    ultimaTentativaMs = millis();
    WiFi.reconnect();
  }
}

void webReconfigurarRede() { reconfigurarRede = true; }

void webMostrarRede(Print& saida) {
  if (WiFi.status() == WL_CONNECTED) {
    saida.printf("Wi-Fi \"%s\" conectado. Pagina: http://%s ou http://sabia.local\r\n", nomeWifi.c_str(),
                 WiFi.localIP().toString().c_str());
  } else if (temWifiDaCasa) {
    saida.printf("Wi-Fi \"%s\": sem conexao\r\n", nomeWifi.c_str());
  } else {
    saida.print("Wi-Fi da casa nao configurado (use: wifi NOME SENHA)\r\n");
  }
  if (apLigado) saida.printf("Rede propria \"%s\" ligada. Pagina: http://%s\r\n", nomeAp.c_str(), WiFi.softAPIP().toString().c_str());
}

// ---------------------------------------------------------------------------
// Relógio pela internet (NTP). O relógio (RTC) guarda a hora local; quando o
// Wi-Fi da casa está conectado, ele é conferido na conexão e uma vez por dia,
// e acertado se estiver errado por 2 s ou mais. Sem internet, vale o RTC.
// ---------------------------------------------------------------------------
static void acertarPelaInternet() {
  int16_t fuso;
  {
    Trava t;
    if (!ajustes.ntp || !temRelogio()) return;
    fuso = ajustes.fusoMin;
  }
  ultimoNtpMs = millis();
  configTime(static_cast<long>(fuso) * 60L, 0, "a.st1.ntp.br", "pool.ntp.org");
  struct tm agora;
  if (!getLocalTime(&agora, 8000) || agora.tm_year + 1900 < 2024) {
    Serial.print("\r\nRelogio: sem resposta do servidor de hora\r\n");
    return;
  }
  ntpFeito = true;
  Momento rede;
  rede.ano = static_cast<uint16_t>(agora.tm_year + 1900);
  rede.mes = static_cast<uint8_t>(agora.tm_mon + 1);
  rede.dia = static_cast<uint8_t>(agora.tm_mday);
  rede.hora = static_cast<uint8_t>(agora.tm_hour);
  rede.minuto = static_cast<uint8_t>(agora.tm_min);
  Trava t;
  DadosTela d = lerDados();
  int64_t segRede = static_cast<int64_t>(minutoAbsoluto(rede)) * 60 + agora.tm_sec;
  int64_t segRtc = d.temHora ? static_cast<int64_t>(minutoAbsoluto(d.agora)) * 60 + d.segundo : 0;
  int64_t diferenca = segRede - segRtc;
  if (diferenca < 0) diferenca = -diferenca;
  if (diferenca < 2) return;
  acertarRelogio(rede.ano, rede.mes, rede.dia, rede.hora, rede.minuto, static_cast<uint8_t>(agora.tm_sec));
  char texto[TAMANHO_EVENTO];
  if (diferenca < 3600) snprintf(texto, sizeof(texto), "Relogio acertado pela internet (%lld s)", static_cast<long long>(diferenca));
  else snprintf(texto, sizeof(texto), "Relogio acertado pela internet");
  registrarEvento(texto);
  Serial.printf("\r\n%s\r\n", texto);
}

static void ntpTick() {
  if (WiFi.status() != WL_CONNECTED) return;
  uint32_t intervalo = ntpFeito ? 24UL * 3600000UL : 10UL * 60000UL;  // sem sucesso, tenta a cada 10 min
  if (ultimoNtpMs != 0 && millis() - ultimoNtpMs < intervalo) return;
  acertarPelaInternet();
}

// ---------------------------------------------------------------------------
// Senha da página e sessões
// ---------------------------------------------------------------------------
constexpr size_t TAMANHO_SAL = 16, TAMANHO_HASH = 32, TAMANHO_TOKEN = 32;
constexpr uint8_t MAX_SESSOES = 4;

struct Sessao {
  bool usada = false;
  char token[TAMANHO_TOKEN + 1] = "";
  uint32_t ultimoUsoMs = 0;
};
static Sessao sessoes[MAX_SESSOES];
static uint32_t ultimaFalhaMs = 0;
static bool houveFalha = false;

static bool iguais(const uint8_t* a, const uint8_t* b, size_t n) {  // em tempo constante
  uint8_t diferenca = 0;
  for (size_t i = 0; i < n; i++) diferenca |= a[i] ^ b[i];
  return diferenca == 0;
}

static void calcularHash(const uint8_t sal[TAMANHO_SAL], const char* senha, uint8_t saida[TAMANHO_HASH]) {
  size_t n = strlen(senha);
  if (n > SENHA_MAX) n = SENHA_MAX;
  uint8_t entrada[TAMANHO_SAL + SENHA_MAX];
  memcpy(entrada, sal, TAMANHO_SAL);
  memcpy(entrada + TAMANHO_SAL, senha, n);
  mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), entrada, TAMANHO_SAL + n, saida);
}

// Chamar com a trava fechada.
static bool senhaGravada() { return memoria.getBytesLength("senhaHash") == TAMANHO_HASH && memoria.getBytesLength("senhaSal") == TAMANHO_SAL; }

static bool senhaConfere(const char* senha) {
  if (!senhaGravada()) {
    size_t n = strlen(senha), m = strlen(WEB_PASSWORD);
    return n == m && iguais(reinterpret_cast<const uint8_t*>(senha), reinterpret_cast<const uint8_t*>(WEB_PASSWORD), n);
  }
  uint8_t sal[TAMANHO_SAL], gravado[TAMANHO_HASH], calculado[TAMANHO_HASH];
  memoria.getBytes("senhaSal", sal, TAMANHO_SAL);
  memoria.getBytes("senhaHash", gravado, TAMANHO_HASH);
  calcularHash(sal, senha, calculado);
  return iguais(gravado, calculado, TAMANHO_HASH);
}

static void gravarSenha(const char* senha) {
  uint8_t sal[TAMANHO_SAL], hash[TAMANHO_HASH];
  esp_fill_random(sal, TAMANHO_SAL);
  calcularHash(sal, senha, hash);
  memoria.putBytes("senhaSal", sal, TAMANHO_SAL);
  memoria.putBytes("senhaHash", hash, TAMANHO_HASH);
}

void webRestaurarSenhaPadrao() {
  memoria.remove("senhaSal");
  memoria.remove("senhaHash");
  for (Sessao& s : sessoes) s.usada = false;
}

static String tokenDoPedido() {
  String cookie = servidor.header("Cookie");
  int inicio = cookie.indexOf("sabia=");
  if (inicio < 0) return String();
  inicio += 6;
  int fim = cookie.indexOf(';', inicio);
  return fim < 0 ? cookie.substring(inicio) : cookie.substring(inicio, fim);
}

static Sessao* sessaoAtual() {
  String token = tokenDoPedido();
  uint32_t validadeMs = static_cast<uint32_t>(ajustes.sessaoMin) * 60000UL;
  Sessao* achada = nullptr;
  for (Sessao& s : sessoes) {
    if (!s.usada) continue;
    if (millis() - s.ultimoUsoMs > validadeMs) {
      s.usada = false;
      continue;
    }
    if (token.length() == TAMANHO_TOKEN &&
        iguais(reinterpret_cast<const uint8_t*>(token.c_str()), reinterpret_cast<const uint8_t*>(s.token), TAMANHO_TOKEN)) {
      achada = &s;
    }
  }
  if (achada != nullptr) achada->ultimoUsoMs = millis();
  return achada;
}

static void responderErro(int codigo, const char* motivo) { servidor.send(codigo, TIPO_TEXTO, motivo); }
static void responderJson(const String& corpo) {
  servidor.sendHeader("Cache-Control", "no-store");
  servidor.send(200, TIPO_JSON, corpo);
}
static void responderOk() { responderJson("{\"ok\":true}"); }

static bool exigirSessao() {
  if (sessaoAtual() != nullptr) return true;
  responderErro(401, "Entre para continuar");
  return false;
}

static bool lerCorpo(JsonDocument& doc) {
  if (!servidor.hasArg("plain")) {
    responderErro(400, "Pedido sem dados");
    return false;
  }
  String corpo = servidor.arg("plain");
  if (corpo.length() > TAMANHO_MAX_CORPO) {
    responderErro(413, "Dados grandes demais");
    return false;
  }
  if (deserializeJson(doc, corpo) != DeserializationError::Ok) {
    responderErro(400, "Dados em formato inválido");
    return false;
  }
  return true;
}

static void tratarEntrar() {
  JsonDocument doc;
  if (!lerCorpo(doc)) return;
  if (houveFalha && millis() - ultimaFalhaMs < 2000UL) {  // freia tentativas em série
    responderErro(429, "Aguarde um instante e tente de novo");
    return;
  }
  const char* senha = doc["senha"] | "";
  bool confere;
  {
    Trava t;
    confere = senhaConfere(senha);
  }
  if (!confere) {
    houveFalha = true;
    ultimaFalhaMs = millis();
    responderErro(401, "Senha incorreta");
    return;
  }
  houveFalha = false;

  Sessao* livre = &sessoes[0];  // sem vaga, a sessão parada há mais tempo dá lugar
  for (Sessao& s : sessoes) {
    if (!s.usada) { livre = &s; break; }
    if (millis() - s.ultimoUsoMs > millis() - livre->ultimoUsoMs) livre = &s;
  }
  uint8_t aleatorio[TAMANHO_TOKEN / 2];
  esp_fill_random(aleatorio, sizeof(aleatorio));
  for (size_t i = 0; i < sizeof(aleatorio); i++) snprintf(livre->token + i * 2, 3, "%02x", aleatorio[i]);
  livre->usada = true;
  livre->ultimoUsoMs = millis();

  servidor.sendHeader("Set-Cookie", String("sabia=") + livre->token + "; Path=/; HttpOnly; SameSite=Strict");
  responderOk();
}

static void tratarSair() {
  Sessao* s = sessaoAtual();
  if (s != nullptr) s->usada = false;
  servidor.sendHeader("Set-Cookie", "sabia=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0");
  responderOk();
}

// ---------------------------------------------------------------------------
// Estado
// ---------------------------------------------------------------------------
static void tratarEstado() {
  bool logado = sessaoAtual() != nullptr;
  String corpo;
  {
    Trava t;
    JsonDocument doc;
    DadosTela d = lerDados();
    doc["versao"] = SABIA_VERSAO;
    if (d.temHora) {
      char hora[32];
      int fuso = ajustes.fusoMin < 0 ? -ajustes.fusoMin : ajustes.fusoMin;
      snprintf(hora, sizeof(hora), "%04u-%02u-%02uT%02u:%02u:%02u%c%02d:%02d", d.agora.ano, d.agora.mes, d.agora.dia, d.hora, d.minuto,
               d.segundo, ajustes.fusoMin < 0 ? '-' : '+', fuso / 60, fuso % 60);
      doc["hora"] = hora;
    } else {
      doc["hora"] = static_cast<const char*>(nullptr);
    }

    JsonObject sistema = doc["sistema"].to<JsonObject>();
    bool conectado = WiFi.status() == WL_CONNECTED;
    sistema["wifi"] = conectado;
    sistema["ip"] = conectado ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
    sistema["modoAp"] = apLigado && !conectado;
    JsonObject cartao = sistema["cartaoSd"].to<JsonObject>();
    cartao["ok"] = registroTemArmazenamento();
    cartao["naFlash"] = !registroNoCartao();  // sem cartão, o registro vai para a memória interna
    if (registroUsoPct() >= 0) cartao["usoPct"] = registroUsoPct();
    else cartao["usoPct"] = static_cast<const char*>(nullptr);
    JsonObject drive = sistema["drive"].to<JsonObject>();
    drive["ativo"] = ajustes.drive;
    if (driveUltimoDiaEnviado() != 0) {
      char data[11];
      dataDeDias(driveUltimoDiaEnviado(), data);
      drive["ultimoEnvio"] = data;
    } else {
      drive["ultimoEnvio"] = static_cast<const char*>(nullptr);
    }
    if (driveUltimoErro()[0] != 0) drive["erro"] = driveUltimoErro();
    else drive["erro"] = static_cast<const char*>(nullptr);
    sistema["rtc"] = temRelogio();
    sistema["ligadoHa"] = static_cast<uint32_t>(esp_timer_get_time() / 1000000LL);

    JsonObject sessao = doc["sessao"].to<JsonObject>();
    sessao["logado"] = logado;
    sessao["senhaPadrao"] = !senhaGravada();

    regaParaJson(doc.as<JsonObject>(), controlador, agenda, d.temHora, d.agora);

    if (retomada.aguardando()) {
      JsonObject r = doc["retomada"].to<JsonObject>();
      r["faltamS"] = retomada.faltamS(millis());
      r["setor"] = retomada.primeiroSetor();
      r["setores"] = retomada.totalSetores();
      r["semEnergiaMin"] = retomada.minutosSemEnergia();
    } else {
      doc["retomada"] = static_cast<const char*>(nullptr);
    }

    // Sem sensor (ou sem leitura ainda), o campo vai vazio. A ET0 vem com o balanço hídrico.
    JsonObject clima = doc["clima"].to<JsonObject>();
    const Leitura& leitura = sensoresLeitura();
    const RegistroDia& hoje = registroHoje();
    auto medida = [&clima](const char* campo, float v, int casas) {
      if (isnan(v)) clima[campo] = static_cast<const char*>(nullptr);
      else clima[campo] = serialized(String(v, static_cast<unsigned int>(casas)));
    };
    medida("chuvaHojeMm", d.chuvaMm, 1);
    medida("chuvaMesMm", d.temHora ? registroChuvaMesMm() : NAN, 1);
    medida("tempC", leitura.tempC, 1);
    medida("tempMaxC", hoje.tempMaxC(), 1);
    medida("tempMinC", hoje.tempMinC(), 1);
    medida("umidadePct", leitura.umidadePct, 0);
    medida("pressaoHpa", leitura.pressaoHpa, 1);
    medida("lux", leitura.lux, 0);
    medida("et0HojeMm", balancoEt0Hoje(), 1);
    medida("chuva24hMm", balancoChuva24hMm(), 1);
    doc["adiandoPorChuva"] = balancoAdiando();
    doc["balancoAtivo"] = ajustes.balancoHidrico && balancoConfiavel();

    JsonArray setores = doc["setores"].to<JsonArray>();
    for (uint8_t i = 0; i < NUM_SETORES; i++) {
      JsonObject s = setores.add<JsonObject>();
      // Sem um dia fechado com temperatura e latitude, o saldo não quer dizer nada.
      if (balancoConfiavel()) s["saldoPct"] = balancoSaldoPct(i + 1);
      else s["saldoPct"] = static_cast<const char*>(nullptr);
      s["adiadoAte"] = static_cast<const char*>(nullptr);
    }
    serializeJson(doc, corpo);
  }
  responderJson(corpo);
}

// ---------------------------------------------------------------------------
// Comandos
// ---------------------------------------------------------------------------
static const char* motivo(Resultado r) {
  switch (r) {
    case Resultado::Aceito: return nullptr;
    case Resultado::SetorInvalido: return "Setor inválido";
    case Resultado::DuracaoInvalida: return "Duração inválida";
    case Resultado::JaNaFila: return "Esse setor já está regando ou na fila";
    case Resultado::FilaCheia: return "A fila está cheia";
    case Resultado::SetorDesativado: return "Esse setor está desativado nos ajustes";
  }
  return "Não foi possível";
}

// Executa o comando com a trava fechada. Devolve nullptr ou o motivo da recusa.
static const char* executarComando(JsonVariantConst c) {
  const char* acao = c["acao"] | "";
  long setor = c["setor"] | 0L;

  if (strcmp(acao, "ligar") == 0) {
    long minutos = c["minutos"] | 0L;
    if (setor < 1 || setor > NUM_SETORES) return "Setor inválido";
    if (minutos < 1 || minutos > 240) return "O tempo deve ser de 1 a 240 min";
    return motivo(controlador.pedir(static_cast<uint8_t>(setor), static_cast<uint32_t>(minutos) * 60UL));
  }
  if (strcmp(acao, "parar") == 0) {
    if (setor < 1 || setor > NUM_SETORES) return "Setor inválido";
    return controlador.parar(static_cast<uint8_t>(setor)) ? nullptr : "Esse setor não está regando nem na fila";
  }
  if (strcmp(acao, "pular") == 0) {
    controlador.pular();
    return nullptr;
  }
  if (strcmp(acao, "pararTudo") == 0) {
    controlador.pararTudo();
    agenda.cancelarPendentes();
    if (retomada.aguardando()) retomada.cancelar();
    return nullptr;
  }
  if (strcmp(acao, "ciclo") == 0) {
    uint32_t duracoes[NUM_SETORES];
    for (uint8_t i = 0; i < NUM_SETORES; i++) duracoes[i] = ajustes.duracaoPadraoMin[i] * 60UL;
    return controlador.iniciarCiclo(duracoes) > 0 ? nullptr : "Nenhum setor entrou na fila";
  }
  if (strcmp(acao, "programa") == 0) {
    long programa = c["programa"] | -1L;
    if (programa < 0 || programa >= NUM_PROGRAMAS) return "Programa inválido";
    return agenda.iniciarPrograma(static_cast<uint8_t>(programa)) ? nullptr : "Esse programa não tem setores com tempo";
  }
  if (strcmp(acao, "chuva") == 0) {
    long dias = c["dias"] | -1L;
    DadosTela d = lerDados();
    if (dias < 0 || dias > SUSPENSAO_MAX_DIAS) return "A pausa deve ser de 0 a 14 dias";
    if (!d.temHora) return "Sem relógio, a agenda não roda";
    agenda.suspender(static_cast<uint8_t>(dias), d.agora);
    salvarAgenda();
    char texto[TAMANHO_EVENTO];
    if (dias > 0) snprintf(texto, sizeof(texto), "Regas em pausa por chuva: %ld dia(s)", dias);
    else snprintf(texto, sizeof(texto), "Pausa por chuva cancelada");
    registrarEvento(texto);
    return nullptr;
  }
  if (strcmp(acao, "acertarHora") == 0) {
    // Hora local do aparelho que abriu a página: "AAAA-MM-DDTHH:MM:SS".
    int ano = 0, mes = 0, dia = 0, hora = 0, minuto = 0, segundo = 0;
    const char* texto = c["hora"] | "";
    if (sscanf(texto, "%d-%d-%dT%d:%d:%d", &ano, &mes, &dia, &hora, &minuto, &segundo) < 5) return "Hora em formato inválido";
    if (ano < 0 || ano > 9999 || mes < 0 || mes > 99 || dia < 0 || dia > 99 || hora < 0 || hora > 99 || minuto < 0 || minuto > 99 ||
        segundo < 0 || segundo > 99) {
      return "Hora em formato inválido";
    }
    if (!temRelogio()) return "Relógio (RTC) não encontrado";
    if (!acertarRelogio(static_cast<uint16_t>(ano), static_cast<uint8_t>(mes), static_cast<uint8_t>(dia), static_cast<uint8_t>(hora),
                        static_cast<uint8_t>(minuto), static_cast<uint8_t>(segundo))) {
      return "Hora fora da faixa";
    }
    registrarEvento("Hora acertada pela pagina");
    return nullptr;
  }
  if (strcmp(acao, "retomar") == 0 || strcmp(acao, "cancelarRetomada") == 0) {
    if (!retomada.aguardando()) return "Não há rega esperando para retomar";
    if (acao[0] == 'r') retomada.retomarAgora();
    else retomada.cancelar();
    return nullptr;
  }
  if (strcmp(acao, "reiniciar") == 0) {
    pedirReinicio();
    return nullptr;
  }
  return "Comando desconhecido";
}

static void tratarComando() {
  if (!exigirSessao()) return;
  JsonDocument doc;
  if (!lerCorpo(doc)) return;
  const char* erro;
  {
    Trava t;
    erro = executarComando(doc.as<JsonVariantConst>());
  }
  if (erro != nullptr) responderErro(400, erro);
  else responderOk();
}

// ---------------------------------------------------------------------------
// Configuração
// ---------------------------------------------------------------------------
static void tratarLerConfig() {
  String corpo;
  {
    Trava t;
    JsonDocument doc;
    configCarregar(doc);
    JsonArray programas = doc["programas"].to<JsonArray>();
    for (uint8_t i = 0; i < NUM_PROGRAMAS; i++) programaParaJson(agenda.programa(i), programas.add<JsonObject>());
    serializeJson(doc, corpo);
  }
  responderJson(corpo);
}

static void tratarGravarConfig() {
  if (!exigirSessao()) return;
  JsonDocument doc;
  if (!lerCorpo(doc)) return;
  String erro;
  bool redeMudou = false;
  {
    Trava t;
    const char* invalido = validarConfig(doc.as<JsonVariantConst>());
    if (invalido != nullptr) {
      erro = invalido;
    } else {
      for (uint8_t i = 0; i < NUM_PROGRAMAS; i++) programaDeJson(doc["programas"][i].as<JsonVariantConst>(), agenda.programa(i));
      salvarAgenda();

      JsonDocument antiga;
      configCarregar(antiga);
      const char* wifiSenha = doc["rede"]["wifiSenha"] | "";
      const char* apSenha = doc["rede"]["apSenha"] | "";
      const char* novaSenha = doc["seguranca"]["novaSenha"] | "";
      redeMudou = strcmp(doc["rede"]["wifiNome"] | "", antiga["rede"]["wifiNome"] | "") != 0 ||
                  strcmp(doc["rede"]["apNome"] | "", antiga["rede"]["apNome"] | "") != 0 ||
                  (doc["rede"]["apSempre"] | false) != (antiga["rede"]["apSempre"] | false) || wifiSenha[0] != 0 || apSenha[0] != 0;
      if (wifiSenha[0] != 0) memoria.putString("wifiSenha", wifiSenha);
      if (apSenha[0] != 0) memoria.putString("apSenha", apSenha);
      if (novaSenha[0] != 0) gravarSenha(novaSenha);

      if (!configGravar(doc)) {
        erro = "Não foi possível gravar os ajustes";
      } else {
        configAplicar(doc.as<JsonVariantConst>());
        registrarEvento("Ajustes salvos pela pagina");
      }
    }
  }
  if (erro.length() > 0) {
    responderErro(400, erro.c_str());
    return;
  }
  responderOk();
  if (redeMudou) webReconfigurarRede();
}

// ---------------------------------------------------------------------------
// Histórico
// ---------------------------------------------------------------------------
// Monta o histórico do período pedido. Devolve false sem relógio (sem data não há registro).
static bool montarHistorico(JsonDocument& doc) {
  Trava t;
  JsonDocument config;
  configCarregar(config);
  const char* nomes[NUM_SETORES];
  for (uint8_t i = 0; i < NUM_SETORES; i++) nomes[i] = config["setores"][i]["nome"] | "Setor";
  String periodo = servidor.arg("periodo");
  return registroHistorico(periodo.c_str(), doc.to<JsonObject>(), nomes);
}

static void tratarHistorico() {
  JsonDocument doc;
  if (!montarHistorico(doc)) {
    responderJson("{\"rotulos\":[],\"chuvaMm\":[],\"tempMaxC\":[],\"tempMinC\":[],\"et0Mm\":[],\"regas\":[],\"eventos\":[]}");
    return;
  }
  String corpo;
  serializeJson(doc, corpo);
  responderJson(corpo);
}

// Número para o CSV: vírgula decimal, vazio se não há dado.
static String numeroCsv(JsonVariantConst v) {
  if (v.isNull()) return String();
  String texto(v.as<float>(), static_cast<unsigned int>(1));
  texto.replace('.', ',');
  return texto;
}

static void tratarCsv() {
  JsonDocument doc;
  if (!montarHistorico(doc)) {
    responderErro(404, "Sem relógio não há registro para baixar");
    return;
  }
  String tipo = servidor.arg("tipo"), periodo = servidor.arg("periodo");
  if (tipo != "chuva" && tipo != "clima" && tipo != "regas") tipo = "chuva";
  if (periodo != "7d" && periodo != "30d") periodo = "hoje";
  String csv;
  JsonArrayConst rotulos = doc["rotulos"].as<JsonArrayConst>();
  if (tipo == "regas") {
    csv = "setor;nome;regas;minutos\r\n";
    for (JsonVariantConst r : doc["regas"].as<JsonArrayConst>()) {
      csv += String(r["setor"].as<int>()) + ';' + (r["nome"] | "") + ';' + String(r["vezes"].as<int>()) + ';' + String(r["minutos"].as<int>()) + "\r\n";
    }
  } else if (tipo == "chuva") {
    csv = "periodo;chuva_mm\r\n";
    for (size_t i = 0; i < rotulos.size(); i++) csv += String(rotulos[i] | "") + ';' + numeroCsv(doc["chuvaMm"][i]) + "\r\n";
  } else {
    csv = "periodo;temp_max_c;temp_min_c;et0_mm\r\n";
    for (size_t i = 0; i < rotulos.size(); i++) {
      csv += String(rotulos[i] | "") + ';' + numeroCsv(doc["tempMaxC"][i]) + ';' + numeroCsv(doc["tempMinC"][i]) + ';' + numeroCsv(doc["et0Mm"][i]) + "\r\n";
    }
  }
  servidor.sendHeader("Content-Disposition", String("attachment; filename=\"sabia-") + tipo + "-" + periodo + ".csv\"");
  servidor.sendHeader("Cache-Control", "no-store");
  servidor.send(200, "text/csv; charset=utf-8", String("\xEF\xBB\xBF") + csv);  // com BOM, para o Excel ler os acentos
}

static void tratarPagina() {
  servidor.sendHeader("Content-Encoding", "gzip");
  servidor.sendHeader("Cache-Control", "no-cache");
  servidor.send_P(200, "text/html; charset=utf-8", reinterpret_cast<const char*>(PAGINA_GZ), PAGINA_GZ_TAMANHO);
}

static void tarefaWeb(void*) {
  redeConfigurar();

  static const char* cabecalhos[] = {"Cookie"};
  servidor.collectHeaders(cabecalhos, 1);
  servidor.on("/", HTTP_GET, tratarPagina);
  servidor.on("/index.html", HTTP_GET, tratarPagina);
  servidor.on("/api/estado", HTTP_GET, tratarEstado);
  servidor.on("/api/sessao", HTTP_POST, tratarEntrar);
  servidor.on("/api/sessao", HTTP_DELETE, tratarSair);
  servidor.on("/api/comando", HTTP_POST, tratarComando);
  servidor.on("/api/config", HTTP_GET, tratarLerConfig);
  servidor.on("/api/config", HTTP_PUT, tratarGravarConfig);
  servidor.on("/api/historico", HTTP_GET, tratarHistorico);
  servidor.on("/api/csv", HTTP_GET, tratarCsv);
  servidor.onNotFound([]() { responderErro(404, "Endereço não encontrado"); });
  servidor.begin();

  for (;;) {
    servidor.handleClient();
    redeTick();
    ntpTick();
    driveTick();
    delay(2);
  }
}

void webIniciar() { xTaskCreatePinnedToCore(tarefaWeb, "web", 16384, nullptr, 1, nullptr, ARDUINO_RUNNING_CORE); }

}  // namespace sabia
