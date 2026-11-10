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
- Nome do projeto: SABIÁ (Sistema Automático de Balanço e Irrigação Agrícola).
- Documentação dividida em README e `docs/` (requisitos, arquitetura, hardware e mapa de pinos).
- Requisitos de segurança RF14 (saídas desligadas no boot), RF15 (watchdog) e RF16 (senha e autenticação na página web).

### Alterado
- O registro de chuva passa a ser descrito em "intervalos de 10 min" (antes "balde de 10 min").
- Fica explícito que a ET0 usa só temperatura, e que umidade, pressão e luz são registradas para uso futuro.
