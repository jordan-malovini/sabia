// SABIÁ - firmware principal
//
// Fila de setores e sequência válvula/bomba, com tela, encoder, relógio e
// pluviômetro. Também aceita comandos pelo monitor serial (115200):
//   l        liga um setor em duas etapas: pergunta o setor e depois o tempo
//   l N S    atalho: liga o setor N por S segundos
//   c S      ciclo: todos os setores, S segundos cada
//   p        pula o setor atual
//   x N      para o setor N (ou tira da fila)
//   s        para tudo
//   ?        mostra o estado
// Agenda (programas A, B e C):
//   a                  mostra a agenda
//   g A on|off         liga ou desliga o programa
//   g A h 1 06:00      horário de início 1 a 4 ("-" apaga)
//   g A d 0101010      dias, de domingo a sábado; ou: impar | par | int 3
//   g A t 3 25         setor 3 por 25 min (0 tira do programa; * = todos)
//   g A % 80           ajuste de 5 a 200 %
//   g A run            inicia o programa agora
//   chuva 3            não rega por 3 dias (0 cancela)
//   hora 2026-11-12 19:55   acerta o relógio
// Queda de energia:
//   retomar | cancelar   durante a espera depois que a energia volta
//   reiniciar            reinicia o ESP (para testar a retomada)
// Rede e página web:
//   wifi                 mostra a rede e o endereço da página
//   wifi NOME SENHA      troca o Wi-Fi da casa (nome sem espaços)
//   senha padrao         volta a senha da página para a de fábrica

#include <Arduino.h>
#include <Preferences.h>
#include <RTClib.h>
#include <SPI.h>
#include <Wire.h>
#include <driver/gpio.h>
#include <esp_idf_version.h>
#include <esp_system.h>
#include <esp_task_wdt.h>

#include "Agenda.h"
#include "Controlador.h"
#include "Retomada.h"
#include "interface.h"
#include "pinos.h"
#include "sistema.h"

using namespace sabia;

class SaidasReles : public Saidas {
 public:
  void valvula(uint8_t setor, bool aberta) override {
    if (setor < 1 || setor > NUM_SETORES) return;
    escrever(PINOS_SETORES[setor - 1], aberta);
  }
  // Com "usar bomba" desligado nos ajustes, o relé da bomba nunca liga.
  void bomba(bool ligada) override { escrever(PINO_BOMBA, ligada && ajustes.usarBomba); }

 private:
  static void escrever(uint8_t pino, bool ligado) {
    digitalWrite(pino, ligado ? RELE_LIGADO : RELE_DESLIGADO);
  }
};

static SaidasReles saidas;
Ajustes sabia::ajustes;
Controlador sabia::controlador(saidas);
Agenda sabia::agenda(sabia::controlador);
Retomada sabia::retomada;
Preferences sabia::memoria;

// Trava entre o loop e a tarefa do servidor web (ver sistema.h).
static SemaphoreHandle_t trava = nullptr;
void sabia::travar() { if (trava != nullptr) xSemaphoreTake(trava, portMAX_DELAY); }
void sabia::destravar() { if (trava != nullptr) xSemaphoreGive(trava); }

static uint32_t reiniciarEmMs = 0;
void sabia::pedirReinicio() { reiniciarEmMs = millis() + 800; if (reiniciarEmMs == 0) reiniciarEmMs = 1; }
static PontoSalvo ultimoPonto;     // o que está gravado na memória
static uint32_t ultimoPontoMs = 0;

#ifdef SABIA_SIMULADOR
static RTC_DS1307 rtc;  // o Wokwi só tem o DS1307
#else
static RTC_DS3231 rtc;
#endif
static bool temRtc = false;

// Pluviômetro: cada basculada fecha o reed e gera um pulso.
static volatile uint32_t pulsosChuva = 0;
static volatile uint32_t ultimoPulsoMs = 0;
static uint32_t pulsosInicioDoDia = 0;
static uint8_t diaAtual = 0;

static void IRAM_ATTR isrPluviometro() {
  uint32_t agora = millis();
  if (agora - ultimoPulsoMs < 80) return;  // filtro de repique do reed
  ultimoPulsoMs = agora;
  pulsosChuva = pulsosChuva + 1;
}

// RF14: o nível de "desligado" vai para o registrador antes de o pino virar
// saída, para nenhum relé pulsar no boot.
static void configurarSaidaSegura(uint8_t pino) {
  gpio_set_level(static_cast<gpio_num_t>(pino), RELE_DESLIGADO);
  pinMode(pino, OUTPUT);
}

DadosTela sabia::lerDados() {
  // O relógio é lido no máximo 4 vezes por segundo, para não ocupar o I2C.
  static DadosTela d;
  static uint32_t ultimaLeituraMs = 0;
  bool lerRelogio = temRtc && (!d.temHora || millis() - ultimaLeituraMs >= 250);
  if (lerRelogio) {
    ultimaLeituraMs = millis();
    DateTime agora = rtc.now();
    d.temHora = true;
    d.hora = agora.hour();
    d.minuto = agora.minute();
    d.segundo = agora.second();
    d.dia = agora.day();
    d.mes = agora.month();
    d.agora.ano = agora.year();
    d.agora.mes = d.mes;
    d.agora.dia = d.dia;
    d.agora.hora = d.hora;
    d.agora.minuto = d.minuto;
    if (d.dia != diaAtual) {  // virou o dia: zera a chuva de hoje
      diaAtual = d.dia;
      pulsosInicioDoDia = pulsosChuva;
    }
  }
  // Com relógio, a chuva de hoje vem do registro (que sobrevive a um reinício);
  // sem relógio, é a contagem desde que o controlador ligou.
  d.chuvaMm = d.temHora ? registroChuvaHojeMm() : (pulsosChuva - pulsosInicioDoDia) * ajustes.mmPorBasculada;
  return d;
}

bool sabia::temRelogio() { return temRtc; }

bool sabia::acertarRelogio(uint16_t ano, uint8_t mes, uint8_t dia, uint8_t hora, uint8_t minuto, uint8_t segundo) {
  if (!temRtc || ano < 2024 || ano > 2099 || mes < 1 || mes > 12 || dia < 1 || dia > 31 || hora > 23 || minuto > 59 || segundo > 59) return false;
  rtc.adjust(DateTime(ano, mes, dia, hora, minuto, segundo));
  return true;
}

static void mostrarEstado() {
  DadosTela d = lerDados();
  if (d.temHora) Serial.printf("[%02u:%02u] ", d.hora, d.minuto);
  else Serial.printf("[%lu s] ", millis() / 1000UL);
  Serial.print(nomeEstado(controlador.estado()));
  if (controlador.setorAtivo() != 0) {
    Serial.printf(" | setor %u, faltam %lu s", controlador.setorAtivo(),
                  static_cast<unsigned long>(controlador.restanteS()));
  }
  Serial.print(" | fila:");
  if (controlador.tamanhoFila() == 0) Serial.print(" vazia");
  for (size_t i = 0; i < controlador.tamanhoFila(); i++) {
    Pedido p = controlador.itemFila(i);
    Serial.printf(" %u(%lus)", p.setor, static_cast<unsigned long>(p.duracaoS));
  }
  Serial.printf(" | chuva %.1f mm\r\n", d.chuvaMm);
}

// ---------------------------------------------------------------------------
// Agenda: gravação na memória do ESP (NVS) e comandos
// ---------------------------------------------------------------------------
constexpr uint8_t VERSAO_AGENDA = 1;

void sabia::salvarAgenda() {
  memoria.putUChar("ver", VERSAO_AGENDA);
  memoria.putBytes("prog", agenda.programas(), sizeof(Programa) * NUM_PROGRAMAS);
  memoria.putULong("susp", agenda.suspensoAteDia());
}

static void carregarAgenda() {
  bool valida = memoria.getUChar("ver", 0) == VERSAO_AGENDA &&
                memoria.getBytesLength("prog") == sizeof(Programa) * NUM_PROGRAMAS;
  if (valida) {
    memoria.getBytes("prog", agenda.programas(), sizeof(Programa) * NUM_PROGRAMAS);
    agenda.definirSuspensoAteDia(memoria.getULong("susp", 0));
    return;
  }
  // Primeira vez: programa A preenchido como no controlador antigo, mas desligado.
  Programa& a = agenda.programa(0);
  a.inicios[0] = 6 * 60;
  for (auto& m : a.minutosSetor) m = 25;
  salvarAgenda();
}

static void mostrarAgenda() {
  DadosTela d = lerDados();
  for (uint8_t i = 0; i < NUM_PROGRAMAS; i++) {
    const Programa& p = agenda.programa(i);
    Serial.printf("%c [%s] inicios:", 'A' + i, p.ativo ? "ON " : "off");
    bool algum = false;
    for (uint16_t ini : p.inicios) {
      if (ini == SEM_HORARIO) continue;
      Serial.printf(" %02u:%02u", ini / 60, ini % 60);
      algum = true;
    }
    if (!algum) Serial.print(" -");
    Serial.print(" | dias: ");
    if (p.modo == ModoDias::Semana) {
      for (uint8_t dia = 0; dia < 7; dia++) Serial.print((p.diasSemana >> dia) & 1 ? "DSTQQSS"[dia] : '.');
    } else if (p.modo == ModoDias::Impares) Serial.print("impares");
    else if (p.modo == ModoDias::Pares) Serial.print("pares");
    else Serial.printf("a cada %u", p.intervaloDias);
    uint32_t total = agenda.duracaoTotalS(i);
    Serial.printf(" | ajuste %u%% | total %luh%02lu\r\n   setores (min):", p.ajustePct,
                  static_cast<unsigned long>(total / 3600), static_cast<unsigned long>(total % 3600 / 60));
    for (uint8_t st = 0; st < NUM_SETORES; st++) Serial.printf(" %u=%u", st + 1, p.minutosSetor[st]);
    Serial.print("\r\n");
  }
  if (!d.temHora) { Serial.print("Sem relogio: a agenda nao roda.\r\n"); return; }
  uint8_t prog = 0, susp = agenda.diasSuspensos(d.agora);
  uint16_t minuto = 0;
  uint32_t faltam = 0;
  if (susp > 0) Serial.printf("Pausa por chuva: %u dia(s)\r\n", susp);
  if (agenda.proximoInicio(d.agora, prog, minuto, faltam))
    Serial.printf("Proxima rega: programa %c as %02u:%02u (faltam %luh%02lu)\r\n", 'A' + prog, minuto / 60, minuto % 60,
                  static_cast<unsigned long>(faltam / 60), static_cast<unsigned long>(faltam % 60));
  else Serial.print("Nenhuma rega agendada.\r\n");
}

// g <A|B|C> <on|off|h|d|t|%|run> [valores]
static void comandoAgenda(const String& linha) {
  char letra = 0, sub[8] = "", v1[12] = "", v2[12] = "";
  int n = sscanf(linha.c_str(), "g %c %7s %11s %11s", &letra, sub, v1, v2);
  int i = toupper(letra) - 'A';
  if (n < 2 || i < 0 || i >= NUM_PROGRAMAS) { Serial.print("Uso: g A on|off|h|d|t|%|run ...\r\n"); return; }
  Programa& p = agenda.programa(i);
  String s(sub);
  bool ok = true;
  if (s == "on") p.ativo = true;
  else if (s == "off") p.ativo = false;
  else if (s == "run") {
    Serial.print(agenda.iniciarPrograma(i) ? "Programa iniciado\r\n" : "Programa sem setores\r\n");
    return;
  } else if (s == "h" && n >= 4) {
    int slot = atoi(v1), hh = 0, mm = 0;
    if (slot < 1 || slot > INICIOS_POR_PROGRAMA) ok = false;
    else if (v2[0] == '-') p.inicios[slot - 1] = SEM_HORARIO;
    else if (sscanf(v2, "%d:%d", &hh, &mm) == 2 && hh >= 0 && hh < 24 && mm >= 0 && mm < 60) p.inicios[slot - 1] = hh * 60 + mm;
    else ok = false;
  } else if (s == "d" && n >= 3) {
    String d(v1);
    if (d == "impar") p.modo = ModoDias::Impares;
    else if (d == "par") p.modo = ModoDias::Pares;
    else if (d == "int" && n >= 4 && atoi(v2) >= 1 && atoi(v2) <= 30) {
      DadosTela agora = lerDados();
      p.modo = ModoDias::Intervalo;
      p.intervaloDias = atoi(v2);
      p.diaReferencia = diasDesde2000(agora.agora.ano, agora.agora.mes, agora.agora.dia);
    } else if (d.length() == 7) {
      uint8_t mascara = 0;
      for (uint8_t k = 0; k < 7; k++) if (d[k] != '0' && d[k] != '.') mascara |= 1 << k;
      p.modo = ModoDias::Semana;
      p.diasSemana = mascara;
    } else ok = false;
  } else if (s == "t" && n >= 4) {
    int minutos = atoi(v2);
    if (minutos < 0 || minutos > MINUTOS_MAX_SETOR) ok = false;
    else if (v1[0] == '*') { for (auto& m : p.minutosSetor) m = minutos; }
    else if (atoi(v1) >= 1 && atoi(v1) <= NUM_SETORES) p.minutosSetor[atoi(v1) - 1] = minutos;
    else ok = false;
  } else if (s == "%" && n >= 3 && atoi(v1) >= AJUSTE_MIN && atoi(v1) <= AJUSTE_MAX) {
    p.ajustePct = atoi(v1);
  } else ok = false;

  if (!ok) { Serial.print("Valor invalido. Veja os exemplos no inicio do main.cpp.\r\n"); return; }
  salvarAgenda();
  mostrarAgenda();
}

// ---------------------------------------------------------------------------
// Ponto salvo, para retomar a rega depois de uma queda de energia
// ---------------------------------------------------------------------------
static uint32_t minutoAgora() {
  DadosTela d = lerDados();
  return d.temHora ? minutoAbsoluto(d.agora) : 0;
}

static bool mesmosSetores(const PontoSalvo& a, const PontoSalvo& b) {
  if (a.n != b.n) return false;
  for (uint8_t i = 0; i < a.n; i++) if (a.itens[i].setor != b.itens[i].setor) return false;
  return true;
}

// Grava quando a fila muda e, enquanto rega, uma vez por minuto (para o tempo
// que falta ficar certo sem gastar a memória flash).
static void salvarPontoSePreciso() {
  if (retomada.aguardando()) return;  // não apaga o ponto antes de retomar
  PontoSalvo atual = capturarPonto(controlador, minutoAgora());
  bool mudou = !mesmosSetores(atual, ultimoPonto);
  bool passouUmMinuto = atual.n > 0 && millis() - ultimoPontoMs >= 60000UL;
  if (!mudou && !passouUmMinuto) return;
  memoria.putBytes("ponto", &atual, sizeof(atual));
  ultimoPonto = atual;
  ultimoPontoMs = millis();
}

static void carregarPonto() {
  if (memoria.getBytesLength("ponto") != sizeof(PontoSalvo)) return;
  memoria.getBytes("ponto", &ultimoPonto, sizeof(ultimoPonto));
  retomada.iniciar(ultimoPonto, minutoAgora(), millis());
  switch (retomada.estado()) {
    case EstadoRetomada::Aguardando:
      Serial.printf("Energia voltou (fora por %lu min). Retomando o setor %u e mais %u em %lu s.\r\n"
                    "Digite 'retomar' para ir agora ou 'cancelar'.\r\n",
                    static_cast<unsigned long>(retomada.minutosSemEnergia()), retomada.primeiroSetor(),
                    retomada.totalSetores() - 1, static_cast<unsigned long>(retomada.faltamS(millis())));
      break;
    case EstadoRetomada::Descartada:
      Serial.printf("Energia ficou fora por %lu min: a rega interrompida nao sera retomada.\r\n",
                    static_cast<unsigned long>(retomada.minutosSemEnergia()));
      break;
    default:
      break;
  }
}

static const char* textoResultado(Resultado r) {
  switch (r) {
    case Resultado::Aceito: return "na fila";
    case Resultado::SetorInvalido: return "setor invalido (1 a 8)";
    case Resultado::DuracaoInvalida: return "duracao invalida";
    case Resultado::JaNaFila: return "setor ja esta regando ou na fila";
    case Resultado::FilaCheia: return "fila cheia";
    case Resultado::SetorDesativado: return "setor desativado nos ajustes";
  }
  return "?";
}

static void ajuda() {
  Serial.print("Comandos: l | l N S | c S | p | x N | s | ?\r\n"
               "Agenda:   a | g A on/off/h/d/t/%/run | chuva N | hora AAAA-MM-DD HH:MM\r\n"
               "Energia:  retomar | cancelar | reiniciar\r\n"
               "Rede:     wifi | wifi NOME SENHA | senha padrao\r\n> ");
}

// Ligar em duas etapas pelo serial: primeiro o setor, depois o tempo.
enum class Pergunta : uint8_t { Nenhuma, Setor, Tempo };
static Pergunta pergunta = Pergunta::Nenhuma;
static unsigned setorPerguntado = 0;

static void ligar(unsigned setor, unsigned segundos) {
  Serial.printf("Setor %u por %u s: %s\r\n", setor, segundos,
                textoResultado(controlador.pedir(setor, segundos)));
}

static void tratarComando(const String& linha) {
  if (pergunta == Pergunta::Setor) {
    setorPerguntado = linha.toInt();
    if (setorPerguntado < 1 || setorPerguntado > NUM_SETORES) {
      Serial.print("Setor invalido. Cancelado.\r\n> ");
      pergunta = Pergunta::Nenhuma;
      return;
    }
    Serial.printf("Tempo do setor %u, em segundos: ", setorPerguntado);
    pergunta = Pergunta::Tempo;
    return;
  }
  if (pergunta == Pergunta::Tempo) {
    pergunta = Pergunta::Nenhuma;
    ligar(setorPerguntado, linha.toInt());
    Serial.print("> ");
    return;
  }
  if (linha.length() == 0) { Serial.print("> "); return; }

  if (linha == "retomar" || linha == "cancelar") {
    if (!retomada.aguardando()) Serial.print("Nao ha rega esperando para retomar\r\n> ");
    else if (linha == "retomar") { retomada.retomarAgora(); Serial.print("Retomando a rega\r\n> "); }
    else { retomada.cancelar(); Serial.print("Retomada cancelada\r\n> "); }
    return;
  }
  if (linha == "reiniciar") {
    Serial.print("Reiniciando...\r\n");
    Serial.flush();
    ESP.restart();
  }
  if (linha == "wifi") {
    webMostrarRede(Serial);
    Serial.print("> ");
    return;
  }
  if (linha.startsWith("wifi ")) {
    String resto = linha.substring(5);
    resto.trim();
    int espaco = resto.indexOf(' ');
    String nome = espaco < 0 ? resto : resto.substring(0, espaco);
    String senha = espaco < 0 ? String() : resto.substring(espaco + 1);
    senha.trim();
    if (nome.length() == 0 || nome.length() > 32 || senha.length() > 63) {
      Serial.print("Uso: wifi NOME SENHA\r\n> ");
      return;
    }
    JsonDocument doc;
    configCarregar(doc);
    doc["rede"]["wifiNome"] = nome;
    configGravar(doc);
    memoria.putString("wifiSenha", senha);
    webReconfigurarRede();
    Serial.printf("Wi-Fi trocado para \"%s\". Conectando...\r\n> ", nome.c_str());
    return;
  }
  if (linha == "senha padrao") {
    webRestaurarSenhaPadrao();
    Serial.print("Senha da pagina voltou para a de fabrica\r\n> ");
    return;
  }
  if (linha.startsWith("chuva")) {
    int dias = linha.substring(5).toInt();
    agenda.suspender(dias < 0 ? 0 : dias, lerDados().agora);
    salvarAgenda();
    Serial.printf("Pausa por chuva: %u dia(s)\r\n> ", agenda.diasSuspensos(lerDados().agora));
    return;
  }
  if (linha.startsWith("hora")) {
    int ano, mes, dia, hh, mm;
    if (temRtc && sscanf(linha.c_str(), "hora %d-%d-%d %d:%d", &ano, &mes, &dia, &hh, &mm) == 5) {
      rtc.adjust(DateTime(ano, mes, dia, hh, mm, 0));
      registrarEvento("Hora acertada pelo serial");
      Serial.print("Relogio acertado\r\n> ");
    } else {
      Serial.print("Uso: hora 2026-11-12 19:55\r\n> ");
    }
    return;
  }
  if (linha.charAt(0) == 'g' && (linha.length() == 1 || linha.charAt(1) == ' ')) {
    comandoAgenda(linha);
    Serial.print("> ");
    return;
  }

  char cmd = linha.charAt(0);
  unsigned a = 0, b = 0;
  int lidos = sscanf(linha.c_str() + 1, "%u %u", &a, &b);

  switch (cmd) {
    case 'l':
      if (lidos < 1) {
        Serial.print("Qual setor (1 a 8)? ");
        pergunta = Pergunta::Setor;
        return;
      }
      if (lidos < 2) {
        setorPerguntado = a;
        Serial.printf("Tempo do setor %u, em segundos: ", a);
        pergunta = Pergunta::Tempo;
        return;
      }
      ligar(a, b);
      break;
    case 'c': {
      uint32_t duracoes[NUM_SETORES];
      for (auto& d : duracoes) d = a;
      Serial.printf("Ciclo: %u setores na fila\r\n", controlador.iniciarCiclo(duracoes));
      break;
    }
    case 'p': controlador.pular(); Serial.print("Pulando setor\r\n"); break;
    case 'x': Serial.print(controlador.parar(a) ? "Setor parado\r\n" : "Setor nao encontrado\r\n"); break;
    case 's': controlador.pararTudo(); Serial.print("Parando tudo\r\n"); break;
    case '?': mostrarEstado(); break;
    case 'a': mostrarAgenda(); break;
    default: ajuda(); return;
  }
  Serial.print("> ");
}

// RF15: se o loop parar de rodar por 15 s, o watchdog reinicia o ESP. As saídas
// voltam desligadas (RF14) e a rega segue pela retomada, depois da espera.
constexpr uint32_t WATCHDOG_S = 15;

static void iniciarWatchdog() {
#if ESP_IDF_VERSION_MAJOR >= 5
  esp_task_wdt_config_t config = {};
  config.timeout_ms = WATCHDOG_S * 1000;
  config.trigger_panic = true;
  if (esp_task_wdt_reconfigure(&config) != ESP_OK) esp_task_wdt_init(&config);
#else
  esp_task_wdt_init(WATCHDOG_S, true);
#endif
  esp_task_wdt_add(nullptr);  // vigia a tarefa do loop
}

static const char* motivoDoReinicio() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "Controlador ligado";
    case ESP_RST_SW: return "Controlador reiniciado";
    case ESP_RST_TASK_WDT:
    case ESP_RST_INT_WDT:
    case ESP_RST_WDT: return "Reiniciado pelo watchdog (o programa travou)";
    case ESP_RST_PANIC: return "Reiniciado depois de um erro do programa";
    case ESP_RST_BROWNOUT: return "Reiniciado por queda de tensao";
    default: return "Controlador reiniciado";
  }
}

// Registra cada rega quando o setor termina, com o tempo que ficou aberto.
static void acompanharRegas(uint32_t agoraMs) {
  static uint8_t setorAnterior = 0;
  static uint32_t inicioMs = 0;
  uint8_t setor = controlador.setorAtivo();
  if (setor == setorAnterior) return;
  if (setorAnterior != 0) registrarRega(setorAnterior, (agoraMs - inicioMs) / 1000UL);
  setorAnterior = setor;
  inicioMs = agoraMs;
}

void setup() {
  for (uint8_t pino : PINOS_SETORES) configurarSaidaSegura(pino);
  configurarSaidaSegura(PINO_BOMBA);
  controlador.iniciar();

  Serial.begin(115200);
  Wire.begin(PINO_SDA, PINO_SCL);
  temRtc = rtc.begin(&Wire);

  pinMode(PINO_PLUVIOMETRO, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PINO_PLUVIOMETRO), isrPluviometro, FALLING);

  trava = xSemaphoreCreateMutex();
  memoria.begin("sabia", false);
  {
    JsonDocument doc;
    configCarregar(doc);
    configAplicar(doc.as<JsonVariantConst>());
  }
  carregarAgenda();

  interfaceIniciar();
  sensoresIniciar();
  registroIniciar();  // pode levar alguns segundos na primeira vez (formata a memória)
  balancoIniciar();
  registrarEvento(motivoDoReinicio());

  delay(200);
  Serial.print("\r\nSABIA v" SABIA_VERSAO "\r\n");
  if (!temRtc) Serial.print("Aviso: relogio (RTC) nao encontrado\r\n");
  carregarPonto();
  if (retomada.estado() == EstadoRetomada::Aguardando) registrarEvento("Energia voltou: rega esperando para retomar");
  else if (retomada.estado() == EstadoRetomada::Descartada) registrarEvento("Energia voltou tarde demais: rega nao retomada");
  webIniciar();
  ajuda();
  iniciarWatchdog();
}

void loop() {
  esp_task_wdt_reset();
  if (reiniciarEmMs != 0 && static_cast<int32_t>(millis() - reiniciarEmMs) >= 0) {
    Serial.print("\r\nReiniciando...\r\n");
    Serial.flush();
    ESP.restart();
  }
  travar();
  static String linha;
  while (Serial.available()) {
    char c = static_cast<char>(Serial.read());
    if (c == '\n' || c == '\r') {
      if (c == '\n' && linha.length() == 0) continue;  // segundo caractere de um \r\n
      Serial.print("\r\n");
      linha.trim();
      tratarComando(linha);
      linha = "";
    } else if (c == 8 || c == 127) {  // apagar
      if (linha.length() > 0) {
        linha.remove(linha.length() - 1);
        Serial.print("\b \b");
      }
    } else if (c >= 32 && linha.length() < 110) {
      linha += c;
      Serial.print(c);  // eco: mostra o que foi digitado
    }
  }

  uint32_t agora = millis();
  Estado antes = controlador.estado();
  uint8_t setorAntes = controlador.setorAtivo();
  if (retomada.tick(controlador, agora)) {
    Serial.print("\r\nRetomando a rega interrompida\r\n");
    registrarEvento("Rega interrompida retomada");
  }
  controlador.tick(agora);
  if (controlador.estado() != antes || controlador.setorAtivo() != setorAntes) {
    Serial.print("\r\n");
    mostrarEstado();
    Serial.print("> ");
    Serial.print(linha);
  }

  if (sensoresTick(agora)) registroLeitura(sensoresLeitura());
  registroTick(lerDados(), pulsosChuva);
  acompanharRegas(agora);

  DadosTela dados = lerDados();
  if (dados.temHora) agenda.tick(dados.agora);  // sem relógio, a agenda não roda
  if (interfaceAtualizar(controlador, agenda, retomada, dados, agora)) salvarAgenda();
  salvarPontoSePreciso();
  destravar();
  delay(2);  // dá a vez à tarefa do servidor web
}
