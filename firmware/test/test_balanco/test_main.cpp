// Testes do balanço hídrico e da chuva das últimas 24 horas.
// Rodar no PC: pio test -e native

#include <math.h>
#include <stdio.h>

#include <unity.h>

#include "Agenda.h"
#include "Balanco.h"

using namespace sabia;

void setUp() {}
void tearDown() {}

static bool perto(float a, float b, float folga = 0.05f) { return fabsf(a - b) <= folga; }

static void parametros(BalancoHidrico& b, float capacidade = 40) {
  ParametrosSetor setores[NUM_SETORES];
  ParametrosSolo solo;
  solo.capacidadeMm = capacidade;
  solo.chuvaEfetivaMinMm = 2;
  setores[1].taxaMmH = 6;       // setor 2: aspersão
  setores[1].eficiencia = 0.8f;
  setores[1].deficitMinMm = 8;
  setores[2].kc = 0.5f;         // setor 3: cultura que consome metade
  b.definir(setores, solo);
}

void test_dia_do_ano() {
  TEST_ASSERT_EQUAL(1, diaDoAno(diasDesde2000(2026, 1, 1)));
  TEST_ASSERT_EQUAL(317, diaDoAno(diasDesde2000(2026, 11, 13)));
  TEST_ASSERT_EQUAL(366, diaDoAno(diasDesde2000(2024, 12, 31)));
  TEST_ASSERT_EQUAL(365, diaDoAno(diasDesde2000(2025, 12, 31)));
}

// Referência: exemplo 8 da FAO-56 (20 graus sul, 3 de setembro, dia 246):
// Ra = 32,2 MJ/m2/dia, ou 13,1 mm/dia.
void test_radiacao_confere_com_a_fao() {
  TEST_ASSERT_TRUE(perto(radiacaoExtraterrestreMm(-20, 246), 32.2f * 0.408f, 0.1f));
  TEST_ASSERT_TRUE(perto(radiacaoExtraterrestreMm(-20, 246), 13.1f, 0.1f));
  // No inverno do hemisfério sul chega menos radiação do que no verão.
  TEST_ASSERT_TRUE(radiacaoExtraterrestreMm(-20, 166) < radiacaoExtraterrestreMm(-20, 349));
  // No hemisfério norte é o contrário.
  TEST_ASSERT_TRUE(radiacaoExtraterrestreMm(20, 166) > radiacaoExtraterrestreMm(20, 349));
  TEST_ASSERT_TRUE(isnan(radiacaoExtraterrestreMm(NAN, 100)));
  TEST_ASSERT_TRUE(isnan(radiacaoExtraterrestreMm(120, 100)));
  TEST_ASSERT_TRUE(radiacaoExtraterrestreMm(-89, 166) >= 0);  // noite polar não dá número negativo
}

void test_et0_hargreaves() {
  float ra = radiacaoExtraterrestreMm(-14.79f, 317);
  float et0 = et0Hargreaves(31, 21, ra);
  TEST_ASSERT_TRUE(et0 > 4.5f && et0 < 6.0f);  // dia quente de novembro no sul da Bahia
  // Conta feita à mão: 0,0023 x (26 + 17,8) x raiz(10) x Ra
  TEST_ASSERT_TRUE(perto(et0, 0.0023f * 43.8f * 3.16228f * ra, 0.01f));
  TEST_ASSERT_TRUE(et0Hargreaves(24, 22, ra) < et0);  // dia nublado, pouca amplitude: evapora menos
  TEST_ASSERT_TRUE(isnan(et0Hargreaves(NAN, 21, ra)));
  TEST_ASSERT_TRUE(isnan(et0Hargreaves(31, 21, NAN)));
  TEST_ASSERT_TRUE(isnan(et0Hargreaves(20, 25, ra)));  // máxima menor que a mínima: leitura ruim
}

void test_comeca_cheio_e_sem_interferir() {
  BalancoHidrico b;
  parametros(b);
  TEST_ASSERT_EQUAL(100, b.saldoPct(1));
  TEST_ASSERT_FALSE(b.confiavel());
  TEST_ASSERT_EQUAL_UINT32(1500, b.ajustar(1, 1500));  // sem dados, vale o tempo do programa
}

void test_dia_seco_gasta_agua() {
  BalancoHidrico b;
  parametros(b);
  b.fecharDia(0, 5);
  TEST_ASSERT_TRUE(b.confiavel());
  TEST_ASSERT_TRUE(perto(35, b.saldoMm(1)));
  TEST_ASSERT_TRUE(perto(37.5f, b.saldoMm(3)));  // Kc 0,5 gasta a metade
  TEST_ASSERT_TRUE(perto(5, b.deficitMm(1)));
  TEST_ASSERT_EQUAL(88, b.saldoPct(1));
}

void test_chuva_fraca_nao_conta_e_chuva_forte_enche() {
  BalancoHidrico b;
  parametros(b);
  b.fecharDia(0, 5);
  b.fecharDia(0, 5);  // saldo 30
  b.fecharDia(1.5f, 0);  // chuvisco abaixo de 2 mm
  TEST_ASSERT_TRUE(perto(30, b.saldoMm(1)));
  b.fecharDia(6, 0);
  TEST_ASSERT_TRUE(perto(36, b.saldoMm(1)));
  b.fecharDia(80, 0);  // temporal: o saldo para na capacidade
  TEST_ASSERT_TRUE(perto(40, b.saldoMm(1)));
  for (int i = 0; i < 30; i++) b.fecharDia(0, 6);  // seca longa: não fica negativo
  TEST_ASSERT_TRUE(b.saldoMm(1) == 0);
}

void test_rega_repoe_agua() {
  BalancoHidrico b;
  parametros(b);
  for (int i = 0; i < 4; i++) b.fecharDia(0, 5);  // saldo 20
  b.rega(1, 3600);  // gotejo: 2,5 mm/h x 0,9 = 2,25 mm
  TEST_ASSERT_TRUE(perto(22.25f, b.saldoMm(1)));
  b.rega(2, 1800);  // aspersão: 6 mm/h x 0,5 h x 0,8 = 2,4 mm
  TEST_ASSERT_TRUE(perto(22.4f, b.saldoMm(2)));
  b.rega(9, 3600);  // setor que não existe
  b.rega(1, 100UL * 3600);
  TEST_ASSERT_TRUE(perto(40, b.saldoMm(1)));
}

void test_tempo_necessario() {
  BalancoHidrico b;
  parametros(b);
  b.fecharDia(0, 2);  // déficit de 2 mm: menos que o mínimo de 3 do setor 1
  TEST_ASSERT_EQUAL_UINT32(0, b.tempoNecessarioS(1));
  b.fecharDia(0, 2.5f);  // déficit de 4,5 mm
  // 4,5 / (2,5 x 0,9) = 2 h
  TEST_ASSERT_TRUE(fabsf(static_cast<float>(b.tempoNecessarioS(1)) - 7200) < 5);
  TEST_ASSERT_EQUAL_UINT32(0, b.tempoNecessarioS(2));  // setor 2 só rega a partir de 8 mm
  b.fecharDia(0, 4.5f);  // déficit de 9 mm
  // 9 / (6 x 0,8) = 1,875 h
  TEST_ASSERT_TRUE(fabsf(static_cast<float>(b.tempoNecessarioS(2)) - 6750) < 5);
}

// O balanço encurta ou pula, mas nunca rega mais do que está no programa.
void test_ajustar_so_economiza() {
  BalancoHidrico b;
  parametros(b);
  b.fecharDia(0, 1);
  TEST_ASSERT_EQUAL_UINT32(0, b.ajustar(1, 1500));  // solo quase cheio: pula
  b.fecharDia(0, 3.5f);  // déficit de 4,5 mm: precisa de 2 h
  TEST_ASSERT_EQUAL_UINT32(1500, b.ajustar(1, 1500));  // o programa tem 25 min: rega os 25 min
  b.rega(1, 1500);  // repõe 0,94 mm -> déficit 3,56
  b.rega(1, 1200);  // repõe 0,75 mm -> déficit 2,81: abaixo do mínimo
  TEST_ASSERT_EQUAL_UINT32(0, b.ajustar(1, 1500));

  BalancoHidrico c;
  parametros(c);
  c.fecharDia(0, 3.2f);  // déficit 3,2 mm: 3,2 / 2,25 = 1,42 h = 5120 s
  TEST_ASSERT_TRUE(fabsf(static_cast<float>(c.ajustar(1, 3UL * 3600)) - 5120) < 5);  // programa de 3 h: encurta
}

// Sem temperatura no fechamento, o balanço para de mandar: melhor regar pelo
// programa do que deixar de regar por causa de um sensor que falhou.
void test_sem_et0_nao_interfere() {
  BalancoHidrico b;
  parametros(b);
  b.fecharDia(0, 1);
  TEST_ASSERT_EQUAL_UINT32(0, b.ajustar(1, 1500));
  b.fecharDia(0, NAN);
  TEST_ASSERT_FALSE(b.confiavel());
  TEST_ASSERT_EQUAL_UINT32(1500, b.ajustar(1, 1500));
  TEST_ASSERT_TRUE(perto(39, b.saldoMm(1)));  // sem ET0, o saldo não desce
  b.fecharDia(0, 5);
  TEST_ASSERT_TRUE(b.confiavel());
}

void test_salvar_e_recuperar() {
  BalancoHidrico a, b;
  parametros(a);
  parametros(b);
  a.fecharDia(0, 5);
  a.rega(4, 1800);
  BalancoHidrico::Salvo salvo = a.salvar();
  b.recuperar(salvo);
  TEST_ASSERT_TRUE(b.confiavel());
  for (uint8_t s = 1; s <= NUM_SETORES; s++) TEST_ASSERT_TRUE(perto(a.saldoMm(s), b.saldoMm(s)));
  salvo.saldoMm[0] = 999;   // memória corrompida: fica dentro dos limites
  salvo.saldoMm[1] = NAN;
  b.recuperar(salvo);
  TEST_ASSERT_TRUE(perto(40, b.saldoMm(1)));
  TEST_ASSERT_TRUE(perto(40, b.saldoMm(2)));
  salvo.versao = 9;  // formato desconhecido: ignora
  salvo.saldoMm[2] = 1;
  b.recuperar(salvo);
  TEST_ASSERT_TRUE(b.saldoMm(3) > 30);
}

void test_capacidade_menor_corta_o_saldo() {
  BalancoHidrico b;
  parametros(b, 40);
  parametros(b, 25);
  TEST_ASSERT_TRUE(perto(25, b.saldoMm(1)));
  TEST_ASSERT_EQUAL(100, b.saldoPct(1));
}

void test_chuva_das_ultimas_24_horas() {
  Chuva24h c;
  uint32_t h = 230000;
  TEST_ASSERT_TRUE(c.total(h) == 0);
  c.somar(h, 2.0f);
  c.somar(h, 1.0f);
  c.somar(h + 5, 4.0f);
  TEST_ASSERT_TRUE(perto(7, c.total(h + 5)));
  TEST_ASSERT_TRUE(perto(7, c.total(h + 23)));
  TEST_ASSERT_TRUE(perto(4, c.total(h + 24)));   // a primeira hora saiu da janela
  TEST_ASSERT_TRUE(c.total(h + 29) == 0);
  c.somar(h + 24, 0.4f);  // mesma gaveta de ontem: começa do zero
  TEST_ASSERT_TRUE(perto(4.4f, c.total(h + 24)));
  TEST_ASSERT_TRUE(perto(0.4f, c.total(h + 47)));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_dia_do_ano);
  RUN_TEST(test_radiacao_confere_com_a_fao);
  RUN_TEST(test_et0_hargreaves);
  RUN_TEST(test_comeca_cheio_e_sem_interferir);
  RUN_TEST(test_dia_seco_gasta_agua);
  RUN_TEST(test_chuva_fraca_nao_conta_e_chuva_forte_enche);
  RUN_TEST(test_rega_repoe_agua);
  RUN_TEST(test_tempo_necessario);
  RUN_TEST(test_ajustar_so_economiza);
  RUN_TEST(test_sem_et0_nao_interfere);
  RUN_TEST(test_salvar_e_recuperar);
  RUN_TEST(test_capacidade_menor_corta_o_saldo);
  RUN_TEST(test_chuva_das_ultimas_24_horas);
  return UNITY_END();
}
