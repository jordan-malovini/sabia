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
static uint16_t minutoDoDia = 0;
static bool temHora = false;

// Decide, nas regas automáticas, se e por quanto tempo cada setor rega:
// primeiro a janela de horário (RF13), depois o balanço hídrico.
class DecisorClima : public Decisor {
 public:
  bool adiarInicio() override {
    if (!balancoAdiando()) return false;
    char texto[TAMANHO_EVENTO];
    snprintf(texto, sizeof(texto), "Rega adiada: choveu %.1f mm em 24 h", static_cast<double>(balancoChuva24hMm()));
    registrarEvento(texto);
    return true;
  }

  uint32_t duracao(uint8_t setor, uint32_t programadaS, uint32_t depoisS) override {
    char texto[TAMANHO_EVENTO];
    if (temHora) {
      // A hora em que o setor deve começar, contando os setores antes dele.
      uint16_t comeca = static_cast<uint16_t>((minutoDoDia + depoisS / 60UL) % 1440UL);
      if (!dentroDaJanela(comeca, ajustes.janelaInicio[setor - 1], ajustes.janelaFim[setor - 1])) {
        snprintf(texto, sizeof(texto), "Setor %u fora da janela de horario: nao regou", setor);
        registrarEvento(texto);
        return 0;
      }
    }
    if (!ajustes.balancoHidrico) return programadaS;
    uint32_t ajustada = balanco.ajustar(setor, programadaS);
    if (ajustada == 0) {
      snprintf(texto, sizeof(texto), "Setor %u com agua no solo: nao regou", setor);
      registrarEvento(texto);
    } else if (ajustada < programadaS) {
      snprintf(texto, sizeof(texto), "Setor %u: balanco reduziu para %lu min", setor, static_cast<unsigned long>((ajustada + 30) / 60));
      registrarEvento(texto);
    }
    return ajustada;
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
  temHora = d.temHora;
  if (!d.temHora) return;
  horaAbsoluta = minutoAbsoluto(d.agora) / 60UL;
  minutoDoDia = static_cast<uint16_t>(d.hora * 60 + d.minuto);
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
