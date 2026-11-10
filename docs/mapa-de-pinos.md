# Mapa de pinos (ESP32-S3)

Válido para módulos com PSRAM octal (série R8, como o N16R8).

| Função | GPIO | Observação |
| --- | --- | --- |
| Relés dos setores 1 a 8 | 4, 5, 6, 7, 15, 16, 17, 18 | A placa costuma ser ativa em nível baixo: escrever HIGH antes do `pinMode(OUTPUT)` |
| Relé da bomba | 47 | Relé 9 da placa |
| Display ST7920 (CS, MOSI, SCK) | 10, 11, 12 | Modo serial, com o pino PSB ao GND |
| I2C (SDA, SCL) | 8, 9 | RTC, BME280 e VEML7700 |
| Pluviômetro (reed) | 14 | Interrupção, com pull-up e filtro |
| Encoder (A, B, botão) | 1, 2, 21 | Capacitor de 100 nF em cada pino |
| Cartão SD (CS, SCK, MISO, MOSI) | 38, 39, 40, 41 | Barramento SPI separado do display |
| Livres | 13, 42 | Reservados para anemômetro e sensor de fluxo |

## Pinos evitados

| GPIO | Motivo |
| --- | --- |
| 0, 3, 45, 46 | Pinos de boot (strapping) |
| 19, 20 | USB nativo |
| 26 a 32 | Flash |
| 33 a 37 | PSRAM octal |
| 43, 44 | Serial (UART0) |

## Endereços no barramento I2C

| Módulo | Endereço |
| --- | --- |
| RTC DS3231 | 0x68 |
| BME280 | 0x76 |
| VEML7700 | 0x10 |
