# SABIÁ - embute a página web no firmware.
#
# Roda antes de cada compilação (extra_scripts no platformio.ini): compacta
# data/index.html com gzip e gera include/pagina_gz.h, que o servidor web
# entrega direto da memória flash. Assim a página vai junto com o firmware,
# numa gravação só, e não depende de sistema de arquivos.

import gzip
import os

Import("env")  # noqa: F821 (fornecido pelo PlatformIO)

projeto = env.subst("$PROJECT_DIR")  # noqa: F821
origem = os.path.join(projeto, "data", "index.html")
destino = os.path.join(projeto, "include", "pagina_gz.h")

with open(origem, "rb") as arquivo:
    dados = gzip.compress(arquivo.read(), compresslevel=9, mtime=0)

linhas = [
    "// Gerado por embutir_pagina.py a partir de data/index.html. Não edite.",
    "#pragma once",
    "#include <stddef.h>",
    "#include <stdint.h>",
    "",
    "const size_t PAGINA_GZ_TAMANHO = %d;" % len(dados),
    "const uint8_t PAGINA_GZ[] = {",
]
for i in range(0, len(dados), 20):
    linhas.append("  " + ",".join(str(b) for b in dados[i:i + 20]) + ",")
linhas.append("};")
texto = "\n".join(linhas) + "\n"

atual = None
if os.path.exists(destino):
    with open(destino, "r", encoding="utf-8") as arquivo:
        atual = arquivo.read()
if atual != texto:  # só regrava se mudou, para não recompilar à toa
    with open(destino, "w", encoding="utf-8") as arquivo:
        arquivo.write(texto)
    print("Pagina web embutida: %d bytes (gzip)" % len(dados))
