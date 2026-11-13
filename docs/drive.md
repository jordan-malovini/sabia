# Envio do registro ao Google Drive

O SABIÁ pode mandar o registro de cada dia para uma planilha no Google Drive. Quem recebe é um pequeno programa (Google Apps Script) que roda na sua conta do Google, de graça. O código dele está em [`tools/drive/Codigo.gs`](../tools/drive/Codigo.gs).

**Estado:** o firmware e o script estão escritos, mas o envio ainda não foi testado com uma conta de verdade.

## Como funciona

- Depois da meia-noite, o controlador envia o arquivo do dia que acabou (chuva, clima, regas e eventos).
- O script grava as linhas numa planilha por semana (`SABIA 2026-S46`, por exemplo), dentro da pasta `SABIA` do seu Drive.
- O controlador só considera o dia enviado quando o script responde que gravou. Enquanto não responde, o arquivo fica no cartão e o envio é tentado de novo (por até 60 dias).
- Reenviar o mesmo dia não duplica linhas.
- Sem internet nada se perde: a irrigação não depende do envio, e os arquivos ficam esperando.

## Instalar o script (uma vez)

1. Entre em [script.google.com](https://script.google.com) com a conta que vai guardar os dados. O ideal é uma conta só para o projeto.
2. Clique em **Novo projeto** e dê o nome `SABIA`.
3. Apague o que estiver no editor e cole o conteúdo de `tools/drive/Codigo.gs`.
4. Clique em **Implantar → Nova implantação**. Em "Tipo", escolha **App da Web**.
5. Em "Executar como", deixe **Eu**. Em "Quem pode acessar", escolha **Qualquer pessoa**.
6. Clique em **Implantar** e autorize o acesso ao Drive e às planilhas quando o Google pedir.
7. Copie o **URL do app da Web** (termina em `/exec`).

Para conferir, abra esse endereço no navegador: deve aparecer `{"ok":true,"servico":"SABIA"}`.

## Ligar no controlador

Na página do SABIÁ, em **Ajustes → Registro**:

1. Cole o endereço em "Endereço do Apps Script".
2. Ligue "Enviar ao Google Drive".
3. Escolha de quantas em quantas horas o controlador confere se há dias a enviar.
4. Salve.

O toque em "Tudo certo", no topo da página, mostra até que dia o registro já foi enviado, ou o motivo da última falha.

## Cuidados

- **O endereço é a chave.** Quem tiver o endereço consegue gravar linhas na sua planilha (não consegue ler nada). Não publique o endereço. Se vazar, crie uma nova implantação e troque nos ajustes.
- O controlador não confere o certificado do Google ao enviar. Os dados não são sigilosos, mas é bom saber.
- Se mudar o código do script, é preciso criar uma **nova versão** da implantação para valer.

## Formato da planilha

| Coluna | Conteúdo |
| --- | --- |
| dia | data do registro |
| tipo | `chuva`, `clima`, `rega` ou `evento` |
| hora | hora do acontecimento |
| valor1 a valor5 | chuva: mm no intervalo de 10 min. Clima: máxima, mínima, umidade, pressão e luz da hora. Rega: setor e segundos. Evento: o texto. |
