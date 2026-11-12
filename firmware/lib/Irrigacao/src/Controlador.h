// SABIÁ - controlador de setores e bomba
//
// Lógica pura, sem Arduino: recebe o tempo em tick() e comanda as saídas por
// uma interface. Assim ela roda igual no ESP32, no Wokwi e nos testes do PC.
//
// Garantias:
//  - um setor regando por vez (RF03). Na troca entre setores da fila, a válvula
//    do próximo abre antes de a do atual fechar (sobreposição) e a bomba
//    continua ligada: ela não fica ligando e desligando a cada setor, e nunca
//    trabalha contra todas as válvulas fechadas;
//  - a bomba só liga com uma válvula aberta e desliga antes de a última fechar (RF05);
//  - nenhuma rega passa do tempo máximo configurado (RF13).

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace sabia {

constexpr uint8_t NUM_SETORES = 8;
constexpr size_t CAPACIDADE_FILA = NUM_SETORES;

// Saídas físicas. No ESP32 vira os relés; nos testes, um registro de eventos.
class Saidas {
 public:
  virtual ~Saidas() = default;
  virtual void valvula(uint8_t setor, bool aberta) = 0;  // setor de 1 a 8
  virtual void bomba(bool ligada) = 0;
};

struct Config {
  uint32_t atrasoBombaMs = 2000;          // válvula abre -> bomba liga
  uint32_t atrasoFechamentoMs = 1000;     // bomba desliga -> válvula fecha
  uint32_t sobreposicaoMs = 2000;         // troca: as duas válvulas abertas, bomba ligada
  uint32_t pausaEntreSetoresMs = 3000;    // depois de a bomba desligar, antes de religar
  uint32_t tempoMaximoS = 60UL * 60UL;    // limite de uma rega (RF13)
};

enum class Estado : uint8_t {
  Ocioso,           // nada ligado
  AbrindoValvula,   // válvula aberta, esperando para ligar a bomba
  Regando,          // válvula e bomba ligadas
  Trocando,         // válvula atual e a próxima abertas, bomba ligada
  FechandoValvula,  // bomba desligada, esperando para fechar a válvula
  Pausa,            // tudo desligado, intervalo antes de religar a bomba
};

enum class Resultado : uint8_t {
  Aceito,
  SetorInvalido,
  DuracaoInvalida,
  JaNaFila,   // o setor já está regando ou esperando
  FilaCheia,
};

struct Pedido {
  uint8_t setor;
  uint32_t duracaoS;
};

class Controlador {
 public:
  explicit Controlador(Saidas& saidas, const Config& config = Config());

  // Desliga todas as saídas e zera o estado. Chamar no boot.
  void iniciar();

  // Pede a rega de um setor. Durações acima do máximo são cortadas no máximo.
  Resultado pedir(uint8_t setor, uint32_t duracaoS);

  // Modo ciclo: enfileira, em ordem, os setores com duração maior que zero.
  // Retorna quantos setores entraram na fila.
  uint8_t iniciarCiclo(const uint32_t duracoesS[NUM_SETORES]);

  void pular();                 // encerra o setor atual e segue a fila (sem desligar a bomba)
  bool parar(uint8_t setor);    // encerra ou tira da fila um setor
  void pararTudo();             // esvazia a fila e encerra o setor atual

  // Avança a máquina de estados. Chamar sempre no loop, com millis().
  void tick(uint32_t agoraMs);

  Estado estado() const { return estado_; }
  uint8_t setorAtivo() const { return atual_.setor; }  // 0 se nenhum
  uint8_t proximoSetor() const { return proximo_.setor; }  // só durante a troca
  uint32_t restanteS() const;
  size_t tamanhoFila() const { return tamanho_; }
  Pedido itemFila(size_t indice) const;

 private:
  bool naFila(uint8_t setor) const;
  void removerDaFila(size_t indice);
  void iniciarProximo(uint32_t agoraMs);
  void mudar(Estado novo, uint32_t agoraMs);
  uint32_t decorrido(uint32_t agoraMs) const { return agoraMs - inicioEstadoMs_; }

  Saidas& saidas_;
  Config config_;

  Estado estado_ = Estado::Ocioso;
  uint32_t inicioEstadoMs_ = 0;
  uint32_t ultimoTickMs_ = 0;
  Pedido atual_ = {0, 0};
  Pedido proximo_ = {0, 0};   // setor que está entrando, durante a troca
  bool encerrar_ = false;     // encerrar o setor atual
  bool pularProximo_ = false; // encerrar o setor que está entrando, assim que entrar
  bool abortar_ = false;      // parar tudo

  Pedido fila_[CAPACIDADE_FILA] = {};
  size_t tamanho_ = 0;
};

const char* nomeEstado(Estado estado);

}  // namespace sabia
