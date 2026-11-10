// SABIÁ - firmware principal
//
// Versão 0.1: fila de setores e sequência válvula/bomba, comandadas pelo
// monitor serial (115200). Comandos:
//   l N S   liga o setor N por S segundos (entra na fila)
//   c S     ciclo: todos os setores, S segundos cada
//   p       pula o setor atual
//   x N     para o setor N (ou tira da fila)
//   s       para tudo
//   ?       mostra o estado

#include <Arduino.h>
#include <driver/gpio.h>

#include "Controlador.h"
#include "pinos.h"

using namespace sabia;

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

// RF14: o nível de "desligado" vai para o registrador antes de o pino virar
// saída, para nenhum relé pulsar no boot. gpio_set_level funciona mesmo com o
// pino ainda como entrada.
static void configurarSaidaSegura(uint8_t pino) {
  gpio_set_level(static_cast<gpio_num_t>(pino), RELE_DESLIGADO);
  pinMode(pino, OUTPUT);
}

static void mostrarEstado() {
  Serial.printf("[%lu s] %s", millis() / 1000UL, nomeEstado(controlador.estado()));
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
  Serial.println();
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

static void tratarComando(const String& linha) {
  if (linha.length() == 0) return;
  char cmd = linha.charAt(0);
  unsigned a = 0, b = 0;
  sscanf(linha.c_str() + 1, "%u %u", &a, &b);

  switch (cmd) {
    case 'l':
      Serial.printf("Setor %u por %u s: %s\n", a, b,
                    textoResultado(controlador.pedir(a, b)));
      break;
    case 'c': {
      uint32_t duracoes[NUM_SETORES];
      for (auto& d : duracoes) d = a;
      Serial.printf("Ciclo: %u setores na fila\n", controlador.iniciarCiclo(duracoes));
      break;
    }
    case 'p': controlador.pular(); Serial.println("Pulando setor"); break;
    case 'x': Serial.println(controlador.parar(a) ? "Setor parado" : "Setor nao encontrado"); break;
    case 's': controlador.pararTudo(); Serial.println("Parando tudo"); break;
    case '?': mostrarEstado(); break;
    default: Serial.println("Comandos: l N S | c S | p | x N | s | ?"); break;
  }
}

void setup() {
  for (uint8_t pino : PINOS_SETORES) configurarSaidaSegura(pino);
  configurarSaidaSegura(PINO_BOMBA);
  controlador.iniciar();

  Serial.begin(115200);
  delay(200);
  Serial.println("\nSABIA v0.1 - fila de setores");
  Serial.println("Comandos: l N S | c S | p | x N | s | ?");
}

void loop() {
  static String linha;
  while (Serial.available()) {
    char c = static_cast<char>(Serial.read());
    if (c == '\n' || c == '\r') {
      linha.trim();
      tratarComando(linha);
      linha = "";
    } else {
      linha += c;
    }
  }

  Estado antes = controlador.estado();
  uint8_t setorAntes = controlador.setorAtivo();
  controlador.tick(millis());
  if (controlador.estado() != antes || controlador.setorAtivo() != setorAntes) mostrarEstado();
}
