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
enum class Tela : uint8_t {
  Boot, Estado,
  Menu, Duracao, Chuva,                 // ligar setor, ciclo, pausa por chuva
  ProgLista, ProgMenu,                  // programar: escolher A/B/C e o que editar
  ProgHorarios, EditHora, EditMinuto,   // horários de início
  ProgDiasModo, ProgDiasSemana, EditIntervalo,
  ProgTempos, EditTempo,                // tempo de cada setor
  EditAjuste,
};

// Itens do menu principal
static const uint8_t ITEM_CICLO = NUM_SETORES;
static const uint8_t ITEM_PROGRAMA = NUM_SETORES + 1;  // um item por programa (A, B, C)
static const uint8_t ITEM_PROGRAMAR = ITEM_PROGRAMA + NUM_PROGRAMAS;
static const uint8_t ITEM_CHUVA = ITEM_PROGRAMAR + 1;
static const uint8_t ITEM_PARAR = ITEM_PROGRAMAR + 2;
static const uint8_t ITEM_VOLTAR = ITEM_PROGRAMAR + 3;
static const uint8_t NUM_ITENS = ITEM_PROGRAMAR + 4;

// Itens do menu de um programa
enum : uint8_t { PM_ATIVO, PM_HORARIOS, PM_DIAS, PM_TEMPOS, PM_AJUSTE, PM_INICIAR, PM_VOLTAR, PM_TOTAL };

static Tela telaAtual = Tela::Boot;
static uint8_t itemMenu = 0;
static uint8_t itemEscolhido = 0;
static uint8_t indiceDuracao = DURACAO_PADRAO;
static uint8_t diasChuva = 1;
static uint8_t progSel = 0;      // programa em edição (0 = A)
static uint8_t curProg = 0, curProgMenu = 0, curSlot = 0, curSetor = 0, curDia = 0;
static int16_t valor = 0;        // valor em edição (hora, minuto, minutos, %, dias)
static int16_t horaEditada = 0;
static uint8_t modoEditado = 0;
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

// Move um cursor de lista, dando a volta nas pontas.
static void girar(uint8_t& cursor, int32_t passos, uint8_t total) {
  if (passos == 0 || total == 0) return;
  int32_t novo = (static_cast<int32_t>(cursor) + passos) % total;
  if (novo < 0) novo += total;
  cursor = static_cast<uint8_t>(novo);
}

// Altera um valor, parando nos limites.
static void ajustar(int16_t& v, int32_t passos, int16_t minimo, int16_t maximo, int16_t passo = 1) {
  int32_t novo = v + passos * passo;
  v = static_cast<int16_t>(novo < minimo ? minimo : (novo > maximo ? maximo : novo));
}

static void centralizar(const char* texto, int y) {
  tela.drawUTF8((128 - tela.getUTF8Width(texto)) / 2, y, texto);
}

static void titulo(const char* texto) {
  tela.setFont(u8g2_font_6x10_tf);
  tela.drawStr(0, 8, texto);
  tela.drawHLine(0, 11, 128);
}

static void dica(const char* texto) {
  tela.setFont(u8g2_font_5x8_tf);
  centralizar(texto, 63);
}

// Lista de 4 linhas com o item selecionado em destaque.
typedef void (*TextoItem)(uint8_t item, char* saida, size_t tamanho, const Agenda& agenda);

static void desenharLista(const char* cabecalho, uint8_t total, uint8_t selecionado, TextoItem texto, const Agenda& agenda) {
  char linha[26];
  titulo(cabecalho);
  uint8_t visiveis = total < 4 ? total : 4;
  uint8_t primeiro = 0;
  if (total > 4) primeiro = selecionado < 2 ? 0 : (selecionado > total - 3 ? total - 4 : selecionado - 1);
  for (uint8_t i = 0; i < visiveis; i++) {
    uint8_t item = primeiro + i;
    int y = 23 + i * 12;
    texto(item, linha, sizeof(linha), agenda);
    if (item == selecionado) {
      tela.drawBox(0, y - 9, 128, 12);
      tela.setDrawColor(0);
      tela.drawStr(4, y, linha);
      tela.setDrawColor(1);
    } else {
      tela.drawStr(4, y, linha);
    }
  }
}

// Tela de edição de um valor: título, legenda, valor grande e dica.
static void desenharValor(const char* cabecalho, const char* legenda, const char* texto, const char* ajuda) {
  titulo(cabecalho);
  centralizar(legenda, 25);
  tela.setFont(u8g2_font_helvB12_tf);
  centralizar(texto, 44);
  dica(ajuda);
}

static void textoDias(const Programa& p, char* saida, size_t tamanho) {
  switch (p.modo) {
    case ModoDias::Semana: {
      char dias[8];
      for (uint8_t d = 0; d < 7; d++) dias[d] = (p.diasSemana >> d) & 1 ? "DSTQQSS"[d] : '.';
      dias[7] = '\0';
      snprintf(saida, tamanho, "Dias: %s", dias);
      break;
    }
    case ModoDias::Impares: strlcpy(saida, "Dias: impares", tamanho); break;
    case ModoDias::Pares: strlcpy(saida, "Dias: pares", tamanho); break;
    case ModoDias::Intervalo: snprintf(saida, tamanho, "Dias: a cada %u", p.intervaloDias); break;
  }
}

static void textoHorario(uint16_t inicio, char* saida, size_t tamanho) {
  if (inicio == SEM_HORARIO) strlcpy(saida, "--:--", tamanho);
  else snprintf(saida, tamanho, "%02u:%02u", inicio / 60, inicio % 60);
}

// --- textos das listas ---
static void itemPrincipal(uint8_t item, char* saida, size_t tamanho, const Agenda&) {
  if (item < NUM_SETORES) snprintf(saida, tamanho, "Setor %u", item + 1);
  else if (item == ITEM_CICLO) strlcpy(saida, "Ciclo (todos)", tamanho);
  else if (item < ITEM_PROGRAMAR) snprintf(saida, tamanho, "Programa %c agora", 'A' + (item - ITEM_PROGRAMA));
  else if (item == ITEM_PROGRAMAR) strlcpy(saida, "Programar agenda", tamanho);
  else if (item == ITEM_CHUVA) strlcpy(saida, "Pausa por chuva", tamanho);
  else if (item == ITEM_PARAR) strlcpy(saida, "Parar tudo", tamanho);
  else strlcpy(saida, "Voltar", tamanho);
}

static void itemProgLista(uint8_t item, char* saida, size_t tamanho, const Agenda& agenda) {
  if (item >= NUM_PROGRAMAS) { strlcpy(saida, "Voltar", tamanho); return; }
  const Programa& p = agenda.programa(item);
  char hora[8];
  uint16_t primeiro = SEM_HORARIO;
  for (uint16_t ini : p.inicios) if (ini != SEM_HORARIO && ini < primeiro) primeiro = ini;
  textoHorario(primeiro, hora, sizeof(hora));
  snprintf(saida, tamanho, "%c %s %s", 'A' + item, p.ativo ? "ligado" : "deslig.", hora);
}

static void itemProgMenu(uint8_t item, char* saida, size_t tamanho, const Agenda& agenda) {
  const Programa& p = agenda.programa(progSel);
  switch (item) {
    case PM_ATIVO: snprintf(saida, tamanho, "Ligado: %s", p.ativo ? "sim" : "nao"); break;
    case PM_HORARIOS: {
      uint8_t n = 0;
      for (uint16_t ini : p.inicios) if (ini != SEM_HORARIO) n++;
      snprintf(saida, tamanho, "Horarios (%u)", n);
      break;
    }
    case PM_DIAS: textoDias(p, saida, tamanho); break;
    case PM_TEMPOS: {
      uint32_t total = agenda.duracaoTotalS(progSel) / 60;
      snprintf(saida, tamanho, "Tempos (%luh%02lu)", static_cast<unsigned long>(total / 60), static_cast<unsigned long>(total % 60));
      break;
    }
    case PM_AJUSTE: snprintf(saida, tamanho, "Ajuste: %u%%", p.ajustePct); break;
    case PM_INICIAR: strlcpy(saida, "Iniciar agora", tamanho); break;
    default: strlcpy(saida, "Voltar", tamanho); break;
  }
}

static void itemHorario(uint8_t item, char* saida, size_t tamanho, const Agenda& agenda) {
  if (item >= INICIOS_POR_PROGRAMA) { strlcpy(saida, "Voltar", tamanho); return; }
  char hora[8];
  textoHorario(agenda.programa(progSel).inicios[item], hora, sizeof(hora));
  snprintf(saida, tamanho, "Inicio %u: %s", item + 1, hora);
}

static void itemTempo(uint8_t item, char* saida, size_t tamanho, const Agenda& agenda) {
  if (item >= NUM_SETORES) { strlcpy(saida, "Voltar", tamanho); return; }
  uint16_t m = agenda.programa(progSel).minutosSetor[item];
  if (m == 0) snprintf(saida, tamanho, "Setor %u: fora", item + 1);
  else snprintf(saida, tamanho, "Setor %u: %u min", item + 1, m);
}

static const char* nomeModo(uint8_t modo) {
  switch (modo) {
    case 0: return "dias da semana";
    case 1: return "dias impares";
    case 2: return "dias pares";
    default: return "a cada N dias";
  }
}

// --- telas fixas ---
static void desenharBoot() {
  tela.drawXBMP(0, 4, LOGO_PASSARO_LARGURA, LOGO_PASSARO_ALTURA, LOGO_PASSARO);
  tela.setFont(u8g2_font_helvB12_tf);
  tela.drawUTF8(72, 30, "SABI\xC3\x81");
  tela.setFont(u8g2_font_6x10_tf);
  tela.drawStr(72, 44, "v" SABIA_VERSAO);
}

static void desenharRetomada(const Retomada& r, uint32_t agoraMs) {
  char linha[26];
  titulo("Energia voltou");
  uint32_t f = r.faltamS(agoraMs);
  centralizar("Retoma a rega em", 25);
  tela.setFont(u8g2_font_helvB12_tf);
  snprintf(linha, sizeof(linha), "%lu:%02lu", static_cast<unsigned long>(f / 60), static_cast<unsigned long>(f % 60));
  centralizar(linha, 43);
  tela.setFont(u8g2_font_6x10_tf);
  if (r.totalSetores() > 1) snprintf(linha, sizeof(linha), "Setor %u e mais %u", r.primeiroSetor(), r.totalSetores() - 1);
  else snprintf(linha, sizeof(linha), "Setor %u", r.primeiroSetor());
  centralizar(linha, 54);
  dica("curto: agora  longo: nao");
}

static void desenharEstado(Controlador& c, Agenda& agenda, const DadosTela& d, uint32_t agoraMs) {
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
      if (r >= 3600) snprintf(linha, sizeof(linha), "%s %lu:%02lu:%02lu", fase, static_cast<unsigned long>(r / 3600),
                              static_cast<unsigned long>(r % 3600 / 60), static_cast<unsigned long>(r % 60));
      else snprintf(linha, sizeof(linha), "%s %lu:%02lu", fase, static_cast<unsigned long>(r / 60), static_cast<unsigned long>(r % 60));
    }
    centralizar(linha, 42);
  } else if (c.estado() == Estado::Pausa) {
    centralizar("aguardando", 42);
  } else if (d.temHora) {
    // Parado: mostra a próxima rega da agenda.
    uint8_t prog = 0;
    uint16_t minuto = 0;
    uint32_t faltam = 0;
    if (agenda.proximoInicio(d.agora, prog, minuto, faltam)) {
      if (faltam < 24 * 60) snprintf(linha, sizeof(linha), "Prox %c %02u:%02u (%luh%02lu)", 'A' + prog, minuto / 60, minuto % 60,
                                     static_cast<unsigned long>(faltam / 60), static_cast<unsigned long>(faltam % 60));
      else snprintf(linha, sizeof(linha), "Prox %c %02u:%02u (%lu d)", 'A' + prog, minuto / 60, minuto % 60,
                    static_cast<unsigned long>(faltam / 1440));
      centralizar(linha, 42);
    } else {
      centralizar("Sem regas agendadas", 42);
    }
  } else {
    centralizar("Sem relogio", 42);
  }

  uint8_t susp = d.temHora ? agenda.diasSuspensos(d.agora) : 0;
  if (agoraMs < avisoAteMs) {
    centralizar(aviso, 53);
  } else if (c.tamanhoFila() > 0) {
    int n = snprintf(linha, sizeof(linha), "Fila:");
    for (size_t i = 0; i < c.tamanhoFila() && n < static_cast<int>(sizeof(linha)) - 3; i++)
      n += snprintf(linha + n, sizeof(linha) - n, " %u", c.itemFila(i).setor);
    centralizar(linha, 53);
  } else if (c.estado() == Estado::Regando || c.estado() == Estado::Trocando) {
    centralizar("Bomba ligada", 53);
  } else if (susp > 0) {
    snprintf(linha, sizeof(linha), "Chuva: pausa de %u d", susp);
    centralizar(linha, 53);
  }

  dica("clique: menu");
}

static void desenharDiasSemana(const Agenda& agenda) {
  char cab[24];
  snprintf(cab, sizeof(cab), "Programa %c: dias", 'A' + progSel);
  titulo(cab);
  uint8_t mascara = agenda.programa(progSel).diasSemana;
  tela.setFont(u8g2_font_6x10_tf);
  for (uint8_t d = 0; d < 7; d++) {
    int x = 4 + d * 15;
    char letra[2] = {"DSTQQSS"[d], '\0'};
    bool marcado = (mascara >> d) & 1;
    if (marcado) {
      tela.drawBox(x, 20, 13, 14);
      tela.setDrawColor(0);
      tela.drawStr(x + 4, 31, letra);
      tela.setDrawColor(1);
    } else {
      tela.drawFrame(x, 20, 13, 14);
      tela.drawStr(x + 4, 31, letra);
    }
    if (curDia == d) tela.drawHLine(x, 37, 13);
  }
  if (curDia == 7) {
    tela.drawBox(104, 40, 22, 12);
    tela.setDrawColor(0);
    tela.drawStr(109, 50, "OK");
    tela.setDrawColor(1);
  } else {
    tela.drawStr(109, 50, "OK");
  }
  dica("clique: marca  gire: anda");
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

bool interfaceAtualizar(Controlador& c, Agenda& agenda, Retomada& retomada, const DadosTela& d, uint32_t agoraMs) {
  bool agendaMudou = false;
  int32_t passos = lerPassos();
  Clique clique = lerBotao(agoraMs);
  bool curto = clique == Clique::Curto, longo = clique == Clique::Longo;
  if (passos != 0 || clique != Clique::Nenhum) { ultimaAcaoMs = agoraMs; redesenhar = true; }
  Programa& p = agenda.programa(progSel);

  switch (telaAtual) {
    case Tela::Boot:
      if (agoraMs - inicioMs > 2500 || clique != Clique::Nenhum) { telaAtual = Tela::Estado; redesenhar = true; }
      break;

    case Tela::Estado:
      if (retomada.aguardando()) {
        // Energia voltou: clique retoma na hora, clique longo cancela.
        if (curto) { retomada.retomarAgora(); avisar("Retomando a rega", agoraMs); }
        else if (longo) { retomada.cancelar(); avisar("Retomada cancelada", agoraMs); }
      } else if (curto) {
        telaAtual = Tela::Menu;
        itemMenu = 0;
      } else if (longo) {
        c.pararTudo();
        avisar("Parando tudo", agoraMs);
      }
      break;

    case Tela::Menu:
      girar(itemMenu, passos, NUM_ITENS);
      if (longo || (curto && itemMenu == ITEM_VOLTAR)) {
        telaAtual = Tela::Estado;
      } else if (curto && itemMenu == ITEM_PARAR) {
        c.pararTudo();
        avisar("Parando tudo", agoraMs);
        telaAtual = Tela::Estado;
      } else if (curto && itemMenu >= ITEM_PROGRAMA && itemMenu < ITEM_PROGRAMAR) {
        uint8_t prog = itemMenu - ITEM_PROGRAMA;
        if (agenda.iniciarPrograma(prog)) {
          char texto[24];
          snprintf(texto, sizeof(texto), "Programa %c iniciado", 'A' + prog);
          avisar(texto, agoraMs);
        } else {
          avisar("Programa sem setores", agoraMs);
        }
        telaAtual = Tela::Estado;
      } else if (curto && itemMenu == ITEM_PROGRAMAR) {
        curProg = 0;
        telaAtual = Tela::ProgLista;
      } else if (curto && itemMenu == ITEM_CHUVA) {
        diasChuva = (d.temHora && agenda.diasSuspensos(d.agora) > 0) ? 0 : 1;
        telaAtual = Tela::Chuva;
      } else if (curto) {
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
      if (longo) {
        telaAtual = Tela::Menu;
      } else if (curto) {
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

    case Tela::Chuva:
      if (passos != 0) {
        int32_t novo = static_cast<int32_t>(diasChuva) + passos;
        diasChuva = static_cast<uint8_t>(constrain(novo, 0, SUSPENSAO_MAX_DIAS));
      }
      if (longo) {
        telaAtual = Tela::Menu;
      } else if (curto) {
        agenda.suspender(diasChuva, d.agora);
        agendaMudou = true;
        avisar(diasChuva == 0 ? "Pausa cancelada" : "Regas em pausa", agoraMs);
        telaAtual = Tela::Estado;
      }
      break;

    // ----- Programar a agenda -----
    case Tela::ProgLista:
      girar(curProg, passos, NUM_PROGRAMAS + 1);
      if (longo || (curto && curProg == NUM_PROGRAMAS)) {
        telaAtual = Tela::Menu;
      } else if (curto) {
        progSel = curProg;
        curProgMenu = 0;
        telaAtual = Tela::ProgMenu;
      }
      break;

    case Tela::ProgMenu:
      girar(curProgMenu, passos, PM_TOTAL);
      if (longo || (curto && curProgMenu == PM_VOLTAR)) {
        telaAtual = Tela::ProgLista;
      } else if (curto) {
        switch (curProgMenu) {
          case PM_ATIVO: p.ativo = !p.ativo; agendaMudou = true; break;
          case PM_HORARIOS: curSlot = 0; telaAtual = Tela::ProgHorarios; break;
          case PM_DIAS: modoEditado = static_cast<uint8_t>(p.modo); telaAtual = Tela::ProgDiasModo; break;
          case PM_TEMPOS: curSetor = 0; telaAtual = Tela::ProgTempos; break;
          case PM_AJUSTE: valor = p.ajustePct; telaAtual = Tela::EditAjuste; break;
          case PM_INICIAR:
            avisar(agenda.iniciarPrograma(progSel) ? "Programa iniciado" : "Programa sem setores", agoraMs);
            telaAtual = Tela::Estado;
            break;
        }
      }
      break;

    case Tela::ProgHorarios:
      girar(curSlot, passos, INICIOS_POR_PROGRAMA + 1);
      if (longo || (curto && curSlot == INICIOS_POR_PROGRAMA)) {
        telaAtual = Tela::ProgMenu;
      } else if (curto) {
        uint16_t ini = p.inicios[curSlot];
        valor = ini == SEM_HORARIO ? 6 : ini / 60;        // sugere 06h num horário vazio
        horaEditada = ini == SEM_HORARIO ? 0 : ini % 60;  // guarda o minuto atual
        telaAtual = Tela::EditHora;
      }
      break;

    case Tela::EditHora:  // -1 = sem horário (apaga)
      ajustar(valor, passos, -1, 23);
      if (longo) {
        telaAtual = Tela::ProgHorarios;
      } else if (curto && valor < 0) {
        p.inicios[curSlot] = SEM_HORARIO;
        agendaMudou = true;
        telaAtual = Tela::ProgHorarios;
      } else if (curto) {
        int16_t minutoAtual = horaEditada;
        horaEditada = valor;
        valor = minutoAtual;
        telaAtual = Tela::EditMinuto;
      }
      break;

    case Tela::EditMinuto:
      if (passos != 0) {
        int32_t novo = (valor + passos) % 60;
        valor = static_cast<int16_t>(novo < 0 ? novo + 60 : novo);
      }
      if (longo) {
        telaAtual = Tela::ProgHorarios;
      } else if (curto) {
        p.inicios[curSlot] = static_cast<uint16_t>(horaEditada * 60 + valor);
        agendaMudou = true;
        telaAtual = Tela::ProgHorarios;
      }
      break;

    case Tela::ProgDiasModo:
      girar(modoEditado, passos, 4);
      if (longo) {
        telaAtual = Tela::ProgMenu;
      } else if (curto) {
        ModoDias modo = static_cast<ModoDias>(modoEditado);
        if (modo == ModoDias::Semana) {
          p.modo = modo;
          agendaMudou = true;
          curDia = 0;
          telaAtual = Tela::ProgDiasSemana;
        } else if (modo == ModoDias::Intervalo) {
          valor = p.intervaloDias < 1 ? 2 : p.intervaloDias;
          telaAtual = Tela::EditIntervalo;
        } else {
          p.modo = modo;
          agendaMudou = true;
          telaAtual = Tela::ProgMenu;
        }
      }
      break;

    case Tela::ProgDiasSemana:  // 7 dias e o "OK"
      girar(curDia, passos, 8);
      if (longo || (curto && curDia == 7)) {
        telaAtual = Tela::ProgMenu;
      } else if (curto) {
        p.diasSemana ^= static_cast<uint8_t>(1 << curDia);
        agendaMudou = true;
      }
      break;

    case Tela::EditIntervalo:
      ajustar(valor, passos, 1, 30);
      if (longo) {
        telaAtual = Tela::ProgDiasModo;
      } else if (curto) {
        p.modo = ModoDias::Intervalo;
        p.intervaloDias = static_cast<uint8_t>(valor);
        p.diaReferencia = diasDesde2000(d.agora.ano, d.agora.mes, d.agora.dia);  // começa a contar de hoje
        agendaMudou = true;
        telaAtual = Tela::ProgMenu;
      }
      break;

    case Tela::ProgTempos:
      girar(curSetor, passos, NUM_SETORES + 1);
      if (longo || (curto && curSetor == NUM_SETORES)) {
        telaAtual = Tela::ProgMenu;
      } else if (curto) {
        valor = p.minutosSetor[curSetor];
        telaAtual = Tela::EditTempo;
      }
      break;

    case Tela::EditTempo:  // 0 = setor fora do programa
      ajustar(valor, passos, 0, MINUTOS_MAX_SETOR);
      if (longo) {
        telaAtual = Tela::ProgTempos;
      } else if (curto) {
        p.minutosSetor[curSetor] = static_cast<uint16_t>(valor);
        agendaMudou = true;
        girar(curSetor, 1, NUM_SETORES + 1);  // já deixa o cursor no próximo setor
        telaAtual = Tela::ProgTempos;
      }
      break;

    case Tela::EditAjuste:
      ajustar(valor, passos, AJUSTE_MIN, AJUSTE_MAX, 5);
      if (longo) {
        telaAtual = Tela::ProgMenu;
      } else if (curto) {
        p.ajustePct = static_cast<uint8_t>(valor);
        agendaMudou = true;
        telaAtual = Tela::ProgMenu;
      }
      break;
  }

  // Sem mexer por 30 s, volta para a tela de estado.
  if (telaAtual != Tela::Boot && telaAtual != Tela::Estado && agoraMs - ultimaAcaoMs > 30000) {
    telaAtual = Tela::Estado;
    redesenhar = true;
  }

  if (!redesenhar && agoraMs - ultimoDesenhoMs < 500) return agendaMudou;
  redesenhar = false;
  ultimoDesenhoMs = agoraMs;

  char cab[26], texto[26];
  snprintf(cab, sizeof(cab), "Programa %c", 'A' + progSel);
  tela.clearBuffer();
  switch (telaAtual) {
    case Tela::Boot: desenharBoot(); break;
    case Tela::Estado:
      if (retomada.aguardando()) desenharRetomada(retomada, agoraMs);
      else desenharEstado(c, agenda, d, agoraMs);
      break;
    case Tela::Menu: desenharLista("Menu", NUM_ITENS, itemMenu, itemPrincipal, agenda); break;
    case Tela::Duracao:
      itemPrincipal(itemEscolhido, cab, sizeof(cab), agenda);
      formatarDuracao(DURACOES_S[indiceDuracao], texto, sizeof(texto));
      desenharValor(cab, itemEscolhido == ITEM_CICLO ? "Tempo de cada setor" : "Tempo de rega", texto, "gire: tempo  clique: ok");
      break;
    case Tela::Chuva:
      if (diasChuva == 0) strlcpy(texto, "cancelar", sizeof(texto));
      else snprintf(texto, sizeof(texto), "%u %s", diasChuva, diasChuva == 1 ? "dia" : "dias");
      desenharValor("Pausa por chuva", "Nao regar por", texto, "gire: dias  clique: ok");
      break;
    case Tela::ProgLista: desenharLista("Programar agenda", NUM_PROGRAMAS + 1, curProg, itemProgLista, agenda); break;
    case Tela::ProgMenu: desenharLista(cab, PM_TOTAL, curProgMenu, itemProgMenu, agenda); break;
    case Tela::ProgHorarios:
      snprintf(cab, sizeof(cab), "Programa %c: inicios", 'A' + progSel);
      desenharLista(cab, INICIOS_POR_PROGRAMA + 1, curSlot, itemHorario, agenda);
      break;
    case Tela::EditHora:
      snprintf(cab, sizeof(cab), "Programa %c: inicio %u", 'A' + progSel, curSlot + 1);
      if (valor < 0) strlcpy(texto, "sem horario", sizeof(texto));
      else snprintf(texto, sizeof(texto), "[%02d]:%02d", valor, horaEditada);
      desenharValor(cab, "Hora", texto, "gire abaixo de 00: apaga");
      break;
    case Tela::EditMinuto:
      snprintf(cab, sizeof(cab), "Programa %c: inicio %u", 'A' + progSel, curSlot + 1);
      snprintf(texto, sizeof(texto), "%02d:[%02d]", horaEditada, valor);
      desenharValor(cab, "Minuto", texto, "clique: salvar");
      break;
    case Tela::ProgDiasModo:
      snprintf(cab, sizeof(cab), "Programa %c: dias", 'A' + progSel);
      desenharValor(cab, "Regar em", nomeModo(modoEditado), "gire: opcao  clique: ok");
      break;
    case Tela::ProgDiasSemana: desenharDiasSemana(agenda); break;
    case Tela::EditIntervalo:
      snprintf(cab, sizeof(cab), "Programa %c: dias", 'A' + progSel);
      if (valor == 1) strlcpy(texto, "todo dia", sizeof(texto));
      else snprintf(texto, sizeof(texto), "a cada %d dias", valor);
      desenharValor(cab, "Regar", texto, "comeca a contar de hoje");
      break;
    case Tela::ProgTempos:
      snprintf(cab, sizeof(cab), "Programa %c: tempos", 'A' + progSel);
      desenharLista(cab, NUM_SETORES + 1, curSetor, itemTempo, agenda);
      break;
    case Tela::EditTempo:
      snprintf(cab, sizeof(cab), "Programa %c: setor %u", 'A' + progSel, curSetor + 1);
      if (valor == 0) strlcpy(texto, "fora", sizeof(texto));
      else snprintf(texto, sizeof(texto), "%d min", valor);
      desenharValor(cab, "Tempo de rega", texto, "0 = fora do programa");
      break;
    case Tela::EditAjuste:
      snprintf(texto, sizeof(texto), "%d%%", valor);
      desenharValor(cab, "Ajuste dos tempos", texto, "100% = tempo normal");
      break;
  }
  tela.sendBuffer();
  return agendaMudou;
}

}  // namespace sabia
