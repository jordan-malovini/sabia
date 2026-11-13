// Testes da retomada depois de uma queda de energia.
// Rodar no PC: pio test -e native

#include <unity.h>

#include "Retomada.h"

using namespace sabia;

class SaidasSimples : public Saidas {
 public:
  bool bombaLigada = false;
  int abertas = 0;
  bool estado[NUM_SETORES + 1] = {};
  void valvula(uint8_t setor, bool aberta) override {
    if (estado[setor] != aberta) abertas += aberta ? 1 : -1;
    estado[setor] = aberta;
  }
  void bomba(bool ligada) override { bombaLigada = ligada; }
};

static Config configTeste() {
  Config c;
  c.atrasoBombaMs = 2000;
  c.atrasoFechamentoMs = 1000;
  c.pausaEntreSetoresMs = 3000;
  c.sobreposicaoMs = 2000;
  c.tempoMaximoS = 3600;
  return c;
}

static uint32_t agora = 0;
static void avancar(Controlador& c, uint32_t ms) {
  for (uint32_t t = 0; t < ms; t += 100) c.tick(agora += 100);
}

// Um controlador regando o setor 3 (600 s) com os setores 5 e 6 na fila.
static void montarRega(Controlador& c) {
  c.iniciar();
  c.pedir(3, 600);
  c.pedir(5, 300);
  c.pedir(6, 120);
  c.tick(agora);
}

void setUp() { agora = 0; }
void tearDown() {}

void test_captura_setor_atual_e_fila() {
  SaidasSimples s; Controlador c(s, configTeste()); montarRega(c);
  avancar(c, 2000 + 100000);  // bomba ligada há 100 s
  PontoSalvo p = capturarPonto(c, 1000);
  TEST_ASSERT_EQUAL(3, p.n);
  TEST_ASSERT_EQUAL(3, p.itens[0].setor);
  TEST_ASSERT_EQUAL(500, p.itens[0].duracaoS);  // faltavam 500 s
  TEST_ASSERT_EQUAL(5, p.itens[1].setor);
  TEST_ASSERT_EQUAL(300, p.itens[1].duracaoS);
  TEST_ASSERT_EQUAL(6, p.itens[2].setor);
  TEST_ASSERT_EQUAL(1000, p.minutoAbs);
}

void test_captura_parado_e_vazia() {
  SaidasSimples s; Controlador c(s, configTeste()); c.iniciar();
  TEST_ASSERT_EQUAL(0, capturarPonto(c, 1000).n);
}

void test_captura_durante_a_troca() {
  SaidasSimples s; Controlador c(s, configTeste()); montarRega(c);
  avancar(c, 2000 + 600000 + 500);  // setor 3 terminou, trocando para o 5
  TEST_ASSERT_EQUAL(Estado::Trocando, c.estado());
  PontoSalvo p = capturarPonto(c, 1000);
  TEST_ASSERT_EQUAL(2, p.n);
  TEST_ASSERT_EQUAL(5, p.itens[0].setor);  // o 3 já terminou: não entra
  TEST_ASSERT_EQUAL(300, p.itens[0].duracaoS);
  TEST_ASSERT_EQUAL(6, p.itens[1].setor);
}

void test_captura_no_fim_nao_repete_o_setor() {
  SaidasSimples s; Controlador c(s, configTeste()); c.iniciar();
  c.pedir(2, 10);
  c.tick(agora);
  avancar(c, 2000 + 10000 + 300);  // bomba desligou, válvula ainda fechando
  TEST_ASSERT_EQUAL(Estado::FechandoValvula, c.estado());
  TEST_ASSERT_EQUAL(0, capturarPonto(c, 1000).n);
}

void test_espera_e_retoma_com_o_tempo_que_faltava() {
  SaidasSimples s; Controlador c(s, configTeste()); montarRega(c);
  avancar(c, 2000 + 100000);
  PontoSalvo p = capturarPonto(c, 1000);

  // A energia cai e volta 5 minutos depois: controlador novo, tudo desligado.
  SaidasSimples s2; Controlador c2(s2, configTeste()); c2.iniciar();
  Retomada r;  // espera de 600 s, limite de 120 min
  agora = 0;
  r.iniciar(p, 1005, agora);
  TEST_ASSERT_TRUE(r.aguardando());
  TEST_ASSERT_EQUAL(5, r.minutosSemEnergia());
  TEST_ASSERT_EQUAL(3, r.primeiroSetor());
  TEST_ASSERT_EQUAL(600, r.faltamS(agora));

  for (int i = 0; i < 5990; i++) { agora += 100; TEST_ASSERT_FALSE(r.tick(c2, agora)); c2.tick(agora); }
  TEST_ASSERT_EQUAL(0, s2.abertas);           // nada liga durante a espera
  TEST_ASSERT_FALSE(s2.bombaLigada);
  agora += 1000;
  TEST_ASSERT_TRUE(r.tick(c2, agora));        // passou a espera: retoma
  TEST_ASSERT_EQUAL(EstadoRetomada::Retomada, r.estado());
  c2.tick(agora);
  TEST_ASSERT_EQUAL(3, c2.setorAtivo());
  TEST_ASSERT_EQUAL(500, c2.restanteS());     // o tempo que faltava, não o tempo todo
  TEST_ASSERT_EQUAL(2, c2.tamanhoFila());
  TEST_ASSERT_EQUAL(5, c2.itemFila(0).setor);
  TEST_ASSERT_FALSE(r.tick(c2, agora + 100)); // só retoma uma vez
}

void test_retomar_agora_pula_a_espera() {
  SaidasSimples s; Controlador c(s, configTeste()); c.iniciar();
  PontoSalvo p; p.n = 1; p.itens[0] = {4, 200}; p.minutoAbs = 1000;
  Retomada r;
  r.iniciar(p, 1001, agora);
  TEST_ASSERT_FALSE(r.tick(c, agora += 1000));
  r.retomarAgora();
  TEST_ASSERT_TRUE(r.tick(c, agora += 100));
  c.tick(agora);
  TEST_ASSERT_EQUAL(4, c.setorAtivo());
}

void test_cancelar_nao_rega() {
  SaidasSimples s; Controlador c(s, configTeste()); c.iniciar();
  PontoSalvo p; p.n = 1; p.itens[0] = {4, 200}; p.minutoAbs = 1000;
  Retomada r;
  r.iniciar(p, 1001, agora);
  r.cancelar();
  TEST_ASSERT_EQUAL(EstadoRetomada::Cancelada, r.estado());
  TEST_ASSERT_FALSE(r.tick(c, agora += 700000));
  c.tick(agora);
  TEST_ASSERT_EQUAL(0, c.setorAtivo());
}

void test_energia_fora_por_muito_tempo_descarta() {
  PontoSalvo p; p.n = 1; p.itens[0] = {4, 200}; p.minutoAbs = 1000;
  Retomada r;
  r.iniciar(p, 1000 + 120, agora);  // exatamente no limite: ainda retoma
  TEST_ASSERT_TRUE(r.aguardando());
  r.iniciar(p, 1000 + 121, agora);  // passou do limite
  TEST_ASSERT_EQUAL(EstadoRetomada::Descartada, r.estado());
  r.iniciar(p, 900, agora);         // relógio voltou no tempo
  TEST_ASSERT_EQUAL(EstadoRetomada::Descartada, r.estado());
}

void test_sem_relogio_retoma_mesmo_assim() {
  PontoSalvo p; p.n = 1; p.itens[0] = {4, 200}; p.minutoAbs = 0;
  Retomada r;
  r.iniciar(p, 0, agora);
  TEST_ASSERT_TRUE(r.aguardando());
}

void test_ponto_vazio_invalido_ou_desligada() {
  Retomada r;
  PontoSalvo vazio;
  r.iniciar(vazio, 1000, agora);
  TEST_ASSERT_EQUAL(EstadoRetomada::Inativa, r.estado());
  PontoSalvo ruim; ruim.n = 1; ruim.itens[0] = {4, 200}; ruim.versao = 99;
  r.iniciar(ruim, 1000, agora);
  TEST_ASSERT_EQUAL(EstadoRetomada::Inativa, r.estado());
  ConfigRetomada cfg; cfg.ativa = false;
  Retomada desligada(cfg);
  PontoSalvo p; p.n = 1; p.itens[0] = {4, 200};
  desligada.iniciar(p, 1000, agora);
  TEST_ASSERT_EQUAL(EstadoRetomada::Inativa, desligada.estado());
}

void test_pontos_iguais_ignora_a_hora() {
  PontoSalvo a; a.n = 1; a.itens[0] = {4, 200}; a.minutoAbs = 10;
  PontoSalvo b = a; b.minutoAbs = 99;
  TEST_ASSERT_TRUE(pontosIguais(a, b));
  b.itens[0].duracaoS = 199;
  TEST_ASSERT_FALSE(pontosIguais(a, b));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_captura_setor_atual_e_fila);
  RUN_TEST(test_captura_parado_e_vazia);
  RUN_TEST(test_captura_durante_a_troca);
  RUN_TEST(test_captura_no_fim_nao_repete_o_setor);
  RUN_TEST(test_espera_e_retoma_com_o_tempo_que_faltava);
  RUN_TEST(test_retomar_agora_pula_a_espera);
  RUN_TEST(test_cancelar_nao_rega);
  RUN_TEST(test_energia_fora_por_muito_tempo_descarta);
  RUN_TEST(test_sem_relogio_retoma_mesmo_assim);
  RUN_TEST(test_ponto_vazio_invalido_ou_desligada);
  RUN_TEST(test_pontos_iguais_ignora_a_hora);
  return UNITY_END();
}
