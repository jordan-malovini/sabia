// SABIÁ - balanço hídrico e adiamento por chuva, ligados ao resto do firmware.
//
// A conta em si está em lib/Irrigacao/src/Balanco.h. Aqui ela recebe as regas
// e a chuva, fecha o dia junto com o registro, guarda o saldo na memória do
// ESP e responde à agenda quanto cada setor deve regar.
//
// Roda com a trava fechada (loop principal ou pedidos da página).

#include <Arduino.h>

#include "sistema.h"

namespace sabia {

static BalancoHidrico balanco;
static Chuva24h chuva24h;
static uint32_t horaAbsoluta = 0;

class DecisorClima : public Decisor {
 public:
  bool adiarInicio() override { return balancoAdiando(); }
  uint32_t duracao(uint8_t setor, uint32_t programadaS) override {
    return ajustes.balancoHidrico ? balanco.ajustar(setor, programadaS) : programadaS;
  }
};
static DecisorClima decisor;

static void gravarSaldo() {
  BalancoHidrico::Salvo salvo = balanco.salvar();
  memoria.putBytes("saldo", &salvo, sizeof(salvo));
}

void balancoAplicarAjustes() { balanco.definir(ajustes.setores, ajustes.solo); }

void balancoIniciar() {
  balancoAplicarAjustes();
  BalancoHidrico::Salvo salvo;
  if (memoria.getBytesLength("saldo") == sizeof(salvo)) {
    memoria.getBytes("saldo", &salvo, sizeof(salvo));
    balanco.recuperar(salvo);
  }
  agenda.definirDecisor(&decisor);
}

void balancoTick(const DadosTela& d) {
  if (d.temHora) horaAbsoluta = minutoAbsoluto(d.agora) / 60UL;
}

void balancoChuva(float mm) { chuva24h.somar(horaAbsoluta, mm); }
void balancoChuvaNaHora(uint32_t hora, float mm) { chuva24h.somar(hora, mm); }

void balancoRega(uint8_t setor, uint32_t segundos) {
  balanco.rega(setor, segundos);
  gravarSaldo();
}

static float et0Do(const RegistroDia& dia) {
  float ra = radiacaoExtraterrestreMm(ajustes.latitude, diaDoAno(dia.dia()));
  return et0Hargreaves(dia.tempMaxC(), dia.tempMinC(), ra);
}

float balancoFecharDia(const RegistroDia& dia) {
  float et0 = et0Do(dia);
  balanco.fecharDia(dia.chuvaMm(), et0);
  gravarSaldo();
  return et0;
}

// Dias em que o controlador ficou desligado: não se sabe quanto evaporou.
void balancoDiasPerdidos() {
  balanco.fecharDia(0, NAN);
  gravarSaldo();
}

float balancoEt0Hoje() { return et0Do(registroHoje()); }
bool balancoConfiavel() { return balanco.confiavel(); }
uint8_t balancoSaldoPct(uint8_t setor) { return balanco.saldoPct(setor); }
float balancoChuva24hMm() { return chuva24h.total(horaAbsoluta); }

bool balancoAdiando() {
  return ajustes.adiarPorChuva && ajustes.chuvaAdiarMm > 0 && chuva24h.total(horaAbsoluta) >= ajustes.chuvaAdiarMm;
}

}  // namespace sabia
