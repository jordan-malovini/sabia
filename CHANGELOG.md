# Changelog

Formato: uma seção por versão, da mais nova para a mais antiga.

## [0.1] - em desenvolvimento

### Adicionado
- Nome do projeto: SABIÁ (Sistema Automático de Balanço e Irrigação Agrícola).
- Documentação dividida em README e `docs/` (requisitos, arquitetura, hardware e mapa de pinos).
- Requisitos de segurança RF14 (saídas desligadas no boot), RF15 (watchdog) e RF16 (senha e autenticação na página web).

### Alterado
- O registro de chuva passa a ser descrito em "intervalos de 10 min" (antes "balde de 10 min").
- Fica explícito que a ET0 usa só temperatura, e que umidade, pressão e luz são registradas para uso futuro.
