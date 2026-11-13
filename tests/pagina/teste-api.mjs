// Testa a página falando com um controlador de mentira, que responde como o
// firmware de verdade (firmware/src/web.cpp): campos ainda sem dados vêm
// nulos, o histórico vem vazio e alterar exige sessão por cookie.
// O modo demonstração não passa por esse caminho; este teste passa.
// Requer Node 22+ e o Google Chrome. Rodar da raiz: node tests/pagina/teste-api.mjs
import { spawn } from "node:child_process";
import { readFileSync } from "node:fs";
import { createServer } from "node:http";

const pagina = readFileSync(new URL("../../firmware/data/index.html", import.meta.url));
const programa = (ativo, inicio, minutos) => ({ ativo, inicios: [inicio, null, null, null], modo: "semana", dias: [1, 1, 1, 1, 1, 1, 1],
  intervaloDias: 2, inicioIntervalo: null, minutos, ajustePct: 100 });
const fake = {
  logado: false, senhaPadrao: true, fila: [], atual: null, retomada: { faltamS: 95, setor: 3, setores: 2, semEnergiaMin: 4 }, pausa: 0,
  comandos: [],
  config: {
    balancoHidrico: true, adiarPorChuva: true, chuvaAdiarMm: 5,
    retomada: { ativa: true, esperaMin: 10, limiteMin: 120 },
    bomba: { usar: true, atrasoLigarS: 2, atrasoFecharS: 1, sobreposicaoS: 2, pausaEntreSetoresS: 3 },
    setores: Array.from({ length: 8 }, (_, i) => ({ nome: `Setor ${i + 1}`, ativo: true, tipo: "gotejo", duracaoMin: 25, kc: 1, taxaMmH: 2.5,
      eficienciaPct: 90, deficitMinMm: 3, tempoMaxMin: 60, janela: ["00:00", "23:59"], fator: 1 })),
    estacao: { mmPorBasculada: 0.2, chuvaEfetivaMinMm: 2, capacidadeSoloMm: 40, latitude: null, ajusteTempC: 0 },
    relogio: { ntp: false, fuso: "America/Sao_Paulo" },
    rede: { wifiNome: "casa", wifiSenha: "", apNome: "SABIA", apSenha: "", apSempre: false },
    registro: { drive: false, driveUrl: "", envioHoras: 24, diasNoCartao: 7 },
    display: { luzSegundos: 60, travarComandos: false },
    seguranca: { sessaoMin: 30 },
    programas: [programa(true, "06:00", [25, 25, 25, 25, 25, 25, 25, 25]), programa(false, null, [0, 0, 0, 0, 0, 0, 0, 0]), programa(false, null, [0, 0, 0, 0, 0, 0, 0, 0])],
  },
  estado() {
    return {
      versao: "0.4-dev", hora: "2026-11-13T05:10:00-03:00",
      sistema: { wifi: true, ip: "192.168.0.50", modoAp: false, cartaoSd: null, drive: { ativo: false }, rtc: true, ligadoHa: 120 },
      sessao: { logado: this.logado, senhaPadrao: this.senhaPadrao },
      atual: this.atual, fila: this.fila,
      proximaRega: { programa: "A", hora: "06:00", quando: "hoje", faltamMin: 50 }, pausaChuvaDias: this.pausa,
      retomada: this.retomada,
      clima: { chuvaHojeMm: 0.4, chuvaMesMm: null, tempC: null, tempMaxC: null, tempMinC: null, et0HojeMm: null },
      setores: Array.from({ length: 8 }, () => ({ saldoPct: null, adiadoAte: null })),
    };
  },
};
const servidor = createServer((req, res) => {
  let corpo = "";
  req.on("data", d => (corpo += d));
  req.on("end", () => {
    const json = (o, extra = {}) => { res.writeHead(200, { "Content-Type": "application/json", ...extra }); res.end(JSON.stringify(o)); };
    const erro = (codigo, texto) => { res.writeHead(codigo, { "Content-Type": "text/plain; charset=utf-8" }); res.end(texto); };
    const comSessao = /sabia=abc/.test(req.headers.cookie || "") && fake.logado;
    const u = new URL(req.url, "http://x");
    if (u.pathname === "/") { res.writeHead(200, { "Content-Type": "text/html; charset=utf-8" }); return res.end(pagina); }
    if (u.pathname === "/api/estado") return json(fake.estado());
    if (u.pathname === "/api/config" && req.method === "GET") return json(fake.config);
    if (u.pathname === "/api/historico") return json({ rotulos: [], chuvaMm: [], tempMaxC: [], tempMinC: [], et0Mm: [], regas: [], eventos: [] });
    if (u.pathname === "/api/sessao" && req.method === "POST") {
      if (JSON.parse(corpo).senha !== "sabia1234") return erro(401, "Senha incorreta");
      fake.logado = true;
      return json({ ok: true }, { "Set-Cookie": "sabia=abc; Path=/; HttpOnly; SameSite=Strict" });
    }
    if (u.pathname === "/api/sessao" && req.method === "DELETE") { fake.logado = false; return json({ ok: true }); }
    if (!comSessao) return erro(401, "Entre para continuar");
    if (u.pathname === "/api/config" && req.method === "PUT") {
      const novo = JSON.parse(corpo);
      if (novo.programas[0].minutos[0] > novo.setores[0].tempoMaxMin) return erro(400, "Programa A: o setor 1 passa do tempo máximo dele (60 min)");
      if (novo.seguranca.novaSenha) fake.senhaPadrao = false;
      delete novo.seguranca.novaSenha;
      fake.config = novo;
      return json({ ok: true });
    }
    if (u.pathname === "/api/comando") {
      const c = JSON.parse(corpo);
      fake.comandos.push(c);
      if (c.acao === "ligar" && c.setor === 8) return erro(400, "Esse setor está desativado nos ajustes");
      if (c.acao === "ligar") fake.atual = { setor: c.setor, fase: "regando", restanteS: c.minutos * 60, duracaoS: c.minutos * 60, bomba: true };
      if (c.acao === "cancelarRetomada" || c.acao === "retomar") fake.retomada = null;
      if (c.acao === "chuva") fake.pausa = c.dias;
      return json({ ok: true });
    }
    erro(404, "Endereço não encontrado");
  });
});
await new Promise(r => servidor.listen(0, "127.0.0.1", r));
const url = `http://127.0.0.1:${servidor.address().port}/`;

const chrome = spawn("google-chrome", ["--headless=new", "--no-sandbox", "--disable-gpu", "--remote-debugging-port=9334",
  "--user-data-dir=" + (process.env.TMPDIR || "/tmp") + "/sabia-teste-api-" + Date.now(), "--window-size=390,900", "about:blank"], { stdio: "ignore" });
process.on("exit", () => chrome.kill());
process.on("uncaughtException", e => { console.log("ERRO:", e.message); chrome.kill(); process.exit(1); });
const espera = ms => new Promise(r => setTimeout(r, ms));
let alvo;
for (let i = 0; i < 40 && !alvo; i++) { await espera(250); try { alvo = (await (await fetch("http://127.0.0.1:9334/json")).json()).find(t => t.type === "page"); } catch {} }
const ws = new WebSocket(alvo.webSocketDebuggerUrl);
await new Promise(r => (ws.onopen = r));
let id = 0; const pend = new Map(); const erros = [];
ws.onmessage = ev => { const m = JSON.parse(ev.data);
  if (m.id && pend.has(m.id)) { pend.get(m.id)(m); pend.delete(m.id); }
  if (m.method === "Runtime.exceptionThrown") erros.push(m.params.exceptionDetails.exception?.description || m.params.exceptionDetails.text);
};
const cmd = (method, params = {}) => new Promise(r => { const i = ++id; pend.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
const ev = async expr => { const r = await cmd("Runtime.evaluate", { expression: expr, awaitPromise: true, returnByValue: true }); if (r.result.exceptionDetails) throw new Error(expr + " -> " + r.result.exceptionDetails.exception?.description); return r.result.result.value; };
await cmd("Runtime.enable"); await cmd("Page.enable");
await cmd("Emulation.setDeviceMetricsOverride", { width: 390, height: 900, deviceScaleFactor: 1, mobile: true });
await cmd("Page.navigate", { url });
for (let i = 0; i < 40; i++) { await espera(250); try { if (await ev(`document.querySelectorAll('#listaSetores .setor').length>0`)) break; } catch {} }
let falhas = 0;
const ok = (cond, msg) => { if (!cond) falhas++; console.log((cond ? "ok   " : "FALHA") + " " + msg); };
const clicar = sel => ev(`(()=>{const e=document.querySelector(${JSON.stringify(sel)}); if(!e) throw new Error("elemento nao encontrado: "+${JSON.stringify(sel)}); e.click(); return true})()`);
const texto = sel => ev(`document.querySelector(${JSON.stringify(sel)})?.textContent || ""`);

ok(await ev(`document.querySelector('#faixaDemo').classList.contains('oculto')`), "com controlador respondendo, não entra em demonstração");
ok((await texto("#numeros")).includes("0,4") && (await texto("#numeros")).includes("–"), "chuva de hoje aparece e o que não tem sensor mostra traço");
ok(!(await texto("#listaSetores")).includes("Água no solo"), "sem balanço hídrico, a barra de água no solo some");
ok((await texto("#listaSetores")).includes("06:00 · prog. A · 25 min"), "próxima rega de cada setor sai da agenda");
ok((await texto("#textoStatus")) === "Troque a senha", "avisa que a senha ainda é a de fábrica");
ok((await texto("#destaque")).includes("Retomando Setor 3 e mais 1 em 1:35"), "mostra a contagem da retomada depois da queda de energia");

await clicar('[data-acao="cancelarRetomada"]'); await espera(100);
ok(await ev(`document.querySelector('#dlgEntrar').open`), "sem sessão, cancelar a retomada pede a senha");
await ev(`document.querySelector('#senhaEntrar').value='errada'`); await clicar('#btnEntrarOk'); await espera(400);
ok(await ev(`document.querySelector('#dlgEntrar').open && !document.querySelector('#erroEntrar').classList.contains('oculto')`), "senha errada é recusada");
await ev(`document.querySelector('#senhaEntrar').value='sabia1234'`); await clicar('#btnEntrarOk'); await espera(600);
ok((await texto("#chipSessao")) === "Sair", "senha certa abre a sessão");

await clicar('[data-acao="cancelarRetomada"]'); await espera(3500);
ok(fake.comandos.at(-1)?.acao === "cancelarRetomada" && (await texto("#destaque")).includes("Próxima: programa A, hoje às 06:00"), "retomada cancelada; mostra a próxima rega");

await clicar('[data-ligar="2"]'); await espera(100);
await ev(`document.querySelector('#dlgLigarMin').value=15`); await clicar('#dlgLigarOk'); await espera(3500);
ok(fake.comandos.at(-1)?.minutos === 15 && (await texto("#destaque")).includes("Setor 2"), "ligar o setor 2 por 15 min chega ao controlador e aparece no painel");

await clicar('#verTodos'); await espera(200);
await clicar('[data-ligar="8"]'); await espera(100); await clicar('#dlgLigarOk'); await espera(500);
ok((await texto("#toast")).includes("desativado"), "a recusa do controlador aparece com o motivo");

await ev(`location.hash='#historico'`); await espera(800);
ok((await texto("#graficos")).includes("Ainda não há histórico") && await ev(`document.querySelector('[data-csv="chuva"]').disabled`), "histórico vazio explica o motivo e desliga os CSV");

await ev(`location.hash='#ajustes'`); await espera(800);
ok((await texto("#formAjustes")).includes("Programa A"), "ajustes carregam a configuração do controlador");
await ev(`(()=>{const i=document.querySelector('[data-cfg="programas.0.ajustePct"]'); i.value=80; i.dispatchEvent(new Event('input',{bubbles:true})); i.dispatchEvent(new Event('change',{bubbles:true}));})()`);
await espera(200); await clicar('#btnSalvar'); await espera(800);
ok(fake.config.programas[0].ajustePct === 80 && !("novaSenha" in fake.config.seguranca), "salvar envia a configuração inteira ao controlador");

await clicar('#chipSessao'); await espera(600);
ok(!fake.logado && (await texto("#chipSessao")).includes("Entrar"), "sair encerra a sessão");

console.log(erros.length ? "ERROS DE JAVASCRIPT:\n" + erros.join("\n") : "sem erros de JavaScript");
ws.close(); chrome.kill(); servidor.close();
process.exit(falhas || erros.length ? 1 : 0);
