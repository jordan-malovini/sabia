# Balanço hídrico: só economiza água

**Data:** 13 de novembro de 2026

## Contexto

O RF08 pede que o controlador calcule o balanço hídrico por setor e converta o déficit em tempo de rega. Com a agenda por programas (A, B e C), faltava decidir como a conta do clima se junta ao tempo que a pessoa programou.

A conta depende de parâmetros que ainda são estimativas: a taxa de aplicação de cada setor (mm/h) não foi medida, e o Kc começa em 1,0. Um erro neles vira erro direto no tempo de rega.

## Decisão

Nas regas automáticas, o tempo de cada setor é **o menor entre o tempo do programa e o tempo que o balanço calcula**:

- setor com água suficiente (déficit abaixo do mínimo): é pulado;
- déficit pequeno: rega só o necessário;
- déficit grande: rega o tempo do programa, nunca mais que isso.

O tempo do programa é o teto. O balanço encurta ou pula, e não tem como regar demais por causa de um parâmetro errado.

**Quando o balanço não manda:**

- Programa iniciado na mão (página, encoder ou serial): rega o tempo programado.
- Sem dados confiáveis: se o último dia fechou sem temperatura (sensor ausente ou com defeito) ou sem a latitude nos ajustes, vale o tempo do programa. É melhor regar pelo programa do que deixar de regar por causa de um sensor que falhou.
- Controlador desligado por dias inteiros: não se sabe quanto evaporou, então o balanço espera fechar um dia completo antes de voltar a mandar.
- "Ajustar o tempo pelo balanço hídrico" desligado nos ajustes.

**Adiamento por chuva:** se choveu mais que o limite (5 mm de fábrica) nas últimas 24 horas, os inícios automáticos são pulados. É separado da "pausa por chuva" manual, que vale por dias.

## Como a conta fecha

Uma vez por dia, à meia-noite:

```
ET0   = 0,0023 x (Tmédia + 17,8) x raiz(Tmáx - Tmín) x Ra
Saldo = saldo anterior + chuva efetiva - ET0 x Kc      (entre 0 e a capacidade do solo)
```

A rega entra no saldo na hora em que termina. A chuva do dia só conta se passar do mínimo ("chuva efetiva"). O saldo começa cheio e fica guardado na memória do ESP.

A radiação Ra foi conferida com o exemplo 8 da FAO-56 (20° sul, 3 de setembro: 32,2 MJ/m²/dia).

## Antes de confiar

1. Informar a **latitude** em Ajustes → Estação.
2. Medir a **taxa de aplicação** de cada setor (copos na aspersão, vazão dos gotejadores no gotejo).
3. Acompanhar o solo por algumas semanas e ajustar o Kc ou o fator de cada setor.

Por isso o balanço **vem desligado de fábrica** ("Ajustar o tempo pelo balanço hídrico", em Ajustes → Ajuste pelo clima). Desligado, o saldo continua sendo calculado e aparece no painel, sem interferir nas regas. O adiamento por chuva vem ligado.

## Ainda não feito

- Janela de horário por setor (RF13): o campo existe nos ajustes, mas o firmware ainda não usa.
- Regar **mais** que o programado quando o déficit é grande. Fica para depois da calibração.
