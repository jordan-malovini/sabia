// Testes da conversão entre o JSON da página e os dados do firmware.
// Rodar no PC: pio test -e native

#include <math.h>
#include <string.h>

#include <ArduinoJson.h>
#include <unity.h>

#include "ApiJson.h"

using namespace sabia;

void setUp() {}
void tearDown() {}

class SaidasMudas : public Saidas {
 public:
  void valvula(uint8_t, bool) override {}
  void bomba(bool) override {}
};

// Configuração completa, como a página envia: o padrão mais os 3 programas.
static void configCompleta(JsonDocument& doc) {
  configPadrao(doc);
  JsonArray programas = doc["programas"].to<JsonArray>();
  for (uint8_t i = 0; i < NUM_PROGRAMAS; i++) programaParaJson(Programa(), programas.add<JsonObject>());
}

void test_hora_de_texto() {
  uint16_t m = 0;
  TEST_ASSERT_TRUE(horaDeTexto("00:00", m));
  TEST_ASSERT_EQUAL_UINT16(0, m);
  TEST_ASSERT_TRUE(horaDeTexto("23:59", m));
  TEST_ASSERT_EQUAL_UINT16(1439, m);
  TEST_ASSERT_FALSE(horaDeTexto("24:00", m));
  TEST_ASSERT_FALSE(horaDeTexto("12:60", m));
  TEST_ASSERT_FALSE(horaDeTexto("5:00", m));
  TEST_ASSERT_FALSE(horaDeTexto("05h00", m));
  TEST_ASSERT_FALSE(horaDeTexto("", m));
  TEST_ASSERT_FALSE(horaDeTexto(nullptr, m));
  char texto[6];
  textoDeHora(6 * 60 + 5, texto);
  TEST_ASSERT_EQUAL_STRING("06:05", texto);
}

void test_data_vai_e_volta() {
  const char* datas[] = {"2000-01-01", "2024-02-29", "2026-11-13", "2026-12-31", "2027-01-01", "2099-12-31"};
  for (const char* data : datas) {
    uint32_t dias = 0;
    char volta[11];
    TEST_ASSERT_TRUE(diasDeData(data, dias));
    dataDeDias(dias, volta);
    TEST_ASSERT_EQUAL_STRING(data, volta);
  }
  uint32_t dias = 0;
  TEST_ASSERT_TRUE(diasDeData("2026-11-13", dias));
  TEST_ASSERT_EQUAL_UINT32(diasDesde2000(2026, 11, 13), dias);
  TEST_ASSERT_FALSE(diasDeData("2026-02-29", dias));
  TEST_ASSERT_FALSE(diasDeData("2026-13-01", dias));
  TEST_ASSERT_FALSE(diasDeData("13/11/2026", dias));
  TEST_ASSERT_FALSE(diasDeData(nullptr, dias));
}

void test_programa_vai_e_volta() {
  Programa p;
  p.ativo = true;
  p.inicios[0] = 5 * 60;
  p.inicios[2] = 20 * 60 + 30;
  p.modo = ModoDias::Intervalo;
  p.diasSemana = 0b0101010;
  p.intervaloDias = 3;
  p.diaReferencia = diasDesde2000(2026, 11, 13);
  p.minutosSetor[1] = 40;
  p.minutosSetor[7] = 180;
  p.ajustePct = 80;

  JsonDocument doc;
  programaParaJson(p, doc.to<JsonObject>());
  TEST_ASSERT_EQUAL_STRING("05:00", doc["inicios"][0].as<const char*>());
  TEST_ASSERT_TRUE(doc["inicios"][1].isNull());
  TEST_ASSERT_EQUAL_STRING("intervalo", doc["modo"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("2026-11-13", doc["inicioIntervalo"].as<const char*>());
  TEST_ASSERT_EQUAL(7, doc["dias"].size());
  TEST_ASSERT_EQUAL(8, doc["minutos"].size());

  Programa q;
  TEST_ASSERT_NULL(programaDeJson(doc.as<JsonVariantConst>(), q));
  TEST_ASSERT_TRUE(q.ativo);
  TEST_ASSERT_EQUAL_UINT16(300, q.inicios[0]);
  TEST_ASSERT_EQUAL_UINT16(SEM_HORARIO, q.inicios[1]);
  TEST_ASSERT_EQUAL_UINT16(1230, q.inicios[2]);
  TEST_ASSERT_TRUE(q.modo == ModoDias::Intervalo);
  TEST_ASSERT_EQUAL_UINT8(0b0101010, q.diasSemana);
  TEST_ASSERT_EQUAL_UINT8(3, q.intervaloDias);
  TEST_ASSERT_EQUAL_UINT32(p.diaReferencia, q.diaReferencia);
  TEST_ASSERT_EQUAL_UINT16(40, q.minutosSetor[1]);
  TEST_ASSERT_EQUAL_UINT16(180, q.minutosSetor[7]);
  TEST_ASSERT_EQUAL_UINT8(80, q.ajustePct);
}

void test_programa_como_a_pagina_envia() {
  JsonDocument doc;
  deserializeJson(doc, R"({"ativo":true,"inicios":["05:00",null,null,null],"modo":"semana","dias":[1,0,1,0,1,0,1],
    "intervaloDias":2,"inicioIntervalo":null,"minutos":[0,40,0,40,0,40,0,40],"ajustePct":100})");
  Programa p;
  p.diaReferencia = 1234;
  TEST_ASSERT_NULL(programaDeJson(doc.as<JsonVariantConst>(), p));
  TEST_ASSERT_EQUAL_UINT8(0b1010101, p.diasSemana);
  TEST_ASSERT_EQUAL_UINT32(1234, p.diaReferencia);  // sem data, a referência fica como estava
}

void test_programa_invalido_nao_altera() {
  struct Caso { const char* campo; const char* valor; };
  const Caso casos[] = {
      {"inicios", R"(["25:00",null,null,null])"}, {"inicios", R"(["05:00"])"}, {"inicios", "[300,null,null,null]"},
      {"modo", "\"quinzenal\""}, {"dias", "[1,1,1]"}, {"intervaloDias", "0"}, {"intervaloDias", "31"},
      {"inicioIntervalo", "\"13/11/2026\""}, {"minutos", "[0,0,0,0,0,0,0,181]"}, {"minutos", "[0,0,0]"},
      {"minutos", "[0,0,0,0,0,0,0,2.5]"}, {"ajustePct", "4"}, {"ajustePct", "201"}, {"ativo", "\"sim\""},
  };
  for (const Caso& caso : casos) {
    JsonDocument doc, valor;
    Programa base;
    base.ajustePct = 77;
    programaParaJson(Programa(), doc.to<JsonObject>());
    deserializeJson(valor, caso.valor);
    doc[caso.campo] = valor;
    TEST_ASSERT_NOT_NULL(programaDeJson(doc.as<JsonVariantConst>(), base));
    TEST_ASSERT_EQUAL_UINT8(77, base.ajustePct);
  }
}

void test_config_padrao_e_valida() {
  JsonDocument doc;
  configCompleta(doc);
  TEST_ASSERT_NULL(validarConfig(doc.as<JsonVariantConst>()));
  TEST_ASSERT_EQUAL(8, doc["setores"].size());
  TEST_ASSERT_TRUE(doc["rede"]["wifiSenha"].is<const char*>());
}

void test_config_invalida_diz_o_motivo() {
  JsonDocument doc;

  configCompleta(doc);
  doc["setores"][2]["duracaoMin"] = 90;  // máximo do setor é 60
  TEST_ASSERT_NOT_NULL(strstr(validarConfig(doc.as<JsonVariantConst>()), "Setor 3"));

  configCompleta(doc);
  doc["programas"][1]["minutos"][4] = 61;
  const char* erro = validarConfig(doc.as<JsonVariantConst>());
  TEST_ASSERT_NOT_NULL(erro);
  TEST_ASSERT_NOT_NULL(strstr(erro, "Programa B"));
  TEST_ASSERT_NOT_NULL(strstr(erro, "setor 5"));

  configCompleta(doc);
  doc["programas"][2]["ajustePct"] = 500;
  TEST_ASSERT_NOT_NULL(strstr(validarConfig(doc.as<JsonVariantConst>()), "Programa C"));

  configCompleta(doc);
  doc["bomba"]["sobreposicaoS"] = 31;
  TEST_ASSERT_NOT_NULL(validarConfig(doc.as<JsonVariantConst>()));

  configCompleta(doc);
  doc["rede"]["apSenha"] = "curta";
  TEST_ASSERT_NOT_NULL(validarConfig(doc.as<JsonVariantConst>()));

  configCompleta(doc);
  doc["seguranca"]["novaSenha"] = "12345";
  TEST_ASSERT_NOT_NULL(validarConfig(doc.as<JsonVariantConst>()));
  doc["seguranca"]["novaSenha"] = "123456";
  TEST_ASSERT_NULL(validarConfig(doc.as<JsonVariantConst>()));

  configCompleta(doc);
  doc.remove("setores");
  TEST_ASSERT_NOT_NULL(validarConfig(doc.as<JsonVariantConst>()));

  JsonDocument vazio;
  TEST_ASSERT_NOT_NULL(validarConfig(vazio.as<JsonVariantConst>()));
}

void test_ler_ajustes() {
  JsonDocument doc;
  configCompleta(doc);
  doc["bomba"]["usar"] = false;
  doc["bomba"]["sobreposicaoS"] = 5;
  doc["setores"][0]["tempoMaxMin"] = 90;
  doc["setores"][0]["duracaoMin"] = 45;
  doc["setores"][3]["ativo"] = false;
  doc["retomada"]["esperaMin"] = 15;
  doc["estacao"]["mmPorBasculada"] = 0.25;

  Ajustes a;
  lerAjustes(doc.as<JsonVariantConst>(), a);
  TEST_ASSERT_FALSE(a.usarBomba);
  TEST_ASSERT_EQUAL_UINT32(5000, a.controlador.sobreposicaoMs);
  TEST_ASSERT_EQUAL_UINT32(2000, a.controlador.atrasoBombaMs);
  TEST_ASSERT_EQUAL_UINT32(90 * 60, a.controlador.tempoMaximoSetorS[0]);
  TEST_ASSERT_EQUAL_UINT32(60 * 60, a.controlador.tempoMaximoSetorS[1]);
  TEST_ASSERT_EQUAL_UINT32(90 * 60, a.controlador.tempoMaximoS);
  TEST_ASSERT_TRUE(a.controlador.setorDesativado[3]);
  TEST_ASSERT_FALSE(a.controlador.setorDesativado[0]);
  TEST_ASSERT_EQUAL_UINT16(45, a.duracaoPadraoMin[0]);
  TEST_ASSERT_EQUAL_UINT32(900, a.retomada.esperaS);
  TEST_ASSERT_TRUE(a.mmPorBasculada > 0.249f && a.mmPorBasculada < 0.251f);

  // Documento vazio: tudo no padrão, sem travar.
  JsonDocument vazio;
  lerAjustes(vazio.as<JsonVariantConst>(), a);
  TEST_ASSERT_TRUE(a.usarBomba);
  TEST_ASSERT_EQUAL_UINT32(3600, a.controlador.tempoMaximoS);
  TEST_ASSERT_EQUAL_UINT16(30, a.sessaoMin);
}

void test_mesclar_completa_config_antiga() {
  JsonDocument padrao, antiga;
  configPadrao(padrao);
  deserializeJson(antiga, R"({"bomba":{"sobreposicaoS":7},"setores":[{"nome":"Horta"}],"extra":1})");
  mesclar(padrao.as<JsonVariant>(), antiga.as<JsonVariantConst>());
  TEST_ASSERT_EQUAL(7, padrao["bomba"]["sobreposicaoS"].as<int>());
  TEST_ASSERT_EQUAL(2, padrao["bomba"]["atrasoLigarS"].as<int>());  // o que faltava vem do padrão
  TEST_ASSERT_EQUAL(1, padrao["setores"].size());                   // listas são trocadas inteiras
  TEST_ASSERT_EQUAL(1, padrao["extra"].as<int>());
  TEST_ASSERT_TRUE(padrao["retomada"]["ativa"].as<bool>());
}

void test_estado_da_rega() {
  SaidasMudas saidas;
  Controlador c(saidas);
  c.iniciar();
  Agenda agenda(c);
  Momento agora;
  agora.ano = 2026; agora.mes = 11; agora.dia = 13; agora.hora = 19; agora.minuto = 0;  // sexta

  JsonDocument doc;
  regaParaJson(doc.to<JsonObject>(), c, agenda, true, agora);
  TEST_ASSERT_TRUE(doc["atual"].isNull());
  TEST_ASSERT_EQUAL(0, doc["fila"].size());
  TEST_ASSERT_TRUE(doc["proximaRega"].isNull());

  Programa& b = agenda.programa(1);
  b.ativo = true;
  b.inicios[0] = 20 * 60;
  b.minutosSetor[0] = 10;
  c.pedir(4, 600);
  c.pedir(6, 300);
  c.tick(100);
  c.tick(2200);  // a bomba liga depois de 2 s

  regaParaJson(doc.to<JsonObject>(), c, agenda, true, agora);
  TEST_ASSERT_EQUAL(4, doc["atual"]["setor"].as<int>());
  TEST_ASSERT_EQUAL_STRING("regando", doc["atual"]["fase"].as<const char*>());
  TEST_ASSERT_EQUAL(600, doc["atual"]["duracaoS"].as<int>());
  TEST_ASSERT_TRUE(doc["atual"]["bomba"].as<bool>());
  TEST_ASSERT_EQUAL(1, doc["fila"].size());
  TEST_ASSERT_EQUAL(6, doc["fila"][0]["setor"].as<int>());
  TEST_ASSERT_EQUAL_STRING("B", doc["proximaRega"]["programa"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("20:00", doc["proximaRega"]["hora"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("hoje", doc["proximaRega"]["quando"].as<const char*>());
  TEST_ASSERT_EQUAL(60, doc["proximaRega"]["faltamMin"].as<int>());

  agora.hora = 21;
  regaParaJson(doc.to<JsonObject>(), c, agenda, true, agora);
  TEST_ASSERT_EQUAL_STRING("amanhã", doc["proximaRega"]["quando"].as<const char*>());

  agenda.suspender(3, agora);
  regaParaJson(doc.to<JsonObject>(), c, agenda, true, agora);
  TEST_ASSERT_EQUAL(3, doc["pausaChuvaDias"].as<int>());
  TEST_ASSERT_EQUAL_STRING("seg", doc["proximaRega"]["quando"].as<const char*>());

  // Sem relógio não há próxima rega.
  regaParaJson(doc.to<JsonObject>(), c, agenda, false, agora);
  TEST_ASSERT_TRUE(doc["proximaRega"].isNull());
}

void test_fila_inclui_o_setor_que_esta_entrando() {
  SaidasMudas saidas;
  Config cfg;
  Controlador c(saidas, cfg);
  c.iniciar();
  Agenda agenda(c);
  c.pedir(1, 10);
  c.pedir(2, 10);
  c.pedir(3, 10);
  uint32_t t = 0;
  while (c.estado() != Estado::Trocando && t < 60000) c.tick(t += 100);
  TEST_ASSERT_TRUE(c.estado() == Estado::Trocando);

  JsonDocument doc;
  regaParaJson(doc.to<JsonObject>(), c, agenda, false, Momento());
  TEST_ASSERT_EQUAL_STRING("trocando", doc["atual"]["fase"].as<const char*>());
  TEST_ASSERT_EQUAL(2, doc["fila"].size());
  TEST_ASSERT_EQUAL(2, doc["fila"][0]["setor"].as<int>());
  TEST_ASSERT_EQUAL(3, doc["fila"][1]["setor"].as<int>());
}

static const char* const NOMES[NUM_SETORES] = {"Horta", "Setor 2", "Setor 3", "Setor 4", "Setor 5", "Setor 6", "Setor 7", "Setor 8"};

void test_historico_de_hoje() {
  RegistroDia hoje;
  hoje.iniciar(diasDesde2000(2026, 11, 13));
  HoraClima h;
  h.tempMaxC = 30.26f;
  h.tempMinC = 28;
  hoje.clima(14 * 60, h);
  hoje.chuva(13 * 60 + 10, 2.2f);
  hoje.rega(6 * 60, 1, 2100);
  hoje.evento(7 * 60, "Ajustes salvos");

  JsonDocument doc;
  historicoHojeParaJson(doc.to<JsonObject>(), hoje, NOMES);
  TEST_ASSERT_EQUAL(24, doc["rotulos"].size());
  TEST_ASSERT_EQUAL_STRING("13h", doc["rotulos"][13].as<const char*>());
  TEST_ASSERT_TRUE(doc["chuvaMm"][13].as<float>() > 2.19f && doc["chuvaMm"][13].as<float>() < 2.21f);
  TEST_ASSERT_TRUE(doc["chuvaMm"][12].as<float>() == 0);
  TEST_ASSERT_TRUE(doc["tempMaxC"][14].as<float>() > 30.29f && doc["tempMaxC"][14].as<float>() < 30.31f);
  TEST_ASSERT_TRUE(doc["tempMaxC"][3].isNull());  // hora sem leitura
  TEST_ASSERT_TRUE(doc["tempMinC"].isNull());
  TEST_ASSERT_EQUAL(1, doc["regas"].size());       // só os setores que regaram
  TEST_ASSERT_EQUAL_STRING("Horta", doc["regas"][0]["nome"].as<const char*>());
  TEST_ASSERT_EQUAL(35, doc["regas"][0]["minutos"].as<int>());
  TEST_ASSERT_EQUAL(2, doc["eventos"].size());
  TEST_ASSERT_EQUAL_STRING("07:00", doc["eventos"][0][0].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("Ajustes salvos", doc["eventos"][0][1].as<const char*>());
}

void test_historico_de_varios_dias() {
  RegistroDia hoje;
  hoje.iniciar(diasDesde2000(2026, 11, 13));  // sexta
  hoje.chuva(600, 1.0f);
  hoje.rega(700, 2, 600);

  ResumoDia resumos[2];
  resumos[0].dia = hoje.dia() - 1;
  resumos[0].chuvaMm = 12;
  resumos[0].tempMaxC = 29.5f;
  resumos[0].tempMinC = 20.2f;
  resumos[0].et0Mm = 3.0f;
  resumos[0].vezes[1] = 2;
  resumos[0].minutos[1] = 50;
  resumos[1].dia = hoje.dia() - 20;  // fora dos 7 dias
  resumos[1].chuvaMm = 99;

  JsonDocument doc;
  historicoDiasParaJson(doc.to<JsonObject>(), 7, resumos, 2, hoje, 4.1f, NOMES);
  TEST_ASSERT_EQUAL(7, doc["rotulos"].size());
  TEST_ASSERT_EQUAL_STRING("sex", doc["rotulos"][6].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("qui", doc["rotulos"][5].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("sáb", doc["rotulos"][0].as<const char*>());
  TEST_ASSERT_EQUAL(12, doc["chuvaMm"][5].as<int>());
  TEST_ASSERT_EQUAL(1, doc["chuvaMm"][6].as<int>());
  TEST_ASSERT_EQUAL(0, doc["chuvaMm"][0].as<int>());   // dia sem registro
  TEST_ASSERT_TRUE(doc["tempMaxC"][0].isNull());
  TEST_ASSERT_TRUE(doc["et0Mm"][6].as<float>() > 4.0f);
  TEST_ASSERT_EQUAL(1, doc["regas"].size());
  TEST_ASSERT_EQUAL(2, doc["regas"][0]["setor"].as<int>());
  TEST_ASSERT_EQUAL(3, doc["regas"][0]["vezes"].as<int>());     // 2 de ontem + 1 de hoje
  TEST_ASSERT_EQUAL(60, doc["regas"][0]["minutos"].as<int>());

  historicoDiasParaJson(doc.to<JsonObject>(), 30, resumos, 2, hoje, NAN, NOMES);
  TEST_ASSERT_EQUAL(30, doc["rotulos"].size());
  TEST_ASSERT_EQUAL_STRING("13", doc["rotulos"][29].as<const char*>());
  TEST_ASSERT_EQUAL(99, doc["chuvaMm"][9].as<int>());  // 20 dias atrás
  TEST_ASSERT_TRUE(doc["et0Mm"][29].isNull());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_hora_de_texto);
  RUN_TEST(test_data_vai_e_volta);
  RUN_TEST(test_programa_vai_e_volta);
  RUN_TEST(test_programa_como_a_pagina_envia);
  RUN_TEST(test_programa_invalido_nao_altera);
  RUN_TEST(test_config_padrao_e_valida);
  RUN_TEST(test_config_invalida_diz_o_motivo);
  RUN_TEST(test_ler_ajustes);
  RUN_TEST(test_mesclar_completa_config_antiga);
  RUN_TEST(test_estado_da_rega);
  RUN_TEST(test_fila_inclui_o_setor_que_esta_entrando);
  RUN_TEST(test_historico_de_hoje);
  RUN_TEST(test_historico_de_varios_dias);
  return UNITY_END();
}
