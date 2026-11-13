// SABIÁ - configuração gravada na memória do ESP (NVS).
//
// O documento JSON da página é guardado inteiro na chave "cfg". Ficam de fora:
//  - os programas A, B e C, que a Agenda grava do jeito dela (chave "prog");
//  - as senhas do Wi-Fi e da rede própria (chaves próprias) e a senha da
//    página (só o hash), que nunca voltam para a página.

#include <Arduino.h>

#include "sistema.h"

namespace sabia {

constexpr size_t TAMANHO_MAX_CONFIG = 6000;

void configCarregar(JsonDocument& doc) {
  configPadrao(doc);
  size_t tamanho = memoria.getBytesLength("cfg");
  if (tamanho == 0 || tamanho > TAMANHO_MAX_CONFIG) return;
  char* texto = static_cast<char*>(malloc(tamanho + 1));
  if (texto == nullptr) return;
  memoria.getBytes("cfg", texto, tamanho);
  texto[tamanho] = 0;
  JsonDocument gravado;
  if (deserializeJson(gravado, texto) == DeserializationError::Ok) {
    mesclar(doc.as<JsonVariant>(), gravado.as<JsonVariantConst>());
  }
  free(texto);
}

bool configGravar(JsonDocument& doc) {
  doc.remove("programas");
  doc["rede"]["wifiSenha"] = "";
  doc["rede"]["apSenha"] = "";
  doc["seguranca"].remove("novaSenha");
  String texto;
  serializeJson(doc, texto);
  if (texto.length() == 0 || texto.length() > TAMANHO_MAX_CONFIG) return false;
  return memoria.putBytes("cfg", texto.c_str(), texto.length()) == texto.length();
}

void configAplicar(JsonVariantConst cfg) {
  Ajustes novos;
  lerAjustes(cfg, novos);
#ifdef SABIA_SIMULADOR
  novos.retomada.esperaS = 30;  // no simulador, meio minuto basta para ver a contagem
#endif
  ajustes = novos;
  controlador.definirConfig(novos.controlador);
  retomada.definirConfig(novos.retomada);
}

}  // namespace sabia
