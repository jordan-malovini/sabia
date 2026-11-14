# SABIÁ - caixa do controlador no Fusion
#
# Desenha, num projeto novo do Fusion, a mesma caixa de hardware/caixa-3d/caixa.scad
# (Rev A): a base com divisórias e suportes, a tampa com a janela da tela e
# os componentes como blocos (só para conferir o encaixe).
#
# Como rodar: Utilitários > Suplementos > Scripts e suplementos > "+" >
# "Script ou suplemento do dispositivo" > escolha a pasta SabiaCaixa > Executar.
# Passo a passo em docs/caixa.md.
#
# Depois de rodar, tudo é editável no próprio Fusion: cada parte vira um
# esboço e uma extrusão com nome na linha do tempo (embaixo). Para mudar uma
# altura, clique duas vezes na extrusão. Para mover um furo ou um suporte,
# clique duas vezes no esboço dele.
#
# As medidas ficam todas aqui embaixo, em mm, com os mesmos nomes do .scad.
# Para trocar várias de uma vez (por exemplo, quando medir os furos reais das
# placas), mude aqui e rode o script de novo: ele cria outro projeto.

import math
import traceback

import adsk.core
import adsk.fusion

# ---------------------------------------------------------------------------
# Medidas (mm)
# ---------------------------------------------------------------------------
t = 2.4            # espessura das paredes, do fundo e da tampa
folga = 3          # espaço livre em volta de cada módulo
div = 2.4          # espessura das divisórias
D_in = 65          # profundidade interna da base
folga_enc = 0.3    # folga de encaixe entre a aba da tampa e a parede

alt_suporte = 5    # altura dos suportes das placas
d_suporte = 6.5    # diâmetro externo dos suportes
d_auto_m3 = 2.6    # furo para parafuso M3 autoatarraxante
d_auto_m25 = 2.2   # furo para parafuso M2.5 autoatarraxante
d_passante = 3.4   # furo passante para M3

# Módulos: (largura X, altura Y, espessura Z), na posição de montagem
rele = (180, 90, 20)
fonte = (99, 131, 40)
expan = (90, 130, 30)
rtc = (22, 38, 17)
sd = (17.5, 18.5, 5)
lcd = (93, 70, 13.5)


def cantos(m, i=3.5):
    # MEDIR: supõe furos nos 4 cantos; troque pelos centros reais
    return [(i, i), (m[0] - i, i), (i, m[1] - i), (m[0] - i, m[1] - i)]


furos_rele = cantos(rele)
furos_expan = cantos(expan)
furos_fonte = cantos(fonte, 10)                      # MEDIR
furos_rtc = [(1.7, 3.4), (1.7, 28.8), (20.2, 28.8)]  # MEDIR

sd_furos_dist = 14
sd_furo_borda = 6  # MEDIR
sd_elev = 7
sd_afasta = 3

lcd_furos = (88, 65)
lcd_janela = (74, 42)
lcd_moldura = 8.9  # MEDIR
acrilico = (80, 48, 1.2)
d_encoder = 7.5

d_pg9 = 15.5
d_pg7 = 12.5
margem_rede = 12

# ---------------------------------------------------------------------------
# Leiaute (igual ao .scad; origem no canto interno inferior esquerdo)
# ---------------------------------------------------------------------------
h_inf = rele[1] + 2 * folga
y_sup = h_inf + div
x_div = margem_rede + fonte[0] + folga
p_rele = (folga, folga)
p_fonte = (margem_rede, y_sup + folga)
p_expan = (x_div + div + folga, y_sup + folga)
x_col = folga + rele[0] + folga
p_rtc = (x_col - 1, folga)
p_sd = (x_col + (rtc[0] - sd[0]) / 2, folga + rtc[1] + 21)

W_in = max(p_expan[0] + expan[0] + folga, x_col + max(rtc[0], sd[0]) + folga)
H_in = max(p_fonte[1] + fonte[1], p_expan[1] + expan[1]) + folga
W = W_in + 2 * t
H = H_in + 2 * t
P = D_in + t

lcd_c = (W / 2, t + H_in - 12 - lcd[1] / 2)
enc_c = (lcd_c[0] + 30, lcd_c[1] - lcd[1] / 2 - 24)
fech = [(W - t - 4, t + folga + rtc[1] + 10), (t + x_div + div / 2, H - t - 4)]

r_dob = 4
d_pino = 3.2
L_dob = H * 0.6
n_dob = 5
gap_dob = 0.5
s_dob = (L_dob - (n_dob - 1) * gap_dob) / n_dob
y_dob = (H - L_dob) / 2
eixo = (-r_dob, P + t - r_dob)  # eixo da dobradiça em (X, Z)

alt_divisoria = D_in - lcd[2] - 2

# ---------------------------------------------------------------------------
# Ajudantes. A API do Fusion trabalha em centímetros.
# ---------------------------------------------------------------------------
CM = 0.1
etapa = ""


def ponto(x, y, z=0.0):
    return adsk.core.Point3D.create(x * CM, y * CM, z * CM)


def valor(mm):
    return adsk.core.ValueInput.createByReal(mm * CM)


class Desenho:
    """Esboços e extrusões dentro de um componente."""

    def __init__(self, comp):
        self.comp = comp
        self.planos = {}

    def plano(self, eixo_normal, posicao):
        """Plano perpendicular a X, Y ou Z, na posição dada (mm)."""
        base = {"x": self.comp.yZConstructionPlane, "y": self.comp.xZConstructionPlane,
                "z": self.comp.xYConstructionPlane}[eixo_normal]
        if abs(posicao) < 1e-9:
            return base
        chave = (eixo_normal, round(posicao, 4))
        if chave in self.planos:
            return self.planos[chave]
        planos = self.comp.constructionPlanes
        indice = "xyz".index(eixo_normal)
        # O sentido do deslocamento depende do plano: tenta e confere.
        for sinal in (1, -1):
            entrada = planos.createInput()
            entrada.setByOffset(base, valor(sinal * posicao))
            plano = planos.add(entrada)
            origem = plano.geometry.origin
            if abs((origem.x, origem.y, origem.z)[indice] - posicao * CM) < 1e-6:
                plano.isLightBulbOn = False
                self.planos[chave] = plano
                return plano
            plano.deleteMe()
        raise RuntimeError("não consegui criar o plano %s = %s mm" % (eixo_normal, posicao))

    def esboco(self, nome, eixo_normal="z", posicao=0.0):
        s = self.comp.sketches.add(self.plano(eixo_normal, posicao))
        s.name = nome
        s.isComputeDeferred = True
        return s

    @staticmethod
    def local(s, x, y, z):
        return s.modelToSketchSpace(ponto(x, y, z))

    def retangulo(self, s, x0, y0, x1, y1, z=0.0):
        s.sketchCurves.sketchLines.addTwoPointRectangle(self.local(s, x0, y0, z), self.local(s, x1, y1, z))

    def circulo(self, s, x, y, z, d):
        s.sketchCurves.sketchCircles.addByCenterRadius(self.local(s, x, y, z), d / 2 * CM)

    def poligono(self, s, pontos):
        # Cada linha começa no ponto final da anterior, para o contorno fechar.
        linhas = s.sketchCurves.sketchLines
        locais = [self.local(s, *p) for p in pontos]
        primeira = anterior = linhas.addByTwoPoints(locais[0], locais[1])
        for p in locais[2:]:
            anterior = linhas.addByTwoPoints(anterior.endSketchPoint, p)
        linhas.addByTwoPoints(anterior.endSketchPoint, primeira.startSketchPoint)

    @staticmethod
    def perfis(s, filtro=None):
        s.isComputeDeferred = False
        colecao = adsk.core.ObjectCollection.create()
        for p in s.profiles:
            if filtro is None or filtro(p):
                colecao.add(p)
        if colecao.count == 0:
            raise RuntimeError("o esboço '%s' não formou nenhuma região fechada" % s.name)
        return colecao

    def extrudar(self, nome, perfis, z0, altura, operacao, corpos=None):
        """Extrusão para +Z a partir de z0 (esboços no plano XY)."""
        extrusoes = self.comp.features.extrudeFeatures
        entrada = extrusoes.createInput(perfis, operacao)
        entrada.startExtent = adsk.fusion.OffsetStartDefinition.create(valor(z0))
        entrada.setDistanceExtent(False, valor(altura))
        if corpos:
            entrada.participantBodies = corpos
        f = extrusoes.add(entrada)
        f.name = nome
        return f

    def atravessar(self, nome, perfis, comprimento, operacao, corpos=None):
        """Extrusão simétrica em volta do plano do esboço (furos em paredes)."""
        extrusoes = self.comp.features.extrudeFeatures
        entrada = extrusoes.createInput(perfis, operacao)
        entrada.setSymmetricExtent(valor(comprimento), True)
        if corpos:
            entrada.participantBodies = corpos
        f = extrusoes.add(entrada)
        f.name = nome
        return f


NOVO = adsk.fusion.FeatureOperations.NewBodyFeatureOperation
UNIR = adsk.fusion.FeatureOperations.JoinFeatureOperation
CORTAR = adsk.fusion.FeatureOperations.CutFeatureOperation


def casca_convexa(pontos):
    """Contorno convexo de uma lista de pontos 2D (sentido anti-horário)."""
    pts = sorted(set(pontos))

    def giro(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])

    baixo, cima = [], []
    for p in pts:
        while len(baixo) >= 2 and giro(baixo[-2], baixo[-1], p) <= 1e-9:
            baixo.pop()
        baixo.append(p)
    for p in reversed(pts):
        while len(cima) >= 2 and giro(cima[-2], cima[-1], p) <= 1e-9:
            cima.pop()
        cima.append(p)
    return baixo[:-1] + cima[:-1]


def perfil_dobradica(retangulo):
    """Contorno (X, Z) de um nó da dobradiça: círculo do eixo + apoio retangular."""
    pts = [(round(eixo[0] + r_dob * math.cos(2 * math.pi * k / 48), 4),
            round(eixo[1] + r_dob * math.sin(2 * math.pi * k / 48), 4)) for k in range(48)]
    x0, z0, x1, z1 = retangulo
    pts += [(x0, z0), (x1, z0), (x1, z1), (x0, z1)]
    return casca_convexa(pts)


def dobradica(d, nome, indices, retangulo, corpo):
    for i in indices:
        y0 = y_dob + i * (s_dob + gap_dob)
        meio = y0 + s_dob / 2
        s = d.esboco("%s %d" % (nome, i + 1), "y", meio)
        d.poligono(s, [(x, meio, z) for x, z in perfil_dobradica(retangulo)])
        d.atravessar("%s %d" % (nome, i + 1), d.perfis(s), s_dob, UNIR, [corpo])
        s = d.esboco("Pino %s %d" % (nome.lower(), i + 1), "y", meio)
        d.circulo(s, eixo[0], meio, eixo[1], d_pino)
        d.atravessar("Pino %s %d" % (nome.lower(), i + 1), d.perfis(s), s_dob + 2, CORTAR, [corpo])


def circulos(d, nome, centros, diametro, z0, altura, operacao, corpo):
    s = d.esboco(nome)
    for x, y in centros:
        d.circulo(s, x, y, 0, diametro)
    return d.extrudar(nome, d.perfis(s), z0, altura, operacao, [corpo] if corpo else None)


def retangulos(d, nome, caixas, z0, altura, operacao, corpo, filtro=None):
    s = d.esboco(nome)
    for x0, y0, x1, y1 in caixas:
        d.retangulo(s, x0, y0, x1, y1)
    return d.extrudar(nome, d.perfis(s, filtro), z0, altura, operacao, [corpo] if corpo else None)


def furos_parede(d, nome, eixo_normal, posicao, centros, diametro, corpo):
    """Furos atravessando uma parede. `centros` em coordenadas do modelo."""
    s = d.esboco(nome, eixo_normal, posicao)
    for c in centros:
        d.circulo(s, c[0], c[1], c[2], diametro)
    d.atravessar(nome, d.perfis(s), 2 * (t + 2), CORTAR, [corpo])


def novo_componente(raiz, nome):
    occ = raiz.occurrences.addNewComponent(adsk.core.Matrix3D.create())
    occ.component.name = nome
    return occ


# ---------------------------------------------------------------------------
# Base
# ---------------------------------------------------------------------------
def desenhar_base(comp):
    global etapa
    d = Desenho(comp)

    etapa = "base: caixa"
    f = retangulos(d, "Caixa", [(0, 0, W, H)], 0, P, NOVO, None)
    corpo = f.bodies.item(0)
    corpo.name = "Base"
    etapa = "base: interior"
    retangulos(d, "Interior", [(t, t, W - t, H - t)], t, D_in + 1, CORTAR, corpo)

    etapa = "base: divisórias"
    retangulos(d, "Divisorias da fonte",
               [(t, t + h_inf, t + x_div + div, t + h_inf + div),
                (t + x_div, t + h_inf, t + x_div + div, t + H_in)], 0, t + alt_divisoria, UNIR, corpo)

    etapa = "base: suportes"
    for nome, pos, furos, d_ext, d_furo in [
            ("reles", p_rele, furos_rele, d_suporte, d_auto_m3),
            ("ESP", p_expan, furos_expan, d_suporte, d_auto_m3),
            ("RTC", p_rtc, furos_rtc, 5, d_auto_m25)]:
        centros = [(t + pos[0] + fx, t + pos[1] + fy) for fx, fy in furos]
        circulos(d, "Suportes " + nome, centros, d_ext, t, alt_suporte, UNIR, corpo)
        circulos(d, "Furos " + nome, centros, d_furo, t + 1, alt_suporte, CORTAR, corpo)

    etapa = "base: suporte do microSD"
    z_furo = sd_elev + sd_furo_borda
    x0, y0 = t + p_sd[0], t + p_sd[1]
    retangulos(d, "Paredinha microSD", [(x0 - 2, y0, x0 + sd[0] + 2, y0 + div)], t, z_furo + 5, UNIR, corpo)
    nervuras = [(x0 + sd[0] / 2 + sx * sd_furos_dist / 2 - 2.5, y0 + div - 0.01,
                 x0 + sd[0] / 2 + sx * sd_furos_dist / 2 + 2.5, y0 + div + sd_afasta) for sx in (-1, 1)]
    retangulos(d, "Nervuras microSD", nervuras, t, z_furo + 3.5, UNIR, corpo)
    meio = y0 + (div + sd_afasta) / 2
    s = d.esboco("Furos microSD", "y", meio)
    for sx in (-1, 1):
        d.circulo(s, x0 + sd[0] / 2 + sx * sd_furos_dist / 2, meio, t + z_furo, d_auto_m25)
    d.atravessar("Furos microSD", d.perfis(s), div + sd_afasta + 2, CORTAR, [corpo])

    etapa = "base: fechamento"
    circulos(d, "Ressaltos de fechamento", fech, 9, 0, P - 3.5, UNIR, corpo)
    circulos(d, "Rosca de fechamento", fech, d_auto_m3, P - 18, 20, CORTAR, corpo)

    etapa = "base: abas de parede"
    abas = [(x, y, x + 14, y + 20) for x in (-14, W) for y in (25, H - 45)]
    retangulos(d, "Abas de parede", abas, 0, 4, UNIR, corpo)
    circulos(d, "Furos das abas", [(x, y) for x in (-7, W + 7) for y in (35, H - 35)], 4.5, -1, 11, CORTAR, corpo)

    etapa = "base: dobradiça"
    dobradica(d, "Dobradica base", range(0, n_dob, 2), (0, eixo[1] - 3 * r_dob, 1, eixo[1] - 3 * r_dob + 1), corpo)

    etapa = "base: furos da fonte"
    circulos(d, "Furos da fonte", [(t + p_fonte[0] + fx, t + p_fonte[1] + fy) for fx, fy in furos_fonte],
             d_passante, -1, t + 2, CORTAR, corpo)

    etapa = "base: prensa-cabos"
    furos_parede(d, "Prensa-cabos valvulas", "y", 0,
                 [(t + p_rele[0] + 45, 0, t + 42), (t + p_rele[0] + 135, 0, t + 42)], d_pg9, corpo)
    furos_parede(d, "Prensa-cabos sensores", "y", 0, [(t + x_col + 12, 0, t + 45)], d_pg7, corpo)
    furos_parede(d, "Entrada da rede", "x", 0, [(0, t + p_fonte[1] + fonte[1] - 15, t + 30)], d_pg9, corpo)

    etapa = "base: passagem dos fios"
    retangulos(d, "Passagem dos fios",
               [(t + 60, t + h_inf - 1, t + 78, t + h_inf + div + 1),
                (t + x_div - 1, t + h_inf + 15, t + x_div + div + 1, t + h_inf + 33)], t, 10, CORTAR, corpo)

    etapa = "base: ventilação"
    s = d.esboco("Ventilacao da fonte", "x", 0)
    for i in range(6):
        y = t + p_fonte[1] + 18 + i * 17
        p0 = d.local(s, 0, y, t + 10)
        p1 = d.local(s, 0, y + 3, t + 40)
        s.sketchCurves.sketchLines.addTwoPointRectangle(p0, p1)
    d.atravessar("Ventilacao da fonte", d.perfis(s), 2 * (t + 2), CORTAR, [corpo])
    return corpo


# ---------------------------------------------------------------------------
# Tampa (na posição fechada, sobre a base)
# ---------------------------------------------------------------------------
def desenhar_tampa(comp):
    global etapa
    d = Desenho(comp)

    etapa = "tampa: placa"
    f = retangulos(d, "Placa da tampa", [(0, 0, W, H)], P, t, NOVO, None)
    corpo = f.bodies.item(0)
    corpo.name = "Tampa"

    etapa = "tampa: aba"
    a0 = t + folga_enc
    retangulos(d, "Aba de encaixe",
               [(a0, a0, W - a0, H - a0), (a0 + 1.6, a0 + 1.6, W - a0 - 1.6, H - a0 - 1.6)],
               P - 3, 3, UNIR, corpo, filtro=lambda p: p.profileLoops.count == 2)  # só o anel
    circulos(d, "Recortes da aba", fech, 9, P - 4, 4, CORTAR, corpo)
    circulos(d, "Espacadores", fech, 8, P - 3.3, 3.3, UNIR, corpo)

    etapa = "tampa: suportes da tela"
    cantos_tela = [(lcd_c[0] + sx * lcd_furos[0] / 2, lcd_c[1] + sy * lcd_furos[1] / 2) for sx in (-1, 1) for sy in (-1, 1)]
    circulos(d, "Suportes da tela", cantos_tela, 6, P - lcd_moldura, lcd_moldura, UNIR, corpo)
    circulos(d, "Furos da tela", cantos_tela, d_auto_m25, P - lcd_moldura - 1, lcd_moldura, CORTAR, corpo)

    etapa = "tampa: dobradiça"
    dobradica(d, "Dobradica tampa", range(1, n_dob, 2), (-0.5, P, 0.5, P + t), corpo)

    etapa = "tampa: janela da tela"
    jx, jy = lcd_janela[0] / 2, lcd_janela[1] / 2
    retangulos(d, "Janela da tela", [(lcd_c[0] - jx, lcd_c[1] - jy, lcd_c[0] + jx, lcd_c[1] + jy)], P - 1, t + 2, CORTAR, corpo)
    ax, ay = acrilico[0] / 2, acrilico[1] / 2
    retangulos(d, "Rebaixo do acrilico", [(lcd_c[0] - ax, lcd_c[1] - ay, lcd_c[0] + ax, lcd_c[1] + ay)],
               P - 1, acrilico[2] + 1, CORTAR, corpo)

    etapa = "tampa: furos"
    circulos(d, "Furo do encoder", [enc_c], d_encoder, P - 4, t + 8, CORTAR, corpo)
    circulos(d, "Furos de fechamento", fech, d_passante, P - 4, t + 8, CORTAR, corpo)

    # A tela vai presa na tampa: o bloco dela fica aqui, para abrir junto.
    # Ao exportar para imprimir, salve só o corpo "Tampa".
    etapa = "tampa: bloco da tela"
    x0, y0 = lcd_c[0] - lcd[0] / 2, lcd_c[1] - lcd[1] / 2
    f = retangulos(d, "Tela (bloco)", [(x0, y0, x0 + lcd[0], y0 + lcd[1])], P - lcd[2], lcd[2], NOVO, None)
    f.bodies.item(0).name = "Tela (so para conferir)"
    return corpo


def juntar_tampa(raiz, occ_base, occ_tampa):
    """Junta de rotação na dobradiça: a tampa abre girando em volta do pino."""
    global etapa
    etapa = "junta da dobradiça"
    s = occ_base.component.sketches.add(occ_base.component.xZConstructionPlane)
    s.name = "Eixo da dobradica"
    ponto_eixo = s.sketchPoints.add(s.modelToSketchSpace(ponto(eixo[0], 0, eixo[1])))
    geometria = adsk.fusion.JointGeometry.createByPoint(ponto_eixo.createForAssemblyContext(occ_base))
    juntas = raiz.asBuiltJoints
    entrada = juntas.createInput(occ_tampa, occ_base, geometria)
    entrada.setAsRevoluteJointMotion(adsk.fusion.JointDirections.YAxisJointDirection)
    junta = juntas.add(entrada)
    junta.name = "Dobradica"
    # Abrir é girar para o lado negativo, até 110 graus.
    limites = junta.jointMotion.rotationLimits
    limites.isMinimumValueEnabled = True
    limites.minimumValue = math.radians(-110)
    limites.isMaximumValueEnabled = True
    limites.maximumValue = 0


# ---------------------------------------------------------------------------
# Componentes (blocos para conferir o encaixe; não vão para a impressão)
# ---------------------------------------------------------------------------
def desenhar_modulos(comp):
    global etapa
    etapa = "componentes"
    d = Desenho(comp)
    blocos = [
        ("Reles", (t + p_rele[0], t + p_rele[1], t + alt_suporte), rele),
        ("Fonte", (t + p_fonte[0], t + p_fonte[1], t), fonte),
        ("ESP32-S3", (t + p_expan[0], t + p_expan[1], t + alt_suporte), expan),
        ("RTC", (t + p_rtc[0], t + p_rtc[1], t + alt_suporte), rtc),
        ("microSD", (t + p_sd[0], t + p_sd[1] + div + sd_afasta, t + sd_elev), (sd[0], sd[2], sd[1])),
    ]
    for nome, (x, y, z), (lx, ly, lz) in blocos:
        f = retangulos(d, nome, [(x, y, x + lx, y + ly)], z, lz, NOVO, None)
        f.bodies.item(0).name = nome


def run(context):
    global etapa
    app = adsk.core.Application.get()
    ui = app.userInterface
    try:
        etapa = "projeto novo"
        app.documents.add(adsk.core.DocumentTypes.FusionDesignDocumentType)
        design = adsk.fusion.Design.cast(app.activeProduct)
        design.designType = adsk.fusion.DesignTypes.ParametricDesignType
        raiz = design.rootComponent

        occ_base = novo_componente(raiz, "Base")
        desenhar_base(occ_base.component)
        occ_tampa = novo_componente(raiz, "Tampa")
        desenhar_tampa(occ_tampa.component)
        occ_mod = novo_componente(raiz, "Componentes (so para conferir)")
        desenhar_modulos(occ_mod.component)

        # Base e componentes ficam fixos; só a tampa se mexe.
        occ_base.isGrounded = True
        occ_mod.isGrounded = True
        aviso_junta = ""
        try:
            juntar_tampa(raiz, occ_base, occ_tampa)
        except Exception:
            aviso_junta = ("\n\nNão consegui criar a junta da dobradiça. Faça à mão: MONTAR > Junta "
                           "conforme construída, Tampa e Base, tipo Rotação, posição no furo do pino "
                           "(passo a passo em docs/caixa.md).")

        app.activeViewport.fit()
        ui.messageBox(
            "Caixa desenhada: %.1f x %.1f x %.1f mm (base %.1f + tampa %.1f).\n\n"
            "Para abrir a tampa, arraste-a com o mouse: ela gira na dobradiça (até 110 graus).\n"
            "Para ver por dentro, esconda o componente 'Tampa' (ícone de olho no navegador, à esquerda).\n"
            "Para imprimir, exporte só o corpo 'Base' e só o corpo 'Tampa' (botão direito > Salvar como malha)."
            "%s" % (W, H, P + t, P, t, aviso_junta), "SABIÁ")
    except Exception:
        ui.messageBox("Parou na etapa: %s\n\n%s" % (etapa, traceback.format_exc()), "SABIÁ: erro")
