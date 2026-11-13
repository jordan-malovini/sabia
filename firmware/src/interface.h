// SABIÁ - interface local: tela de 128x64 e encoder com botão.
//
// Girar navega ou altera o valor, clique curto seleciona, clique longo volta.
// Fluxo para ligar um setor: menu -> escolher o setor -> escolher o tempo.
// O menu também inicia um programa, edita a agenda (Programar agenda) e
// suspende as regas por chuva. Depois de uma queda de energia, a tela de
// estado mostra a contagem para retomar a rega.
#pragma once

#include <stdint.h>

#include "Agenda.h"
#include "Controlador.h"
#include "Retomada.h"

namespace sabia {

struct DadosTela {
  bool temHora = false;
  uint8_t hora = 0, minuto = 0, segundo = 0, dia = 0, mes = 0;
  float chuvaMm = 0;
  Momento agora;  // data e hora completas, para a agenda
};

void interfaceIniciar();
// Retorna true quando a agenda foi alterada pelo menu e precisa ser salva.
bool interfaceAtualizar(Controlador& controlador, Agenda& agenda, Retomada& retomada, const DadosTela& dados,
                        uint32_t agoraMs);

// Durações oferecidas ao ligar um setor (em segundos), também usadas no serial.
extern const uint32_t DURACOES_S[];
extern const uint8_t NUM_DURACOES;
void formatarDuracao(uint32_t segundos, char* saida, size_t tamanho);

}  // namespace sabia
