// SABIÁ - balanço hídrico por setor (RF08)
//
// Cada setor tem um saldo de água no solo, como uma conta: a chuva e a rega
// entram, a evapotranspiração sai. A conta fecha uma vez por dia, à meia-noite:
//
//   ET0   = 0,0023 x (Tmédia + 17,8) x raiz(Tmáx - Tmín) x Ra     (Hargreaves)
//   Saldo = saldo anterior + chuva efetiva + rega - ET0 x Kc
//
// O balanço só economiza água: ele encurta ou pula a rega de um setor que
// ainda tem água, e nunca passa do tempo que está no programa. Sem dados
// confiáveis (sem temperatura ou sem latitude no último fechamento), ele não
// interfere e vale o tempo do programa.
//
// Lógica pura, sem Arduino.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "Controlador.h"

namespace sabia {

// Radiação no topo da atmosfera, já em mm de evaporação por dia (FAO-56).
float radiacaoExtraterrestreMm(float latitudeGraus, uint16_t diaDoAno);
// ET0 do dia em mm. NAN se faltar algum dado.
float et0Hargreaves(float tempMaxC, float tempMinC, float raMm);
uint16_t diaDoAno(uint32_t diasDesde2000);  // 1 a 366

struct ParametrosSetor {
  float kc = 1.0f;            // coeficiente da cultura
  float taxaMmH = 2.5f;       // taxa de aplicação medida
  float eficiencia = 0.9f;    // 0,1 a 1
  float deficitMinMm = 3.0f;  // abaixo disso, não vale regar
  float fator = 1.0f;         // ajuste fino do tempo calculado
};

struct ParametrosSolo {
  float capacidadeMm = 40.0f;      // teto do saldo
  float chuvaEfetivaMinMm = 2.0f;  // chuva do dia menor que isso não conta
};

class BalancoHidrico {
 public:
  BalancoHidrico();

  // Troca os parâmetros. O saldo é mantido, limitado à nova capacidade.
  void definir(const ParametrosSetor setores[NUM_SETORES], const ParametrosSolo& solo);

  // Água aplicada por uma rega que durou `segundos`.
  void rega(uint8_t setor, uint32_t segundos);

  // Fecha o dia: soma a chuva efetiva e tira a ET0 x Kc de cada setor.
  // Com ET0 = NAN (sem temperatura ou latitude), entra só a chuva e o balanço
  // deixa de ser confiável até o próximo fechamento completo.
  void fecharDia(float chuvaMm, float et0Mm);

  float saldoMm(uint8_t setor) const;
  float deficitMm(uint8_t setor) const;
  uint8_t saldoPct(uint8_t setor) const;
  bool confiavel() const { return confiavel_; }

  // Tempo para repor o déficit do setor, em segundos (0 se ainda tem água).
  uint32_t tempoNecessarioS(uint8_t setor) const;
  // Tempo que o setor deve regar, dado o tempo do programa.
  uint32_t ajustar(uint8_t setor, uint32_t programadoS) const;

  // Para guardar e recuperar na memória do ESP.
  struct Salvo {
    uint8_t versao = 1;
    bool confiavel = false;
    float saldoMm[NUM_SETORES] = {};
  };
  Salvo salvar() const;
  void recuperar(const Salvo& salvo);

 private:
  ParametrosSetor setores_[NUM_SETORES];
  ParametrosSolo solo_;
  float saldo_[NUM_SETORES];
  bool confiavel_ = false;
};

// Chuva das últimas 24 horas, em 24 gavetas de uma hora.
class Chuva24h {
 public:
  void somar(uint32_t horaAbsoluta, float mm);     // hora absoluta = minuto absoluto / 60
  float total(uint32_t horaAbsoluta) const;        // soma das 24 horas até esta

 private:
  float mm_[24] = {};
  uint32_t hora_[24] = {};
  bool usada_[24] = {};
};

}  // namespace sabia
