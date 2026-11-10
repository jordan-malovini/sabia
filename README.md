<p align="center"><img src="assets/logo/sabia-galho.png" alt="SABIÁ" width="460"></p>

# SABIÁ

**Sistema Automático de Balanço e Irrigação Agrícola**

Controlador de irrigação com ESP32-S3 que decide quanto regar a partir do clima medido no próprio local, e não por horário fixo.

Autor: Jordan · Versão 0.1 (em desenvolvimento) · Novembro de 2026

---

## O que é

O SABIÁ controla 8 setores de irrigação (aspersão e gotejo) e uma bomba. Ele mede chuva e temperatura no local, mantém um balanço hídrico por setor e converte o déficit de água em tempo de rega. Depois de uma chuva, a rega é adiada sozinha.

Tudo roda no próprio ESP32-S3, sem servidor. O acompanhamento e o comando são feitos por uma página web local, no celular ou no computador, e também pelo display com encoder na tampa da caixa.

O projeto substitui um controlador antigo que só trabalhava em ciclo e exigia ir até o equipamento para qualquer ajuste. A primeira aplicação é uma plantação de cacau, mas os parâmetros por setor servem para qualquer cultura.

## Principais funções

- 8 setores com agenda própria, ou em ciclo, sempre um setor por vez, com fila.
- Comando da bomba sincronizado com as válvulas.
- Estação meteorológica: pluviômetro basculante impresso em 3D, temperatura, umidade, pressão e luz.
- Balanço hídrico por setor (ET0 por Hargreaves).
- Página web local com painel, histórico em gráficos e download em CSV.
- Registro em flash e cartão SD, com envio opcional ao Google Drive.
- Funciona sem rede: a agenda continua, e o ESP cria o próprio Wi-Fi como acesso alternativo.

## Princípios de projeto

1. O controle nunca depende da rede.
2. Só um setor ligado por vez.
3. Na dúvida, desligado: as saídas ficam desligadas desde o boot, nenhuma rega é retomada após queda de energia, e o watchdog reinicia o sistema se o firmware travar.

## Estado atual

Projeto e esquema fechados, com os esboços da interface web prontos. Aguardando a chegada das peças. As pendências estão em [docs/requisitos.md](docs/requisitos.md#pendências-antes-de-montar).

## Documentação

| Documento | Conteúdo |
| --- | --- |
| [docs/requisitos.md](docs/requisitos.md) | Objetivos, requisitos, escopo, limitações e roteiro |
| [docs/arquitetura.md](docs/arquitetura.md) | Software, lógica de irrigação, interface e dados |
| [docs/hardware.md](docs/hardware.md) | Componentes, alimentação, ligações, caixa e proteções |
| [docs/mapa-de-pinos.md](docs/mapa-de-pinos.md) | GPIOs do ESP32-S3 e endereços I2C |
| [docs/api.md](docs/api.md) | Comunicação entre a página web e o firmware |
| [CHANGELOG.md](CHANGELOG.md) | Histórico de versões |

## Como compilar e gravar

O firmware usa o **PlatformIO** (extensão do VS Code). Abra a pasta `firmware/` no VS Code.

| Tarefa | Comando |
| --- | --- |
| Compilar para o simulador | `pio run -e esp32s3` |
| Gravar na placa real (N16R8) | `pio run -e esp32s3-n16r8 -t upload` |
| Rodar os testes no computador | `pio test -e native` |
| Abrir o monitor serial | `pio device monitor` |

**Simulação no Wokwi:** instale a extensão Wokwi no VS Code, compile com `pio run -e esp32s3` e rode **Wokwi: Start Simulator**. Os LEDs fazem o papel dos relés e acendem quando o pino vai a LOW, como a placa ativa em nível baixo. Os setores ficam em verde e a bomba em vermelho.

**Comandos no monitor serial (v0.1):** `l N S` liga o setor N por S segundos, `c S` faz o ciclo com S segundos por setor, `p` pula, `x N` para o setor N, `s` para tudo e `?` mostra o estado.

**Página web:** abra `firmware/data/index.html` no navegador para ver em modo demonstração (dados simulados, qualquer senha). Testes: `node tests/pagina/teste-pagina.mjs` (precisa do Node 22 e do Google Chrome).

**Credenciais:** copie `firmware/include/secrets.example.h` para `secrets.h` e preencha. O `secrets.h` não vai para o repositório.

Bibliotecas previstas: U8g2 (display), ESP32Encoder (encoder), RTClib (DS3231), drivers do BME280 e do VEML7700, LittleFS, SD e Preferences.

## Estrutura do repositório

```
sabia/
├── README.md
├── CHANGELOG.md
├── docs/          requisitos, arquitetura, hardware, pinos, diário e decisões
├── firmware/      código (src) e página web (data)
├── hardware/      esquemáticos, PCB, caixa 3D e lista de materiais
├── tests/         roteiros de bancada e simulação no Wokwi
├── tools/         Apps Script e análise em Python
└── assets/        logo, fotos e esboços
```

## Versionamento

- **Firmware:** X.Y. X muda numa fase nova do projeto e Y a cada atualização. A série 0.x é a fase de desenvolvimento, e a **1.0** sai quando o SABIÁ estiver regando de verdade no campo.
- **Hardware:** por revisão (Rev A, Rev B).
- **Branches:** `main` só com código testado, com uma tag por versão, e `feature/nome` para cada tarefa.
- **Commits:** `feat:`, `fix:`, `docs:`, seguido da descrição.

## Licença

- **Firmware, ferramentas e documentação:** [MIT](LICENSE)
- **Hardware** (pasta `hardware/`): [CC BY-SA 4.0](hardware/LICENSE.md)
