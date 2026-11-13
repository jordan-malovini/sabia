# Roteiro de teste no Wokwi

Um teste de cada parte do SABIÁ no simulador, na ordem que faz sentido. Cada passo diz o que fazer e o que deve acontecer. Leva cerca de 40 minutos.

**Quando algo não bater:** anote o número do passo, tire um print da tela (simulador e monitor serial) e me mande. Não precisa saber o motivo; o número do passo e o print bastam.

## Antes de começar

1. Mescle os PRs na ordem: #36, #37 e #38.
2. No VS Code, atualize o projeto (Source Control → Pull, ou `git pull`).
3. Compile: `pio run -e esp32s3`.
4. Inicie a simulação: `Ctrl+Shift+P` → **Wokwi: Start Simulator**.

**No circuito:**

| Peça | No simulador |
| --- | --- |
| Setores 1 a 8 | LEDs verdes (acesos = válvula aberta) |
| Bomba | LED vermelho |
| Tela | OLED pequeno (no real será o ST7920) |
| Encoder | clique nele; as setas giram e a barra de espaço clica |
| Pluviômetro | botão azul (cada clique vale 0,2 mm) |
| Cartão microSD | módulo ao lado do ESP |

**No monitor serial:** clique no terminal, digite o comando e tecle Enter. `?` mostra o estado a qualquer momento.

---

## Parte 1: ligar

| # | Faça | Deve acontecer |
| --- | --- | --- |
| 1.1 | Só inicie a simulação. | A tela mostra a logo e, depois de 2 s, a tela de estado com a hora. Todos os LEDs apagados. |
| 1.2 | Olhe o monitor serial. | Aparece `SABIA v0.4-dev`, `Sensores: BME280 nao encontrado, VEML7700 nao encontrado` (normal: o Wokwi não tem esses sensores) e `Registro: cartao SD`. |
| 1.3 | Espere até 20 s. | Aparece `Wi-Fi "Wokwi-GUEST" conectado. Pagina: http://...`. |
| 1.4 | Espere mais alguns segundos. | Pode aparecer `Relogio acertado pela internet`. A hora da tela deve ficar igual à de Brasília. |

> Se a hora ficar errada, acerte na mão: `hora 2026-11-13 15:30` (com a data e a hora de agora).

## Parte 2: um setor e a fila

| # | Faça | Deve acontecer |
| --- | --- | --- |
| 2.1 | Digite `l 1 20`. | O LED do setor 1 acende. **Cerca de 2 s depois**, a bomba (vermelho) acende. |
| 2.2 | Espere os 20 s. | A bomba apaga primeiro; **cerca de 1 s depois**, o setor 1 apaga. |
| 2.3 | Digite `l 1 20` e logo depois `l 2 20`. | Setor 1 e bomba acendem. O `?` mostra o setor 2 na fila. |
| 2.4 | Observe a troca, no fim do setor 1. | **A bomba não apaga.** O setor 2 acende, os dois ficam acesos por cerca de 2 s e só então o setor 1 apaga. |
| 2.5 | Espere o setor 2 terminar. | Bomba apaga, depois o setor 2. |
| 2.6 | Digite `c 15` (ciclo, 15 s cada). | Os 8 setores passam em sequência, com a bomba acesa do começo ao fim, sem piscar. |
| 2.7 | No meio do ciclo, digite `p`. | O setor atual é pulado (com a mesma troca do 2.4). |
| 2.8 | Digite `s`. | Bomba apaga, depois o setor; a fila esvazia. |

## Parte 3: tela e encoder

| # | Faça | Deve acontecer |
| --- | --- | --- |
| 3.1 | Clique no encoder (barra de espaço). | Abre o menu. |
| 3.2 | Gire até "Setor 3", clique, gire até 1 min e clique. | O setor 3 liga e a tela de estado mostra o tempo descendo. |
| 3.3 | Segure o clique (clique longo) na tela de estado. | "Parando tudo": o setor 3 desliga. |
| 3.4 | Menu → "Programar agenda" → Programa A. | Abre o menu do programa, com "Ligado", "Horarios", os dias, "Tempos", "Ajuste" e "Iniciar agora". |
| 3.5 | Mude algo (por exemplo, o ajuste para 80 %) e volte. | Digite `a` no serial: a mudança aparece lá. |
| 3.6 | Fique 1 min sem mexer em nada. | A tela apaga. Um clique só acende, sem abrir o menu. |

## Parte 4: agenda

Coloque o relógio 2 min antes do horário de teste e programe o A:

```
hora 2026-11-13 05:58
g A on
g A h 1 06:00
g A t * 1
g A d 1111111
```

| # | Faça | Deve acontecer |
| --- | --- | --- |
| 4.1 | Digite `a`. | O programa A aparece ligado, início às 06:00, todos os dias, 1 min por setor, e "Proxima rega: programa A as 06:00". |
| 4.2 | Espere dar 06:00. | Os 8 setores entram na fila sozinhos e regam em sequência, a bomba ligada direto. |
| 4.3 | Digite `s` para parar. Depois `hora 2026-11-14 05:59` e espere. | **No dia seguinte, rega de novo às 06:00.** Este é o defeito do controlador antigo; aqui precisa funcionar. |
| 4.4 | Digite `s`, depois `chuva 2` e `a`. | "Pausa por chuva: 2 dia(s)", e a próxima rega passa para depois da pausa. `chuva 0` cancela. |

## Parte 5: queda de energia

| # | Faça | Deve acontecer |
| --- | --- | --- |
| 5.1 | Digite `l 4 300` e espere uns 20 s regando. | Setor 4 e bomba acesos. |
| 5.2 | Digite `reiniciar`. | Tudo apaga. O ESP reinicia e aparece "Energia voltou... Retomando o setor 4". |
| 5.3 | Olhe a tela. | Mostra "Energia voltou" com a contagem de 30 s (no simulador a espera é curta; na placa é de 10 min). |
| 5.4 | Espere a contagem. | O setor 4 volta a regar com o tempo que faltava, não com os 300 s inteiros. |
| 5.5 | Repita o 5.1 e o 5.2 e, durante a contagem, clique no encoder. | Retoma na hora. Um clique longo, em vez disso, cancela. |

## Parte 6: watchdog

| # | Faça | Deve acontecer |
| --- | --- | --- |
| 6.1 | Ligue um setor (`l 1 120`) e digite `testar watchdog`. | "Travando o programa de proposito..." |
| 6.2 | Espere 15 s. | O ESP reinicia sozinho, com tudo desligado, e depois retoma o setor 1 (como no passo 5.4). |

## Parte 7: página web

Abra **http://localhost:8180** no navegador do computador, com a simulação rodando.

| # | Faça | Deve acontecer |
| --- | --- | --- |
| 7.1 | Abra a página. | Painel com a hora do controlador e os 8 setores. **Não** aparece a faixa amarela de "modo demonstração". |
| 7.2 | O topo mostra "Troque a senha". | Normal: a senha ainda é a de fábrica. |
| 7.3 | Clique em "Ligar" no Setor 2. | Pede a senha. Entre com `sabia1234`. |
| 7.4 | Ligue o setor 2 por 1 min. | O LED do setor 2 acende no simulador e o painel mostra "Regando agora". |
| 7.5 | Clique em "Parar tudo" e confirme. | Os LEDs apagam. |
| 7.6 | "Iniciar programa" → Programa A. | Os setores do A entram na fila. Pare tudo depois. |
| 7.7 | Ajustes → Agenda → Programa B: ligue, ponha um horário e salve. | "Ajustes salvos". Digite `a` no serial: o programa B aparece igual. |
| 7.8 | Ajustes → Segurança: troque a senha e salve. | O aviso "Troque a senha" some. |
| 7.9 | Digite `reiniciar` no serial e, depois, recarregue a página. | Os ajustes do 7.7 continuam lá. A senha nova continua valendo. |
| 7.10 | Clique em "Sair" e tente ligar um setor. | Pede a senha de novo. |

## Parte 8: chuva, registro e histórico

| # | Faça | Deve acontecer |
| --- | --- | --- |
| 8.1 | Clique 10 vezes no botão azul. | O painel mostra "Chuva hoje 2,0 mm". |
| 8.2 | Clique mais 15 vezes (total 5 mm). | O painel avisa "Choveu 5,0 mm em 24 h: as regas automáticas estão adiadas". |
| 8.3 | Na página, vá em Histórico → Hoje. | O gráfico de chuva mostra a hora de agora. A lista de eventos tem "Controlador ligado", as regas e "Ajustes salvos pela pagina". |
| 8.4 | Clique em "Chuva" para baixar o CSV. | Baixa um arquivo que abre no Excel com as 24 horas. |
| 8.5 | Toque em "Tudo certo" no topo. | Mostra "Cartão SD: ok". |
| 8.6 | Digite `reiniciar` e volte ao Histórico. | **A chuva e os eventos de hoje continuam lá** (relidos do cartão). |

## Parte 9: janela de horário

| # | Faça | Deve acontecer |
| --- | --- | --- |
| 9.1 | Ajustes → Setores → Setor 1: janela de 04:00 a 05:00. Salve. | "Ajustes salvos". |
| 9.2 | Repita a agenda da Parte 4 (06:00). | Às 06:00, o setor 1 **não** rega e os outros regam. |
| 9.3 | Histórico → Hoje → eventos. | "Setor 1 fora da janela de horario: nao regou". |
| 9.4 | Ligue o setor 1 na mão. | Rega normalmente: a janela só vale para a agenda. |

---

## O que não dá para testar no Wokwi

- Sensores BME280 e VEML7700 (não existem no simulador).
- Display ST7920 e DS3231 de verdade (o Wokwi usa OLED e DS1307).
- Balanço hídrico ao longo de dias (precisa de temperatura).
- Envio ao Google Drive (precisa do script da sua conta, ver `docs/drive.md`).
- A rede própria `SABIA` (modo AP).

Esses ficam para a bancada, quando as peças chegarem.
