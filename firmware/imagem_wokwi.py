# SABIÁ - imagem completa da memória flash para o simulador Wokwi.
#
# Roda depois de cada compilação do ambiente esp32s3 (extra_scripts no
# platformio.ini). Junta num arquivo só o bootloader, a tabela de partições
# e o programa, nos endereços certos: .pio/build/esp32s3/sabia-wokwi.bin.
#
# Por quê: com só o firmware.bin, o Wokwi usa uma tabela de partições
# própria, com 1,25 MB para o programa. O SABIÁ passou desse tamanho, e o ESP
# simulado reiniciava sem parar logo no boot. Com a imagem completa, vale a
# tabela de partições do projeto.

Import("env")  # noqa: F821 (fornecido pelo PlatformIO)

programa = "$BUILD_DIR/${PROGNAME}.bin"
imagem = "$BUILD_DIR/sabia-wokwi.bin"
placa = env.BoardConfig()  # noqa: F821


def esptool(env):
    # Nas versões antigas da plataforma o esptool é um script Python; nas
    # novas, um executável. Os dois aceitam os mesmos argumentos.
    caminho = env.subst("$OBJCOPY")
    return ["$PYTHONEXE", "$OBJCOPY"] if caminho.endswith(".py") else ["$OBJCOPY"]


def juntar(source, target, env):
    partes = env.Flatten(env.get("FLASH_EXTRA_IMAGES", [])) + ["$ESP32_APP_OFFSET", programa]
    comando = esptool(env) + ["--chip", placa.get("build.mcu", "esp32s3"), "merge_bin",
               "--flash_mode", "dio", "--flash_size", placa.get("upload.flash_size", "8MB"), "-o", imagem] + partes
    env.Execute(" ".join('"%s"' % p if " " in env.subst(p) else p for p in comando))


env.AddPostAction(programa, juntar)  # noqa: F821
