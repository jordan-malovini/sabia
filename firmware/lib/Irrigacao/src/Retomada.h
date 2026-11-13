// SABIÁ - retomada da rega depois de uma queda de energia
//
// Enquanto rega, o firmware grava um "ponto salvo": o setor atual com o tempo
// que falta, a fila e a hora. Ao ligar de novo com um ponto salvo:
//  - se a energia ficou fora por mais que o limite, a rega é descartada (para
//    não regar fora de hora por causa de uma queda antiga);
//  - senão, espera um tempo com tudo desligado, para a rede estabilizar, e
//    então devolve o setor e a fila ao Controlador, com o tempo que faltava.
// Durante a espera dá para retomar na hora ou cancelar.
//
// Lógica pura, sem Arduino. A ideia vem do controlador antigo (RECOVERY_WAIT).

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "Controlador.h"

namespace sabia {

constexpr uint8_t VERSAO_PONTO = 1;
constexpr uint8_t MAX_ITENS_PONTO = 1 + CAPACIDADE_FILA;

struct PontoSalvo {
  uint8_t versao = VERSAO_PONTO;
  uint8_t n = 0;                       // 0 = nada a retomar
  Pedido itens[MAX_ITENS_PONTO] = {};  // o primeiro é o setor que estava regando
  uint32_t minutoAbs = 0;              // hora da gravação (0 = sem relógio)
};

// Tira um ponto salvo do estado atual do controlador.
PontoSalvo capturarPonto(const Controlador& controlador, uint32_t minutoAbs);
bool pontosIguais(const PontoSalvo& a, const PontoSalvo& b);  // ignora a hora

struct ConfigRetomada {
  bool ativa = true;
  uint32_t esperaS = 600;       // tempo com tudo desligado antes de retomar
  uint32_t limiteMin = 120;     // energia fora por mais que isso: não retoma
};

enum class EstadoRetomada : uint8_t {
  Inativa,      // nada a retomar
  Aguardando,   // esperando a energia estabilizar
  Retomada,     // devolveu a rega ao controlador
  Descartada,   // energia ficou fora por tempo demais
  Cancelada,    // cancelada na tela ou no serial
};

class Retomada {
 public:
  explicit Retomada(const ConfigRetomada& config = ConfigRetomada()) : config_(config) {}

  void definirConfig(const ConfigRetomada& config) { config_ = config; }
  const ConfigRetomada& config() const { return config_; }

  // Chamar uma vez ao ligar. minutoAgora = 0 quando não há relógio.
  void iniciar(const PontoSalvo& ponto, uint32_t minutoAgora, uint32_t agoraMs);

  // Chamar no loop. Retorna true no momento em que a rega é retomada.
  bool tick(Controlador& controlador, uint32_t agoraMs);

  void retomarAgora() { if (estado_ == EstadoRetomada::Aguardando) pular_ = true; }
  void cancelar() { if (estado_ == EstadoRetomada::Aguardando) estado_ = EstadoRetomada::Cancelada; }

  EstadoRetomada estado() const { return estado_; }
  bool aguardando() const { return estado_ == EstadoRetomada::Aguardando; }
  uint32_t faltamS(uint32_t agoraMs) const;
  uint8_t primeiroSetor() const { return ponto_.n > 0 ? ponto_.itens[0].setor : 0; }
  uint8_t totalSetores() const { return ponto_.n; }
  uint32_t minutosSemEnergia() const { return minutosFora_; }

 private:
  ConfigRetomada config_;
  PontoSalvo ponto_;
  EstadoRetomada estado_ = EstadoRetomada::Inativa;
  uint32_t inicioMs_ = 0;
  uint32_t minutosFora_ = 0;
  bool pular_ = false;
};

}  // namespace sabia
