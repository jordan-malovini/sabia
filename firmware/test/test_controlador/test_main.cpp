// Testes da fila de setores e da sequência válvula/bomba.
// Rodar no PC: pio test -e native

#include <unity.h>

#include "Controlador.h"

using namespace sabia;

// Registra as saídas e falha o teste se alguma regra de segurança for quebrada.
class SaidasFalsas : public Saidas {
 public:
  bool valvulas[NUM_SETORES + 1] = {};
  bool bombaLigada = false;
  int aberturas[NUM_SETORES + 1] = {};

  void valvula(uint8_t setor, bool aberta) override {
    TEST_ASSERT_TRUE_MESSAGE(setor >= 1 && setor <= NUM_SETORES, "setor fora da faixa");
    if (!aberta) {
      TEST_ASSERT_FALSE_MESSAGE(bombaLigada && valvulas[setor], "valvula fechou com a bomba ligada");
    } else {
      TEST_ASSERT_EQUAL_MESSAGE(0, abertas(), "duas valvulas abertas ao mesmo tempo");
      aberturas[setor]++;
    }
    valvulas[setor] = aberta;
  }

  void bomba(bool ligada) override {
    if (ligada) TEST_ASSERT_EQUAL_MESSAGE(1, abertas(), "bomba ligou sem valvula aberta");
    bombaLigada = ligada;
  }

  int abertas() const {
    int n = 0;
    for (uint8_t s = 1; s <= NUM_SETORES; s++) n += valvulas[s] ? 1 : 0;
    return n;
  }
};

static Config configTeste() {
  Config c;
  c.atrasoBombaMs = 2000;
  c.atrasoFechamentoMs = 1000;
  c.pausaEntreSetoresMs = 3000;
  c.tempoMaximoS = 600;
  return c;
}

// Avança o tempo de 100 em 100 ms, como o loop faria.
static uint32_t agora = 0;
static void avancar(Controlador& c, uint32_t ms) {
  for (uint32_t t = 0; t < ms; t += 100) c.tick(agora += 100);
}

void setUp() { agora = 0; }
void tearDown() {}

void test_sequencia_valvula_bomba() {
  SaidasFalsas s;
  Controlador c(s, configTeste());
  c.iniciar();

  TEST_ASSERT_EQUAL(Resultado::Aceito, c.pedir(3, 10));
  c.tick(agora);
  TEST_ASSERT_TRUE(s.valvulas[3]);
  TEST_ASSERT_FALSE(s.bombaLigada);

  avancar(c, 1900);
  TEST_ASSERT_FALSE(s.bombaLigada);  // ainda dentro dos 2 s
  avancar(c, 100);
  TEST_ASSERT_TRUE(s.bombaLigada);
  TEST_ASSERT_EQUAL(Estado::Regando, c.estado());

  avancar(c, 10000);
  TEST_ASSERT_FALSE(s.bombaLigada);  // bomba desliga primeiro
  TEST_ASSERT_TRUE(s.valvulas[3]);   // válvula ainda aberta

  avancar(c, 1000);
  TEST_ASSERT_FALSE(s.valvulas[3]);
  TEST_ASSERT_EQUAL(Estado::Pausa, c.estado());

  avancar(c, 3000);
  TEST_ASSERT_EQUAL(Estado::Ocioso, c.estado());
}

void test_fila_um_setor_por_vez() {
  SaidasFalsas s;
  Controlador c(s, configTeste());
  c.iniciar();

  c.pedir(1, 5);
  c.pedir(2, 5);
  c.pedir(3, 5);
  c.tick(agora);
  TEST_ASSERT_EQUAL(1, c.setorAtivo());
  TEST_ASSERT_EQUAL(2, c.tamanhoFila());

  avancar(c, 60000);  // tempo de sobra para os três
  TEST_ASSERT_EQUAL(1, s.aberturas[1]);
  TEST_ASSERT_EQUAL(1, s.aberturas[2]);
  TEST_ASSERT_EQUAL(1, s.aberturas[3]);
  TEST_ASSERT_EQUAL(Estado::Ocioso, c.estado());
}

void test_setor_repetido_nao_entra() {
  SaidasFalsas s;
  Controlador c(s, configTeste());
  c.iniciar();

  c.pedir(4, 5);
  c.tick(agora);
  TEST_ASSERT_EQUAL(Resultado::JaNaFila, c.pedir(4, 5));  // já regando
  c.pedir(5, 5);
  TEST_ASSERT_EQUAL(Resultado::JaNaFila, c.pedir(5, 5));  // já na fila
}

void test_pedidos_invalidos() {
  SaidasFalsas s;
  Controlador c(s, configTeste());
  c.iniciar();

  TEST_ASSERT_EQUAL(Resultado::SetorInvalido, c.pedir(0, 5));
  TEST_ASSERT_EQUAL(Resultado::SetorInvalido, c.pedir(9, 5));
  TEST_ASSERT_EQUAL(Resultado::DuracaoInvalida, c.pedir(1, 0));
}

void test_tempo_maximo() {
  SaidasFalsas s;
  Controlador c(s, configTeste());  // máximo de 600 s
  c.iniciar();

  c.pedir(1, 99999);
  TEST_ASSERT_EQUAL(600, c.itemFila(0).duracaoS);
}

void test_pular_segue_a_fila() {
  SaidasFalsas s;
  Controlador c(s, configTeste());
  c.iniciar();

  c.pedir(1, 100);
  c.pedir(2, 100);
  c.tick(agora);
  avancar(c, 5000);
  TEST_ASSERT_TRUE(s.bombaLigada);

  c.pular();
  avancar(c, 100);
  TEST_ASSERT_FALSE(s.bombaLigada);
  avancar(c, 1000 + 3000 + 100);
  TEST_ASSERT_EQUAL(2, c.setorAtivo());
}

void test_pular_antes_da_bomba() {
  SaidasFalsas s;
  Controlador c(s, configTeste());
  c.iniciar();

  c.pedir(6, 100);
  c.tick(agora);
  avancar(c, 500);
  c.pular();
  avancar(c, 5000);
  TEST_ASSERT_FALSE(s.valvulas[6]);
  TEST_ASSERT_FALSE(s.bombaLigada);
  TEST_ASSERT_EQUAL(Estado::Ocioso, c.estado());
}

void test_parar_tudo() {
  SaidasFalsas s;
  Controlador c(s, configTeste());
  c.iniciar();

  c.pedir(1, 100);
  c.pedir(2, 100);
  c.pedir(3, 100);
  c.tick(agora);
  avancar(c, 3000);

  c.pararTudo();
  avancar(c, 10000);
  TEST_ASSERT_EQUAL(0, c.tamanhoFila());
  TEST_ASSERT_EQUAL(0, s.abertas());
  TEST_ASSERT_FALSE(s.bombaLigada);
  TEST_ASSERT_EQUAL(0, s.aberturas[2]);
}

void test_parar_setor_da_fila() {
  SaidasFalsas s;
  Controlador c(s, configTeste());
  c.iniciar();

  c.pedir(1, 5);
  c.pedir(2, 5);
  c.pedir(3, 5);
  c.tick(agora);
  TEST_ASSERT_TRUE(c.parar(2));
  avancar(c, 60000);
  TEST_ASSERT_EQUAL(0, s.aberturas[2]);
  TEST_ASSERT_EQUAL(1, s.aberturas[3]);
}

void test_ciclo_pula_setores_zerados() {
  SaidasFalsas s;
  Controlador c(s, configTeste());
  c.iniciar();

  uint32_t duracoes[NUM_SETORES] = {5, 0, 5, 0, 5, 0, 0, 5};
  TEST_ASSERT_EQUAL(4, c.iniciarCiclo(duracoes));
  avancar(c, 120000);
  TEST_ASSERT_EQUAL(1, s.aberturas[1]);
  TEST_ASSERT_EQUAL(0, s.aberturas[2]);
  TEST_ASSERT_EQUAL(1, s.aberturas[8]);
}

void test_millis_estourando() {
  // millis() volta a zero depois de ~49 dias; a conta não pode quebrar.
  SaidasFalsas s;
  Controlador c(s, configTeste());
  c.iniciar();

  agora = 0xFFFFFFFFUL - 1500;
  c.pedir(1, 5);
  c.tick(agora);
  avancar(c, 2000);
  TEST_ASSERT_TRUE(s.bombaLigada);
  avancar(c, 5000);
  TEST_ASSERT_FALSE(s.bombaLigada);
}

void test_restante() {
  SaidasFalsas s;
  Controlador c(s, configTeste());
  c.iniciar();

  c.pedir(1, 30);
  c.tick(agora);
  TEST_ASSERT_EQUAL(30, c.restanteS());
  avancar(c, 2000 + 10000);
  TEST_ASSERT_EQUAL(20, c.restanteS());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_sequencia_valvula_bomba);
  RUN_TEST(test_fila_um_setor_por_vez);
  RUN_TEST(test_setor_repetido_nao_entra);
  RUN_TEST(test_pedidos_invalidos);
  RUN_TEST(test_tempo_maximo);
  RUN_TEST(test_pular_segue_a_fila);
  RUN_TEST(test_pular_antes_da_bomba);
  RUN_TEST(test_parar_tudo);
  RUN_TEST(test_parar_setor_da_fila);
  RUN_TEST(test_ciclo_pula_setores_zerados);
  RUN_TEST(test_millis_estourando);
  RUN_TEST(test_restante);
  return UNITY_END();
}
