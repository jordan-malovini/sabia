// SABIÁ - registro do dia: chuva, clima, regas e eventos
//
// Lógica pura, sem Arduino. Guarda na memória o resumo do dia corrente e
// converte de e para as linhas de texto gravadas no cartão (ou na flash).
// Depois de um reinício, o dia é refeito relendo o arquivo de hoje.
//
// Arquivo do dia (um por dia), uma linha por acontecimento:
//   C;HH:MM;mm                      chuva num intervalo de 10 min (só se choveu)
//   T;HH:MM;max;min;umid;pressao;lux   clima de uma hora (campo vazio = sem leitura)
//   R;HH:MM;setor;segundos          rega encerrada
//   E;HH:MM;texto                   evento
// Resumo (um arquivo só), uma linha por dia encerrado:
//   AAAA-MM-DD;chuva;max;min;et0;v1;m1;...;v8;m8   (vezes e minutos de cada setor)

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "Controlador.h"

namespace sabia {

constexpr uint8_t MAX_EVENTOS = 24;
constexpr size_t TAMANHO_EVENTO = 56;
constexpr size_t TAMANHO_LINHA = 160;

struct Leitura {  // NAN = sem leitura
  float tempC, umidadePct, pressaoHpa, lux;
  Leitura();
};

struct Evento {
  uint16_t minutoDoDia = 0;
  char texto[TAMANHO_EVENTO] = "";
};

// Clima acumulado de uma hora, para a linha T.
struct HoraClima {
  float tempMaxC, tempMinC, umidadePct, pressaoHpa, lux;
  HoraClima();
};

struct ResumoDia {
  uint32_t dia = 0;  // dias desde 01/01/2000
  float chuvaMm = 0;
  float tempMaxC, tempMinC, et0Mm;  // NAN = sem dado
  uint16_t vezes[NUM_SETORES] = {};
  uint16_t minutos[NUM_SETORES] = {};
  ResumoDia();
};

class RegistroDia {
 public:
  RegistroDia() { iniciar(0); }

  void iniciar(uint32_t dia);
  uint32_t dia() const { return dia_; }

  // Cada função registra na memória e, se `linha` não for nulo, escreve nela
  // (TAMANHO_LINHA bytes) o texto a gravar no arquivo, sem a quebra de linha.
  void chuva(uint16_t minutoDoDia, float mm, char* linha = nullptr);
  void clima(uint16_t minutoDoDia, const HoraClima& hora, char* linha = nullptr);
  void rega(uint16_t minutoDoDia, uint8_t setor, uint32_t segundos, char* linha = nullptr);
  void evento(uint16_t minutoDoDia, const char* texto, char* linha = nullptr);

  // Temperatura lida agora: entra na máxima e na mínima do dia sem esperar a hora fechar.
  void temperatura(uint16_t minutoDoDia, float tempC);

  // Refaz o dia a partir de uma linha do arquivo. Linhas estranhas são ignoradas.
  bool aplicarLinha(const char* linha);

  float chuvaMm() const;
  float chuvaHoraMm(uint8_t hora) const { return hora < 24 ? chuvaHora_[hora] : 0; }
  float tempHoraC(uint8_t hora) const;  // máxima da hora; NAN sem leitura
  float tempMaxC() const;
  float tempMinC() const;
  uint16_t vezes(uint8_t setor) const { return setor >= 1 && setor <= NUM_SETORES ? vezes_[setor - 1] : 0; }
  uint32_t segundos(uint8_t setor) const { return setor >= 1 && setor <= NUM_SETORES ? segundos_[setor - 1] : 0; }

  uint8_t totalEventos() const { return nEventos_; }
  const Evento& eventoRecente(uint8_t i) const;  // 0 = o mais recente

  ResumoDia resumo(float et0Mm) const;

 private:
  void guardarEvento(uint16_t minutoDoDia, const char* texto);

  uint32_t dia_ = 0;
  float chuvaHora_[24];
  float tempMaxHora_[24], tempMinHora_[24];
  uint16_t vezes_[NUM_SETORES];
  uint32_t segundos_[NUM_SETORES];
  Evento eventos_[MAX_EVENTOS];
  uint8_t nEventos_ = 0, proximoEvento_ = 0;
};

// Linha do arquivo de resumo.
void formatarResumo(const ResumoDia& r, char linha[TAMANHO_LINHA]);
bool lerResumo(const char* linha, ResumoDia& r);

// Soma as leituras de uma hora. `n` conta as leituras já somadas.
void acumular(HoraClima& hora, const Leitura& leitura, uint16_t& n);

// Luz do VEML7700 em lux (ganho 1/8, integração de 25 ms).
float luxDoVeml7700(uint16_t bruto);

}  // namespace sabia
