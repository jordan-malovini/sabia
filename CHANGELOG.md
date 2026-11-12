# Changelog

Formato: uma seção por versão, da mais nova para a mais antiga.

## [0.1] - em desenvolvimento

### Adicionado
- Estrutura do firmware no PlatformIO, com ambientes para o Wokwi, para a placa N16R8 e para testes no computador.
- Controlador de setores: fila com um setor por vez, modo ciclo, pular, parar setor e parar tudo, e tempo máximo por rega (RF01 a RF04, RF13).
- Sequência válvula e bomba: a bomba liga 2 s depois de a válvula abrir e desliga 1 s antes de ela fechar, com pausa de 3 s entre setores (RF05).
- Saídas configuradas como desligadas antes de virarem saída, no boot (RF14).
- Circuito do Wokwi com LEDs no lugar dos relés e comandos pelo monitor serial.
- 12 testes automáticos da fila e da sequência.
- GitHub Actions: testes e compilação do firmware a cada PR e push na `main`.
- Página web local (`firmware/data/index.html`): painel, histórico e ajustes completos, com modo demonstração enquanto o firmware não tem a API.
- Contrato da API entre a página e o firmware (`docs/api.md`).
- 31 testes de interação da página no Chrome sem janela (`tests/pagina/`).
- Logo do SABIÁ em `assets/logo/` (horizontal, com galho, só o pássaro e ícone), aplicada no README, como favicon e no topo da página web, e em 1 bit para a tela de boot do display (`firmware/include/logo_boot.h`).
- Caixa do controlador em OpenSCAD (`hardware/caixa-3d/caixa.scad`): base com divisórias, suportes, berços, prensa-cabos e abas de parede; tampa com dobradiça, tela atrás de visor de acrílico e encoder. STL gerados no GitHub Actions. Documentação em `docs/caixa.md`.
- Caixa: microSD em pé numa paredinha com 2 parafusos (cartão para a tampa, pinos para o ESP) e RTC deitado sobre 3 suportes, no lugar dos berços.
- Interface local: tela de 128x64 com a logo no boot, tela de estado e menu pelo encoder (escolher o setor, depois o tempo). Relógio e contagem de chuva do pluviômetro na tela.
- Simulação no Wokwi com tela, encoder, relógio e botão de pluviômetro.
- Monitor serial com eco do que é digitado e o comando `l` em duas etapas.
- Troca entre setores com sobreposição de 2 s e a bomba ligada do começo ao fim da fila (ideia do controlador antigo), em vez de desligar a bomba a cada setor.
- Agenda de regas com 3 programas (A, B, C): até 4 horários de início, dias da semana, ímpares, pares ou a cada N dias, tempo por setor e ajuste de 5 a 200 %. Empilhamento de programas, pausa por chuva de até 14 dias, próxima rega na tela e gravação na memória do ESP. Modelo baseado em controladores Rain Bird e Hunter.
- 14 testes da agenda, incluindo o defeito do controlador antigo (regar todos os dias no mesmo horário).
- Nome do projeto: SABIÁ (Sistema Automático de Balanço e Irrigação Agrícola).
- Documentação dividida em README e `docs/` (requisitos, arquitetura, hardware e mapa de pinos).
- Requisitos de segurança RF14 (saídas desligadas no boot), RF15 (watchdog) e RF16 (senha e autenticação na página web).

### Alterado
- O registro de chuva passa a ser descrito em "intervalos de 10 min" (antes "balde de 10 min").
- Fica explícito que a ET0 usa só temperatura, e que umidade, pressão e luz são registradas para uso futuro.
