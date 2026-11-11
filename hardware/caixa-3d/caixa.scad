// SABIÁ - caixa do controlador (Rev A)
//
// Caixa de parede impressa em 3D (PETG ou ASA): base com divisórias e suportes
// para cada módulo, e tampa com dobradiça, janela da tela e furo do encoder.
//
// Como usar:
//   1. Abra no OpenSCAD (gratuito) e confira as medidas marcadas MEDIR.
//   2. Mude "parte" para "base" ou "tampa", aperte F6 e exporte o STL (F7).
//   3. Para ver por dentro, use "por_dentro", "montagem" ou "aberta" e aperte F5
//      (a prévia mostra cores e transparência; o F6 descarta as cores).
//
// Medidas em mm. Coordenadas da base: X para a direita, Y para cima (caixa
// presa na parede), Z saindo da parede em direção à tampa.

// "por_dentro": base e componentes, sem tampa (melhor para ver o interior)
// "montagem":   caixa fechada, com a tampa transparente
// "aberta":     tampa aberta na dobradiça
// "base" / "tampa": peças sozinhas, na posição de impressão (para exportar STL)
parte = "por_dentro";  // [por_dentro, montagem, aberta, base, tampa]

$fn = 48;

// --------------------------------------------------------------------------
// Parâmetros gerais
// --------------------------------------------------------------------------
t          = 2.4;   // espessura das paredes, do fundo e da tampa
folga      = 3;     // espaço livre em volta de cada módulo
div        = 2.4;   // espessura das divisórias
D_in       = 65;    // profundidade interna da base
folga_enc  = 0.3;   // folga de encaixe entre a aba da tampa e a parede

alt_suporte = 5;    // altura dos suportes (as placas ficam afastadas do fundo)
d_suporte   = 6.5;  // diâmetro externo dos suportes
d_auto_m3   = 2.6;  // furo para parafuso M3 autoatarraxante em PETG
d_auto_m25  = 2.2;  // furo para parafuso M2.5 autoatarraxante
d_passante  = 3.4;  // furo passante para M3

// --------------------------------------------------------------------------
// Módulos: [largura (X), altura (Y), espessura (Z)], já na posição de montagem
// --------------------------------------------------------------------------
rele  = [180, 90, 20];   // módulo de 16 relés
fonte = [99, 131, 40];   // fonte 5 V 10 A (131 x 99 x 40), em pé
expan = [90, 130, 30];   // placa de expansão do ESP32-S3 (130 x 90 x 30), em pé
rtc   = [22, 38, 17];    // módulo RTC DS3231 HW-084, deitado, com a pilha para a frente
sd    = [17.5, 18.5, 5]; // módulo microSD (largura, comprimento, espessura), medido com régua
sd_cartao = 28;          // comprimento com o cartão encaixado
lcd   = [93, 70, 13.5];  // tela ST7920 12864B (desenho do fabricante)

// Furos de fixação de cada placa, medidos a partir do canto inferior esquerdo
// do módulo já na posição de montagem. MEDIR: os valores abaixo supõem furos
// nos 4 cantos, a 3,5 mm das bordas. Troque pelos centros reais.
inset = 3.5;
function cantos(m, i = inset) = [[i, i], [m[0] - i, i], [i, m[1] - i], [m[0] - i, m[1] - i]];
furos_rele  = cantos(rele);
furos_expan = cantos(expan);
furos_fonte = cantos(fonte, 10);  // MEDIR: furos roscados no fundo da fonte

// RTC: 3 furos. MEDIR: posições estimadas por foto do HW-084 (placa de 38 x 22),
// vista pelo lado da pilha, com os pinos apontando para cima (para o SD).
furos_rtc = [[1.7, 3.4], [1.7, 28.8], [20.2, 28.8]];

// microSD em pé: cartão apontando para a tampa, pinos apontando para o ESP.
// A placa é parafusada por 2 furos numa paredinha vertical.
sd_furos_dist  = 14;   // entre os centros dos 2 furos (Ø3), medido com régua
sd_furo_borda  = 6;    // MEDIR: do centro dos furos até a borda da placa do lado dos pinos
sd_elev        = 7;    // altura da borda de baixo da placa (espaço para os conectores)
sd_afasta      = 3;    // afastamento entre a paredinha e a placa (soldas e componentes)

// Tela: medidas do desenho do fabricante
lcd_furos   = [88, 65];   // distância entre os centros dos furos (Ø3)
lcd_janela  = [74, 42];   // área visível (72 x 40) + 1 mm de cada lado
lcd_moldura = 8.9;        // MEDIR: da frente da moldura até a frente da placa
                          // (o desenho dá 10,5 até as costas da placa de 1,6)

acrilico  = [80, 48, 1.2]; // rebaixo para o visor de acrílico de 1 mm (corte 79,5 x 47,5)

d_encoder = 7.5;          // furo do encoder (eixo roscado M7, tipo EC11)

// Passa-cabos (prensa-cabos): diâmetros dos furos
d_pg9 = 15.5;
d_pg7 = 12.5;
margem_rede = 12;   // espaço à esquerda da fonte para o prensa-cabos da rede

// --------------------------------------------------------------------------
// Leiaute (coordenadas internas: origem no canto interno inferior esquerdo)
// --------------------------------------------------------------------------
h_inf   = rele[1] + 2 * folga;               // faixa de baixo: relés
y_sup   = h_inf + div;                       // início da faixa de cima
x_div   = margem_rede + fonte[0] + folga;    // divisória à direita da fonte
p_rele  = [folga, folga];
p_fonte = [margem_rede, y_sup + folga];
p_expan = [x_div + div + folga, y_sup + folga];
x_col   = folga + rele[0] + folga;           // coluna ao lado dos relés
p_rtc   = [x_col, folga];
p_sd    = [x_col + (rtc[0] - sd[0]) / 2, folga + rtc[1] + 21];  // 21 mm livres para os conectores do RTC

W_in = max(p_expan[0] + expan[0] + folga, x_col + max(rtc[0], sd[0]) + folga);
H_in = max(p_fonte[1] + fonte[1], p_expan[1] + expan[1]) + folga;
W = W_in + 2 * t;
H = H_in + 2 * t;
P = D_in + t;                                // altura da base (fundo + interno)

// Tela centralizada na tampa, perto do topo; encoder abaixo, sobre a coluna do ESP
lcd_c = [W / 2, t + H_in - 12 - lcd[1] / 2];
enc_c = [lcd_c[0] + 30, lcd_c[1] - lcd[1] / 2 - 24];

// Parafusos de fechamento: canto de baixo à direita e alto da divisória
// (o canto de cima à direita fica sobre a placa do ESP)
fech = [[W - t - 4, t + 4], [t + x_div + div / 2, H - t - 4]];

// Dobradiça (lado esquerdo, na borda da tampa)
r_dob   = 4;
d_pino  = 3.2;   // pino: vareta de 3 mm ou parafuso M3 longo
L_dob   = H * 0.6;
n_dob   = 5;
gap_dob = 0.5;
s_dob   = (L_dob - (n_dob - 1) * gap_dob) / n_dob;
y_dob   = (H - L_dob) / 2;
eixo    = [-r_dob, P + t - r_dob];   // eixo em (X, Z): rente à face externa da tampa

alt_divisoria = D_in - lcd[2] - 2;  // divisórias param abaixo da tela (e acima da fonte)

echo(str("Caixa externa: ", W, " x ", H, " x ", P + t, " mm (base ", P, " + tampa ", t, ")"));
echo(str("Área interna: ", W_in, " x ", H_in, " x ", D_in, " mm"));

// --------------------------------------------------------------------------
// Peças auxiliares
// --------------------------------------------------------------------------
module suportes(pos, furos, d_furo = d_auto_m3, d_ext = d_suporte) {
  for (f = furos)
    translate([pos[0] + f[0], pos[1] + f[1], 0])
      difference() {
        cylinder(d = d_ext, h = alt_suporte);
        translate([0, 0, 1]) cylinder(d = d_furo, h = alt_suporte);
      }
}

// Paredinha do microSD: a placa fica em pé, encostada em duas nervuras, e é
// presa por 2 parafusos M2.5 horizontais. Imprime sem suporte.
module suporte_sd() {
  z_furo = sd_elev + sd_furo_borda;
  translate([p_sd[0], p_sd[1], 0])
    difference() {
      union() {
        translate([-2, 0, 0]) cube([sd[0] + 4, div, z_furo + 5]);
        for (sx = [-1, 1])
          translate([sd[0] / 2 + sx * sd_furos_dist / 2 - 2.5, div - 0.01, 0])
            cube([5, sd_afasta + 0.01, z_furo + 3.5]);
      }
      for (sx = [-1, 1])
        translate([sd[0] / 2 + sx * sd_furos_dist / 2, -1, z_furo])
          rotate([-90, 0, 0]) cylinder(d = d_auto_m25, h = div + sd_afasta + 2);
    }
}

module furo_parede_y(x, z, d, y0) {  // furo atravessando uma parede horizontal (normal Y)
  translate([x, y0 - 1, z]) rotate([-90, 0, 0]) cylinder(d = d, h = t + 2);
}

// --------------------------------------------------------------------------
// Base
// --------------------------------------------------------------------------
module dobradica_base() {
  for (i = [0 : 2 : n_dob - 1])
    translate([0, y_dob + i * (s_dob + gap_dob), 0])
      difference() {
        hull() {
          translate([eixo[0], 0, eixo[1]]) rotate([-90, 0, 0]) cylinder(r = r_dob, h = s_dob);
          // rampa até a parede: imprime sem suporte
          translate([0, 0, eixo[1] - 3 * r_dob]) cube([1, s_dob, 1]);
        }
        translate([eixo[0], -1, eixo[1]]) rotate([-90, 0, 0]) cylinder(d = d_pino, h = s_dob + 2);
      }
}

module abas_parede() {  // abas para parafusar na parede, nas laterais
  for (x = [-14, W], y = [25, H - 45])
    translate([x, y, 0]) cube([14, 20, 4]);
}

module base() {
  difference() {
    union() {
      difference() {
        cube([W, H, P]);
        translate([t, t, t]) cube([W_in, H_in, D_in + 1]);
      }
      translate([t, t, 0]) {
        // divisórias que isolam a fonte (parte de rede elétrica)
        translate([0, h_inf, 0]) cube([x_div + div, div, t + alt_divisoria]);
        translate([x_div, h_inf, 0]) cube([div, H_in - h_inf, t + alt_divisoria]);
      }
      translate([t, t, t]) {
        suportes(p_rele, furos_rele);
        suportes(p_expan, furos_expan);
        suportes(p_rtc, furos_rtc, d_auto_m25, 5);
        suporte_sd();
      }
      // ressaltos dos parafusos de fechamento
      // d = 9: invade 0,5 mm a parede (tangente gera sólido inválido)
      for (f = fech) translate([f[0], f[1], 0]) cylinder(d = 9, h = P - 3.5);
      abas_parede();
      dobradica_base();
    }
    // rosca dos parafusos de fechamento
    for (f = fech) translate([f[0], f[1], P - 18]) cylinder(d = d_auto_m3, h = 20);
    // fonte presa por baixo, com parafusos atravessando o fundo
    for (f = furos_fonte)
      translate([t + p_fonte[0] + f[0], t + p_fonte[1] + f[1], -1]) {
        cylinder(d = d_passante, h = t + 2);
        cylinder(d1 = 6.5, d2 = d_passante, h = 1.6);  // escareado
      }
    // furos das abas de parede
    for (x = [-7, W + 7], y = [35, H - 35])
      translate([x, y, -1]) cylinder(d = 4.5, h = 10);
    // prensa-cabos: saídas das válvulas e da bomba (embaixo, sob os relés)
    furo_parede_y(t + p_rele[0] + 45,  t + 42, d_pg9, 0);
    furo_parede_y(t + p_rele[0] + 135, t + 42, d_pg9, 0);
    // sensores (embaixo, ao lado do RTC)
    furo_parede_y(t + x_col + 12, t + 45, d_pg7, 0);
    // entrada da rede elétrica (parede esquerda, no alto do compartimento da fonte)
    translate([-1, t + p_fonte[1] + fonte[1] - 15, t + 30]) rotate([0, 90, 0]) cylinder(d = d_pg9, h = t + 2);
    // passagem dos fios de 5 V nas divisórias, junto ao fundo
    translate([t + 60, t + h_inf - 1, t]) cube([18, div + 2, 10]);
    translate([t + x_div - 1, t + h_inf + 15, t]) cube([div + 2, 18, 10]);
    // ventilação da fonte na parede esquerda (fendas verticais)
    for (i = [0 : 5])
      translate([-1, t + p_fonte[1] + 18 + i * 17, t + 10]) cube([t + 2, 3, 30]);
  }
}

// --------------------------------------------------------------------------
// Tampa (na posição fechada, sobre a base)
// --------------------------------------------------------------------------
module dobradica_tampa() {
  for (i = [1 : 2 : n_dob - 1])
    translate([0, y_dob + i * (s_dob + gap_dob), 0])
      difference() {
        hull() {
          translate([eixo[0], 0, eixo[1]]) rotate([-90, 0, 0]) cylinder(r = r_dob, h = s_dob);
          translate([-0.5, 0, P]) cube([1, s_dob, t]);
        }
        translate([eixo[0], -1, eixo[1]]) rotate([-90, 0, 0]) cylinder(d = d_pino, h = s_dob + 2);
      }
}

module tampa() {
  difference() {
    union() {
      translate([0, 0, P]) cube([W, H, t]);
      // aba interna que encaixa nas paredes e segura a chuva
      translate([0, 0, P - 3])
        difference() {
          translate([t + folga_enc, t + folga_enc, 0]) cube([W_in - 2 * folga_enc, H_in - 2 * folga_enc, 3.01]);
          translate([t + folga_enc + 1.6, t + folga_enc + 1.6, -1]) cube([W_in - 2 * folga_enc - 3.2, H_in - 2 * folga_enc - 3.2, 5]);
          // recortes da aba nos ressaltos de fechamento
          for (f = fech) translate([f[0], f[1], -1]) cylinder(d = 9, h = 5);
        }
      // espaçadores sobre os ressaltos de fechamento
      for (f = fech) translate([f[0], f[1], P - 3.3]) cylinder(d = 8, h = 3.31);
      // suportes da tela, pendurados na face interna
      for (sx = [-1, 1], sy = [-1, 1])
        translate([lcd_c[0] + sx * lcd_furos[0] / 2, lcd_c[1] + sy * lcd_furos[1] / 2, P - lcd_moldura])
          cylinder(d = 6, h = lcd_moldura + 0.01);
      dobradica_tampa();
    }
    // janela da tela, com chanfro por fora
    translate([lcd_c[0], lcd_c[1], P - 1])
      linear_extrude(height = t + 2) square(lcd_janela, center = true);
    translate([lcd_c[0], lcd_c[1], P + t - 1])
      linear_extrude(height = 1.01, scale = [(lcd_janela[0] + 2) / lcd_janela[0], (lcd_janela[1] + 2) / lcd_janela[1]])
        square(lcd_janela, center = true);
    // rebaixo do visor de acrílico, por dentro (a moldura da tela prende a placa)
    translate([lcd_c[0], lcd_c[1], P - 1])
      linear_extrude(height = acrilico[2] + 1) square([acrilico[0], acrilico[1]], center = true);
    // furos dos parafusos da tela (M2.5 entram por trás da placa)
    for (sx = [-1, 1], sy = [-1, 1])
      translate([lcd_c[0] + sx * lcd_furos[0] / 2, lcd_c[1] + sy * lcd_furos[1] / 2, P - lcd_moldura - 1])
        cylinder(d = d_auto_m25, h = lcd_moldura);
    // encoder
    translate([enc_c[0], enc_c[1], P - 4]) cylinder(d = d_encoder, h = t + 8);
    // parafusos de fechamento (cabeça escareada por fora)
    for (f = fech) translate([f[0], f[1], P - 4]) {
      cylinder(d = d_passante, h = t + 8);
      translate([0, 0, 4 + t - 1.7]) cylinder(d1 = d_passante, d2 = 6.5, h = 1.71);
    }
  }
}

// --------------------------------------------------------------------------
// Módulos (só para visualizar; não vão para o STL)
// --------------------------------------------------------------------------
module modulos(com_tela = true) {
  translate([t, t, t]) {
    color("SteelBlue") translate([p_rele[0], p_rele[1], alt_suporte]) cube(rele);
    color("Silver") translate([p_fonte[0], p_fonte[1], 0]) cube(fonte);
    color("MediumSeaGreen") translate([p_expan[0], p_expan[1], alt_suporte]) cube(expan);
    color("Orange") translate([p_rtc[0], p_rtc[1], alt_suporte]) cube(rtc);
    translate([p_sd[0], p_sd[1] + div + sd_afasta, sd_elev]) {
      color("MediumPurple") cube([sd[0], sd[2], sd[1]]);
      color("White") translate([(sd[0] - 11) / 2, 2, sd[1]]) cube([11, 1, sd_cartao - sd[1]]);
    }
  }
  if (com_tela) tela();
}

module tela() {
  color("YellowGreen")
    translate([lcd_c[0] - lcd[0] / 2, lcd_c[1] - lcd[1] / 2, P - lcd[2]]) cube(lcd);
}

// --------------------------------------------------------------------------
// Saída
// --------------------------------------------------------------------------
if (parte == "base") {
  base();
} else if (parte == "tampa") {
  // virada para imprimir com a face externa na mesa
  translate([W, 0, P + t]) rotate([0, 180, 0]) tampa();
} else if (parte == "aberta") {
  color("Tan") base();
  translate([eixo[0], 0, eixo[1]]) rotate([0, -110, 0]) translate([-eixo[0], 0, -eixo[1]]) {
    color("Wheat") tampa();
    tela();
  }
  modulos(com_tela = false);
} else if (parte == "montagem") {
  // os opacos primeiro: o OpenSCAD esconde o que vem depois de um transparente
  modulos();
  color("Tan", 0.35) base();
  color("Wheat", 0.35) tampa();
} else {  // por_dentro
  color("Tan") base();
  modulos(com_tela = false);
}
