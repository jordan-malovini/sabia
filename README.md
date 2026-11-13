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
2. Um setor regando por vez, com a bomba ligada do começo ao fim da fila.
3. Na dúvida, desligado: as saídas ficam desligadas desde o boot, depois de uma queda de energia a rega só é retomada após 10 min de espera, e o watchdog reinicia o sistema se o firmware travar.

## Estado atual

Projeto e esquema fechados, com os esboços da interface web prontos. Aguardando a chegada das peças. As pendências estão em [docs/requisitos.md](docs/requisitos.md#pendências-antes-de-montar).

## Documentação

| Documento | Conteúdo |
| --- | --- |
| [docs/requisitos.md](docs/requisitos.md) | Objetivos, requisitos, escopo, limitações e roteiro |
| [docs/arquitetura.md](docs/arquitetura.md) | Software, lógica de irrigação, interface e dados |
| [docs/hardware.md](docs/hardware.md) | Componentes, alimentação, ligações, caixa e proteções |
| [docs/caixa.md](docs/caixa.md) | Caixa impressa em 3D: leiaute, medidas, impressão e montagem |
| [docs/mapa-de-pinos.md](docs/mapa-de-pinos.md) | GPIOs do ESP32-S3 e endereços I2C |
| [docs/api.md](docs/api.md) | Comunicação entre a página web e o firmware |
| [docs/decisoes/](docs/decisoes/) | Por que cada escolha foi feita (agenda por programas, por exemplo) |
| [CHANGELOG.md](CHANGELOG.md) | Histórico de versões |

## Como compilar e gravar

O firmware usa o **PlatformIO** (extensão do VS Code). Abra a pasta `firmware/` no VS Code.

| Tarefa | Comando |
| --- | --- |
| Compilar para o simulador | `pio run -e esp32s3` |
| Gravar na placa real (N16R8) | `pio run -e esp32s3-n16r8 -t upload` |
| Rodar os testes no computador | `pio test -e native` |
| Abrir o monitor serial | `pio device monitor` |

**Simulação no Wokwi:** instale a extensão Wokwi no VS Code, compile com `pio run -e esp32s3` e rode **Wokwi: Start Simulator** (`Ctrl+Shift+P`). O circuito tem:
- 9 LEDs no lugar dos relés (setores em verde, bomba em vermelho), que acendem com o pino em LOW;
- a tela, que no simulador é um OLED SSD1306, porque o Wokwi não tem o ST7920. O firmware troca o driver sozinho;
- o encoder: clique nele e use as setas para girar e a barra de espaço para clicar;
- o relógio (DS1307 no lugar do DS3231) e um botão azul que faz o papel do pluviômetro: cada clique vale 0,2 mm de chuva.

O BME280 e o VEML7700 não existem no Wokwi e ficam para a bancada.

**Na tela:** clique abre o menu. Para ligar um setor: gire até o setor, clique, gire para escolher o tempo e clique. O menu também tem "Programa A/B/C agora", **"Programar agenda"** (ligar o programa, horários de início, dias, tempo de cada setor e ajuste) e "Pausa por chuva". O clique longo volta um nível, e na tela inicial ele para tudo.

**No monitor serial:** `l` pergunta o setor e depois o tempo; `l N S` liga o setor N por S segundos de uma vez; `c S` faz o ciclo; `p` pula; `x N` para o setor N; `s` para tudo; `?` mostra o estado.

**Agenda (programas A, B e C):** `a` mostra a agenda. Exemplos: `g A on`, `g A h 1 06:00` (horário 1 de 4), `g A d 0101010` (dias, de domingo a sábado; ou `impar`, `par`, `int 3`), `g A t 3 25` (setor 3 por 25 min; `*` vale para todos), `g A % 80` (ajuste), `g A run` (inicia agora), `chuva 3` (não rega por 3 dias) e `hora 2026-11-12 19:55` (acerta o relógio). O programa A já vem preenchido (06:00, todos os dias, 25 min por setor), mas desligado.

**Queda de energia:** se a energia cair no meio da rega, ao voltar o SABIÁ espera 10 min (30 s no simulador) e retoma de onde parou. Na tela, clique retoma na hora e clique longo cancela; no serial, `retomar` e `cancelar`. Para testar, ligue um setor, espere um pouco e digite `reiniciar`.

**Página web:** a página vai embutida no firmware (uma gravação só) e comanda o controlador pela rede.
- **Primeira vez, na placa:** sem Wi-Fi configurado, o SABIÁ liga a rede própria **SABIA** (senha `sabia1234`). Conecte o celular nela e abra `http://192.168.4.1`. Em Ajustes → Rede, informe o Wi-Fi da casa. Outra forma é digitar `wifi NOME SENHA` no monitor serial.
- **No dia a dia:** com o Wi-Fi da casa, abra `http://sabia.local` ou o endereço que o comando `wifi` mostra no serial. Se o Wi-Fi da casa cair, a rede própria volta sozinha depois de 20 s.
- **Senha da página:** de fábrica é `sabia1234`. Ver o painel é livre; ligar, parar e mudar ajustes pede a senha. Troque em Ajustes → Segurança (a página avisa enquanto não trocar). Esqueceu? `senha padrao` no serial volta para a de fábrica.
- **No Wokwi:** com a simulação rodando, abra `http://localhost:8180`.
- **Sem placa:** abra `firmware/data/index.html` direto no navegador para ver em modo demonstração (dados simulados, qualquer senha).
- Temperatura, umidade e luz aparecem quando os sensores estiverem ligados. A água no solo aparece depois de um dia completo com temperatura e com a latitude informada em Ajustes → Estação.

**Registro e histórico:** chuva, clima, regas e eventos são gravados num arquivo por dia, no cartão microSD ou, sem cartão, na memória do ESP. A página mostra hoje, 7 e 30 dias e baixa em CSV. Para mandar ao Google Drive, veja [docs/drive.md](docs/drive.md).

**Balanço hídrico:** nas regas automáticas, o SABIÁ encurta ou pula a rega de um setor que ainda tem água, e nunca rega mais que o tempo do programa. Antes de confiar nele, informe a latitude e meça a taxa de aplicação de cada setor. Detalhes em [docs/decisoes/2026-11-13-balanco-hidrico-so-economiza.md](docs/decisoes/2026-11-13-balanco-hidrico-so-economiza.md).

Testes da página (Node 22 e Google Chrome): `node tests/pagina/teste-pagina.mjs` (modo demonstração) e `node tests/pagina/teste-api.mjs` (contra um controlador simulado).

**Credenciais:** opcional. Para já gravar a placa com o seu Wi-Fi e as suas senhas, copie `firmware/include/secrets.example.h` para `secrets.h` e preencha. O `secrets.h` não vai para o repositório.

Bibliotecas em uso: U8g2 (display), RTClib (relógio), ArduinoJson (API da página) e, do próprio ESP32, WiFi, WebServer, ESPmDNS e Preferences. Previstas: drivers do BME280 e do VEML7700 e SD.

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
