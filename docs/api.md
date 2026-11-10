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
  "modo": "individual",
  "atual": {
    "setor": 4, "fase": "regando", "restanteS": 720, "duracaoS": 2400,
    "bomba": true, "origem": "ciclo"
  },
  "fila": [ { "setor": 5, "duracaoS": 1800 } ],
  "proximaRega": { "setor": 5, "hora": "20:00" },
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
- `fase` segue o `Controlador`: `abrindo`, `regando`, `fechando` ou `pausa`.
- `origem` pode ser `agenda`, `ciclo` ou `manual`.
- `setores` tem sempre 8 itens. `saldoPct` é a água no solo em relação a `estacao.capacidadeSoloMm`.

## Comandos

`POST /api/comando` (exige sessão)

| `acao` | Campos | O que faz |
| --- | --- | --- |
| `ligar` | `setor`, `minutos` | Põe o setor na fila (ou liga, se nada estiver regando). `minutos` é cortado no `tempoMaxMin` do setor. |
| `parar` | `setor` | Encerra o setor, se for o atual, ou o tira da fila. |
| `pular` | | Encerra o setor atual e segue a fila. |
| `pararTudo` | | Esvazia a fila e encerra o setor atual. |
| `ciclo` | | Põe todos os setores ativos na fila, cada um com a duração padrão. |
| `acertarHora` | `hora` (ISO 8601) | Acerta o RTC pela hora do aparelho. |
| `reiniciar` | | Reinicia o ESP. Nenhuma rega é retomada (RF13). |

Resposta: `200 {"ok": true}`, ou `400` com o motivo em texto (por exemplo, "Esse setor já está na fila"). A página mostra esse texto.

## Configuração

`GET /api/config` devolve tudo o que pode ser ajustado. `PUT /api/config` (exige sessão) recebe o objeto inteiro e grava na NVS.

```json
{
  "modo": "individual",
  "balancoHidrico": true, "adiarPorChuva": true, "chuvaAdiarMm": 5,
  "ciclo": { "inicio": "05:30", "dias": [1,1,1,1,1,1,1] },
  "bomba": { "usar": true, "atrasoLigarS": 2, "atrasoFecharS": 1, "pausaEntreSetoresS": 3 },
  "setores": [
    {
      "nome": "Setor 1", "ativo": true, "tipo": "gotejo",
      "duracaoMin": 35, "horarios": ["20:00"], "dias": [1,1,1,1,1,1,1],
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
- `dias` começa no domingo (índice 0).
- **Senhas nunca saem do ESP.** No `GET`, `rede.wifiSenha` e `rede.apSenha` voltam vazias. No `PUT`, uma senha vazia significa "manter a atual".
- Para trocar a senha da página, o `PUT` traz `seguranca.novaSenha`. Esse campo não é guardado como texto e não volta no `GET`.
- O firmware valida tudo de novo (faixas, `duracaoMin` ≤ `tempoMaxMin`, senha do AP com 8 caracteres ou mais) e responde `400` com o motivo se algo estiver errado. A validação da página serve só para ajudar quem está digitando.

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
  "eventos": [ ["sáb 14:20", "Setor 4 iniciou (ciclo, 40 min)"] ]
}
```

Em `hoje`, os rótulos são as 24 horas, `tempMaxC` traz a temperatura de cada hora, e `tempMinC` e `et0Mm` vêm `null`.

`GET /api/csv?tipo=chuva|clima|regas&periodo=...` devolve um CSV com `;` como separador e vírgula decimal, que abre direto no Excel em português.
