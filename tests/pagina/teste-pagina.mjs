// Testes da página web em modo demonstração, no Chrome sem janela.
// Requer Node 22+ e o Google Chrome instalado (comando "google-chrome").
// Rodar da raiz do repositório: node tests/pagina/teste-pagina.mjs
import { spawn } from "node:child_process";
const url = new URL("../../firmware/data/index.html#painel", import.meta.url).href;
const chrome = spawn("google-chrome", ["--headless=new","--no-sandbox","--disable-gpu","--remote-debugging-port=9333","--user-data-dir=" + (process.env.TMPDIR || "/tmp") + "/sabia-teste-" + Date.now(),"--window-size=390,900","about:blank"], { stdio: "ignore" });
process.on("exit", () => chrome.kill());
process.on("uncaughtException", e => { console.log("ERRO:", e.message); chrome.kill(); process.exit(1); });
const espera = ms => new Promise(r => setTimeout(r, ms));
let alvo;
for (let i = 0; i < 40 && !alvo; i++) { await espera(250); try { alvo = (await (await fetch("http://127.0.0.1:9333/json")).json()).find(t => t.type === "page"); } catch {} }
const ws = new WebSocket(alvo.webSocketDebuggerUrl);
await new Promise(r => ws.onopen = r);
let id = 0; const pend = new Map(); const erros = [];
ws.onmessage = ev => { const m = JSON.parse(ev.data);
  if (m.id && pend.has(m.id)) { pend.get(m.id)(m); pend.delete(m.id); }
  if (m.method === "Runtime.exceptionThrown") erros.push(m.params.exceptionDetails.exception?.description || m.params.exceptionDetails.text);
  if (m.method === "Runtime.consoleAPICalled" && m.params.type === "error") erros.push("console: " + m.params.args.map(a => a.value ?? a.description).join(" "));
};
const cmd = (method, params = {}) => new Promise(r => { const i = ++id; pend.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
const ev = async expr => { const r = await cmd("Runtime.evaluate", { expression: expr, awaitPromise: true, returnByValue: true }); if (r.result.exceptionDetails) throw new Error(expr + " -> " + r.result.exceptionDetails.exception?.description); return r.result.result.value; };
await cmd("Runtime.enable"); await cmd("Page.enable");
await cmd("Emulation.setDeviceMetricsOverride", { width: 390, height: 900, deviceScaleFactor: 1, mobile: true });
await cmd("Page.navigate", { url });
for (let i = 0; i < 40; i++) { await espera(250); try { if (await ev(`document.querySelectorAll('#listaSetores .setor').length>0`)) break; } catch {} }
let falhas = 0;
const ok = (cond, msg) => { if (!cond) falhas++; console.log((cond ? "ok   " : "FALHA") + " " + msg); };
const clicar = sel => ev(`(()=>{const e=document.querySelector(${JSON.stringify(sel)}); if(!e) throw new Error("elemento nao encontrado"); e.click(); return true})()`);

ok(await ev(`!document.querySelector('#faixaDemo').classList.contains('oculto')`), "modo demonstração ativo");
ok((await ev(`document.querySelectorAll('#listaSetores .setor').length`)) === 4, "celular mostra 4 setores");
await clicar('#verTodos'); await espera(200);
ok((await ev(`document.querySelectorAll('#listaSetores .setor').length`)) === 8, "ver todos mostra 8");

// sem login: ligar abre o diálogo de entrar
await clicar('[data-ligar="1"]'); await espera(100);
ok(await ev(`document.querySelector('#dlgEntrar').open`), "sem login, Ligar pede senha");
await ev(`document.querySelector('#senhaEntrar').value='x'`); await clicar('#btnEntrarOk'); await espera(400);
ok(await ev(`!document.querySelector('#dlgEntrar').open && document.querySelector('#chipSessao').textContent==='Sair'`), "entrou");

// pôr na fila o setor 1 com 20 min
await clicar('[data-ligar="1"]'); await espera(100);
ok(await ev(`document.querySelector('#dlgLigar').open && document.querySelector('#dlgLigarOk').textContent==='Pôr na fila'`), "com algo regando, o botão diz Pôr na fila");
await ev(`document.querySelector('#dlgLigarMin').value=20`); await clicar('#dlgLigarOk'); await espera(500);
ok(await ev(`[...document.querySelectorAll('#destaque .fila .item')].some(e=>e.textContent.includes('Setor 1 · 20 min'))`), "setor 1 entrou no fim da fila com 20 min");

// tirar da fila pelo ×
await clicar('[data-tirar="5"]'); await espera(500);
ok(await ev(`![...document.querySelectorAll('#destaque .fila .item')].some(e=>e.textContent.includes('Setor 5'))`), "× tira o setor 5 da fila");

// parar tudo pede confirmação
await clicar('[data-acao="pararTudo"]'); await espera(100);
ok(await ev(`document.querySelector('#dlgConfirmar').open`), "parar tudo pede confirmação");
await clicar('#dlgConfirmarOk'); await espera(6500);
ok(await ev(`document.querySelector('#destaque').classList.contains('parado')`), "depois de parar tudo, nada regando");
ok(await ev(`document.querySelectorAll('#destaque .fila .item').length===0`), "fila vazia");

// ligar com nada regando
await clicar('[data-ligar="2"]'); await espera(100);
ok(await ev(`document.querySelector('#dlgLigarOk').textContent==='Ligar'`), "parado, o botão diz Ligar");
await clicar('#dlgLigarOk'); await espera(1500);
ok(await ev(`document.querySelector('#destaque').textContent.includes('Setor 2')`), "setor 2 começou");
await espera(2200);
ok(await ev(`document.querySelector('#destaque .bomba:not(.desligada)')!==null`), "bomba liga depois do atraso");

// histórico
await ev(`location.hash='historico'`); await espera(400);
ok((await ev(`document.querySelectorAll('#graficos svg').length`)) === 2, "hoje: 2 gráficos");
await clicar('#periodos [data-p="7d"]'); await espera(300);
ok((await ev(`document.querySelectorAll('#graficos svg').length`)) === 3, "7 dias: 3 gráficos");
await clicar('#periodos [data-p="30d"]'); await espera(300);
ok((await ev(`document.querySelectorAll('#graficos svg rect').length`)) > 0, "30 dias desenha");

// ajustes
await ev(`location.hash='ajustes'`); await espera(400);
ok(await ev(`document.querySelector('#avisoLeitura').classList.contains('oculto')`), "logado: sem aviso de leitura");
ok(await ev(`!document.querySelector('[data-cfg="chuvaAdiarMm"]').disabled`), "campos habilitados");
await clicar('[data-seg="modo"] [data-valor="ciclo"]'); await espera(100);
ok(await ev(`document.querySelector('#blocoCiclo').style.display==='grid'`), "modo ciclo mostra o bloco do ciclo");
ok(await ev(`!document.querySelector('#barraSalvar').classList.contains('oculto')`), "barra de salvar aparece");
await clicar('[data-seg="modo"] [data-valor="individual"]'); await espera(100);
// renomear o setor 3 e trocar o horário
await ev(`(()=>{const i=document.querySelector('[data-cfg="setores.2.nome"]'); i.value='Horta'; i.dispatchEvent(new Event('input',{bubbles:true}));})()`);
// horário do setor 1 igual ao do setor 3 (20:00) -> conflito
await ev(`(()=>{const i=document.querySelector('[data-horario="2.0"]'); i.value='20:10'; i.dispatchEvent(new Event('input',{bubbles:true}));})()`);
ok(await ev(`!document.querySelector('[data-conflito="0"]').classList.contains('oculto')`), "aviso de conflito entre setores no mesmo horário");
ok((await ev(`document.querySelector('[data-conflito="0"]').textContent`)).includes("Horta"), "o aviso cita o setor certo");
// duração acima do máximo bloqueia salvar
await ev(`(()=>{const i=document.querySelector('[data-cfg="setores.0.duracaoMin"]'); i.value='100'; i.dispatchEvent(new Event('input',{bubbles:true}));})()`);
await clicar('#btnSalvar'); await espera(200);
ok((await ev(`document.querySelector('#toast').textContent`)).includes("passa do tempo máximo"), "não salva duração acima do máximo");
await ev(`(()=>{const i=document.querySelector('[data-cfg="setores.0.duracaoMin"]'); i.value='30'; i.dispatchEvent(new Event('input',{bubbles:true}));})()`);
await clicar('[data-add-horario="3"]'); await espera(100);
ok((await ev(`estadoApp.rascunho.setores[3].horarios.length`)) === 2, "+ Horário adiciona");
await clicar('#btnSalvar'); await espera(300);
ok((await ev(`document.querySelector('#toast').textContent`)) === "Ajustes salvos", "salvou");
ok((await ev(`demo.config.setores[2].nome`)) === "Horta", "nome novo chegou na config");
await ev(`location.hash='painel'`); await espera(1200);
ok(await ev(`document.querySelector('#listaSetores').textContent.includes('Horta')`), "painel usa o nome novo");

// sair: campos travam
await clicar('#chipSessao'); await espera(400);
await ev(`location.hash='ajustes'`); await espera(300);
ok(await ev(`document.querySelector('[data-cfg="chuvaAdiarMm"]').disabled`), "sem login, ajustes ficam só leitura");

// dados velhos
await ev(`api.estado = async () => { throw new Error('offline') }; estadoApp.ultimaAtualizacao = Date.now() - 40000; desenharTopo();`);
ok((await ev(`document.querySelector('#textoStatus').textContent`)) === "Sem resposta", "sem resposta vira alerta");
ok(await ev(`document.querySelector('#app').classList.contains('desatualizado')`), "página acinzenta quando os dados ficam velhos");

console.log(erros.length ? "ERROS JS:\n" + erros.join("\n") : "sem erros de JavaScript");
if (erros.length || falhas) process.exitCode = 1;
ws.close(); chrome.kill();
