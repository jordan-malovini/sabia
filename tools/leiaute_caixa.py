# Desenha o leiaute 2D da caixa (vista da base, da tampa e corte lateral).
# Usa os mesmos parâmetros de hardware/caixa-3d/caixa.scad: se mudar lá, mude aqui.
# Rodar da raiz do repositório: python3 tools/leiaute_caixa.py
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Circle
# mesmos parâmetros do caixa.scad
t=2.4; folga=3; div=2.4; D_in=65; margem=12
rele=(180,90,20); fonte=(99,131,40); expan=(90,130,30); rtc=(17,38,22); sd=(24,42,6); lcd=(93,70,13.5)
inset=3.5
h_inf=rele[1]+2*folga; y_sup=h_inf+div; x_div=margem+fonte[0]+folga
p_rele=(folga,folga); p_fonte=(margem,y_sup+folga); p_expan=(x_div+div+folga,y_sup+folga)
x_col=folga+rele[0]+folga; p_rtc=(x_col,folga); p_sd=(x_col,folga+rtc[1]+4)
W_in=max(p_expan[0]+expan[0]+folga, x_col+max(rtc[0],sd[0])+folga)
H_in=max(p_fonte[1]+fonte[1], p_expan[1]+expan[1])+folga
W=W_in+2*t; H=H_in+2*t; P=D_in+t
lcd_c=(W/2, t+H_in-12-lcd[1]/2); enc_c=(lcd_c[0]+30, lcd_c[1]-lcd[1]/2-24)
fech=[(W-t-4,t+4),(t+x_div+div/2,H-t-4)]
L_dob=H*0.6; y_dob=(H-L_dob)/2; s_dob=(L_dob-4*0.5)/5
print(f"externa {W:.1f} x {H:.1f} x {P+t:.1f}  interna {W_in:.1f} x {H_in:.1f} x {D_in}")

fig, axs = plt.subplots(1, 2, figsize=(15, 8.6), gridspec_kw={"width_ratios":[1.12,1]})
def caixa_base(ax, titulo):
    ax.set_title(titulo, fontsize=13, loc="left", fontweight="bold")
    ax.add_patch(Rectangle((0,0),W,H,fc="#E9E4D6",ec="#555",lw=1.5))
    ax.add_patch(Rectangle((t,t),W_in,H_in,fc="#FFFFFF",ec="#999",lw=0.8))
    ax.set_xlim(-22, W+22); ax.set_ylim(-22, H+14); ax.set_aspect("equal"); ax.axis("off")
ax=axs[0]; caixa_base(ax, "Base (vista de frente, tampa aberta)")
# divisórias
ax.add_patch(Rectangle((t,t+h_inf),x_div+div,div,fc="#555",ec="none"))
ax.add_patch(Rectangle((t+x_div,t+h_inf),div,H_in-h_inf,fc="#555",ec="none"))
def mod(p,m,cor,nome,furos=None,d=6.5):
    x,y=t+p[0],t+p[1]
    ax.add_patch(Rectangle((x,y),m[0],m[1],fc=cor,ec="#333",lw=1,alpha=.55))
    ax.text(x+m[0]/2,y+m[1]/2,f"{nome}\n{m[0]:g}×{m[1]:g}×{m[2]:g}",ha="center",va="center",fontsize=9)
    if furos:
        for fx,fy in furos: ax.add_patch(Circle((x+fx,y+fy),d/2,fc="#222",ec="none"))
cantos=lambda m,i=inset:[(i,i),(m[0]-i,i),(i,m[1]-i),(m[0]-i,m[1]-i)]
mod(p_rele,rele,"#7FA7D9","Relés (16)",cantos(rele))
mod(p_fonte,fonte,"#B9B9B9","Fonte 5 V 10 A\n(rede elétrica)",[(c[0],c[1]) for c in cantos(fonte,10)],d=3.4)
mod(p_expan,expan,"#8CC7A1","ESP32-S3\n+ expansão",cantos(expan))
mod(p_rtc,rtc,"#F2B36B","RTC")
mod(p_sd,sd,"#B79AD9","SD")
for f in fech: ax.add_patch(Circle(f,4,fc="#666",ec="none"))
# prensa-cabos
for x,d,n in [(t+3+45,15.5,"PG9 válvulas"),(t+3+135,15.5,"PG9 válvulas/bomba"),(t+x_col+12,12.5,"PG7 sensores")]:
    ax.add_patch(Rectangle((x-d/2,-1.5),d,t+3,fc="#C0392B",ec="none")); ax.text(x,-10,n,ha="center",fontsize=8,color="#C0392B")
yg=t+p_fonte[1]+fonte[1]-15
ax.add_patch(Rectangle((-1.5,yg-7.75),t+3,15.5,fc="#C0392B",ec="none")); ax.text(-4,yg,"PG9\nrede",ha="right",va="center",fontsize=8,color="#C0392B")
for i in range(6): ax.add_patch(Rectangle((-0.5,t+p_fonte[1]+18+i*17),t+1,3,fc="#FFFFFF",ec="none"))
ax.text(-4,t+p_fonte[1]+60,"ventilação",ha="right",va="center",fontsize=8,color="#555")
# abas e dobradiça
for x in (-14,W):
    for y in (25,H-45): ax.add_patch(Rectangle((x,y),14,20,fc="#D5CFBF",ec="#555",lw=.8)); ax.add_patch(Circle((x+7,y+10),2.25,fc="white",ec="#555"))
for i in range(0,5,2): ax.add_patch(Rectangle((-8,y_dob+i*(s_dob+.5)),8,s_dob,fc="#A0522D",ec="none",alpha=.8))
ax.text(-10,y_dob+L_dob/2,"dobradiça",rotation=90,ha="center",va="center",fontsize=8,color="#A0522D")
ax.annotate("",xy=(0,H+7),xytext=(W,H+7),arrowprops=dict(arrowstyle="<->",lw=.8)); ax.text(W/2,H+9,f"{W:.1f} mm",ha="center",fontsize=9)
ax.annotate("",xy=(W+17,0),xytext=(W+17,H),arrowprops=dict(arrowstyle="<->",lw=.8)); ax.text(W+19,H/2,f"{H:.1f} mm",rotation=90,va="center",fontsize=9)

ax=axs[1]; caixa_base(ax, "Tampa (vista de frente, fechada)")
ax.patches[-1].set_fc("#E9E4D6")
lx,ly=lcd_c[0]-lcd[0]/2, lcd_c[1]-lcd[1]/2
ax.add_patch(Rectangle((lx,ly),lcd[0],lcd[1],fc="none",ec="#2E7D32",lw=1,ls="--"))
ax.add_patch(Rectangle((lcd_c[0]-37,lcd_c[1]-21),74,42,fc="#9CCC65",ec="#2E7D32",lw=1.2))
ax.text(lcd_c[0],lcd_c[1],"janela 74×42\n(tela 128×64)",ha="center",va="center",fontsize=9)
for sx in (-1,1):
    for sy in (-1,1): ax.add_patch(Circle((lcd_c[0]+sx*44,lcd_c[1]+sy*32.5),3,fc="#2E7D32",ec="none"))
ax.add_patch(Circle(enc_c,3.75,fc="#333")); ax.text(enc_c[0],enc_c[1]-9,"encoder Ø7,5",ha="center",fontsize=9)
for f in fech: ax.add_patch(Circle(f,3.25,fc="white",ec="#333")); 
ax.text(W-t-12,t+10,"fechamento M3",ha="right",va="center",fontsize=8); ax.text(t+x_div+div/2-7,H-t-4,"fechamento M3",ha="right",va="center",fontsize=8)
for i in range(1,5,2): ax.add_patch(Rectangle((-8,y_dob+i*(s_dob+.5)),8,s_dob,fc="#A0522D",ec="none",alpha=.8))
ax.text(W/2,30,"SABIÁ",ha="center",fontsize=16,color="#1B4D2C",fontweight="bold",alpha=.5)
ax.text(W/2,18,"(espaço para a logo em relevo ou adesivo)",ha="center",fontsize=8,color="#777")
fig.text(0.5,0.015,f"Externa {W:.1f} × {H:.1f} × {P+t:.1f} mm  ·  interna {W_in:.0f} × {H_in:.1f} × {D_in} mm  ·  "
         "pontos pretos = suportes (furos MEDIR, supostos a 3,5 mm dos cantos)  ·  vermelho = prensa-cabos",ha="center",fontsize=10)
plt.tight_layout(rect=(0,0.03,1,1)); plt.savefig("hardware/caixa-3d/leiaute.png",dpi=110)

# perfil (corte lateral) para conferir a profundidade
fig,ax=plt.subplots(figsize=(10,3.6))
ax.set_title("Corte lateral: o que fica sob a tela e o encoder",loc="left",fontweight="bold")
ax.add_patch(Rectangle((0,0),H,t,fc="#999")); ax.add_patch(Rectangle((0,P),H,t,fc="#999"))
ax.add_patch(Rectangle((0,0),t,P+t,fc="#999")); ax.add_patch(Rectangle((H-t,0),t,P+t,fc="#999"))
def bloco(y0,alt,z0,esp,cor,nome):
    ax.add_patch(Rectangle((y0,z0),alt,esp,fc=cor,alpha=.6,ec="#333")); ax.text(y0+alt/2,z0+esp/2,nome,ha="center",va="center",fontsize=8)
bloco(t+p_rele[1],rele[1],t+5,rele[2],"#7FA7D9","relés (20)")
bloco(t+p_fonte[1],fonte[1],t,fonte[2],"#B9B9B9","fonte (40)")
bloco(lcd_c[1]-35,70,P-13.5,13.5,"#9CCC65","tela (13,5)")
bloco(enc_c[1]-10,20,P-20,20,"#555","encoder")
ax.add_patch(Rectangle((t+h_inf,t),div,65-lcd[2]-2,fc="#333"))
ax.text(t+h_inf+4,t+40,"divisória\n(até 49,5)",fontsize=7)
ax.set_xlim(-5,H+5); ax.set_ylim(-5,P+t+5); ax.set_aspect("equal"); ax.set_xlabel("altura da caixa (Y), mm"); ax.set_ylabel("profundidade (Z), mm")
plt.tight_layout(); plt.savefig("hardware/caixa-3d/corte.png",dpi=110)
