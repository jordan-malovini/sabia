# Requisitos e escopo

## Objetivos

**Objetivo geral:** irrigar com base no clima medido no local, e não em horário fixo, economizando água e energia da bomba.

**Objetivos específicos**

- Regar cada setor com horário, duração e parâmetros próprios, e também em ciclo.
- Medir chuva, temperatura, umidade, pressão e luz no local.
- Estimar a água necessária por setor com um balanço hídrico.
- Acompanhar e comandar tudo pelo celular, sem servidor dedicado.
- Guardar o histórico para análise.
- Usar peças acessíveis e componentes impressos em 3D.

## Requisitos funcionais

| ID | Requisito |
| --- | --- |
| RF01 | Controlar 8 setores individualmente, cada um com tipo (gotejo ou aspersão), horário e duração. |
| RF02 | Operar em dois modos: individual (horários por setor) e ciclo (setores em sequência). |
| RF03 | Ligar um setor por vez: pedidos sobrepostos entram em fila, com pausa curta entre os setores. |
| RF04 | Permitir comando manual: ligar, parar, pular setor e parar tudo. |
| RF05 | Comandar a bomba: ligar depois que a válvula abre e desligar antes de ela fechar. |
| RF06 | Medir a chuva com pluviômetro basculante e registrar em intervalos de 10 min enquanto chove, com totais por hora, dia e mês. |
| RF07 | Medir temperatura, umidade, pressão e luz. |
| RF08 | Calcular o balanço hídrico por setor, converter o déficit em tempo de rega e adiar a rega quando choveu. |
| RF09 | Oferecer interface local: display 128x64 e encoder com botão. |
| RF10 | Servir página web local com painel, destaque do setor regando, fila, histórico em gráficos e download em CSV. |
| RF11 | Registrar dados na flash e no cartão SD, e enviá-los ao Google Drive, apagando do cartão só depois da confirmação. |
| RF12 | Continuar irrigando pela agenda se faltar rede, e criar o próprio Wi-Fi (modo AP) como acesso alternativo. |
| RF13 | Aplicar limites de segurança: tempo máximo por setor, janela de horário e nenhuma rega retomada após queda de energia. |
| RF14 | Manter todas as saídas desligadas desde o boot: o nível de desligado é escrito antes de o pino virar saída. |
| RF15 | Usar o watchdog do ESP32 para reiniciar o sistema, com todas as saídas desligadas, se o firmware travar. |
| RF16 | Proteger os comandos: senha no Wi-Fi próprio (AP) e autenticação na página para ligar, parar e alterar ajustes. |

## Requisitos não funcionais

- Funcionar em ambiente externo (calor, umidade, poeira).
- Tolerar quedas de energia sem perder a configuração nem o histórico.
- Ter baixo custo, com peças acessíveis.
- Ser extensível pelo barramento I2C e pelos pinos reservados.

## Escopo

**Incluído na versão 1**

- Controle de 8 setores e da bomba, em ciclo e individual, com fila.
- Estação com chuva, temperatura, umidade, pressão e luz.
- Balanço hídrico por setor.
- Display, encoder e página web local com histórico.
- Registro em flash e cartão SD, com envio ao Drive.

**Fora do escopo por enquanto**

- Instalação das válvulas solenoides e do transformador que as alimenta (a caixa já prevê as saídas).
- Anemômetro e biruta.
- Sensores de umidade do solo, um por setor.
- Confirmação de que a bomba ligou (fluxo, pressão ou corrente).
- Acesso de fora de casa, aplicativo nativo e análise com IA em servidor.

## Limitações conhecidas

- A quantidade de água é uma estimativa e depende da calibração do Kc e da taxa de aplicação de cada setor.
- A ET0 por Hargreaves usa só a temperatura. Umidade, pressão e luz são registradas, mas ainda não entram na decisão (ver [arquitetura](arquitetura.md#sensores-que-entram-na-decisão)).
- O pluviômetro precisa de limpeza periódica.
- O envio ao Drive precisa de internet. Sem ela, o histórico fica na flash e no cartão.

## Roteiro

| Versão | Entrega |
| --- | --- |
| 0.1 | Lógica de agenda e fila no simulador Wokwi; peças impressas em 3D |
| 0.2 | Controlador na protoboard: ESP, RTC, display, encoder e relés, regando por horário |
| 0.3 | Sensores e pluviômetro instalados, com balanço hídrico ligado |
| 0.4 | Página web com gráficos |
| 0.5 | Cartão SD e envio ao Drive |
| 1.0 | Placa de circuito impresso e instalação final no campo |
| 2.0 | Anemômetro com biruta (ET0 completa), umidade do solo, confirmação da bomba e análise dos dados |

## Pendências antes de montar

- Confirmar o modelo exato do ESP32-S3 (N16R8 provável).
- Testar o display direto com o sinal de 3,3 V do ESP.
- Conferir o circuito de recarga da pilha do módulo RTC.
- Medir as peças para projetar a caixa e a tampa.
- Verificar a corrente de partida da bomba.
- Confirmar se a placa de relés é ativa em nível baixo e como ela se comporta com o ESP em reset.
