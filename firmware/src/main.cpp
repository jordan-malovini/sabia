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

#include <Arduino.h>
#include <RTClib.h>
#include <SPI.h>
#include <Wire.h>
#include <driver/gpio.h>

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

static void ajuda() { Serial.print("Comandos: l | l N S | c S | p | x N | s | ?\r\n> "); }

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

  interfaceAtualizar(controlador, lerDados(), agora);
}
