// SABIÁ - o que o programa principal compartilha com o servidor web.
//
// O servidor web roda numa tarefa própria, para que um cliente lento na rede
// nunca atrase a sequência de válvulas e bomba. Por isso, tudo o que está
// aqui só pode ser usado com a trava fechada (classe Trava): o loop principal
// fecha a trava a cada volta e a tarefa web fecha enquanto lê ou altera algo.
#pragma once

#include <ArduinoJson.h>
#include <Preferences.h>

#include "Agenda.h"
#include "ApiJson.h"
#include "Controlador.h"
#include "Registro.h"
#include "Retomada.h"
#include "interface.h"

namespace sabia {

extern Controlador controlador;
extern Agenda agenda;
extern Retomada retomada;
extern Preferences memoria;
extern Ajustes ajustes;  // a configuração em uso

void travar();
void destravar();
class Trava {
 public:
  Trava() { travar(); }
  ~Trava() { destravar(); }
  Trava(const Trava&) = delete;
  Trava& operator=(const Trava&) = delete;
};

DadosTela lerDados();
bool temRelogio();
bool acertarRelogio(uint16_t ano, uint8_t mes, uint8_t dia, uint8_t hora, uint8_t minuto, uint8_t segundo);
void salvarAgenda();
void pedirReinicio();  // reinicia o ESP daqui a instantes, depois de responder

// Configuração (config.cpp). O documento é o mesmo de GET/PUT /api/config,
// sem os programas e sem as senhas.
void configCarregar(JsonDocument& doc);   // padrão de fábrica + o que está gravado
bool configGravar(JsonDocument& doc);     // tira o que não fica no documento e grava
void configAplicar(JsonVariantConst cfg); // passa a valer para o controlador

// Sensores de clima (sensores.cpp).
void sensoresIniciar();
bool sensoresTick(uint32_t agoraMs);     // true quando fez uma leitura nova
const Leitura& sensoresLeitura();        // última leitura; NAN no que não há

// Registro e histórico (registro.cpp).
void registroIniciar();
void registroAoFecharDia(float (*et0)());  // de onde vem a ET0 do dia, para o resumo
void registroTick(const DadosTela& dados, uint32_t pulsosChuva);
void registroLeitura(const Leitura& leitura);
void registrarRega(uint8_t setor, uint32_t segundos);
void registrarEvento(const char* texto);
const RegistroDia& registroHoje();
float registroChuvaHojeMm();
float registroChuvaMesMm();
bool registroTemArmazenamento();
bool registroNoCartao();
int registroUsoPct();                    // -1 se não deu para medir
bool registroHistorico(const char* periodo, JsonObject saida, const char* const nomes[NUM_SETORES]);

// Servidor web e Wi-Fi (web.cpp).
void webIniciar();
void webReconfigurarRede();              // depois de trocar o Wi-Fi
void webMostrarRede(Print& saida);       // estado da rede, para o serial
void webRestaurarSenhaPadrao();

}  // namespace sabia
