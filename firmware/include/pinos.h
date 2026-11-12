// Mapa de pinos do SABIÁ (ESP32-S3). Ver docs/mapa-de-pinos.md.
#pragma once

#include <stdint.h>

// Relés: a placa costuma ser ativa em nível baixo (LOW liga).
// Se a sua for ativa em nível alto, troque os dois valores.
constexpr uint8_t RELE_LIGADO = 0;     // LOW
constexpr uint8_t RELE_DESLIGADO = 1;  // HIGH

constexpr uint8_t PINOS_SETORES[8] = {4, 5, 6, 7, 15, 16, 17, 18};
constexpr uint8_t PINO_BOMBA = 47;

// Display ST7920 (modo serial)
constexpr uint8_t PINO_LCD_CS = 10;
constexpr uint8_t PINO_LCD_MOSI = 11;
constexpr uint8_t PINO_LCD_SCK = 12;

// I2C: RTC, BME280 e VEML7700
constexpr uint8_t PINO_SDA = 8;
constexpr uint8_t PINO_SCL = 9;

// Entradas
constexpr uint8_t PINO_PLUVIOMETRO = 14;
constexpr uint8_t PINO_ENCODER_A = 1;
constexpr uint8_t PINO_ENCODER_B = 2;
constexpr uint8_t PINO_ENCODER_BOTAO = 21;

// Cartão SD (SPI separado)
constexpr uint8_t PINO_SD_CS = 38;
constexpr uint8_t PINO_SD_SCK = 39;
constexpr uint8_t PINO_SD_MISO = 40;
constexpr uint8_t PINO_SD_MOSI = 41;

#define SABIA_VERSAO "0.2-dev"
