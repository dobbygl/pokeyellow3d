# Plan: Pokémon Crystal en 3D

Fecha: 2026-09-22. Estado: propuesto; ninguna fase iniciada.

## Evaluación de GB-Recomp/pokecrystal

Hechos verificados el 2026-09-22 con un clon del repositorio, su compilación
local y la API de GitHub:

- Repositorio `github.com/GB-Recomp/pokecrystal`, última actividad
  2026-05-15, mismo autor y mismas herramientas que `GB-Recomp/pokeyellow`,
  del que deriva este proyecto: `tools/regen.sh`, `launcher_main.cpp`,
  manifiesto de assets y `CMakeLists.txt` casi idéntico. Diferencias: exige
  CMake 3.16 en vez de 3.18, no tiene la rama MSVC que añadimos en la fase 5
  de `PLAN_API_CI.md`, y fija `GBRT_REF` a `main`.
- 287 MB clonado. C generado en 62 ficheros de funciones y 5 de despacho,
  frente a 34 y 3 en Amarillo; cada fichero ronda los 4 MB. Compila con el
  runtime sin cambios.
- ROM esperada: Crystal (UE) v1.1, SHA-1 `f2f52230…226cfb`, la misma que
  pret usa por defecto. La v1.0 y la versión australiana se rechazan. Un
  hack como CrystalComplete, basado en la v1.0 de 2014, queda fuera.
- `pokecrystal_internal.h` de 9 MB: 17.086 símbolos de función y 4.262 de
  WRAM con el prefijo `GB_ADDR_`. Cubre `wMapGroup`, `wMapNumber`,
  `wXCoord`, `wYCoord`, `wPlayerDirection`, `wTimeOfDay`, `wTilemap`,
  `wAttrmap`, `wMapTileset`, `wTilesetBank`, `wPokedexSeen`,
  `wPokedexCaught`, `wMenuCursorY` y `wHallOfFameCount`. **No** incluye
  `wBattleMode`, `wBattleMonHP`, `wPartyMon1`, `wObjectStructs`,
  `wMapObjects`, `wBGPals1` ni `wOverworldMapBlocks`: el generador solo
  exporta los símbolos que su análisis referencia. El repositorio tampoco
  incluye el `pokecrystal_metadata.json` que Amarillo sí tiene.
- Manifiesto de 170 secciones que el runtime extrae de la ROM: `maps`,
  `events`, `map_blocks_1–3`, `tileset_data_1–8`, `roofs`, `pics_1–19`,
  `pic_animations_1–3`, `sprites_1–2`, `pic_pointers`, `title`,
  `intro_logo`, fuentes y textos. Es la lista de lo que la capa 3D podrá
  leer de `ctx->rom`; lo que no esté en ella se ve como ceros.
- El runtime de GB-Recomp declara soporte de modo color, doble velocidad,
  bancos de VRAM y WRAM, HDMA y MBC3 con reloj, y cita Crystal como juego
  "funcionando bien". Su rama `main` no ha cambiado desde nuestro commit
  base `6581880`; nuestro fork va nueve commits por delante con la API de
  presentación, así que el cart de Crystal compila contra el fork sin
  conflicto.
- Crystal arranca en velocidad normal en Game Boy Color; `hCGB` solo
  decide la paleta. No exige modo color para ejecutarse.
- El README dice "straight into Red" por copia del de Rojo; no hay tests,
  CI ni ninguna evidencia publicada de partida completa. La afirmación de
  que funciona es del proyecto upstream y no se ha comprobado aquí porque
  no hay ROM de Crystal en esta máquina.

El resultado de la compilación local y del arranque sin ROM se anota en el
registro de ejecución al final.

## Análisis del estado actual de la capa 3D

De las 8.820 líneas de `src/`, con 303 direcciones únicas de Amarillo:

| Bloque | Líneas | Módulos | Situación frente a Crystal |
| --- | --- | --- | --- |
| Reutilizable | ~1.800 | Renderer, cámaras, primera persona, compositor LCD, tema, fuente y texto, efectos, preferencias, fundidos, API de presentación, CI y ROM sintética | Independiente del juego |
| Adaptar | ~4.100 | `pallet3d.cpp`, combate, Pokédex, PC, título, clasificador de disposiciones | Lógica válida; reglas por tile, rectángulos y paletas de Amarillo |
| Rehacer | ~2.900 | Lector de ROM, terreno, interiores, nombres, y todos los detectores de estado | Estructuras y direcciones de Gen 2 |

Diferencias de Gen 2 que condicionan el diseño, verificadas en pret:

- Mapas por grupos: `MapGroupPointers` apunta a cabeceras de 9 bytes
  (banco de atributos, tileset, entorno, puntero de atributos, localización,
  música, teléfono, paleta, grupo de pesca). Los atributos dan bloque de
  borde, alto, ancho, banco y puntero de bloques, scripts, eventos y la
  máscara de conexiones; cada conexión lleva mapa, puntero de bloques,
  destino en `wOverworldMapBlocks`, longitud, anchura y desplazamiento.
- Tilesets: `dba GFX, Meta, Coll; dw Anim; dw NULL; dw PalMap`. Los
  gráficos van comprimidos en LZ3 (ocho comandos, longitud de 5 o 10 bits,
  fin en `$ff`), los metatiles son bloques de 4×4 tiles y la colisión es
  un byte por tile del bloque.
- Colisiones semánticas: `COLL_FLOOR`, `COLL_WALL`, `COLL_WATER`,
  `COLL_TALL_GRASS`, `COLL_CUT_TREE`, `COLL_HEADBUTT_TREE`, `COLL_DOOR`,
  `COLL_LADDER`, `COLL_STAIRCASE`, `COLL_CAVE`, alfombras de warp,
  `COLL_COUNTER`, `COLL_BOOKSHELF`, `COLL_PC`, `COLL_TV`, `COLL_RADIO`,
  `COLL_MART_SHELF`, `COLL_WINDOW`, cornisas por dirección y muros por
  dirección. Sustituyen a la clasificación por índice de tile de Amarillo.
- Color: `wAttrmap` guarda paleta, volteo y banco por tile; los tilesets
  tienen mapa de paletas y las paletas cambian con `wTimeOfDay` (mañana,
  día, noche) y por mapa. Los fundidos son de paleta, no de BGP.
- Mundo vivo: `wOverworldMapBlocks` ocupa 1.300 bytes en WRAM0; el jugador
  es el objeto 0 de `wObjectStructs`; `wMapObjects` describe hasta 15 NPC
  con radio de movimiento y franjas horarias.
- Retratos: punteros por especie y por forma de Unown, con animaciones de
  frente en `pic_animations`; el descompresor es el mismo LZ3.
- Texto: `wTilemap` en `C4A0`; ocho marcos de cuadro de texto elegibles en
  opciones, así que el clasificador de disposiciones no puede fijar los
  tiles de borde como en Amarillo.

## Objetivo y alcance

Que `pokeyellow3d` pase a ser una aplicación con dos juegos, Amarillo y
Crystal, con la misma capa de presentación 3D, el mismo runtime y la misma
batería de pruebas. Crystal recibe, por este orden, el mundo exterior con
ambas cámaras, los interiores, los combates, los menús y las pantallas
propias. El motor recompilado sigue siendo la única autoridad y ningún
módulo escribe en memoria.

Quedan fuera: modelos 3D, Oro y Plata (aunque el perfil de Gen 2 los deja
al alcance), enlace por cable, el adaptador móvil, y cualquier cambio en
el C generado de ninguno de los dos juegos.

## Decisiones de arquitectura

1. Un perfil de juego por título en `src/game_profile.h`: direcciones,
   lector de ROM, clasificadores y anclas de detección detrás de una
   interfaz común. Amarillo se convierte en el primer perfil sin cambiar su
   comportamiento; es la refactorización que también necesita el punto 7
   de `PLAN_MEJORAS.md`.
2. El cart de Crystal entra por `FetchContent` de `GB-Recomp/pokecrystal`
   como `pokecrystal_cart`, siguiendo el patrón de `GB-Recomp/pgbcomp`. El
   launcher lista ambos juegos, verifica cada ROM por SHA-256 y activa el
   perfil según el juego lanzado. El repositorio no cambia de nombre en
   este plan; es una decisión aparte.
3. Los símbolos de Crystal salen de `pokecrystal.sym`, generado con la
   toolchain de pret, y se convierten a una cabecera de constantes
   `src/crystal_symbols.h` con un script del repositorio. No se dependerá
   del subconjunto exportado en `pokecrystal_internal.h`. La cabecera es
   derivada del código fuente público de pret, no de la ROM.
4. Lector de Gen 2 en `src/johto_rom.h`, con el mismo contrato que
   `kanto_rom.h`: catálogo de escenas, conexiones, orígenes, warps,
   tilesets y auditoría reproducible. Un descompresor LZ3 en
   `src/lz3.h` verificado byte a byte contra la VRAM que el motor carga.
5. El terreno y los interiores se clasifican por colisión semántica, con
   el índice de tile solo para retoques. Las familias del manifiesto del
   pase artístico se reutilizan.
6. El atlas se construye por tile con su paleta CGB real, leída del mapa
   de paletas del tileset y de las paletas de `wTimeOfDay`; el ciclo
   día/noche presentacional se sustituye en Crystal por la hora del juego.
7. Cada detector de estado se reescribe con la técnica de `live_return`
   sobre las rutinas de Gen 2, verificando la instrucción de llamada en
   ROM como hasta ahora. Toda pantalla no reconocida cae a la imagen
   original.
8. Las pruebas sin ROM añaden una ROM sintética de Gen 2 con un grupo de
   mapas, un tileset comprimido con LZ3 y colisiones semánticas. Las
   pruebas con ROM y savestates de Crystal quedan privadas, como las de
   Amarillo.

## Fase 0: perfil de juego sobre Amarillo

Trabajo:

- Definir `game_profile.h` con las direcciones, el lector, los
  clasificadores y las anclas que hoy usan `pallet_state.h`,
  `battle_state.h`, `fade_state.h`, `title_state.h`, `dex_state.h`,
  `pc_state.h`, `menu_state.h` y `battle_transition_state.h`.
- Mover las 303 constantes de Amarillo al perfil `yellow_profile.h` y
  hacer que todos los módulos lean del perfil activo.
- El launcher fija el perfil por juego; con un solo juego no cambia nada.

Criterios de aceptación:

- [ ] Ningún módulo de `src/` fuera de `yellow_profile.h` contiene direcciones de WRAM o de ROM de Amarillo.
- [ ] CTest completo, `ctest -LE rom` y las veinte baterías pasan; todas las capturas son idénticas byte a byte a la release anterior.
- [ ] `PALLET3D.md` documenta la interfaz del perfil y cómo añadir un juego.

## Fase 1: cart de Crystal, símbolos y arranque en 2D

Trabajo:

- `FetchContent` de `GB-Recomp/pokecrystal` fijado por commit, enlazado como
  `pokecrystal_cart`; el launcher añade la entrada con su SHA-256 y la ROM
  en `roms/pokecrystal.gbc`. Opción de CMake para excluirlo.
- Generar `pokecrystal.sym` con rgbds desde pret y convertirlo a
  `src/crystal_symbols.h` con `tools/symbols.py`; test que comprueba las
  direcciones conocidas de `pokecrystal_internal.h` contra la cabecera.
- Verificar que la ROM v1.1 arranca, extrae sus 170 secciones, llega a
  New Bark Town, guarda y mantiene el reloj tras reiniciar. Arranque
  headless en el helper con un modo `boot-crystal`.
- CI: ambos carts compilan; medir el tiempo y ajustar la caché.

Criterios de aceptación:

- [ ] El launcher ofrece los dos juegos, rechaza una ROM de Crystal v1.0 y arranca la v1.1 hasta el menú principal sin savestate.
- [ ] Una partida privada guarda, reinicia y conserva la hora del juego.
- [ ] CI compila los dos carts por debajo de veinte minutos con caché caliente y `ctest -LE rom` sigue pasando.

## Fase 2: lector de ROM de Gen 2 y auditoría

Trabajo:

- `johto_rom.h`: grupos, cabeceras, atributos, bloques, conexiones,
  eventos, tilesets, metatiles, colisiones, mapas de paletas y punteros de
  retratos, con comprobación de rangos y bancos como en Amarillo.
- `lz3.h`: descompresor con límites; test que compara cada tileset y
  veinte retratos con la VRAM cargada por el motor en savestates privados.
- Auditoría: todos los mapas de los 26 grupos, grafo de exteriores
  conectados desde New Bark Town y desde Pallet Town, orígenes y ciclos,
  CSV bajo `build/qa/logs/`.
- ROM sintética de Gen 2 en `tests/synthetic_rom_gen2.h` y tests sin ROM
  del lector, del LZ3 y de la colisión semántica.

Criterios de aceptación:

- [ ] La auditoría lista todos los mapas con dimensiones, tileset, conexiones y warps sin errores, y el grafo de Johto y el de Kanto no tienen ciclos contradictorios.
- [ ] LZ3 coincide byte a byte con la VRAM para todos los tilesets y al menos veinte retratos de tres tamaños.
- [ ] Los tests sintéticos de Gen 2 pasan sin ROM y quedan en CI.

## Fase 3: mundo exterior con ambas cámaras

Trabajo:

- Escenas de Johto y Kanto desde bloques y colisiones; casas por
  `roofs.bin` y bloques de tejado; terreno por `COLL_*`.
- Atlas CGB por tile y hora; `wOverworldMapBlocks` como estado vivo;
  jugador y NPC desde `wObjectStructs` con sus hojas de sprites y paletas.
- `view()` de Crystal: mapa y tileset cargados, fuera de combate, LCD y
  buffers válidos; fundidos por paleta en lugar de BGP; primera persona con
  la orientación de `wPlayerDirection` y el mismo mapeo relativo.
- Recorrido real: New Bark Town, Ruta 29, Cherrygrove, Ruta 30, un
  encuentro y regreso, en ambas cámaras.

Criterios de aceptación:

- [ ] Todos los exteriores de Johto y Kanto tienen suelo completo y mallas válidas; el catálogo los presenta sin errores GL.
- [ ] El recorrido pasa en ambas cámaras con memoria intacta por frame, incluidas las tres horas del día con sus paletas.
- [ ] Corte, surf, bicicleta y una conexión con desplazamiento lateral no producen presentación incompatible con el estado real.

## Fase 4: interiores y cuevas

Trabajo:

- Familias de mobiliario directamente desde `COLL_COUNTER`,
  `COLL_BOOKSHELF`, `COLL_PC`, `COLL_TV`, `COLL_RADIO`, `COLL_MART_SHELF`
  y `COLL_WINDOW`; paredes y escaleras por sus colisiones.
- Auditoría de todos los interiores alcanzables por warp, con la misma
  columna de casillas sin regla.
- Recorridos: casa del jugador, laboratorio de Elm, centro Pokémon y tienda
  de Cherrygrove, Cueva Oscura.

Criterios de aceptación:

- [ ] La auditoría decodifica todos los interiores; las casillas sin regla se registran y se presentan planas.
- [ ] Los cinco recorridos pasan en ambas cámaras con la escena correcta en cada frame.
- [ ] La caché de mallas sigue acotada al mapa actual y sus vecinos.

## Fase 5: combates

Trabajo:

- Detección por `wBattleMode` y `wBattleType`, con `DoBattleTransition`
  como ancla de entrada; exclusión de enlace y torre.
- Retratos desde VRAM con la animación de frente que el motor reproduce;
  HUD con los datos de `wEnemyMonHP`, `wBattleMonHP`, estado, experiencia
  y bolas de equipo; menús compuestos con `menu_text.h`.
- Efectos por categoría desde la tabla de movimientos de Gen 2, con el
  respaldo del LCD original para el resto.

Criterios de aceptación:

- [ ] Un encuentro salvaje en Ruta 29 y un combate contra el rival se juegan hasta el final en 3D con marcadores exactos.
- [ ] Cambios de Pokémon, debilitamiento y captura no dejan frames con retrato incorrecto.
- [ ] Todas las animaciones se presentan completas, en 3D o compuestas, sin frames en negro.

## Fase 6: menús, texto y pantallas propias

Trabajo:

- Clasificador de disposiciones que reconozca los ocho marcos de cuadro de
  texto; fuente de la ROM de Gen 2 en `rom_font`; estilo integrado con el
  mismo tema.
- Start, equipo, mochila, guardado, opciones, Pokédex, PC, Pokégear con
  mapa, radio y teléfono, y la pantalla de nombres.
- Título e intro de Crystal con la escena de New Bark Town.

Criterios de aceptación:

- [ ] Todas las disposiciones reconocidas se presentan con equivalencia glifo a glifo en los ocho marcos.
- [ ] Pokégear, Pokédex y PC mantienen la escena y son operables con los controles originales.
- [ ] El arranque desde ROM hasta el mapa se graba sin cortes.

## Fase 7: hora del juego, iluminación y release

Trabajo:

- El ciclo día/noche lee `wTimeOfDay` y las paletas del juego en Crystal;
  el reloj del sistema deja de intervenir. El pase artístico se aplica a
  las familias de Gen 2 a través del manifiesto.
- Regresión completa de Amarillo y de Crystal; release conjunta con ambos
  carts, Linux y Windows.

Criterios de aceptación:

- [ ] Mañana, día y noche de Crystal se revisan en New Bark Town, Ruta 29 y Cherrygrove en ambas cámaras.
- [ ] Todas las baterías de Amarillo siguen idénticas; las de Crystal pasan.
- [ ] La release publica un solo ejecutable con los dos juegos y sin ROM.

## Limitaciones asumidas

- Sin ROM de Crystal en la máquina de desarrollo no hay validación en
  tiempo de ejecución; la fase 1 empieza por conseguirla de forma privada.
- La afirmación de que el juego funciona es de GB-Recomp; los errores del
  recompilador que aparezcan se reportan upstream y no se parchean aquí.
- Los ocho marcos de texto, Unown, el adaptador móvil y la Torre de Batalla
  pueden quedar en imagen original si su detección no es fiable.
- Oro y Plata quedan fuera; el perfil de Gen 2 los deja a una tabla de
  direcciones de distancia.

## Validación y entrega

- Las baterías de Amarillo son la puerta de regresión de cada fase: nada
  cambia en sus capturas.
- Cada fase añade su batería `tests/crystal_*_qa.sh` con fixtures
  privadas, comprobación por frame de GL, memoria y modo presentado.
- Tests sin ROM con la ROM sintética de Gen 2 en CI.
- Entregar el ejecutable con ambos juegos, `PALLET3D.md` con la sección de
  Crystal, `README.md` con capturas y el registro de cada fase.
- Marcar las casillas solo con evidencia registrada.

## Orden recomendado

0, 1, 2, 3, 4, 5, 6, 7. La fase 0 puede hacerse antes de tener la ROM; la
fase 2 depende de la 1 solo para las comparaciones con VRAM.

## Referencias técnicas

- [GB-Recomp/pokecrystal](https://github.com/GB-Recomp/pokecrystal) y
  [GB-Recomp/pgbcomp](https://github.com/GB-Recomp/pgbcomp).
- [pret/pokecrystal](https://github.com/pret/pokecrystal): `data/maps/maps.asm`,
  `data/maps/attributes.asm`, `data/tilesets.asm`,
  `constants/collision_constants.asm`, `home/decompress.asm`,
  `home/double_speed.asm`, `ram/wram.asm`.
- `src/kanto_rom.h`, `src/mon_pic.h` y `src/battle_state.h` de este
  repositorio como modelos del lector, del descompresor verificado y de la
  detección por pila.

## Ejecución como goal

```text
/goal Ejecuta las fases 0 a 7 de /home/jgomez/Projects/jgomez/pokemon2/pokeyellow/PLAN_CRYSTAL_3D.md en orden. Amarillo no puede cambiar: sus capturas y estados siguen idénticos en cada fase. Crystal se integra como cart de GB-Recomp con perfil propio, símbolos de pret y lector de Gen 2 con LZ3 verificado contra VRAM. Marca las casillas solo con evidencia y actualiza el registro con comandos reproducibles.
```

## Registro de ejecución

### Evaluación previa, 2026-09-22

- Clon superficial de `GB-Recomp/pokecrystal` en `aec0f64`, configurado con
  Ninja y `MinSizeRel` sin tocar nada. Compila en 6 min 18 s con cuatro
  hilos, 104 pasos, un solo aviso del compilador y sin errores. Ejecutable
  `pokecrystal` de 605 KB, con el runtime de GB-Recomp `main`.
- `./pokecrystal --list-games` responde "Pokemon Crystal [pokecrystal]";
  sin ROM, el launcher gráfico cae a selección por terminal y termina con
  código 0. No había ROM de Crystal en esta máquina: no se ha verificado
  el arranque del juego, la extracción de las 170 secciones ni el reloj.
- Verificado que la rama `main` del runtime coincide con nuestro commit
  base `6581880` y que el fork `dobbygl/gb-recompiled` va nueve commits por
  delante: el cart se puede enlazar contra el fork con la API de
  presentación sin cambios en su C generado.
- Verificado en `pokecrystal_internal.h` que faltan `wBattleMode`,
  `wBattleMonHP`, `wPartyMon1`, `wObjectStructs`, `wMapObjects`,
  `wBGPals1` y `wOverworldMapBlocks`; la fase 1 genera los símbolos desde
  pret por ese motivo.
