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
ok(await ev(`document.querySelector('.topo').getBoundingClientRect().top >= 0 && scrollY === 0`), "abre no topo, com o cabeçalho visível");
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

// iniciar um programa da agenda pelo painel
await clicar('#btnPrograma'); await espera(150);
ok(await ev(`document.querySelector('#dlgPrograma').open`), "Iniciar programa abre a lista");
ok((await ev(`document.querySelectorAll('#listaProgramas [data-iniciar-prog]').length`)) === 3, "lista os programas A, B e C");
ok(await ev(`document.querySelector('[data-iniciar-prog="2"]').disabled`), "programa sem setores fica desabilitado");
ok((await ev(`document.querySelector('[data-iniciar-prog="0"]').textContent`)).includes("2h40"), "mostra o tempo total do programa A");
await clicar('[data-iniciar-prog="1"]'); await espera(600);
ok((await ev(`demo.fila.map(f=>f.setor).join(",")`)) === "1,3,5,7", "programa B põe os setores de gotejo na fila");
ok((await ev(`demo.fila[0].duracaoS`)) === 35 * 60, "com o tempo do programa");

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
const setar = (sel, val) => ev(`(()=>{const i=document.querySelector(${JSON.stringify(sel)}); i.value=${JSON.stringify(String(val))}; i.dispatchEvent(new Event('input',{bubbles:true}));})()`);
const texto = sel => ev(`document.querySelector(${JSON.stringify(sel)}).textContent`);
const visivel = sel => ev(`getComputedStyle(document.querySelector(${JSON.stringify(sel)})).display !== 'none'`);

// agenda por programas
ok((await texto('[data-resumo-prog="0"]')) === "05:00 · todo dia · 2h40", "resumo do programa A: horário, dias e tempo total");
ok((await texto('[data-resumo-prog="2"]')) === "desligado", "programa C desligado");
ok((await visivel('[data-bloco-semana="0"]')) && !(await visivel('[data-bloco-intervalo="0"]')), "modo semana mostra os dias");
await clicar('[data-seg="programas.0.modo"] [data-valor="intervalo"]'); await espera(100);
ok(!(await visivel('[data-bloco-semana="0"]')) && (await visivel('[data-bloco-intervalo="0"]')), "modo intervalo mostra o campo de dias");
ok((await texto('[data-resumo-prog="0"]')).includes("a cada 2 dias"), "resumo mostra o intervalo");
ok(await ev(`!document.querySelector('#barraSalvar').classList.contains('oculto')`), "barra de salvar aparece");
await clicar('[data-seg="programas.0.modo"] [data-valor="semana"]'); await espera(100);
await clicar('[data-dias="programas.0.dias"] [data-i="0"]'); await espera(100);
ok((await texto('[data-resumo-prog="0"]')).includes("seg, ter, qua, qui, sex, sáb"), "desmarcar o domingo muda o resumo");
// segundo início antes de o primeiro terminar
await setar('[data-cfg="programas.0.inicios.1"]', "06:00");
ok((await texto('[data-aviso-prog="0"]')).includes("espera na fila"), "avisa quando um início chega antes de o anterior terminar");
await setar('[data-cfg="programas.0.inicios.1"]', "17:00");
ok(await ev(`document.querySelector('[data-aviso-prog="0"]').classList.contains('oculto')`), "sem aviso quando os inícios não se cruzam");
// ajuste percentual
await setar('[data-cfg="programas.1.ajustePct"]', 50);
ok((await texto('[data-resumo-prog="1"]')).includes("1h10"), "ajuste de 50% corta o tempo total pela metade");
// ligar o programa C
await ev(`(()=>{const c=document.querySelector('[data-cfg="programas.2.ativo"]'); c.checked=true; c.dispatchEvent(new Event('input',{bubbles:true})); c.dispatchEvent(new Event('change',{bubbles:true}));})()`);
ok((await texto('[data-aviso-prog="2"]')).includes("não tem horário"), "programa ligado sem horário gera aviso");
await setar('[data-cfg="programas.2.inicios.0"]', "12:00");
ok((await texto('[data-aviso-prog="2"]')).includes("nenhum setor"), "programa ligado sem setores gera aviso");
await setar('[data-cfg="programas.2.minutos.0"]', 10);
ok((await texto('[data-resumo-prog="2"]')) === "12:00 · todo dia · 10 min", "programa C configurado");
// renomear o setor 3
await setar('[data-cfg="setores.2.nome"]', "Horta");
ok((await texto('[data-nome-setor-prog="2"]')) === "Horta", "o nome novo aparece nos tempos dos programas");
// tempo do programa acima do máximo do setor bloqueia salvar
await setar('[data-cfg="programas.2.minutos.0"]', 100);
await clicar('#btnSalvar'); await espera(200);
ok((await texto('#toast')).includes("passa do tempo máximo do setor"), "não salva tempo de programa acima do máximo do setor");
await setar('[data-cfg="programas.2.minutos.0"]', 10);
// duração padrão acima do máximo também bloqueia
await setar('[data-cfg="setores.0.duracaoMin"]', 100);
await clicar('#btnSalvar'); await espera(200);
ok((await texto('#toast')).includes("passa do tempo máximo"), "não salva duração acima do máximo");
await setar('[data-cfg="setores.0.duracaoMin"]', 30);
await clicar('#btnSalvar'); await espera(300);
ok((await ev(`document.querySelector('#toast').textContent`)) === "Ajustes salvos", "salvou");
ok((await ev(`demo.config.setores[2].nome`)) === "Horta", "nome novo chegou na config");
ok(await ev(`demo.config.programas[2].ativo === true && demo.config.programas[2].inicios[0] === "12:00" && demo.config.programas[2].minutos[0] === 10`), "programa C chegou na config");
ok((await ev(`demo.config.programas[0].inicios[1]`)) === "17:00", "segundo início do programa A salvo");
// pausa por chuva
await setar('#diasPausa', 2);
await clicar('#btnPausar'); await espera(1300);
ok(await ev(`demo.pausaAte !== null`), "pausa por chuva enviada");
ok((await texto('#textoPausa')).includes("em pausa por 2 dias"), "mostra os dias de pausa");
await setar('#diasPausa', 0);
await clicar('#btnPausar'); await espera(1300);
ok(await ev(`demo.pausaAte === null`), "0 cancela a pausa");
await ev(`location.hash='painel'`); await espera(1200);
ok(await ev(`document.querySelector('#listaSetores').textContent.includes('Horta')`), "painel usa o nome novo");
ok(await ev(`document.querySelector('#listaSetores').textContent.includes('prog.')`), "setores mostram a próxima rega pelo programa");

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
