// SABIÁ - agenda de regas
//
// Programas A, B e C, no modelo dos controladores comerciais (Rain Bird, Hunter):
// cada programa tem até 4 horários de início, os dias em que rega, o tempo de
// cada setor e um ajuste percentual. Quando um horário chega, o programa põe
// os setores dele na fila do Controlador, que rega um por vez sem desligar a bomba.
//
// Lógica pura, sem Arduino: recebe a data e a hora em tick().
//
// Garantias:
//  - cada início dispara uma vez por dia, todos os dias permitidos. A marca do
//    último minuto verificado inclui a data (o controlador antigo guardava só
//    a hora e deixava de regar no dia seguinte);
//  - não depende de ler o relógio no segundo exato: minutos perdidos (até 10)
//    são verificados no tick seguinte;
//  - se um programa começa com outro em andamento, ele espera a vez
//    (empilhamento), em vez de ser perdido ou de abrir dois setores juntos;
//  - mudar o relógio, para frente ou para trás, não dispara regas atrasadas.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "Controlador.h"

namespace sabia {

constexpr uint8_t NUM_PROGRAMAS = 3;
constexpr uint8_t INICIOS_POR_PROGRAMA = 4;
constexpr uint16_t SEM_HORARIO = 0xFFFF;
constexpr uint8_t MAX_PENDENTES = 8;
constexpr uint8_t AJUSTE_MIN = 5, AJUSTE_MAX = 200;
constexpr uint16_t MINUTOS_MAX_SETOR = 180;
constexpr uint8_t SUSPENSAO_MAX_DIAS = 14;

struct Momento {
  uint16_t ano = 2000;
  uint8_t mes = 1, dia = 1, hora = 0, minuto = 0;
};

uint32_t diasDesde2000(uint16_t ano, uint8_t mes, uint8_t dia);
uint8_t diaDaSemana(uint32_t dias);  // 0 = domingo ... 6 = sábado
uint8_t diaDoMes(uint32_t dias);

enum class ModoDias : uint8_t {
  Semana,     // dias da semana marcados em diasSemana
  Impares,    // dias ímpares do mês
  Pares,      // dias pares do mês
  Intervalo,  // a cada intervaloDias, contando de diaReferencia
};

struct Programa {
  bool ativo = false;
  uint16_t inicios[INICIOS_POR_PROGRAMA] = {SEM_HORARIO, SEM_HORARIO, SEM_HORARIO, SEM_HORARIO};  // minuto do dia
  ModoDias modo = ModoDias::Semana;
  uint8_t diasSemana = 0x7F;      // bit 0 = domingo ... bit 6 = sábado
  uint8_t intervaloDias = 2;      // 1 a 30
  uint32_t diaReferencia = 0;     // dia em que o intervalo começa a contar
  uint16_t minutosSetor[NUM_SETORES] = {};  // 0 = setor fora do programa
  uint8_t ajustePct = 100;        // ajuste sazonal: 5 a 200 %
};

bool diaPermitido(const Programa& p, uint32_t dias);

class Agenda {
 public:
  explicit Agenda(Controlador& controlador) : controlador_(controlador) {}

  Programa& programa(uint8_t i) { return programas_[i < NUM_PROGRAMAS ? i : 0]; }
  const Programa& programa(uint8_t i) const { return programas_[i < NUM_PROGRAMAS ? i : 0]; }
  Programa* programas() { return programas_; }

  // Chamar no loop, com a data e a hora do relógio.
  void tick(const Momento& agora);

  // Inicia um programa agora (ou põe na espera, se algo estiver regando).
  bool iniciarPrograma(uint8_t i);

  // Suspensão por chuva: nenhum programa começa por `dias` dias, contando hoje.
  // 0 cancela a suspensão. Também descarta os programas em espera.
  void suspender(uint8_t dias, const Momento& agora);
  uint8_t diasSuspensos(const Momento& agora) const;
  uint32_t suspensoAteDia() const { return suspensoAteDia_; }
  void definirSuspensoAteDia(uint32_t dia) { suspensoAteDia_ = dia; }

  // Próximo início agendado. Retorna false se não há nenhum em 31 dias.
  bool proximoInicio(const Momento& agora, uint8_t& programa, uint16_t& minutoDoDia, uint32_t& faltamMin) const;

  uint8_t pendentes() const { return nPendentes_; }
  void cancelarPendentes() { nPendentes_ = 0; }
  uint32_t puladosPorChuva() const { return puladosPorChuva_; }

  // Tempo total de um programa, já com o ajuste, em segundos.
  uint32_t duracaoTotalS(uint8_t i) const;

 private:
  void verificarMinuto(uint32_t minutoAbs);
  void empilhar(uint8_t i);
  void liberarPendente();
  uint32_t duracaoSetorS(const Programa& p, uint8_t setor) const;

  Controlador& controlador_;
  Programa programas_[NUM_PROGRAMAS];
  bool iniciada_ = false;
  uint32_t ultimoMinuto_ = 0;
  uint32_t suspensoAteDia_ = 0;
  uint32_t puladosPorChuva_ = 0;
  uint8_t pendentes_[MAX_PENDENTES] = {};
  uint8_t nPendentes_ = 0;
};

}  // namespace sabia
