// SABIÁ - caixa do controlador (Rev A)
//
// Caixa de parede impressa em 3D (PETG ou ASA): base com divisórias e suportes
// para cada módulo, e tampa com dobradiça, janela da tela e furo do encoder.
//
// Como usar:
//   1. Abra no OpenSCAD (gratuito) e confira as medidas marcadas MEDIR.
//   2. Mude "parte" para "base" ou "tampa", aperte F6 e exporte o STL (F7).
//   3. "montagem" mostra as duas partes fechadas, com os módulos em fantasma.
//
// Medidas em mm. Coordenadas da base: X para a direita, Y para cima (caixa
// presa na parede), Z saindo da parede em direção à tampa.

parte = "montagem";  // "base", "tampa", "montagem", "aberta"

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
rtc   = [17, 38, 22];    // módulo RTC DS3231 (38 x 17 x 22), em pé
sd    = [24, 42, 6];     // módulo microSD. MEDIR: medida estimada, não informada
lcd   = [93, 70, 13.5];  // tela ST7920 12864B (desenho do fabricante)

// Furos de fixação de cada placa, medidos a partir do canto inferior esquerdo
// do módulo já na posição de montagem. MEDIR: os valores abaixo supõem furos
// nos 4 cantos, a 3,5 mm das bordas. Troque pelos centros reais.
inset = 3.5;
function cantos(m, i = inset) = [[i, i], [m[0] - i, i], [i, m[1] - i], [m[0] - i, m[1] - i]];
furos_rele  = cantos(rele);
furos_expan = cantos(expan);
furos_fonte = cantos(fonte, 10);  // MEDIR: furos roscados no fundo da fonte

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
p_sd    = [x_col, folga + rtc[1] + 4];

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
module suportes(pos, furos, d_furo = d_auto_m3) {
  for (f = furos)
    translate([pos[0] + f[0], pos[1] + f[1], 0])
      difference() {
        cylinder(d = d_suporte, h = alt_suporte);
        translate([0, 0, 1]) cylinder(d = d_furo, h = alt_suporte);
      }
}

// Berço para módulos sem furo: moldura baixa onde a placa encaixa
// (prender com fita dupla face ou abraçadeira)
module berco(pos, m, alt = 6, parede = 1.6) {
  translate([pos[0] - parede - 0.3, pos[1] - parede - 0.3, 0])
    difference() {
      cube([m[0] + 2 * parede + 0.6, m[1] + 2 * parede + 0.6, alt]);
      translate([parede, parede, -1]) cube([m[0] + 0.6, m[1] + 0.6, alt + 2]);
      // aberturas para os fios
      translate([-1, (m[1] + 2 * parede) / 2 - 5, 2]) cube([m[0] + 2 * parede + 3, 10, alt]);
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
        berco(p_rtc, rtc);
        berco(p_sd, sd);
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
// Módulos em fantasma (só para visualizar)
// --------------------------------------------------------------------------
module modulos() {
  translate([t, t, t]) {
    color("SteelBlue", 0.5) translate([p_rele[0], p_rele[1], alt_suporte]) cube(rele);
    color("Gray", 0.5) translate([p_fonte[0], p_fonte[1], 0]) cube(fonte);
    color("SeaGreen", 0.5) translate([p_expan[0], p_expan[1], alt_suporte]) cube(expan);
    color("Orange", 0.5) translate([p_rtc[0], p_rtc[1], 1]) cube(rtc);
    color("Purple", 0.5) translate([p_sd[0], p_sd[1], 1]) cube(sd);
  }
  color("LimeGreen", 0.5)
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
  base();
  translate([eixo[0], 0, eixo[1]]) rotate([0, -110, 0]) translate([-eixo[0], 0, -eixo[1]]) tampa();
  %modulos();
} else {
  base();
  tampa();
  %modulos();
}
