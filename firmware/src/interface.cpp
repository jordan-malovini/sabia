#include "interface.h"

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

#include "logo_boot.h"
#include "pinos.h"

namespace sabia {

#ifdef SABIA_SIMULADOR
// Wokwi: OLED SSD1306 de 128x64 no barramento I2C
static U8G2_SSD1306_128X64_NONAME_F_HW_I2C tela(U8G2_R0, U8X8_PIN_NONE);
#else
// Placa real: LCD ST7920 de 128x64 em modo serial (PSB no GND)
static U8G2_ST7920_128X64_F_SW_SPI tela(U8G2_R0, PINO_LCD_SCK, PINO_LCD_MOSI, PINO_LCD_CS, U8X8_PIN_NONE);
#endif

const uint32_t DURACOES_S[] = {10, 30, 60, 120, 300, 600, 900, 1200, 1800, 2700, 3600};
const uint8_t NUM_DURACOES = sizeof(DURACOES_S) / sizeof(DURACOES_S[0]);
#ifdef SABIA_SIMULADOR
static const uint8_t DURACAO_PADRAO = 1;  // 30 s, para não esperar muito no simulador
#else
static const uint8_t DURACAO_PADRAO = 5;  // 10 min
#endif

void formatarDuracao(uint32_t s, char* saida, size_t tamanho) {
  if (s < 60) snprintf(saida, tamanho, "%lu s", static_cast<unsigned long>(s));
  else snprintf(saida, tamanho, "%lu min", static_cast<unsigned long>(s / 60));
}

// ---------------------------------------------------------------------------
// Encoder: passos por interrupção, botão por leitura com filtro
// ---------------------------------------------------------------------------
static volatile int32_t encPassos = 0;
static volatile uint32_t encUltimoUs = 0;

static void IRAM_ATTR isrEncoder() {
  uint32_t agora = micros();
  if (agora - encUltimoUs < 2000) return;  // filtro de repique
  encUltimoUs = agora;
  encPassos += digitalRead(PINO_ENCODER_B) ? 1 : -1;
}

static int32_t lerPassos() {
  noInterrupts();
  int32_t p = encPassos;
  encPassos = 0;
  interrupts();
  return p;
}

enum class Clique : uint8_t { Nenhum, Curto, Longo };

static Clique lerBotao(uint32_t agoraMs) {
  static bool estavel = false, ultimaLeitura = false, longoEnviado = false;
  static uint32_t mudouMs = 0, apertouMs = 0;
  bool leitura = digitalRead(PINO_ENCODER_BOTAO) == LOW;
  if (leitura != ultimaLeitura) { ultimaLeitura = leitura; mudouMs = agoraMs; }
  if (agoraMs - mudouMs < 30 || leitura == estavel) {
    if (estavel && !longoEnviado && agoraMs - apertouMs >= 800) { longoEnviado = true; return Clique::Longo; }
    return Clique::Nenhum;
  }
  estavel = leitura;
  if (estavel) { apertouMs = agoraMs; longoEnviado = false; return Clique::Nenhum; }
  return longoEnviado ? Clique::Nenhum : Clique::Curto;
}

// ---------------------------------------------------------------------------
// Telas
// ---------------------------------------------------------------------------
enum class Tela : uint8_t { Boot, Estado, Menu, Duracao };

static const uint8_t ITEM_CICLO = NUM_SETORES;
static const uint8_t ITEM_PARAR = NUM_SETORES + 1;
static const uint8_t ITEM_VOLTAR = NUM_SETORES + 2;
static const uint8_t NUM_ITENS = NUM_SETORES + 3;

static Tela telaAtual = Tela::Boot;
static uint8_t itemMenu = 0;
static uint8_t itemEscolhido = 0;
static uint8_t indiceDuracao = DURACAO_PADRAO;
static uint32_t ultimaAcaoMs = 0, ultimoDesenhoMs = 0, inicioMs = 0;
static bool redesenhar = true;
static char aviso[24] = "";
static uint32_t avisoAteMs = 0;

static void avisar(const char* texto, uint32_t agoraMs) {
  strlcpy(aviso, texto, sizeof(aviso));
  avisoAteMs = agoraMs + 2500;
}

static const char* textoResultado(Resultado r) {
  switch (r) {
    case Resultado::Aceito: return "Na fila";
    case Resultado::JaNaFila: return "Ja esta na fila";
    case Resultado::FilaCheia: return "Fila cheia";
    default: return "Invalido";
  }
}

static void nomeItem(uint8_t item, char* saida, size_t tamanho) {
  if (item < NUM_SETORES) snprintf(saida, tamanho, "Setor %u", item + 1);
  else if (item == ITEM_CICLO) strlcpy(saida, "Ciclo (todos)", tamanho);
  else if (item == ITEM_PARAR) strlcpy(saida, "Parar tudo", tamanho);
  else strlcpy(saida, "Voltar", tamanho);
}

static void centralizar(const char* texto, int y) {
  tela.drawUTF8((128 - tela.getUTF8Width(texto)) / 2, y, texto);
}

static void desenharBoot() {
  tela.drawXBMP(0, 4, LOGO_PASSARO_LARGURA, LOGO_PASSARO_ALTURA, LOGO_PASSARO);
  tela.setFont(u8g2_font_helvB12_tf);
  tela.drawUTF8(72, 30, "SABI\xC3\x81");
  tela.setFont(u8g2_font_6x10_tf);
  tela.drawStr(72, 44, "v" SABIA_VERSAO);
}

static void desenharEstado(Controlador& c, const DadosTela& d, uint32_t agoraMs) {
  char linha[32];
  tela.setFont(u8g2_font_6x10_tf);
  if (d.temHora) snprintf(linha, sizeof(linha), "%02u:%02u %02u/%02u", d.hora, d.minuto, d.dia, d.mes);
  else strlcpy(linha, "--:--", sizeof(linha));
  tela.drawStr(0, 8, linha);
  snprintf(linha, sizeof(linha), "%.1fmm", d.chuvaMm);
  tela.drawStr(128 - tela.getStrWidth(linha), 8, linha);
  tela.drawHLine(0, 11, 128);

  tela.setFont(u8g2_font_helvB12_tf);
  if (c.setorAtivo() == 0) {
    centralizar("Parado", 30);
  } else {
    snprintf(linha, sizeof(linha), "Setor %u", c.setorAtivo());
    centralizar(linha, 30);
  }

  tela.setFont(u8g2_font_6x10_tf);
  if (c.setorAtivo() != 0) {
    uint32_t r = c.restanteS();
    if (c.estado() == Estado::Trocando) {
      snprintf(linha, sizeof(linha), "trocando para o %u", c.proximoSetor());
    } else {
      const char* fase = c.estado() == Estado::Regando ? "regando"
                       : c.estado() == Estado::AbrindoValvula ? "abrindo" : "fechando";
      snprintf(linha, sizeof(linha), "%s %lu:%02lu", fase, static_cast<unsigned long>(r / 60),
               static_cast<unsigned long>(r % 60));
    }
    centralizar(linha, 42);
  } else if (c.estado() == Estado::Pausa) {
    centralizar("pausa entre setores", 42);
  }

  if (agoraMs < avisoAteMs) {
    centralizar(aviso, 53);
  } else if (c.tamanhoFila() > 0) {
    int n = snprintf(linha, sizeof(linha), "Fila:");
    for (size_t i = 0; i < c.tamanhoFila() && n < static_cast<int>(sizeof(linha)) - 3; i++)
      n += snprintf(linha + n, sizeof(linha) - n, " %u", c.itemFila(i).setor);
    centralizar(linha, 53);
  } else if (c.estado() == Estado::Regando || c.estado() == Estado::Trocando) {
    centralizar("Bomba ligada", 53);
  }

  tela.setFont(u8g2_font_5x8_tf);
  centralizar("clique: menu", 63);
}

static void desenharMenu() {
  char linha[24];
  tela.setFont(u8g2_font_6x10_tf);
  tela.drawStr(0, 8, "Menu");
  tela.drawHLine(0, 11, 128);
  uint8_t primeiro = itemMenu < 2 ? 0 : (itemMenu > NUM_ITENS - 3 ? NUM_ITENS - 4 : itemMenu - 1);
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t item = primeiro + i;
    int y = 23 + i * 12;
    nomeItem(item, linha, sizeof(linha));
    if (item == itemMenu) {
      tela.drawBox(0, y - 9, 128, 12);
      tela.setDrawColor(0);
      tela.drawStr(4, y, linha);
      tela.setDrawColor(1);
    } else {
      tela.drawStr(4, y, linha);
    }
  }
}

static void desenharDuracao() {
  char linha[24];
  tela.setFont(u8g2_font_6x10_tf);
  nomeItem(itemEscolhido, linha, sizeof(linha));
  tela.drawStr(0, 8, linha);
  tela.drawHLine(0, 11, 128);
  centralizar(itemEscolhido == ITEM_CICLO ? "Tempo de cada setor" : "Tempo de rega", 25);
  tela.setFont(u8g2_font_helvB12_tf);
  formatarDuracao(DURACOES_S[indiceDuracao], linha, sizeof(linha));
  centralizar(linha, 44);
  tela.setFont(u8g2_font_5x8_tf);
  centralizar("gire: tempo  clique: ligar", 63);
}

void interfaceIniciar() {
  pinMode(PINO_ENCODER_A, INPUT_PULLUP);
  pinMode(PINO_ENCODER_B, INPUT_PULLUP);
  pinMode(PINO_ENCODER_BOTAO, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PINO_ENCODER_A), isrEncoder, FALLING);
  tela.begin();
  tela.enableUTF8Print();
  inicioMs = millis();
}

void interfaceAtualizar(Controlador& c, const DadosTela& d, uint32_t agoraMs) {
  int32_t passos = lerPassos();
  Clique clique = lerBotao(agoraMs);
  if (passos != 0 || clique != Clique::Nenhum) { ultimaAcaoMs = agoraMs; redesenhar = true; }

  switch (telaAtual) {
    case Tela::Boot:
      if (agoraMs - inicioMs > 2500 || clique != Clique::Nenhum) { telaAtual = Tela::Estado; redesenhar = true; }
      break;

    case Tela::Estado:
      if (clique == Clique::Curto) { telaAtual = Tela::Menu; itemMenu = 0; }
      else if (clique == Clique::Longo) { c.pararTudo(); avisar("Parando tudo", agoraMs); }
      break;

    case Tela::Menu:
      if (passos != 0) itemMenu = static_cast<uint8_t>((itemMenu + NUM_ITENS + (passos % NUM_ITENS)) % NUM_ITENS);
      if (clique == Clique::Longo || (clique == Clique::Curto && itemMenu == ITEM_VOLTAR)) {
        telaAtual = Tela::Estado;
      } else if (clique == Clique::Curto && itemMenu == ITEM_PARAR) {
        c.pararTudo();
        avisar("Parando tudo", agoraMs);
        telaAtual = Tela::Estado;
      } else if (clique == Clique::Curto) {
        itemEscolhido = itemMenu;
        indiceDuracao = DURACAO_PADRAO;
        telaAtual = Tela::Duracao;
      }
      break;

    case Tela::Duracao:
      if (passos != 0) {
        int32_t novo = static_cast<int32_t>(indiceDuracao) + passos;
        indiceDuracao = static_cast<uint8_t>(constrain(novo, 0, NUM_DURACOES - 1));
      }
      if (clique == Clique::Longo) {
        telaAtual = Tela::Menu;
      } else if (clique == Clique::Curto) {
        uint32_t dur = DURACOES_S[indiceDuracao];
        if (itemEscolhido == ITEM_CICLO) {
          uint32_t todas[NUM_SETORES];
          for (auto& x : todas) x = dur;
          c.iniciarCiclo(todas);
          avisar("Ciclo iniciado", agoraMs);
        } else {
          avisar(textoResultado(c.pedir(itemEscolhido + 1, dur)), agoraMs);
        }
        telaAtual = Tela::Estado;
      }
      break;
  }

  // Sem mexer por 20 s, volta para a tela de estado.
  if ((telaAtual == Tela::Menu || telaAtual == Tela::Duracao) && agoraMs - ultimaAcaoMs > 20000) {
    telaAtual = Tela::Estado;
    redesenhar = true;
  }

  if (!redesenhar && agoraMs - ultimoDesenhoMs < 500) return;
  redesenhar = false;
  ultimoDesenhoMs = agoraMs;
  tela.clearBuffer();
  switch (telaAtual) {
    case Tela::Boot: desenharBoot(); break;
    case Tela::Estado: desenharEstado(c, d, agoraMs); break;
    case Tela::Menu: desenharMenu(); break;
    case Tela::Duracao: desenharDuracao(); break;
  }
  tela.sendBuffer();
}

}  // namespace sabia
