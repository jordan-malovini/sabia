// SABIÁ - conversão entre o JSON da página web e os dados do firmware
//
// Sem Arduino: usa só a ArduinoJson, então roda nos testes do computador.
// O contrato está em docs/api.md.
//
// A configuração da página tem mais campos do que o firmware usa hoje (clima,
// registro, display...). O firmware guarda o documento inteiro e tira dele
// só o que já sabe aplicar (struct Ajustes). Os programas A, B e C ficam de
// fora desse documento: a fonte deles é a Agenda, que também é editada pelo
// encoder.

#pragma once

#include <ArduinoJson.h>

#include "Agenda.h"
#include "Balanco.h"
#include "Controlador.h"
#include "Registro.h"
#include "Retomada.h"

namespace sabia {

constexpr size_t TAMANHO_MAX_CORPO = 12288;  // maior corpo aceito num PUT/POST
constexpr uint8_t SENHA_MIN = 6, SENHA_MAX = 64;
constexpr size_t TAMANHO_URL = 201;

// O que o firmware usa da configuração.
struct Ajustes {
  Config controlador;
  ConfigRetomada retomada;
  bool usarBomba = true;
  uint16_t duracaoPadraoMin[NUM_SETORES] = {};
  uint16_t sessaoMin = 30;
  float mmPorBasculada = 0.2f;
  float ajusteTempC = 0;        // correção do termômetro
  uint16_t diasNoCartao = 7;    // quantos arquivos diários ficam guardados

  // Balanço hídrico e adiamento por chuva
  bool balancoHidrico = true;
  bool adiarPorChuva = true;
  float chuvaAdiarMm = 5.0f;    // chuva em 24 h que adia as regas automáticas
  float latitude;               // NAN = não informada (sem ela não há ET0)
  ParametrosSolo solo;
  ParametrosSetor setores[NUM_SETORES];

  // Envio ao Google Drive
  bool drive = false;
  char driveUrl[TAMANHO_URL] = "";
  uint16_t envioHoras = 24;

  Ajustes();
};

// "HH:MM" <-> minuto do dia.
bool horaDeTexto(const char* texto, uint16_t& minutoDoDia);
void textoDeHora(uint16_t minutoDoDia, char saida[6]);

// "AAAA-MM-DD" <-> dias desde 01/01/2000.
bool diasDeData(const char* texto, uint32_t& dias);
void dataDeDias(uint32_t dias, char saida[11]);

// Programa da agenda. programaDeJson devolve nullptr se deu certo ou o motivo
// do erro; em caso de erro, `p` não é alterado.
void programaParaJson(const Programa& p, JsonObject saida);
const char* programaDeJson(JsonVariantConst entrada, Programa& p);

// Configuração de fábrica, sem os programas.
void configPadrao(JsonDocument& doc);

// Copia para `destino` o que existe em `origem`, objeto por objeto. Serve
// para completar uma configuração antiga com campos novos do padrão.
void mesclar(JsonVariant destino, JsonVariantConst origem);

// Confere a configuração inteira que veio da página (com "programas").
// Devolve nullptr se está tudo certo ou o motivo do erro.
const char* validarConfig(JsonVariantConst cfg);

// Tira da configuração o que o firmware aplica. Campos ausentes ficam no padrão.
void lerAjustes(JsonVariantConst cfg, Ajustes& ajustes);

// Parte do estado que depende só da lógica: "atual", "fila", "proximaRega" e
// "pausaChuvaDias".
void regaParaJson(JsonObject saida, const Controlador& controlador, const Agenda& agenda, bool temHora,
                  const Momento& agora);

// Histórico (GET /api/historico). `nomes` são os nomes dos 8 setores.
// Hoje: 24 horas, com a chuva e a temperatura de cada hora.
void historicoHojeParaJson(JsonObject saida, const RegistroDia& hoje, const char* const nomes[NUM_SETORES]);
// Últimos `nDias` dias, terminando em hoje. `resumos` traz os dias já
// encerrados que existirem, em qualquer ordem; dia sem registro sai zerado.
void historicoDiasParaJson(JsonObject saida, uint16_t nDias, const ResumoDia* resumos, size_t nResumos, const RegistroDia& hoje,
                           float et0HojeMm, const char* const nomes[NUM_SETORES]);

}  // namespace sabia
