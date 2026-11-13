# Decisão: agenda por programas (A, B e C)

**Data:** 12 de novembro de 2026

## Contexto

O controlador antigo (Arduino, `irrigacao_v9.ino`) tinha até 4 "ciclos" por dia, cada um com horário, liga/desliga e dias da semana, e uma duração por setor igual para todos os ciclos. Para definir a agenda do SABIÁ, comparei esse modelo com o de controladores comerciais residenciais (Rain Bird ESP-TM2, Hunter Pro-C e X2).

## O que os comerciais fazem

| Função | Rain Bird ESP-TM2 | Hunter Pro-C / X2 |
| --- | --- | --- |
| Programas independentes | 3 (A, B, C) | 3 (A, B, C) |
| Horários de início por programa | 4 | 4 |
| Tempo por estação | 1 min a 6 h | 1 min a 6 h |
| Dias de rega | dias da semana, ímpares, pares, cíclico (1 a 30 dias) | dias da semana, ímpares, pares, intervalo |
| Ajuste sazonal | 5 a 200 %, global ou por programa | 5 a 300 % |
| Suspensão por chuva | até 14 dias | "rain off" programável |
| Inícios sobrepostos | empilhados em sequência | idem |
| Bomba | relé de partida de bomba / válvula mestra | idem, por estação |
| Entre estações | atraso configurável | atraso configurável (válvulas lentas, recarga da bomba) |
| Rega manual | estação, programa ou teste de todas | idem |
| Outros | sensor de chuva com desvio por estação, memória não volátil, detecção de curto | Cycle and Soak, Solar Sync, backup da programação |

Fontes: [especificação do ESP-TM2](https://rainbird.com/media/15798), [manual do ESP-TM2](https://rainbird.com/media/1865), [suporte do Hunter X2](https://hunterindustries.com/en-metric/support/controllers/x2), [manual do Pro-C](https://hunterirrigation.com/sites/default/files/om_pcc_en.pdf).

## Decisão

Adotar o modelo de **programas**:

- **3 programas (A, B, C)**, cada um com até **4 horários de início**, **dias de rega** (dias da semana, ímpares, pares ou a cada N dias), **tempo por setor** (0 a 180 min; 0 tira o setor do programa) e **ajuste de 5 a 200 %**.
- **Empilhamento:** se um programa começa com outro em andamento, ele espera a vez.
- **Pausa por chuva** de 1 a 14 dias, que termina sozinha.
- **Iniciar um programa na mão**, além de ligar um setor avulso e do ciclo de teste.
- **Gravação na memória do ESP (NVS)**, que não depende de pilha.

Diferenças em relação aos comerciais, mantidas de propósito:

- **Troca de setor com sobreposição**, com a bomba ligada o tempo todo, em vez de atraso entre estações. Vem do controlador antigo e evita ligar e desligar a bomba a cada setor.
- **Tempo por setor por programa.** No antigo, a duração era uma só para todos os ciclos. Com programas, dá para ter, por exemplo, o gotejo todo dia no A e a aspersão três vezes por semana no B.

Por que não continuar com os 4 ciclos do antigo: o modelo de programas cobre o mesmo uso (um programa com 4 horários equivale a 4 ciclos) e ainda permite tempos e dias diferentes por grupo de setores.

## Defeito do controlador antigo, evitado aqui

O antigo guardava o horário do último ciclo iniciado como `hora × 100 + minuto`, sem a data, e só iniciava outro ciclo se o horário fosse diferente. Com um ciclo só, ele regava num dia e se recusava a regar no mesmo horário do dia seguinte. A agenda do SABIÁ marca o último minuto verificado **com a data**, e há um teste que rega 9 dias seguidos no mesmo horário.

## Ainda não feito

- A página web (`firmware/data/index.html`) e o `docs/api.md` ainda descrevem horários por setor ("modo individual" e "modo ciclo"). Precisam passar para o modelo de programas.
- Cycle and Soak, desvio do sensor de chuva por setor e ligar ou não a bomba por setor.
- O ajuste percentual é manual. O balanço hídrico (#12) vai calculá-lo sozinho, como o Solar Sync faz nos Hunter.

Feito depois (13 de novembro): edição dos programas pelo encoder e retomada após queda de energia (ver `2026-11-13-retomada-apos-queda-de-energia.md`).
