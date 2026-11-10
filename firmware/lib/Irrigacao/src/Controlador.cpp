#include "Controlador.h"

namespace sabia {

Controlador::Controlador(Saidas& saidas, const Config& config)
    : saidas_(saidas), config_(config) {}

void Controlador::iniciar() {
  saidas_.bomba(false);
  for (uint8_t s = 1; s <= NUM_SETORES; s++) saidas_.valvula(s, false);
  estado_ = Estado::Ocioso;
  atual_ = {0, 0};
  encerrar_ = false;
  tamanho_ = 0;
}

Resultado Controlador::pedir(uint8_t setor, uint32_t duracaoS) {
  if (setor < 1 || setor > NUM_SETORES) return Resultado::SetorInvalido;
  if (duracaoS == 0) return Resultado::DuracaoInvalida;
  if (duracaoS > config_.tempoMaximoS) duracaoS = config_.tempoMaximoS;

  // Um setor que já está regando (e não está sendo encerrado) não entra de novo.
  if (setor == atual_.setor && !encerrar_) return Resultado::JaNaFila;
  if (naFila(setor)) return Resultado::JaNaFila;
  if (tamanho_ >= CAPACIDADE_FILA) return Resultado::FilaCheia;

  fila_[tamanho_++] = {setor, duracaoS};
  return Resultado::Aceito;
}

uint8_t Controlador::iniciarCiclo(const uint32_t duracoesS[NUM_SETORES]) {
  uint8_t aceitos = 0;
  for (uint8_t i = 0; i < NUM_SETORES; i++) {
    if (duracoesS[i] > 0 && pedir(i + 1, duracoesS[i]) == Resultado::Aceito) aceitos++;
  }
  return aceitos;
}

void Controlador::pular() {
  if (atual_.setor != 0) encerrar_ = true;
}

bool Controlador::parar(uint8_t setor) {
  if (setor != 0 && setor == atual_.setor) {
    encerrar_ = true;
    return true;
  }
  for (size_t i = 0; i < tamanho_; i++) {
    if (fila_[i].setor == setor) {
      removerDaFila(i);
      return true;
    }
  }
  return false;
}

void Controlador::pararTudo() {
  tamanho_ = 0;
  pular();
}

void Controlador::tick(uint32_t agoraMs) {
  ultimoTickMs_ = agoraMs;

  switch (estado_) {
    case Estado::Ocioso:
      if (tamanho_ > 0) iniciarProximo(agoraMs);
      break;

    case Estado::AbrindoValvula:
      if (encerrar_) {
        // Encerrado antes de a bomba ligar: só fecha a válvula.
        mudar(Estado::FechandoValvula, agoraMs);
      } else if (decorrido(agoraMs) >= config_.atrasoBombaMs) {
        saidas_.bomba(true);
        mudar(Estado::Regando, agoraMs);
      }
      break;

    case Estado::Regando:
      if (encerrar_ || decorrido(agoraMs) >= atual_.duracaoS * 1000UL) {
        saidas_.bomba(false);
        mudar(Estado::FechandoValvula, agoraMs);
      }
      break;

    case Estado::FechandoValvula:
      if (decorrido(agoraMs) >= config_.atrasoFechamentoMs) {
        saidas_.valvula(atual_.setor, false);
        atual_ = {0, 0};
        encerrar_ = false;
        mudar(Estado::Pausa, agoraMs);
      }
      break;

    case Estado::Pausa:
      if (decorrido(agoraMs) >= config_.pausaEntreSetoresMs) {
        mudar(Estado::Ocioso, agoraMs);
        if (tamanho_ > 0) iniciarProximo(agoraMs);
      }
      break;
  }
}

uint32_t Controlador::restanteS() const {
  switch (estado_) {
    case Estado::AbrindoValvula:
      return atual_.duracaoS;
    case Estado::Regando: {
      uint32_t passadoS = decorrido(ultimoTickMs_) / 1000UL;
      return passadoS >= atual_.duracaoS ? 0 : atual_.duracaoS - passadoS;
    }
    default:
      return 0;
  }
}

Pedido Controlador::itemFila(size_t indice) const {
  if (indice >= tamanho_) return {0, 0};
  return fila_[indice];
}

bool Controlador::naFila(uint8_t setor) const {
  for (size_t i = 0; i < tamanho_; i++) {
    if (fila_[i].setor == setor) return true;
  }
  return false;
}

void Controlador::removerDaFila(size_t indice) {
  for (size_t i = indice; i + 1 < tamanho_; i++) fila_[i] = fila_[i + 1];
  tamanho_--;
}

void Controlador::iniciarProximo(uint32_t agoraMs) {
  atual_ = fila_[0];
  removerDaFila(0);
  encerrar_ = false;
  saidas_.valvula(atual_.setor, true);
  mudar(Estado::AbrindoValvula, agoraMs);
}

void Controlador::mudar(Estado novo, uint32_t agoraMs) {
  estado_ = novo;
  inicioEstadoMs_ = agoraMs;
}

const char* nomeEstado(Estado estado) {
  switch (estado) {
    case Estado::Ocioso: return "ocioso";
    case Estado::AbrindoValvula: return "abrindo valvula";
    case Estado::Regando: return "regando";
    case Estado::FechandoValvula: return "fechando valvula";
    case Estado::Pausa: return "pausa";
  }
  return "?";
}

}  // namespace sabia
