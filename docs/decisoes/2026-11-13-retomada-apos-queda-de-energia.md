# Decisão: retomar a rega depois de uma queda de energia

**Data:** 13 de novembro de 2026

## Contexto

O RF13 original dizia que nenhuma rega seria retomada depois de uma queda de energia. O controlador antigo fazia o contrário: ao religar no meio de uma rega, esperava 10 minutos com tudo desligado ("AGUARDANDO ENERGIA") e retomava o setor, com um botão para cancelar. A espera existe para a bomba não ficar ligando e desligando enquanto a rede oscila.

## Decisão

Retomar, como o antigo, com três melhorias:

1. **Tempo certo.** O ponto salvo guarda o tempo que falta do setor atual, atualizado a cada minuto, e a fila inteira. O antigo só gravava ao ligar a bomba e nas trocas, então recomeçava o setor quase do início.
2. **Limite de 2 horas.** Com o relógio (RTC), o SABIÁ sabe quanto tempo a energia ficou fora. Se passou de 2 h, a rega é descartada, para não regar aspersão ao meio-dia por causa de uma queda de madrugada. Sem relógio, retoma sempre, como o antigo.
3. **Retomar na hora ou cancelar.** Na tela, clique retoma e clique longo cancela. No serial, `retomar` e `cancelar`. No antigo, o botão só cancelava.

Se a energia piscar de novo durante a espera, o ponto salvo continua guardado e a contagem recomeça.

## Custo

A gravação a cada minuto acontece só enquanto rega. Com algumas horas de rega por dia, são poucas centenas de gravações diárias na NVS, que distribui o desgaste entre as páginas. É desprezível para a vida útil da flash.

## Como testar

No simulador: ligue um setor, espere alguns segundos e digite `reiniciar` no terminal. A tela deve mostrar "Energia voltou" com a contagem (30 s no simulador, 10 min na placa).
