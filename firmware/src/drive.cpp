// SABIÁ - envio do registro ao Google Drive (RF11).
//
// Manda o arquivo de cada dia já encerrado para um Google Apps Script
// (tools/drive/Codigo.gs), que grava numa planilha por semana. O dia só é
// marcado como enviado depois de o script confirmar; enquanto não confirma,
// o arquivo não é apagado do cartão e o envio é tentado de novo.
//
// Roda na tarefa do servidor web: a conversa com a internet leva segundos e
// não pode atrasar as válvulas. A trava só fecha para ler o arquivo e para
// marcar o envio.

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "sistema.h"

namespace sabia {

constexpr uint32_t INTERVALO_VERIFICACAO_MS = 60000;
constexpr uint32_t DIAS_PARA_TRAS = 60;
constexpr size_t TAMANHO_MAX_ARQUIVO = 60000;

static uint32_t ultimaVerificacaoMs = 0, ultimaRodadaMs = 0;
static bool houveRodada = false, rodadaEmCurso = false;
static char ultimoErro[64] = "";

uint32_t driveUltimoDiaEnviado() { return memoria.getULong("driveDia", 0); }
const char* driveUltimoErro() { return ultimoErro; }

// Acha o próximo dia a enviar e lê o arquivo dele. Com a trava fechada.
static bool proximoArquivo(uint32_t& dia, String& conteudo, String& url) {
  Trava t;
  if (!ajustes.drive) return false;
  uint32_t hoje = registroHoje().dia();
  if (hoje == 0) return false;  // sem relógio
  url = ajustes.driveUrl;
  uint32_t enviado = driveUltimoDiaEnviado();
  uint32_t primeiro = hoje > DIAS_PARA_TRAS ? hoje - DIAS_PARA_TRAS : 1;
  if (enviado + 1 > primeiro) primeiro = enviado + 1;
  for (dia = primeiro; dia < hoje; dia++) {
    if (registroLerDia(dia, conteudo, TAMANHO_MAX_ARQUIVO)) return true;
  }
  // Nada para trás: anota que está em dia, para a limpeza do cartão seguir.
  if (hoje > 0 && enviado + 1 < hoje) memoria.putULong("driveDia", hoje - 1);
  return false;
}

static bool enviar(uint32_t dia, const String& conteudo, const String& url) {
  char data[11];
  dataDeDias(dia, data);
  WiFiClientSecure cliente;
  cliente.setInsecure();  // sem conferir o certificado: o ESP não guarda as autoridades do Google
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);  // o Apps Script responde com um redirecionamento
  http.setTimeout(15000);
  String destino = url + (url.indexOf('?') < 0 ? "?dia=" : "&dia=") + data;
  if (!http.begin(cliente, destino)) {
    snprintf(ultimoErro, sizeof(ultimoErro), "endereco invalido");
    return false;
  }
  http.addHeader("Content-Type", "text/plain; charset=utf-8");
  int codigo = http.POST(conteudo);
  String resposta = codigo > 0 ? http.getString() : String();
  http.end();
  if (codigo != 200) {
    snprintf(ultimoErro, sizeof(ultimoErro), "sem resposta do Drive (codigo %d)", codigo);
    return false;
  }
  if (resposta.indexOf("\"ok\":true") < 0) {
    snprintf(ultimoErro, sizeof(ultimoErro), "o script nao confirmou o envio");
    return false;
  }
  ultimoErro[0] = 0;
  return true;
}

// Chamar no laço da tarefa web. Envia no máximo um arquivo por minuto, para
// a página não ficar parada esperando vários envios seguidos.
void driveTick() {
  if (millis() - ultimaVerificacaoMs < INTERVALO_VERIFICACAO_MS) return;
  ultimaVerificacaoMs = millis();
  if (WiFi.status() != WL_CONNECTED) return;

  uint32_t intervaloMs;
  {
    Trava t;
    if (!ajustes.drive) return;
    intervaloMs = static_cast<uint32_t>(ajustes.envioHoras) * 3600000UL;
  }
  // Uma rodada começa a cada `envioHoras` e segue, um arquivo por minuto, até acabar a fila.
  if (!rodadaEmCurso && houveRodada && millis() - ultimaRodadaMs < intervaloMs) return;
  if (!rodadaEmCurso) {
    rodadaEmCurso = true;
    houveRodada = true;
    ultimaRodadaMs = millis();
  }

  uint32_t dia = 0;
  String conteudo, url;
  if (!proximoArquivo(dia, conteudo, url)) {
    rodadaEmCurso = false;
    return;
  }
  if (enviar(dia, conteudo, url)) {
    Trava t;
    memoria.putULong("driveDia", dia);
    char texto[TAMANHO_EVENTO], data[11];
    dataDeDias(dia, data);
    snprintf(texto, sizeof(texto), "Registro de %s enviado ao Drive", data);
    registrarEvento(texto);
  } else {
    rodadaEmCurso = false;  // falhou: tenta de novo na próxima rodada
  }
}

}  // namespace sabia
