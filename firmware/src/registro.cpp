// SABIÁ - gravação do registro (chuva, clima, regas, eventos) e histórico.
//
// Grava no cartão microSD; sem cartão, na memória flash do ESP (LittleFS).
// Sem relógio não há data, então nada é gravado. O formato das linhas e o
// resumo do dia ficam em lib/Irrigacao/src/Registro.h.
//
//   /d/AAAAMMDD.csv   um arquivo por dia, apagado depois de `diasNoCartao`
//   /resumo.csv       uma linha por dia encerrado (alimenta 7 e 30 dias)
//
// Tudo aqui roda com a trava fechada (loop principal ou pedidos da página).

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <SD.h>
#include <SPI.h>

#include "pinos.h"
#include "sistema.h"

namespace sabia {

constexpr uint8_t MAX_RESUMOS = 31;
constexpr uint8_t MAX_PENDENTES_BOOT = 3;
static const char* const ARQUIVO_RESUMO = "/resumo.csv";

static SPIClass spiCartao(HSPI);
static fs::FS* arquivos = nullptr;
static bool noCartao = false, falhaDeGravacao = false;
static int usoPct = -1;

static RegistroDia hoje;
static bool diaAberto = false;
static uint8_t ultimoIntervalo = 0, ultimaHora = 0;
static uint16_t ultimoMinuto = 0;
static uint32_t pulsosVistos = 0;
static bool pulsosSincronizados = false;
static float chuvaPendenteMm = 0, chuvaMesAnteriorMm = 0;
static HoraClima horaClima;
static uint16_t leiturasDaHora = 0;
static ResumoDia resumos[MAX_RESUMOS];
static uint8_t nResumos = 0;
static char eventosPendentes[MAX_PENDENTES_BOOT][TAMANHO_EVENTO];
static uint8_t nEventosPendentes = 0;

static void caminhoDoDia(uint32_t dia, char saida[20]) {
  char data[11];
  dataDeDias(dia, data);  // AAAA-MM-DD
  snprintf(saida, 20, "/d/%.4s%.2s%.2s.csv", data, data + 5, data + 8);
}

static void acrescentar(const char* caminho, const char* linha) {
  if (arquivos == nullptr || linha[0] == 0) return;
  File f = arquivos->open(caminho, FILE_APPEND);
  if (!f) {
    falhaDeGravacao = true;
    return;
  }
  falhaDeGravacao = f.println(linha) == 0;
  f.close();
}

static void gravarNoDia(const char* linha) {
  char caminho[20];
  caminhoDoDia(hoje.dia(), caminho);
  acrescentar(caminho, linha);
}

static void medirUso() {
  if (arquivos == nullptr) return;
  uint64_t total = noCartao ? SD.totalBytes() : LittleFS.totalBytes();
  uint64_t usado = noCartao ? SD.usedBytes() : LittleFS.usedBytes();
  usoPct = total == 0 ? -1 : static_cast<int>(usado * 100 / total);
}

// Lê uma linha do arquivo. Devolve false no fim.
static bool lerLinha(File& f, char* linha, size_t tamanho) {
  if (!f.available()) return false;
  size_t n = f.readBytesUntil('\n', linha, tamanho - 1);
  linha[n] = 0;
  return true;
}

static void carregarResumos(uint32_t diaDeHoje) {
  nResumos = 0;
  chuvaMesAnteriorMm = 0;
  if (arquivos == nullptr) return;
  File f = arquivos->open(ARQUIVO_RESUMO, FILE_READ);
  if (!f) return;
  // Só interessam as últimas linhas: pula o começo de um arquivo grande.
  size_t interesse = static_cast<size_t>(MAX_RESUMOS + 4) * 110;
  if (f.size() > interesse) {
    f.seek(f.size() - interesse);
    char resto[TAMANHO_LINHA];
    lerLinha(f, resto, sizeof(resto));  // descarta a linha cortada
  }
  char linha[TAMANHO_LINHA];
  char dataHoje[11], data[11];
  dataDeDias(diaDeHoje, dataHoje);
  while (lerLinha(f, linha, sizeof(linha))) {
    ResumoDia r;
    if (!lerResumo(linha, r) || r.dia >= diaDeHoje) continue;
    // Uma data repetida (relógio acertado para trás) fica com a linha mais nova.
    uint8_t posicao = nResumos;
    for (uint8_t i = 0; i < nResumos; i++) if (resumos[i].dia == r.dia) posicao = i;
    if (posicao == nResumos) {
      if (nResumos == MAX_RESUMOS) {
        memmove(&resumos[0], &resumos[1], sizeof(ResumoDia) * (MAX_RESUMOS - 1));
        posicao = MAX_RESUMOS - 1;
      } else {
        nResumos++;
      }
    }
    resumos[posicao] = r;
  }
  f.close();
  for (uint8_t i = 0; i < nResumos; i++) {
    dataDeDias(resumos[i].dia, data);
    if (strncmp(data, dataHoje, 7) == 0) chuvaMesAnteriorMm += resumos[i].chuvaMm;  // mesmo ano e mês
  }
}

static void apagarAntigos(uint32_t diaDeHoje) {
  if (arquivos == nullptr) return;
  uint32_t manter = ajustes.diasNoCartao < 1 ? 1 : ajustes.diasNoCartao;
  // Com o envio ao Drive ligado, um dia só sai do cartão depois de confirmado (RF11).
  uint32_t enviadoAte = ajustes.drive ? driveUltimoDiaEnviado() : UINT32_MAX;
  for (uint8_t passada = 0; passada < 4; passada++) {  // em passadas: não apaga enquanto lista
    char apagar[8][20];
    uint8_t n = 0;
    File pasta = arquivos->open("/d");
    if (!pasta || !pasta.isDirectory()) return;
    for (File f = pasta.openNextFile(); f && n < 8; f = pasta.openNextFile()) {
      const char* nome = f.name();
      const char* base = strrchr(nome, '/');
      base = base == nullptr ? nome : base + 1;
      char data[11];
      uint32_t dia = 0;
      if (strlen(base) >= 8) {
        snprintf(data, sizeof(data), "%.4s-%.2s-%.2s", base, base + 4, base + 6);
        bool velho = diasDeData(data, dia) && dia + manter < diaDeHoje;
        bool podeApagar = dia <= enviadoAte || dia + 60 < diaDeHoje;  // sem confirmação, espera até 60 dias
        if (velho && podeApagar) snprintf(apagar[n++], 20, "/d/%s", base);
      }
      f.close();
    }
    pasta.close();
    for (uint8_t i = 0; i < n; i++) arquivos->remove(apagar[i]);
    if (n < 8) return;
  }
}

// Relê o arquivo de um dia para dentro de `registro`.
static void relerDia(uint32_t dia, RegistroDia& registro) {
  registro.iniciar(dia);
  if (arquivos == nullptr) return;
  char caminho[20], linha[TAMANHO_LINHA];
  caminhoDoDia(dia, caminho);
  File f = arquivos->open(caminho, FILE_READ);
  if (!f) return;
  while (lerLinha(f, linha, sizeof(linha))) registro.aplicarLinha(linha);
  f.close();
}

// Ao ligar: se o controlador estava desligado na virada do dia, o último dia
// registrado ficou sem resumo e sem entrar no balanço. Fecha agora.
static void fecharDiaEsquecido(uint32_t diaDeHoje) {
  uint32_t ultimo = memoria.getULong("regDia", 0);
  if (ultimo == 0 || ultimo >= diaDeHoje) return;
  static RegistroDia antigo;
  relerDia(ultimo, antigo);
  char linha[TAMANHO_LINHA];
  formatarResumo(antigo.resumo(balancoFecharDia(antigo)), linha);
  acrescentar(ARQUIVO_RESUMO, linha);
  if (diaDeHoje - ultimo > 1) balancoDiasPerdidos();  // dias inteiros sem medir nada
}

static void abrirDia(uint32_t dia, uint16_t minuto) {
  bool primeiraVez = !diaAberto;
  if (primeiraVez) fecharDiaEsquecido(dia);
  if (memoria.getULong("regDia", 0) != dia) memoria.putULong("regDia", dia);
  hoje.iniciar(dia);
  diaAberto = true;
  ultimoIntervalo = static_cast<uint8_t>(minuto / 10);
  ultimaHora = static_cast<uint8_t>(minuto / 60);
  horaClima = HoraClima();
  leiturasDaHora = 0;
  relerDia(dia, hoje);
  if (primeiraVez) {  // a chuva de hoje, até agora, volta a contar para o adiamento
    for (uint8_t h = 0; h <= minuto / 60 && h < 24; h++) balancoChuvaNaHora(dia * 24UL + h, hoje.chuvaHoraMm(h));
  }
  carregarResumos(dia);
  medirUso();
}

static void fecharIntervalo() {
  if (chuvaPendenteMm <= 0) return;
  char linha[TAMANHO_LINHA];
  hoje.chuva(static_cast<uint16_t>(ultimoIntervalo * 10), chuvaPendenteMm, linha);
  gravarNoDia(linha);
  chuvaPendenteMm = 0;
}

static void fecharHora() {
  if (leiturasDaHora == 0) return;
  char linha[TAMANHO_LINHA];
  hoje.clima(static_cast<uint16_t>(ultimaHora * 60), horaClima, linha);
  gravarNoDia(linha);
  horaClima = HoraClima();
  leiturasDaHora = 0;
}

static void fecharDia() {
  fecharIntervalo();
  fecharHora();
  char linha[TAMANHO_LINHA];
  formatarResumo(hoje.resumo(balancoFecharDia(hoje)), linha);
  acrescentar(ARQUIVO_RESUMO, linha);
}

void registroIniciar() {
  spiCartao.begin(PINO_SD_SCK, PINO_SD_MISO, PINO_SD_MOSI, PINO_SD_CS);
  if (SD.begin(PINO_SD_CS, spiCartao, 4000000)) {
    arquivos = &SD;
    noCartao = true;
  } else if (LittleFS.begin(true)) {  // sem cartão: memória interna (formata na primeira vez)
    arquivos = &LittleFS;
  }
  if (arquivos != nullptr && !arquivos->exists("/d")) arquivos->mkdir("/d");
  Serial.printf("Registro: %s\r\n", noCartao ? "cartao SD" : arquivos != nullptr ? "sem cartao SD, gravando na memoria interna" : "sem onde gravar");
}

void registroTick(const DadosTela& d, uint32_t pulsosChuva) {
  if (!pulsosSincronizados) {
    pulsosVistos = pulsosChuva;
    pulsosSincronizados = true;
  }
  float choveuMm = (pulsosChuva - pulsosVistos) * ajustes.mmPorBasculada;
  pulsosVistos = pulsosChuva;
  chuvaPendenteMm += choveuMm;
  balancoTick(d);
  if (choveuMm > 0) balancoChuva(choveuMm);
  if (!d.temHora) return;

  uint32_t dia = diasDesde2000(d.agora.ano, d.agora.mes, d.agora.dia);
  uint16_t minuto = static_cast<uint16_t>(d.hora * 60 + d.minuto);
  if (!diaAberto) {
    abrirDia(dia, minuto);
    for (uint8_t i = 0; i < nEventosPendentes; i++) registrarEvento(eventosPendentes[i]);
    nEventosPendentes = 0;
  } else if (dia != hoje.dia()) {
    uint32_t anterior = hoje.dia();
    fecharDia();
    abrirDia(dia, minuto);
    if (dia > anterior) apagarAntigos(dia);
  }
  ultimoMinuto = minuto;
  if (minuto / 10 != ultimoIntervalo) {
    fecharIntervalo();
    ultimoIntervalo = static_cast<uint8_t>(minuto / 10);
  }
  if (minuto / 60 != ultimaHora) {
    fecharHora();
    ultimaHora = static_cast<uint8_t>(minuto / 60);
  }
}

void registroLeitura(const Leitura& leitura) {
  if (!diaAberto) return;
  acumular(horaClima, leitura, leiturasDaHora);
  hoje.temperatura(ultimoMinuto, leitura.tempC);
}

void registrarRega(uint8_t setor, uint32_t segundos) {
  if (!diaAberto) return;
  char linha[TAMANHO_LINHA];
  hoje.rega(ultimoMinuto, setor, segundos, linha);
  gravarNoDia(linha);
  balancoRega(setor, segundos);
}

void registrarEvento(const char* texto) {
  if (!diaAberto) {  // ainda sem data (começo do boot): guarda para gravar depois
    if (nEventosPendentes < MAX_PENDENTES_BOOT) snprintf(eventosPendentes[nEventosPendentes++], TAMANHO_EVENTO, "%s", texto);
    return;
  }
  char linha[TAMANHO_LINHA];
  hoje.evento(ultimoMinuto, texto, linha);
  gravarNoDia(linha);
}

const RegistroDia& registroHoje() { return hoje; }
float registroChuvaHojeMm() { return (diaAberto ? hoje.chuvaMm() : 0) + chuvaPendenteMm; }
float registroChuvaMesMm() { return chuvaMesAnteriorMm + registroChuvaHojeMm(); }
bool registroTemArmazenamento() { return arquivos != nullptr && !falhaDeGravacao; }
bool registroNoCartao() { return noCartao; }
int registroUsoPct() { return usoPct; }

bool registroHistorico(const char* periodo, JsonObject saida, const char* const nomes[NUM_SETORES]) {
  if (!diaAberto) return false;
  float et0 = balancoEt0Hoje();
  if (strcmp(periodo, "7d") == 0) historicoDiasParaJson(saida, 7, resumos, nResumos, hoje, et0, nomes);
  else if (strcmp(periodo, "30d") == 0) historicoDiasParaJson(saida, 30, resumos, nResumos, hoje, et0, nomes);
  else historicoHojeParaJson(saida, hoje, nomes);
  return true;
}

bool registroLerDia(uint32_t dia, String& conteudo, size_t limite) {
  conteudo = "";
  if (arquivos == nullptr) return false;
  char caminho[20];
  caminhoDoDia(dia, caminho);
  File f = arquivos->open(caminho, FILE_READ);
  if (!f) return false;
  size_t tamanho = f.size();
  if (tamanho == 0 || tamanho > limite || !conteudo.reserve(tamanho + 1)) {
    f.close();
    return false;
  }
  char bloco[257];
  while (f.available()) {
    size_t n = f.readBytes(bloco, sizeof(bloco) - 1);
    if (n == 0) break;
    bloco[n] = 0;
    conteudo += bloco;
  }
  f.close();
  return conteudo.length() > 0;
}

}  // namespace sabia
