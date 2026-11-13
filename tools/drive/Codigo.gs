/**
 * SABIÁ - recebe o registro do controlador e grava no Google Drive.
 *
 * O controlador manda, uma vez por dia encerrado, o arquivo CSV daquele dia
 * (POST com o texto no corpo e ?dia=AAAA-MM-DD no endereço). Este script
 * grava as linhas numa planilha por semana, dentro da pasta "SABIA" do Drive
 * de quem publicou o script, e responde {"ok":true}. O controlador só apaga
 * o arquivo do cartão depois dessa resposta.
 *
 * Reenviar o mesmo dia não duplica: as linhas antigas daquele dia são
 * trocadas pelas novas.
 *
 * Como instalar: docs/drive.md.
 */

const PASTA = "SABIA";
const CABECALHO = ["dia", "tipo", "hora", "valor1", "valor2", "valor3", "valor4", "valor5"];

function doPost(e) {
  const trava = LockService.getScriptLock();
  try {
    trava.waitLock(20000);
    const dia = String((e.parameter && e.parameter.dia) || "");
    if (!/^\d{4}-\d{2}-\d{2}$/.test(dia)) return responder({ ok: false, erro: "dia invalido" });
    const texto = (e.postData && e.postData.contents) || "";
    const linhas = texto.split(/\r?\n/).filter(function (l) { return l.length > 0; }).map(function (l) { return linhaParaCelulas(dia, l); });

    const aba = abaDaSemana(dia);
    apagarDia(aba, dia);
    if (linhas.length > 0) aba.getRange(aba.getLastRow() + 1, 1, linhas.length, CABECALHO.length).setValues(linhas);
    return responder({ ok: true, linhas: linhas.length });
  } catch (erro) {
    return responder({ ok: false, erro: String(erro) });
  } finally {
    trava.releaseLock();
  }
}

// Abrir o endereço no navegador mostra se o script está no ar.
function doGet() {
  return responder({ ok: true, servico: "SABIA" });
}

function responder(objeto) {
  return ContentService.createTextOutput(JSON.stringify(objeto)).setMimeType(ContentService.MimeType.JSON);
}

// "C;13:10;0.4" -> [dia, "chuva", "13:10", 0.4, "", "", "", ""]
function linhaParaCelulas(dia, linha) {
  const TIPOS = { C: "chuva", T: "clima", R: "rega", E: "evento" };
  const campos = linha.split(";");
  const tipo = campos[0];
  let valores = campos.slice(2);
  if (tipo === "E") valores = [valores.join(";")];  // o texto do evento pode ter ponto e vírgula
  const celulas = [dia, TIPOS[tipo] || tipo, campos[1] || ""];
  for (let i = 0; i < CABECALHO.length - 3; i++) {
    const v = valores[i];
    if (v === undefined || v === "") celulas.push("");
    else if (tipo !== "E" && !isNaN(Number(v))) celulas.push(Number(v));
    else celulas.push("'" + v);  // texto puro: não deixa a planilha ler como fórmula
  }
  return celulas;
}

function abaDaSemana(dia) {
  const partes = dia.split("-").map(Number);
  const data = new Date(partes[0], partes[1] - 1, partes[2]);
  const nome = "SABIA " + anoSemana(data);
  const pastas = DriveApp.getFoldersByName(PASTA);
  const pasta = pastas.hasNext() ? pastas.next() : DriveApp.createFolder(PASTA);
  const arquivos = pasta.getFilesByName(nome);
  let planilha;
  if (arquivos.hasNext()) {
    planilha = SpreadsheetApp.open(arquivos.next());
  } else {
    planilha = SpreadsheetApp.create(nome);
    DriveApp.getFileById(planilha.getId()).moveTo(pasta);
  }
  const aba = planilha.getSheets()[0];
  if (aba.getLastRow() === 0) {
    aba.appendRow(CABECALHO);
    aba.setFrozenRows(1);
    aba.getRange("A:A").setNumberFormat("@");  // a data fica como texto, para comparar ao reenviar
    aba.getRange("C:C").setNumberFormat("@");
  }
  return aba;
}

// Semana ISO: "2026-S46".
function anoSemana(data) {
  const d = new Date(Date.UTC(data.getFullYear(), data.getMonth(), data.getDate()));
  const diaDaSemana = d.getUTCDay() || 7;
  d.setUTCDate(d.getUTCDate() + 4 - diaDaSemana);
  const inicioDoAno = new Date(Date.UTC(d.getUTCFullYear(), 0, 1));
  const semana = Math.ceil(((d - inicioDoAno) / 86400000 + 1) / 7);
  return d.getUTCFullYear() + "-S" + (semana < 10 ? "0" : "") + semana;
}

function apagarDia(aba, dia) {
  const ultima = aba.getLastRow();
  if (ultima < 2) return;
  const dias = aba.getRange(2, 1, ultima - 1, 1).getDisplayValues();
  // De baixo para cima, em blocos, para os números das linhas não mudarem no caminho.
  let fim = -1;
  for (let i = dias.length - 1; i >= -1; i--) {
    const igual = i >= 0 && dias[i][0] === dia;
    if (igual && fim < 0) fim = i;
    if (!igual && fim >= 0) {
      aba.deleteRows(i + 3, fim - i);
      fim = -1;
    }
  }
}
