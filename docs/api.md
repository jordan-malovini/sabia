# API da página web

A página (`firmware/data/index.html`) conversa com o ESP32 por JSON. Este documento é o contrato: o firmware implementa estes endereços, e a página já está escrita para usá-los.

Enquanto o firmware não responder a `/api/estado`, a página entra em **modo demonstração** e simula tudo no navegador. É assim que ela funciona quando aberta direto do computador.

## Sessão e segurança (RF16)

- Ler (`GET`) é livre. Quem está na rede vê o painel.
- Alterar (`POST`, `PUT`, `DELETE`) exige sessão. Sem ela, a resposta é **401** e a página pede a senha.
- A sessão é um cookie `HttpOnly` criado pelo `POST /api/sessao`, que expira depois de `seguranca.sessaoMin` sem uso.

| Método | Caminho | Corpo | Resposta |
| --- | --- | --- | --- |
| `POST` | `/api/sessao` | `{"senha": "..."}` | `200 {"ok": true}` ou `401` |
| `DELETE` | `/api/sessao` | | `200 {"ok": true}` |

## Estado atual

`GET /api/estado`: a página chama a cada 3 s. Se passar de 30 s sem resposta, ela avisa e acinzenta os dados.

```json
{
  "versao": "0.4.0",
  "hora": "2026-11-07T14:32:10-03:00",
  "sistema": {
    "wifi": true, "ip": "192.168.0.50", "modoAp": false,
    "cartaoSd": { "ok": true, "usoPct": 12 },
    "drive": { "ativo": true, "ultimoEnvio": "ontem, 23:00" },
    "rtc": true, "ligadoHa": 273600
  },
  "sessao": { "logado": false },
  "atual": {
    "setor": 4, "fase": "regando", "restanteS": 720, "duracaoS": 2400,
    "bomba": true, "origem": "programa A"
  },
  "fila": [ { "setor": 6, "duracaoS": 2400 } ],
  "proximaRega": { "programa": "B", "hora": "20:00", "quando": "hoje" },
  "pausaChuvaDias": 0,
  "clima": {
    "chuvaHojeMm": 4.2, "chuvaMesMm": 86, "tempC": 27.5,
    "tempMaxC": 31.2, "tempMinC": 21.4, "et0HojeMm": 4.6
  },
  "setores": [
    { "saldoPct": 62, "proxima": "20:00 · 35 min", "adiadoAte": null }
  ]
}
```

- `atual` é `null` quando nada está regando.
- `fase` segue o `Controlador`: `abrindo`, `regando`, `trocando`, `fechando` ou `pausa`. Em `trocando`, a válvula do próximo setor já abriu e a bomba continua ligada.
- `origem` pode ser `programa A`, `programa B`, `programa C`, `ciclo`, `manual` ou `retomada`.
- `proximaRega` é o próximo início da agenda (`null` se nenhum programa estiver ligado). `quando` vem pronto para mostrar: `hoje`, `amanhã` ou o dia da semana.
- `pausaChuvaDias` é quantos dias faltam na pausa por chuva (0 = sem pausa).
- `setores` tem sempre 8 itens. `saldoPct` é a água no solo em relação a `estacao.capacidadeSoloMm`.

## Comandos

`POST /api/comando` (exige sessão)

| `acao` | Campos | O que faz |
| --- | --- | --- |
| `ligar` | `setor`, `minutos` | Põe o setor na fila (ou liga, se nada estiver regando). `minutos` é cortado no `tempoMaxMin` do setor. |
| `parar` | `setor` | Encerra o setor, se for o atual, ou o tira da fila. |
| `pular` | | Encerra o setor atual e segue a fila. |
| `pararTudo` | | Esvazia a fila e encerra o setor atual. |
| `programa` | `programa` (0 = A, 1 = B, 2 = C) | Põe na fila os setores do programa, com os tempos e o ajuste dele, mesmo que o programa esteja desligado ou hoje não seja dia de rega. |
| `ciclo` | | Ciclo de teste: põe todos os setores ativos na fila, cada um com a duração padrão. |
| `chuva` | `dias` (0 a 14) | Pausa a agenda por esse número de dias. 0 cancela a pausa. Não afeta o que for ligado na mão. |
| `acertarHora` | `hora` (ISO 8601) | Acerta o RTC pela hora do aparelho. |
| `reiniciar` | | Reinicia o ESP. A rega em andamento volta depois da espera, se a retomada estiver ligada (RF13). |

Resposta: `200 {"ok": true}`, ou `400` com o motivo em texto (por exemplo, "Esse setor já está na fila"). A página mostra esse texto.

## Configuração

`GET /api/config` devolve tudo o que pode ser ajustado. `PUT /api/config` (exige sessão) recebe o objeto inteiro e grava na NVS.

```json
{
  "balancoHidrico": true, "adiarPorChuva": true, "chuvaAdiarMm": 5,
  "programas": [
    {
      "ativo": true,
      "inicios": ["05:00", null, null, null],
      "modo": "semana", "dias": [1,1,1,1,1,1,1],
      "intervaloDias": 2, "inicioIntervalo": "2026-11-13",
      "minutos": [0, 40, 0, 40, 0, 40, 0, 40],
      "ajustePct": 100
    }
  ],
  "retomada": { "ativa": true, "esperaMin": 10, "limiteMin": 120 },
  "bomba": { "usar": true, "atrasoLigarS": 2, "atrasoFecharS": 1, "sobreposicaoS": 2, "pausaEntreSetoresS": 3 },
  "setores": [
    {
      "nome": "Setor 1", "ativo": true, "tipo": "gotejo",
      "duracaoMin": 35,
      "kc": 1.0, "taxaMmH": 2.5, "eficienciaPct": 90,
      "deficitMinMm": 3, "tempoMaxMin": 60,
      "janela": ["00:00", "23:59"], "fator": 1.0
    }
  ],
  "estacao": { "mmPorBasculada": 0.2, "chuvaEfetivaMinMm": 2, "capacidadeSoloMm": 40, "latitude": null, "ajusteTempC": 0 },
  "relogio": { "ntp": true, "fuso": "America/Sao_Paulo" },
  "rede": { "wifiNome": "", "wifiSenha": "", "apNome": "SABIA", "apSenha": "", "apSempre": false },
  "registro": { "drive": false, "driveUrl": "", "envioHoras": 24, "diasNoCartao": 7 },
  "display": { "luzSegundos": 60, "travarComandos": false },
  "seguranca": { "sessaoMin": 30 }
}
```

Regras:
- `programas` tem sempre 3 itens (A, B, C), no modelo da classe `Agenda` do firmware:
  - `inicios`: 4 posições, `"HH:MM"` ou `null`.
  - `modo`: `semana` (usa `dias`), `impares`, `pares` (dia do mês) ou `intervalo` (a cada `intervaloDias`, de 1 a 31, contando de `inicioIntervalo`).
  - `minutos`: 8 posições, de 0 a 180. 0 deixa o setor fora do programa.
  - `ajustePct`: de 5 a 200. O tempo de cada setor é `minutos × ajustePct / 100`, calculado em segundos e cortado no `tempoMaxMin` do setor.
- `dias` começa no domingo (índice 0).
- `duracaoMin` do setor é a duração padrão para ligar na mão e para o ciclo de teste. Os tempos da agenda ficam nos programas.
- `retomada`: ver `docs/decisoes/2026-11-13-retomada-apos-queda-de-energia.md`.
- **Senhas nunca saem do ESP.** No `GET`, `rede.wifiSenha` e `rede.apSenha` voltam vazias. No `PUT`, uma senha vazia significa "manter a atual".
- Para trocar a senha da página, o `PUT` traz `seguranca.novaSenha`. Esse campo não é guardado como texto e não volta no `GET`.
- O firmware valida tudo de novo (faixas, `duracaoMin` e os `minutos` dos programas ≤ `tempoMaxMin`, senha do AP com 8 caracteres ou mais) e responde `400` com o motivo se algo estiver errado. A validação da página serve só para ajudar quem está digitando.

## Histórico

`GET /api/historico?periodo=hoje|7d|30d`

```json
{
  "rotulos": ["dom", "seg", "ter"],
  "chuvaMm": [0, 12, 4.2],
  "tempMaxC": [31.0, 29.5, 30.1],
  "tempMinC": [21.0, 20.2, 21.4],
  "et0Mm": [4.1, 3.0, 3.8],
  "regas": [ { "setor": 1, "nome": "Setor 1", "vezes": 6, "minutos": 210 } ],
  "eventos": [ ["sáb 14:20", "Setor 4 iniciou (programa A, 40 min)"] ]
}
```

Em `hoje`, os rótulos são as 24 horas, `tempMaxC` traz a temperatura de cada hora, e `tempMinC` e `et0Mm` vêm `null`.

`GET /api/csv?tipo=chuva|clima|regas&periodo=...` devolve um CSV com `;` como separador e vírgula decimal, que abre direto no Excel em português.
