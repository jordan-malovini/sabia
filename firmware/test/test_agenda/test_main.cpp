// Testes da agenda de regas (programas A, B e C).
// Rodar no PC: pio test -e native

#include <unity.h>

#include "Agenda.h"

using namespace sabia;

class SaidasSimples : public Saidas {
 public:
  int aberturas[NUM_SETORES + 1] = {};
  void valvula(uint8_t setor, bool aberta) override { if (aberta) aberturas[setor]++; }
  void bomba(bool) override {}
};

static Config configRapida() {
  Config c;
  c.atrasoBombaMs = 100;
  c.atrasoFechamentoMs = 100;
  c.pausaEntreSetoresMs = 100;
  c.sobreposicaoMs = 100;
  c.tempoMaximoS = 3UL * 3600UL;
  return c;
}

static Momento em(uint16_t ano, uint8_t mes, uint8_t dia, uint8_t hora, uint8_t minuto) {
  Momento m;
  m.ano = ano; m.mes = mes; m.dia = dia; m.hora = hora; m.minuto = minuto;
  return m;
}

static uint32_t relogioMs = 0;
// Deixa o controlador terminar tudo o que está na fila.
static void terminarRegas(Controlador& c) {
  c.pararTudo();
  for (int i = 0; i < 100; i++) c.tick(relogioMs += 100);
}
static bool regando(Controlador& c) { return c.setorAtivo() != 0 || c.tamanhoFila() != 0; }

// Programa A: todos os dias às 06:00, 1 min nos setores 1 e 2.
static void programaPadrao(Agenda& a) {
  Programa& p = a.programa(0);
  p.ativo = true;
  p.inicios[0] = 6 * 60;
  p.minutosSetor[0] = 1;
  p.minutosSetor[1] = 1;
}

void setUp() { relogioMs = 0; }
void tearDown() {}

void test_calendario() {
  TEST_ASSERT_EQUAL(0, diasDesde2000(2000, 1, 1));
  TEST_ASSERT_EQUAL(6, diaDaSemana(diasDesde2000(2000, 1, 1)));    // sábado
  TEST_ASSERT_EQUAL(4, diaDaSemana(diasDesde2000(2026, 11, 12)));  // quinta
  TEST_ASSERT_EQUAL(0, diaDaSemana(diasDesde2000(2026, 11, 15)));  // domingo
  TEST_ASSERT_EQUAL(29, diaDoMes(diasDesde2000(2028, 2, 29)));     // ano bissexto
  TEST_ASSERT_EQUAL(1, diaDoMes(diasDesde2000(2028, 3, 1)));
  TEST_ASSERT_EQUAL(31, diaDoMes(diasDesde2000(2026, 12, 31)));
  TEST_ASSERT_EQUAL(1, diasDesde2000(2027, 1, 1) - diasDesde2000(2026, 12, 31));
}

void test_dispara_no_horario() {
  SaidasSimples s; Controlador c(s, configRapida()); c.iniciar();
  Agenda a(c); programaPadrao(a);
  a.tick(em(2026, 11, 12, 5, 59));
  TEST_ASSERT_FALSE(regando(c));
  a.tick(em(2026, 11, 12, 6, 0));
  c.tick(relogioMs += 100);
  TEST_ASSERT_EQUAL(1, c.setorAtivo());
  TEST_ASSERT_EQUAL(1, c.tamanhoFila());  // setor 2 esperando
  a.tick(em(2026, 11, 12, 6, 0));         // o mesmo minuto não dispara de novo
  TEST_ASSERT_EQUAL(0, a.pendentes());
}

// O defeito do controlador antigo: regava um dia e não regava no seguinte,
// porque a marca do último início não incluía a data.
void test_rega_todos_os_dias_no_mesmo_horario() {
  SaidasSimples s; Controlador c(s, configRapida()); c.iniciar();
  Agenda a(c); programaPadrao(a);
  a.tick(em(2026, 11, 12, 5, 59));
  for (uint8_t dia = 12; dia <= 20; dia++) {
    a.tick(em(2026, 11, dia, 5, 59));
    a.tick(em(2026, 11, dia, 6, 0));
    c.tick(relogioMs += 100);
    TEST_ASSERT_TRUE_MESSAGE(regando(c), "deveria regar todos os dias as 06:00");
    terminarRegas(c);
    a.tick(em(2026, 11, dia, 6, 5));
    TEST_ASSERT_FALSE(regando(c));
  }
  TEST_ASSERT_EQUAL(9, s.aberturas[1]);
}

void test_nao_precisa_do_minuto_exato() {
  SaidasSimples s; Controlador c(s, configRapida()); c.iniciar();
  Agenda a(c); programaPadrao(a);
  a.tick(em(2026, 11, 12, 5, 58));
  a.tick(em(2026, 11, 12, 6, 3));  // o loop ficou ocupado e perdeu o minuto das 06:00
  c.tick(relogioMs += 100);
  TEST_ASSERT_TRUE(regando(c));
}

void test_dias_da_semana() {
  SaidasSimples s; Controlador c(s, configRapida()); c.iniciar();
  Agenda a(c); programaPadrao(a);
  a.programa(0).diasSemana = (1 << 1) | (1 << 4);  // segunda e quinta
  a.tick(em(2026, 11, 12, 5, 59));
  a.tick(em(2026, 11, 12, 6, 0));  // quinta
  c.tick(relogioMs += 100);
  TEST_ASSERT_TRUE(regando(c));
  terminarRegas(c);
  a.tick(em(2026, 11, 13, 5, 59));
  a.tick(em(2026, 11, 13, 6, 0));  // sexta
  c.tick(relogioMs += 100);
  TEST_ASSERT_FALSE(regando(c));
  a.tick(em(2026, 11, 16, 5, 59));
  a.tick(em(2026, 11, 16, 6, 0));  // segunda
  c.tick(relogioMs += 100);
  TEST_ASSERT_TRUE(regando(c));
}

void test_dias_impares_pares_e_intervalo() {
  Programa p;
  p.modo = ModoDias::Impares;
  TEST_ASSERT_TRUE(diaPermitido(p, diasDesde2000(2026, 11, 13)));
  TEST_ASSERT_FALSE(diaPermitido(p, diasDesde2000(2026, 11, 12)));
  p.modo = ModoDias::Pares;
  TEST_ASSERT_TRUE(diaPermitido(p, diasDesde2000(2026, 11, 12)));
  p.modo = ModoDias::Intervalo;
  p.intervaloDias = 3;
  p.diaReferencia = diasDesde2000(2026, 11, 12);
  TEST_ASSERT_TRUE(diaPermitido(p, diasDesde2000(2026, 11, 12)));
  TEST_ASSERT_FALSE(diaPermitido(p, diasDesde2000(2026, 11, 13)));
  TEST_ASSERT_FALSE(diaPermitido(p, diasDesde2000(2026, 11, 14)));
  TEST_ASSERT_TRUE(diaPermitido(p, diasDesde2000(2026, 11, 15)));
  TEST_ASSERT_TRUE(diaPermitido(p, diasDesde2000(2026, 12, 3)));  // 21 dias depois
}

void test_varios_horarios_no_mesmo_programa() {
  SaidasSimples s; Controlador c(s, configRapida()); c.iniciar();
  Agenda a(c); programaPadrao(a);
  a.programa(0).inicios[1] = 18 * 60;
  a.tick(em(2026, 11, 12, 5, 59));
  a.tick(em(2026, 11, 12, 6, 0));
  c.tick(relogioMs += 100);
  terminarRegas(c);
  a.tick(em(2026, 11, 12, 17, 59));
  a.tick(em(2026, 11, 12, 18, 0));
  c.tick(relogioMs += 100);
  TEST_ASSERT_TRUE(regando(c));
  TEST_ASSERT_EQUAL(2, s.aberturas[1]);
}

void test_empilha_programa_que_comeca_com_outro_regando() {
  SaidasSimples s; Controlador c(s, configRapida()); c.iniciar();
  Agenda a(c); programaPadrao(a);
  Programa& b = a.programa(1);
  b.ativo = true;
  b.inicios[0] = 6 * 60 + 1;  // um minuto depois do A
  b.minutosSetor[0] = 2;      // usa o mesmo setor 1
  b.minutosSetor[4] = 2;
  a.tick(em(2026, 11, 12, 5, 59));
  a.tick(em(2026, 11, 12, 6, 0));
  c.tick(relogioMs += 100);
  a.tick(em(2026, 11, 12, 6, 1));
  TEST_ASSERT_EQUAL_MESSAGE(1, a.pendentes(), "o programa B deve esperar o A terminar");
  TEST_ASSERT_EQUAL(1, c.tamanhoFila());  // só o setor 2 do A
  terminarRegas(c);
  a.tick(em(2026, 11, 12, 6, 2));
  c.tick(relogioMs += 100);
  TEST_ASSERT_EQUAL(0, a.pendentes());
  TEST_ASSERT_EQUAL(1, c.setorAtivo());   // agora o setor 1 do B
  TEST_ASSERT_EQUAL(5, c.itemFila(0).setor);
}

void test_suspensao_por_chuva() {
  SaidasSimples s; Controlador c(s, configRapida()); c.iniciar();
  Agenda a(c); programaPadrao(a);
  a.tick(em(2026, 11, 12, 5, 0));
  a.suspender(2, em(2026, 11, 12, 5, 0));  // hoje e amanhã
  TEST_ASSERT_EQUAL(2, a.diasSuspensos(em(2026, 11, 12, 5, 0)));
  a.tick(em(2026, 11, 12, 5, 59)); a.tick(em(2026, 11, 12, 6, 0)); c.tick(relogioMs += 100);
  TEST_ASSERT_FALSE(regando(c));
  a.tick(em(2026, 11, 13, 5, 59)); a.tick(em(2026, 11, 13, 6, 0)); c.tick(relogioMs += 100);
  TEST_ASSERT_FALSE(regando(c));
  TEST_ASSERT_EQUAL(2, a.puladosPorChuva());
  TEST_ASSERT_EQUAL(0, a.diasSuspensos(em(2026, 11, 14, 0, 0)));
  a.tick(em(2026, 11, 14, 5, 59)); a.tick(em(2026, 11, 14, 6, 0)); c.tick(relogioMs += 100);
  TEST_ASSERT_TRUE(regando(c));  // volta sozinho no terceiro dia
}

void test_cancelar_suspensao() {
  SaidasSimples s; Controlador c(s, configRapida()); c.iniciar();
  Agenda a(c); programaPadrao(a);
  a.suspender(5, em(2026, 11, 12, 5, 0));
  a.suspender(0, em(2026, 11, 12, 5, 30));
  a.tick(em(2026, 11, 12, 5, 59)); a.tick(em(2026, 11, 12, 6, 0)); c.tick(relogioMs += 100);
  TEST_ASSERT_TRUE(regando(c));
}

void test_ajuste_percentual() {
  SaidasSimples s; Controlador c(s, configRapida()); c.iniciar();
  Agenda a(c);
  Programa& p = a.programa(0);
  p.ativo = true;
  p.minutosSetor[0] = 20;
  p.minutosSetor[1] = 10;
  p.ajustePct = 50;
  TEST_ASSERT_EQUAL(15 * 60, a.duracaoTotalS(0));
  TEST_ASSERT_TRUE(a.iniciarPrograma(0));
  c.tick(relogioMs += 100);
  TEST_ASSERT_EQUAL(10 * 60, c.restanteS());         // setor 1: 20 min x 50 %
  TEST_ASSERT_EQUAL(5 * 60, c.itemFila(0).duracaoS);  // setor 2: 10 min x 50 %
  p.ajustePct = 150;
  TEST_ASSERT_EQUAL(45 * 60, a.duracaoTotalS(0));
}

void test_programa_vazio_ou_inativo_nao_roda() {
  SaidasSimples s; Controlador c(s, configRapida()); c.iniciar();
  Agenda a(c);
  TEST_ASSERT_FALSE(a.iniciarPrograma(0));  // sem setores
  programaPadrao(a);
  a.programa(0).ativo = false;
  a.tick(em(2026, 11, 12, 5, 59)); a.tick(em(2026, 11, 12, 6, 0)); c.tick(relogioMs += 100);
  TEST_ASSERT_FALSE(regando(c));
  TEST_ASSERT_TRUE(a.iniciarPrograma(0));   // manualmente pode, mesmo inativo
}

void test_ajuste_do_relogio_nao_dispara_atrasados() {
  SaidasSimples s; Controlador c(s, configRapida()); c.iniciar();
  Agenda a(c); programaPadrao(a);
  a.tick(em(2026, 11, 12, 5, 0));
  a.tick(em(2026, 11, 12, 9, 0));   // relógio adiantado 4 h: o das 06:00 não dispara
  c.tick(relogioMs += 100);
  TEST_ASSERT_FALSE(regando(c));
  a.tick(em(2026, 11, 10, 5, 59));  // relógio atrasado 2 dias: nada quebra
  a.tick(em(2026, 11, 10, 6, 0));
  c.tick(relogioMs += 100);
  TEST_ASSERT_TRUE(regando(c));
}

void test_proximo_inicio() {
  SaidasSimples s; Controlador c(s, configRapida()); c.iniciar();
  Agenda a(c);
  uint8_t prog = 9; uint16_t minuto = 0; uint32_t faltam = 0;
  TEST_ASSERT_FALSE(a.proximoInicio(em(2026, 11, 12, 10, 0), prog, minuto, faltam));
  programaPadrao(a);
  Programa& b = a.programa(1);
  b.ativo = true; b.inicios[0] = 18 * 60; b.minutosSetor[2] = 5;
  TEST_ASSERT_TRUE(a.proximoInicio(em(2026, 11, 12, 10, 0), prog, minuto, faltam));
  TEST_ASSERT_EQUAL(1, prog);            // B, hoje às 18:00
  TEST_ASSERT_EQUAL(18 * 60, minuto);
  TEST_ASSERT_EQUAL(8 * 60, faltam);
  TEST_ASSERT_TRUE(a.proximoInicio(em(2026, 11, 12, 19, 0), prog, minuto, faltam));
  TEST_ASSERT_EQUAL(0, prog);            // A, amanhã às 06:00
  TEST_ASSERT_EQUAL(11 * 60, faltam);
  a.suspender(3, em(2026, 11, 12, 19, 0));  // dias 12, 13 e 14
  TEST_ASSERT_TRUE(a.proximoInicio(em(2026, 11, 12, 19, 0), prog, minuto, faltam));
  TEST_ASSERT_EQUAL(0, prog);
  TEST_ASSERT_EQUAL((5 + 48 + 6) * 60, faltam);  // dia 15 às 06:00
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_calendario);
  RUN_TEST(test_dispara_no_horario);
  RUN_TEST(test_rega_todos_os_dias_no_mesmo_horario);
  RUN_TEST(test_nao_precisa_do_minuto_exato);
  RUN_TEST(test_dias_da_semana);
  RUN_TEST(test_dias_impares_pares_e_intervalo);
  RUN_TEST(test_varios_horarios_no_mesmo_programa);
  RUN_TEST(test_empilha_programa_que_comeca_com_outro_regando);
  RUN_TEST(test_suspensao_por_chuva);
  RUN_TEST(test_cancelar_suspensao);
  RUN_TEST(test_ajuste_percentual);
  RUN_TEST(test_programa_vazio_ou_inativo_nao_roda);
  RUN_TEST(test_ajuste_do_relogio_nao_dispara_atrasados);
  RUN_TEST(test_proximo_inicio);
  return UNITY_END();
}
