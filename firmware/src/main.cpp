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

#include <Arduino.h>
#include <Preferences.h>
#include <RTClib.h>
#include <SPI.h>
#include <Wire.h>
#include <driver/gpio.h>

#include "Agenda.h"
#include "Controlador.h"
#include "interface.h"
#include "pinos.h"

using namespace sabia;

constexpr float MM_POR_BASCULADA = 0.2f;

class SaidasReles : public Saidas {
 public:
  void valvula(uint8_t setor, bool aberta) override {
    if (setor < 1 || setor > NUM_SETORES) return;
    escrever(PINOS_SETORES[setor - 1], aberta);
  }
  void bomba(bool ligada) override { escrever(PINO_BOMBA, ligada); }

 private:
  static void escrever(uint8_t pino, bool ligado) {
    digitalWrite(pino, ligado ? RELE_LIGADO : RELE_DESLIGADO);
  }
};

static SaidasReles saidas;
static Controlador controlador(saidas);
static Agenda agenda(controlador);
static Preferences memoria;

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

static DadosTela lerDados() {
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
  d.chuvaMm = (pulsosChuva - pulsosInicioDoDia) * MM_POR_BASCULADA;
  return d;
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

static void salvarAgenda() {
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

static const char* textoResultado(Resultado r) {
  switch (r) {
    case Resultado::Aceito: return "na fila";
    case Resultado::SetorInvalido: return "setor invalido (1 a 8)";
    case Resultado::DuracaoInvalida: return "duracao invalida";
    case Resultado::JaNaFila: return "setor ja esta regando ou na fila";
    case Resultado::FilaCheia: return "fila cheia";
  }
  return "?";
}

static void ajuda() {
  Serial.print("Comandos: l | l N S | c S | p | x N | s | ?\r\n"
               "Agenda:   a | g A on/off/h/d/t/%/run | chuva N | hora AAAA-MM-DD HH:MM\r\n> ");
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

void setup() {
  for (uint8_t pino : PINOS_SETORES) configurarSaidaSegura(pino);
  configurarSaidaSegura(PINO_BOMBA);
  controlador.iniciar();

  Serial.begin(115200);
  Wire.begin(PINO_SDA, PINO_SCL);
  temRtc = rtc.begin(&Wire);

  pinMode(PINO_PLUVIOMETRO, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PINO_PLUVIOMETRO), isrPluviometro, FALLING);

  memoria.begin("sabia", false);
  carregarAgenda();

  interfaceIniciar();

  delay(200);
  Serial.print("\r\nSABIA v" SABIA_VERSAO "\r\n");
  if (!temRtc) Serial.print("Aviso: relogio (RTC) nao encontrado\r\n");
  ajuda();
}

void loop() {
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
    } else if (c >= 32 && linha.length() < 40) {
      linha += c;
      Serial.print(c);  // eco: mostra o que foi digitado
    }
  }

  uint32_t agora = millis();
  Estado antes = controlador.estado();
  uint8_t setorAntes = controlador.setorAtivo();
  controlador.tick(agora);
  if (controlador.estado() != antes || controlador.setorAtivo() != setorAntes) {
    Serial.print("\r\n");
    mostrarEstado();
    Serial.print("> ");
    Serial.print(linha);
  }

  DadosTela dados = lerDados();
  if (dados.temHora) agenda.tick(dados.agora);  // sem relógio, a agenda não roda
  if (interfaceAtualizar(controlador, agenda, dados, agora)) salvarAgenda();
}
