// Modelo de credenciais do SABIÁ.
// Copie este arquivo para "secrets.h" na mesma pasta e preencha os valores.
// O secrets.h está no .gitignore e nunca deve ir para o repositório.

#pragma once

// Wi-Fi da casa
#define WIFI_SSID        "nome-da-rede"
#define WIFI_PASSWORD    "senha-da-rede"

// Wi-Fi próprio do SABIÁ (modo AP), usado quando a rede da casa falha
#define AP_SSID          "SABIA"
#define AP_PASSWORD      "troque-esta-senha"   // mínimo de 8 caracteres

// Senha da página web para ligar, parar e alterar ajustes
#define WEB_USER         "admin"
#define WEB_PASSWORD     "troque-esta-senha"

// Envio ao Google Drive (URL de implantação do Google Apps Script)
#define DRIVE_SCRIPT_URL "https://script.google.com/macros/s/SEU_ID/exec"

// Localização (usada no cálculo da radiação Ra do Hargreaves)
#define LATITUDE_GRAUS   0.0      // preencha com a latitude do local (sul é negativo)
