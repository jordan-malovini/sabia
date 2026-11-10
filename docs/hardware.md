# Hardware

Revisão atual: **Rev A** (protoboard)

## Componentes

| Item | Especificação |
| --- | --- |
| Microcontrolador | ESP32-S3 com PSRAM octal (série R8), Wi-Fi e Bluetooth integrados |
| Saídas | Placa de 16 relés, 5 V, com conversor interno para 12 V nas bobinas; 9 usados (8 setores e a bomba) |
| Alimentação | Fonte de 5 V e 10 A; ESP e placa de relés em 5 V; sensores em 3,3 V |
| Tempo | RTC DS3231 (I2C), com pilha |
| Display | LCD gráfico 128x64 (12864B-V2.3, ST7920) em modo serial, com backlight |
| Entrada local | Encoder rotativo com botão |
| Clima | BME280 (temperatura, umidade e pressão) e VEML7700 (luz), ambos em I2C |
| Chuva | Pluviômetro basculante impresso em 3D, com reed switch; cerca de 0,2 mm por basculada, a calibrar |
| Armazenamento | LittleFS (flash do ESP) e cartão microSD (SPI) |
| Nuvem (opcional) | Planilhas no Google Drive, via Google Apps Script |
| Caixa | Impressa em 3D (PETG ou ASA), com seções internas e tampa com dobradiça |

## Topologia de alimentação

```
Rede --> Fonte 5 V 10 A --> Fusível (3 A) --> TVS --> Distribuição de 5 V e GND
                                                      |--> ESP32-S3
                                                      |--> Placa de relés
                                                      |--> Display (VCC e backlight)
                                                      |--> Módulo SD
ESP32-S3 3V3 --> RTC, BME280, VEML7700, encoder e pull-up do reed
GND em comum para todos os módulos
```

O mapa de pinos está em [mapa-de-pinos.md](mapa-de-pinos.md).

## Detalhes de ligação

- **Reed do pluviômetro:** pull-up de 10 kΩ ao 3V3, capacitor de 100 nF ao GND e resistor de 1 kΩ em série, perto do ESP. Cabo trançado ou blindado, com até 3 m.
- **Display:** alimentado em 5 V. O sinal de 3,3 V do ESP é testado direto, e um conversor de nível de 4 canais só entra se for necessário.
- **Relés:** GND em comum com o ESP. Se algum relé ficar parcialmente ligado, as saídas passam a open-drain. Se a placa acionar algum relé durante o reset do ESP, entram pull-ups externos de 10 kΩ nas entradas dela, para que os pinos não fiquem flutuando.
- **Encoder:** capacitor de 100 nF do pino ao GND em A, B e no botão.
- **Alimentação:** capacitor de 470 µF perto do ESP para absorver o ruído do conversor da placa de relés.
- **Distribuição:** placa de expansão com bornes para 5 V, 3V3 e GND, sem emendas.

## Estrutura física

- **Caixa do controlador**, impressa em 3D: seção da placa de relés na parte de baixo, com as saídas dos cabos para baixo; seção da fonte, com a rede separada do resto; seção do ESP e do RTC. A tampa tem dobradiça, display e encoder.
- **Pluviômetro**, impresso em PETG ou ASA, com tela no funil, ímã de 5x2 mm e parafusos de calibração.
- **Abrigo anti-radiação do BME280**, à sombra, a 1,5 a 2 m do chão.
- **Caixa do sensor de luz**, com janela de acrílico leitoso, vedada.

## Proteções e segurança elétrica

- Fusível de 3 A na saída de 5 V para proteger a fiação, já que a fonte entrega até 10 A, e diodo TVS contra picos.
- Fonte com aterramento, bornes da rede cobertos e cabo com alívio de tração.
- Conferir a corrente de partida da bomba e usar uma contatora se o relé não aguentar.
- Cabos externos resistentes a UV, emendas seladas e curva de pingar antes de cada entrada.

As proteções de software (saídas desligadas no boot, watchdog, tempo máximo e janela de horário) estão em [arquitetura.md](arquitetura.md#princípios).
