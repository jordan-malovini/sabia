#include "Retomada.h"

namespace sabia {

PontoSalvo capturarPonto(const Controlador& c, uint32_t minutoAbs) {
  PontoSalvo p;
  p.minutoAbs = minutoAbs;
  auto incluir = [&p](uint8_t setor, uint32_t duracaoS) {
    if (setor != 0 && duracaoS > 0 && p.n < MAX_ITENS_PONTO) p.itens[p.n++] = {setor, duracaoS};
  };
  switch (c.estado()) {
    case Estado::AbrindoValvula:
    case Estado::Regando:
      incluir(c.setorAtivo(), c.restanteS());
      break;
    case Estado::Trocando: {
      // O setor atual já terminou; o que está entrando ainda tem o tempo todo.
      Pedido proximo = c.proximoPedido();
      incluir(proximo.setor, proximo.duracaoS);
      break;
    }
    default:
      break;  // fechando, em pausa ou ocioso: o setor atual não tem mais nada a regar
  }
  for (size_t i = 0; i < c.tamanhoFila(); i++) {
    Pedido item = c.itemFila(i);
    incluir(item.setor, item.duracaoS);
  }
  return p;
}

bool pontosIguais(const PontoSalvo& a, const PontoSalvo& b) {
  if (a.n != b.n) return false;
  for (uint8_t i = 0; i < a.n; i++) {
    if (a.itens[i].setor != b.itens[i].setor || a.itens[i].duracaoS != b.itens[i].duracaoS) return false;
  }
  return true;
}

void Retomada::iniciar(const PontoSalvo& ponto, uint32_t minutoAgora, uint32_t agoraMs) {
  ponto_ = ponto;
  estado_ = EstadoRetomada::Inativa;
  pular_ = false;
  minutosFora_ = 0;
  if (!config_.ativa || ponto.versao != VERSAO_PONTO || ponto.n == 0 || ponto.n > MAX_ITENS_PONTO) {
    ponto_.n = 0;
    return;
  }
  // Com relógio dos dois lados, dá para saber quanto tempo a energia ficou fora.
  if (minutoAgora != 0 && ponto.minutoAbs != 0) {
    if (minutoAgora < ponto.minutoAbs) {  // relógio voltou no tempo: não confia
      estado_ = EstadoRetomada::Descartada;
      return;
    }
    minutosFora_ = minutoAgora - ponto.minutoAbs;
    if (minutosFora_ > config_.limiteMin) {
      estado_ = EstadoRetomada::Descartada;
      return;
    }
  }
  estado_ = EstadoRetomada::Aguardando;
  inicioMs_ = agoraMs;
}

uint32_t Retomada::faltamS(uint32_t agoraMs) const {
  if (estado_ != EstadoRetomada::Aguardando) return 0;
  uint32_t passouS = (agoraMs - inicioMs_) / 1000UL;
  return passouS >= config_.esperaS ? 0 : config_.esperaS - passouS;
}

bool Retomada::tick(Controlador& controlador, uint32_t agoraMs) {
  if (estado_ != EstadoRetomada::Aguardando) return false;
  if (!pular_ && (agoraMs - inicioMs_) / 1000UL < config_.esperaS) return false;
  for (uint8_t i = 0; i < ponto_.n; i++) controlador.pedir(ponto_.itens[i].setor, ponto_.itens[i].duracaoS);
  estado_ = EstadoRetomada::Retomada;
  return true;
}

}  // namespace sabia
