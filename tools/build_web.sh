#!/bin/bash
# Regenera los binarios del instalador web PARA LAS TRES PLACAS: cada una
# tiene su propio manifest-<placa>.json y su propia carpeta web/firmware/<placa>/,
# seleccionadas por -DTAMAPOKE_BOARD_* via compiler.cpp.extra_flags (NO
# build.extra_flags -- esa propiedad ya trae los -D del propio core, como
# ARDUINO_USB_CDC_ON_BOOT, y sobreescribirla en vez de sumarle rompe la
# compilacion en formas que no tienen nada que ver con el board_select.h).
#
# Uso: bash tools/build_web.sh
set -e
cd "$(dirname "$0")/.."
FQBN="esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB"

# esptool no esta en el PATH; el core de Arduino trae uno y es la version que
# coincide con lo que acabamos de compilar.
ESPTOOL="$(ls ~/Library/Arduino15/packages/esp32/tools/esptool_py/*/esptool 2>/dev/null | head -1)"
[ -z "$ESPTOOL" ] && ESPTOOL="$(ls "$HOME/AppData/Local/Arduino15/packages/esp32/tools/esptool_py"/*/esptool.exe 2>/dev/null | head -1)"
[ -z "$ESPTOOL" ] && ESPTOOL="$(command -v esptool.py || command -v esptool)"
[ -z "$ESPTOOL" ] && { echo "no esptool found"; exit 1; }

FW="$(grep -o '"[0-9.]*"' TamaPoke.ino | head -1 | tr -d '"')"

# id de placa -> (macro TAMAPOKE_BOARD_*, subcarpeta en web/firmware/, manifest)
build_one() {
  local macro="$1" dir="$2" manifest="$3"
  echo "=== $dir ($macro) ==="
  local BP="build/$dir"
  rm -rf "$BP"
  arduino-cli compile --fqbn "$FQBN" --build-property "compiler.cpp.extra_flags=-D$macro" \
    --build-path "$BP" .

  mkdir -p "web/firmware/$dir"
  # THE FOUR PARTS ARE LO QUE ENVIA EL INSTALADOR, y eso no es un detalle. Una
  # imagen fusionada empieza en el offset 0 y merge-bin RELLENA LOS HUECOS con
  # 0xFF, asi que escribe flash en blanco justo encima de la particion NVS en
  # 0x9000 -- que es el guardado del jugador. Cada parte en su propio offset
  # deja 0x9000..0xE000 intacto, igual que la subida por USB de arduino-cli.
  cp "$BP/TamaPoke.ino.bootloader.bin" "web/firmware/$dir/bootloader.bin"
  cp "$BP/TamaPoke.ino.partitions.bin" "web/firmware/$dir/partitions.bin"
  cp "$BP/boot_app0.bin"               "web/firmware/$dir/boot_app0.bin"
  cp "$BP/TamaPoke.ino.bin"            "web/firmware/$dir/app.bin"

  # Fusionado tambien, solo para quien flashea una placa EN BLANCO por linea de
  # comandos de una vez. A proposito NO esta en el manifest: borraria el guardado.
  "$ESPTOOL" --chip esp32s3 merge-bin -o "web/firmware/$dir/tamapoke.bin" \
    0x0     "$BP/TamaPoke.ino.bootloader.bin" \
    0x8000  "$BP/TamaPoke.ino.partitions.bin" \
    0xe000  "$BP/boot_app0.bin" \
    0x10000 "$BP/TamaPoke.ino.bin"

  python3 - "$manifest" "$FW" <<'PYEOF'
import json, sys
path, fw = sys.argv[1], sys.argv[2]
m = json.load(open(path))
m['version'] = fw
# ALWAYS true, y el nombre es lo contrario de lo que hace. En el camino
# sin-Improv de esp-web-tools -- el nuestro, ya que este firmware no habla
# Improv -- el boton Install es:
#
#   new_install_prompt_erase ? state = "ASK_ERASE" : _startInstall(true)
#
# Asi que FALSE significa "no preguntes, borra ya", y llama a eraseFlash(),
# un borrado de TODO el chip que se lleva la NVS sin importar que partes
# liste el manifest. TRUE muestra una pantalla con una casilla "Erase
# device" que empieza DESMARCADA, y dejarla asi instala sin borrar -- lo que,
# con las cuatro partes de arriba, deja el guardado intacto.
#
# Esto se puso en False a proposito una vez, creyendo que el nombre decia lo
# que dice. Borro dos guardados reales. No "arreglarlo" de vuelta.
m['new_install_prompt_erase'] = True
json.dump(m, open(path, 'w'), indent=2)
print('  manifest %s -> version %s' % (path, fw))
PYEOF

  echo "  Comprobando que el instalador no pueda borrar un guardado..."
  python3 tools/check_installer.py "$(basename "$manifest")" \
    || { echo "el instalador borraria guardados -- me niego"; exit 1; }
}

build_one TAMAPOKE_BOARD_175     175     web/manifest-175.json
build_one TAMAPOKE_BOARD_143     143     web/manifest-143.json
build_one TAMAPOKE_BOARD_28ROUND 28round web/manifest-28round.json

echo
echo "OK -> web/firmware/{175,143,28round}/ + web/manifest-{175,143,28round}.json"

echo "Empaquetando sprites..."
python3 tools/pack_bundle.py
