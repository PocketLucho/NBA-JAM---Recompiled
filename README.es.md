# NBA JAM Recomp

*English: [README.md](README.md)*

La saga recreativa NBA JAM de Midway **recompilada desde su codigo fuente original** y
jugable en una sola aplicacion de Windows con menu de juegos, shaders CRT, modos
panoramicos y juego en red.

Creado por **Pocket_lucho** con **Claude** (Anthropic).
Si quieres apoyarme ve a https://buymeacoffee.com/pocket_lucho

| Juego | Placa | Estado |
|---|---|---|
| NBA JAM (1993), rev 3.01 | T-Unit | funciona con el romset de MAME (su codigo fuente nunca se publico) |
| NBA JAM Tournament Edition, rev 4.0 | T-Unit | **compilado desde la fuente: programa y graficos identicos byte a byte a la ROM original** |
| NBA JAM T.E. Special Edition, rev 5.02 (Asure, 2025) | T-Unit | **compilado desde la fuente: programa identico** |
| NBA Hangtime, L1.3 / L1.1 | Wolf Unit | funciona con las ROMs |
| NBA Maximum Hangtime, 1.3 | Wolf Unit | **compilado desde la fuente: programa identico** (tambien la 1.03 de MAME) |

## No incluye datos de los juegos

El codigo, los graficos, el sonido y la musica de Midway tienen derechos de autor, asi que
ni este repositorio ni las versiones publicadas los contienen. Cada usuario copia sus
propios ficheros en `roms/`:

* romsets de MAME: `nbajam.zip`, `nbajamte.zip`, `nbahangt.zip`, `nbamht.zip`
* y/o los ZIP de los repositorios publicos del codigo fuente ("Code → Download ZIP"):
  [historicalsource/nba-jam-tournament-edition](https://github.com/historicalsource/nba-jam-tournament-edition),
  [Asure/nba-jam-tournament-edition-SE](https://github.com/Asure/nba-jam-tournament-edition-SE),
  [historicalsource/nba-hangtime](https://github.com/historicalsource/nba-hangtime)

La aplicacion recorre `roms/` (subcarpetas y ZIP incluidos), reconoce cada fichero por su
CRC32 y extrae lo que necesita cada juego al arrancarlo. La lista por juego esta en
`dist/NBAJamRecomp/LEEME.txt`. Los juegos compilados con `tools/build_roms.sh` (abajo) van
a `data/<juego>/` y se usan si no se encuentran las ROMs.

## Caracteristicas

* Menu de juegos por fecha de salida, interfaz en castellano / ingles
* Shaders OpenGL 2.1 / GLES 2: nitido, suavizado, scanlines, CRT, CRT curvo; escalado
  entero (pixel perfect); 4:3, 16:9, 16:10 o llenar
* Juego en red hasta 4 jugadores: lockstep por UDP, con huella de ROMs y deteccion de
  desincronizacion
* Teclado (4 jugadores) y mandos, NVRAM por juego, capturas de pantalla (F12)

## Compilar la aplicacion

Windows (compilacion cruzada desde Linux con
[llvm-mingw](https://github.com/mstorsjo/llvm-mingw) y SDL2 2.30 para mingw):

```sh
make BUILD=build/win CXX=x86_64-w64-mingw32-g++ build/win/libcore.a
make win SDL2W=/ruta/a/SDL2-2.30.x/x86_64-w64-mingw32   # -> dist/NBAJamRecomp/NBAJamRecomp.exe
```

Linux: `make linux-app SDL2L=/ruta/a/sdl2-estatico` → `build/linux/NBAJamRecomp` (x86-64, glibc 2.38+; SDL2 y libstdc++ enlazados estaticamente).

## Compilar los juegos desde la fuente

Necesita bash, python3 y emu2 con `tools/emu2-ems.patch` (Linux, o WSL en Windows):

```sh
git clone https://github.com/historicalsource/nba-jam-tournament-edition repos/nba-jam-tournament-edition
git clone https://github.com/Asure/nba-jam-tournament-edition-SE        repos/nba-jam-tournament-edition-SE
git clone https://github.com/historicalsource/nba-hangtime              repos/nba-hangtime
tools/build_roms.sh repos /ruta/a/emu2 dist/NBAJamRecomp/data
```

El script ensambla y enlaza cada juego con `gspasm`, ejecuta el `LOAD2.EXE` de Midway para
empaquetar el arte, compara el resultado con las ROMs originales y escribe `data/<juego>/`.

### Que hizo falta para obtener ROMs identicas

* **gspasm** reproduce las rarezas de GSPA: la precedencia de operadores de TI (`| ^` como
  `+ -`, `& << >>` como `*`), los argumentos de macro sobrantes van al ultimo parametro y el
  relleno `0x0000FFFF` de GSPLNK.
* **Empaquetado del arte de la T.E.** - en 1994 se uso un LOAD2 anterior que daba a cada
  imagen los bits por pixel de su paleta; el LOAD2 de 1995 de los fuentes usa el minimo.
  `lod_palbpp.py` anade lineas `PPP> n` a los LOD (si no, los rotulos salen como ruido).
* **Imagenes estrechas** - el LOAD2 de 1995 rellena hasta 3 pixeles de ancho;
  `fix_narrow.py` quita la columna de relleno y recoloca las direcciones posteriores.
* **Segundo punto de animacion** - el LOAD2 de 1995 escribe `-1` en `PWRD1..3` (donde se
  sujeta el balon); `fix_pwrd.py` los toma de los `.IMG`. Sin esto el balon flota y no bota.

## Licencia

El codigo de este proyecto se publica con licencia BSD de 3 clausulas (ver `LICENSE`).
Codigo de terceros: nucleos de CPU de MAME e ymfm (BSD 3 clausulas), miniz (MIT / Unlicense),
font8x8 (dominio publico), SDL2 (zlib). NBA JAM, NBA Hangtime y Midway son marcas de sus
propietarios; es un proyecto de aficionados sin animo de lucro y sin relacion con ellos.

## Creditos

* Midway Games: los juegos originales y el codigo fuente publicado por
  [historicalsource](https://github.com/historicalsource)
* [Asure](https://github.com/Asure): NBA JAM T.E. Special Edition y sus fuentes
* El equipo de MAME, Aaron Giles (ymfm), dmsc (emu2)
