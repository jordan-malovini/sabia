// Testes do registro do dia e das linhas gravadas no cartão.
// Rodar no PC: pio test -e native

#include <math.h>
#include <string.h>

#include <unity.h>

#include "Agenda.h"
#include "Registro.h"

using namespace sabia;

void setUp() {}
void tearDown() {}

static bool perto(float a, float b) { return fabsf(a - b) < 0.051f; }

void test_dia_vazio() {
  RegistroDia d;
  d.iniciar(100);
  TEST_ASSERT_EQUAL_UINT32(100, d.dia());
  TEST_ASSERT_TRUE(d.chuvaMm() == 0);
  TEST_ASSERT_TRUE(isnan(d.tempMaxC()));
  TEST_ASSERT_TRUE(isnan(d.tempMinC()));
  TEST_ASSERT_EQUAL(0, d.totalEventos());
}

void test_chuva_soma_por_hora() {
  RegistroDia d;
  char linha[TAMANHO_LINHA];
  d.chuva(13 * 60 + 10, 0.4f, linha);
  TEST_ASSERT_EQUAL_STRING("C;13:10;0.4", linha);
  d.chuva(13 * 60 + 50, 1.2f, linha);
  d.chuva(14 * 60, 0.2f, linha);
  TEST_ASSERT_TRUE(perto(1.6f, d.chuvaHoraMm(13)));
  TEST_ASSERT_TRUE(perto(0.2f, d.chuvaHoraMm(14)));
  TEST_ASSERT_TRUE(perto(1.8f, d.chuvaMm()));
  d.chuva(15 * 60, 0, linha);  // sem chuva não grava nada
  TEST_ASSERT_EQUAL_STRING("", linha);
}

void test_clima_guarda_maxima_e_minima() {
  RegistroDia d;
  char linha[TAMANHO_LINHA];
  HoraClima h;
  h.tempMaxC = 31.24f; h.tempMinC = 28.0f; h.umidadePct = 62.4f; h.pressaoHpa = 1009.83f; h.lux = 54321.2f;
  d.clima(14 * 60, h, linha);
  TEST_ASSERT_EQUAL_STRING("T;14:00;31.2;28.0;62;1009.8;54321", linha);
  HoraClima madrugada;
  madrugada.tempMaxC = 21.5f; madrugada.tempMinC = 20.1f;
  d.clima(5 * 60, madrugada, linha);
  TEST_ASSERT_EQUAL_STRING("T;05:00;21.5;20.1;;;", linha);  // sem leitura, o campo fica vazio
  TEST_ASSERT_TRUE(perto(31.2f, d.tempMaxC()));
  TEST_ASSERT_TRUE(perto(20.1f, d.tempMinC()));
  TEST_ASSERT_TRUE(perto(31.2f, d.tempHoraC(14)));
  TEST_ASSERT_TRUE(isnan(d.tempHoraC(10)));
  d.temperatura(15 * 60 + 3, 33.0f);  // leitura avulsa entra na máxima na hora
  TEST_ASSERT_TRUE(perto(33.0f, d.tempMaxC()));
}

void test_rega_conta_vezes_e_tempo() {
  RegistroDia d;
  char linha[TAMANHO_LINHA];
  d.rega(6 * 60 + 40, 4, 2400, linha);
  TEST_ASSERT_EQUAL_STRING("R;06:40;4;2400", linha);
  d.rega(20 * 60, 4, 45, linha);
  d.rega(20 * 60, 9, 100, linha);  // setor que não existe
  TEST_ASSERT_EQUAL_STRING("", linha);
  TEST_ASSERT_EQUAL(2, d.vezes(4));
  TEST_ASSERT_EQUAL_UINT32(2445, d.segundos(4));
  TEST_ASSERT_EQUAL(2, d.totalEventos());
  TEST_ASSERT_EQUAL_STRING("Setor 4 regou 45 s", d.eventoRecente(0).texto);
  TEST_ASSERT_EQUAL_STRING("Setor 4 regou 40 min", d.eventoRecente(1).texto);
}

void test_eventos_guardam_so_os_mais_recentes() {
  RegistroDia d;
  char linha[TAMANHO_LINHA], texto[24];
  for (int i = 1; i <= MAX_EVENTOS + 5; i++) {
    snprintf(texto, sizeof(texto), "evento %d", i);
    d.evento(static_cast<uint16_t>(i), texto, linha);
  }
  TEST_ASSERT_EQUAL(MAX_EVENTOS, d.totalEventos());
  snprintf(texto, sizeof(texto), "evento %d", MAX_EVENTOS + 5);
  TEST_ASSERT_EQUAL_STRING(texto, d.eventoRecente(0).texto);
  TEST_ASSERT_EQUAL_STRING("evento 6", d.eventoRecente(MAX_EVENTOS - 1).texto);
  d.evento(600, "linha\ncom quebra; e ponto e virgula", linha);
  TEST_ASSERT_EQUAL_STRING("E;10:00;linha com quebra; e ponto e virgula", linha);
}

// Depois de um reinício, reler o arquivo de hoje tem de devolver o mesmo dia.
void test_reler_o_arquivo_refaz_o_dia() {
  RegistroDia a, b;
  a.iniciar(9813);
  b.iniciar(9813);
  char linhas[6][TAMANHO_LINHA];
  HoraClima h;
  h.tempMaxC = 30.5f; h.tempMinC = 27.0f; h.umidadePct = 70; h.pressaoHpa = 1010; h.lux = 20000;
  a.evento(1, "Controlador ligado", linhas[0]);
  a.chuva(13 * 60 + 20, 2.2f, linhas[1]);
  a.clima(14 * 60, h, linhas[2]);
  a.rega(20 * 60 + 35, 1, 2100, linhas[3]);
  a.evento(21 * 60, "Ajustes salvos; pela pagina", linhas[4]);
  a.chuva(22 * 60, 0.6f, linhas[5]);
  for (auto& linha : linhas) {
    char comQuebra[TAMANHO_LINHA + 3];
    strcpy(comQuebra, linha);
    strcat(comQuebra, "\r\n");
    TEST_ASSERT_TRUE(b.aplicarLinha(comQuebra));
  }
  TEST_ASSERT_TRUE(perto(a.chuvaMm(), b.chuvaMm()));
  TEST_ASSERT_TRUE(perto(2.2f, b.chuvaHoraMm(13)));
  TEST_ASSERT_TRUE(perto(30.5f, b.tempMaxC()));
  TEST_ASSERT_TRUE(perto(27.0f, b.tempMinC()));
  TEST_ASSERT_EQUAL(1, b.vezes(1));
  TEST_ASSERT_EQUAL_UINT32(2100, b.segundos(1));
  TEST_ASSERT_EQUAL(a.totalEventos(), b.totalEventos());
  TEST_ASSERT_EQUAL_STRING("Ajustes salvos; pela pagina", b.eventoRecente(0).texto);
  TEST_ASSERT_EQUAL_STRING("Setor 1 regou 35 min", b.eventoRecente(1).texto);

  // Lixo no arquivo não derruba nem altera nada.
  TEST_ASSERT_FALSE(b.aplicarLinha(""));
  TEST_ASSERT_FALSE(b.aplicarLinha("X;10:00;1"));
  TEST_ASSERT_FALSE(b.aplicarLinha("C;25:00;1"));
  TEST_ASSERT_FALSE(b.aplicarLinha("R;10:00;3"));
  TEST_ASSERT_FALSE(b.aplicarLinha("qualquer coisa"));
  TEST_ASSERT_TRUE(perto(a.chuvaMm(), b.chuvaMm()));
}

void test_resumo_vai_e_volta() {
  RegistroDia d;
  d.iniciar(diasDesde2000(2026, 11, 13));
  HoraClima h;
  h.tempMaxC = 31.2f; h.tempMinC = 21.4f;
  d.clima(14 * 60, h);
  d.chuva(600, 4.2f);
  d.rega(700, 1, 2100);
  d.rega(800, 8, 2400);
  d.rega(900, 8, 2400);
  char linha[TAMANHO_LINHA];
  formatarResumo(d.resumo(4.6f), linha);
  TEST_ASSERT_EQUAL_STRING("2026-11-13;4.2;31.2;21.4;4.6;1;35;0;0;0;0;0;0;0;0;0;0;0;0;2;80", linha);

  ResumoDia r;
  TEST_ASSERT_TRUE(lerResumo(linha, r));
  TEST_ASSERT_EQUAL_UINT32(d.dia(), r.dia);
  TEST_ASSERT_TRUE(perto(4.2f, r.chuvaMm));
  TEST_ASSERT_TRUE(perto(31.2f, r.tempMaxC));
  TEST_ASSERT_TRUE(perto(4.6f, r.et0Mm));
  TEST_ASSERT_EQUAL(2, r.vezes[7]);
  TEST_ASSERT_EQUAL(80, r.minutos[7]);

  // Dia sem sensor: os campos vazios voltam como "sem dado".
  RegistroDia semSensor;
  semSensor.iniciar(diasDesde2000(2026, 1, 1));
  formatarResumo(semSensor.resumo(NAN), linha);
  TEST_ASSERT_EQUAL_STRING("2026-01-01;0.0;;;;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0", linha);
  TEST_ASSERT_TRUE(lerResumo(linha, r));
  TEST_ASSERT_TRUE(isnan(r.tempMaxC) && isnan(r.et0Mm));

  TEST_ASSERT_FALSE(lerResumo("2026-11-13;4.2", r));
  TEST_ASSERT_FALSE(lerResumo("data;1;2;3;4;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0;0", r));
}

void test_acumular_leituras_da_hora() {
  HoraClima h;
  uint16_t n = 0;
  Leitura l;
  l.tempC = 25; l.umidadePct = 60; l.pressaoHpa = 1010; l.lux = 1000;
  acumular(h, l, n);
  l.tempC = 29; l.umidadePct = 50; l.lux = 3000;
  acumular(h, l, n);
  l.tempC = 27; l.umidadePct = 40; l.lux = 2000;
  acumular(h, l, n);
  TEST_ASSERT_EQUAL(3, n);
  TEST_ASSERT_TRUE(perto(29, h.tempMaxC));
  TEST_ASSERT_TRUE(perto(25, h.tempMinC));
  TEST_ASSERT_TRUE(perto(50, h.umidadePct));
  TEST_ASSERT_TRUE(perto(2000, h.lux));
}

void test_lux_do_veml7700() {
  TEST_ASSERT_TRUE(luxDoVeml7700(0) == 0);
  TEST_ASSERT_TRUE(fabsf(luxDoVeml7700(100) - 184.3f) < 0.1f);
  TEST_ASSERT_TRUE(luxDoVeml7700(65535) > 120000);  // alcança o sol pleno
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_dia_vazio);
  RUN_TEST(test_chuva_soma_por_hora);
  RUN_TEST(test_clima_guarda_maxima_e_minima);
  RUN_TEST(test_rega_conta_vezes_e_tempo);
  RUN_TEST(test_eventos_guardam_so_os_mais_recentes);
  RUN_TEST(test_reler_o_arquivo_refaz_o_dia);
  RUN_TEST(test_resumo_vai_e_volta);
  RUN_TEST(test_acumular_leituras_da_hora);
  RUN_TEST(test_lux_do_veml7700);
  return UNITY_END();
}
