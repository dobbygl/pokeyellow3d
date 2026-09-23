# Kanto en 3D

Presentación 3D para la ROM inglesa UE de Pokémon Amarillo, SHA-1
`cc7d03262ebfaf2f06772c1a480c7d9d5f4a38e1`. El juego recompilado conserva
movimiento, colisiones, encuentros, combates, historia y guardado.

## Ejecutar

Los [paquetes v0.2.0 para Linux y Windows x86_64](https://github.com/dobbygl/pokeyellow3d/releases/tag/v0.2.0) incluyen el launcher. Descomprime el ZIP completo, coloca tu ROM en `roms/pokeyellow.gbc` junto al ejecutable y sigue `RUN.md`. En Linux, abre un terminal en esa carpeta y ejecuta `./pokeyellow3d`; requiere las bibliotecas de Ubuntu 24.04 indicadas en `DEPENDENCIES.txt`. En Windows 10/11, usa `Start.cmd`; las DLL de SDL2, CURL, ANGLE y MSVC van incluidas. Cada ZIP tiene un archivo `.sha256`. No incluyen ROM, recursos extraídos ni partida. Para la compilación local:

```sh
cd build
./pokeyellow3d
```

Usa `build/roms/pokeyellow.gbc`, los recursos extraídos y `build/pokeyellow.sav`.
Cierra otra instancia del juego antes de jugar para evitar escrituras simultáneas
sobre la misma partida.

El 3D aparece automáticamente en los **36 mapas exteriores conectados** de Kanto,
**Bosque Verde**, **muelle de Carmín** y las **cuatro zonas exteriores de Safari**
accesibles por warp. Las conexiones de borde mantienen una
posición mundial continua; los exteriores separados tienen su propia escena.
F3 alterna entre la cámara ortográfica y la primera persona. Los interiores se
cargan bajo demanda: se generan los 179 interiores alcanzables. Están verificados
recorridos por la casa, el laboratorio, centro Pokémon, tienda, escaleras,
ascensor y cueva. Las salas pequeñas se encuadran completas; las grandes permiten
giro limitado y zoom. También admiten primera persona, con techo y niebla oscura
en cuevas. El mobiliario es una interpretación: 2.960 casillas sin clasificación
artística conservan su textura plana y se registran en el CSV.

Los combates normales tienen una escena 3D con retratos de VRAM, paletas
originales, vida, estado y experiencia. Incluye cambios de equipo, presentación
del entrenador, efectos de golpe, proyectil, estado y mejora propia, además de
trayectoria y sacudidas de Poké Ball. El daño provoca sacudida y parpadeo; la
barra de vida interpola las lecturas del motor. Texto y menús conservan el
framebuffer original. Los movimientos complejos conservan su animación completa
mediante una composición con fundido, sin duplicar los retratos. Link, tutorial,
Safari y estados no reconocidos siguen en
2D. Los cuadros de diálogo inferiores reconocidos se superponen al mundo
3D en ambas cámaras, con el HUD oculto mientras se lee. Los warps reconocidos
conservan la escena saliente y siguen el fundido BGP del motor; la carga de un
savestate en otro mapa hace un fundido breve a negro. El título 3D, los menús
y las transiciones especiales están verificados y registrados en
`PLAN_MENUS_TITULO_TRANSICIONES.md`.

| Tecla | Acción |
| --- | --- |
| F2 | Alternar presentación 2D / 3D |
| F3 | Alternar cámara ortográfica / primera persona |
| W, en primera persona | Avanzar hacia donde mira el jugador |
| A / D, en primera persona | Girar 90° a izquierda / derecha sin avanzar |
| S, en primera persona | Dar media vuelta sin avanzar |
| Q / E, en cámara ortográfica | Girar cámara dentro del arco frontal |
| Rueda, en cámara ortográfica | Zoom |
| R, en cámara ortográfica | Restablecer cámara |
| Flechas | Movimiento original, según los ejes del mapa |
| WASD fuera de primera persona | Mando original: arriba, izquierda, abajo, derecha |
| Z / J | A: hablar / confirmar |
| X / K | B: cancelar |
| Enter | Menú original |
| Esc | Ajustes de la aplicación |

## Pase artístico

Esc → **Pase artistico** activa las copas facetadas de tres capas (cuatro en
árboles altos), rocas, tejados a dos o cuatro aguas, marcos de ventanas, vallas
de dos travesaños y bordes de terreno. Silph, el centro comercial de Azulona y
el gimnasio de Plateada conservan tejados planos. Los árboles varían hasta un
10 % según su posición; cargar una partida reproduce la misma forma. Corte
retira la copa y deja una marca rasa del tocón.

El ajuste también habilita las sombras direccionales cuando hay luz solar.
Al desactivarlo se recupera la geometría de referencia. No modifica el estado
del motor, sus colisiones ni la altura del suelo transitable. Las mejoras de
interiores y materiales de las siguientes fases se describen en
[PLAN_PASE_ARTISTICO.md](PLAN_PASE_ARTISTICO.md).

## Compilar y probar

```sh
cmake -S . -B build -G Ninja -DPOKEYELLOW_3D=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

Se necesitan las dependencias habituales del proyecto, SDL2 y OpenGL ES 2.
La revisión inmutable del runtime se declara en `CMakeLists.txt`. Si una
compilación anterior fijó otro runtime, elimina las opciones de caché con
`-U GBRT_REF -U GBRT_URL -U FETCHCONTENT_SOURCE_DIR_GB_RECOMPILED` al configurar.
`POKEYELLOW_3D=OFF` produce el ejecutable original `pokeyellow`.
CTest registra pruebas de controles, menús, fundidos y escenarios sintéticos.
Al configurar con la ROM local en `build/roms/pokeyellow.gbc` añade las
comprobaciones del cartucho. La equivalencia de retratos con VRAM requiere
generar primero la evidencia privada con `tests/dex_portraits_qa.sh`.
`ctest --test-dir build -LE rom --output-on-failure` ejecuta el grupo sin ROM;
no se distribuye esa ROM.

Windows/MSVC también compila y pasa los 32 tests sin ROM, incluido el renderer
sintético con el controlador Windows de SDL2 y ANGLE. Los comandos de compilación
con las dependencias fijadas están en [README.md](README.md#building-on-windows).
Los recorridos con ROM y las comparaciones de capturas se validan en Linux/Mesa;
la CI no equivale a una partida completa en Windows.

Para repetir la integración completa con las fixtures locales verificadas:

```sh
tests/world_qa.sh build/roms/pokeyellow.gbc \
  build/qa/progress.state10 build/qa/fixture.state1
```

El script crea una carpeta nueva `build/qa/kanto-XXXXXX`, copia el ejecutable,
ROM y estados y ejecuta las pruebas allí. No invoca el guardado de batería.
Conserva logs, CSV, capturas PPM y estados intermedios privados. La primera
fixture empieza en Paleta (9,7), con Pikachu sano y listo para combatir; la segunda,
opcional, en Paleta (9,8), permite repetir el recorrido de menú/pared/casa.
Las secuencias de menús están verificadas con estas fixtures; otros estados pueden
tener otra selección de menú, eventos o RNG y requerir ajustar el recorrido.

Para repetir la integración de primera persona:

```sh
tests/firstperson_qa.sh build/roms/pokeyellow.gbc \
  build/qa/progress.state10 build/qa/fixture.state1 build/qa/first-battle.state
```

La tercera fixture está en Ruta 1 (10,28), fuera de combate. El script crea
`build/qa/firstperson-XXXXXX`, conserva hashes de las entradas, comprueba el mismo
recorrido en ambas cámaras y guarda ocho vistas cardinales, el diálogo y las
mediciones con cinco mapas. El helper no guarda batería. La preparación de
Azafrán usa un warp del motor únicamente en la copia privada de prueba.

Para repetir la batería de interiores A1/A2:

```sh
tests/interiors_qa.sh build/roms/pokeyellow.gbc \
  build/qa/progress.state10 build/qa/fixture.state1 \
  build/qa/kanto-0dH5RK/viridian.state
```

La última fixture es Ciudad Verde (20,33), generada por `world_qa.sh`.
La batería guarda el CSV, las 179 capturas de catálogo y los recorridos en
`build/qa/interiors-XXXXXX/`. `town` cura, resuelve la entrega del paquete si
está pendiente, compra diez Poké Balls y vuelve a Ruta 1; `interior-transitions`
comprueba escaleras, ascensor y cueva. Todos usan los controles del motor y
verifican GL, selección de escena, caché y WRAM/VRAM/framebuffer por frame.
La preparación de mapas lejanos usa únicamente copias privadas.

El auditor también se puede ejecutar por separado:

```sh
build/interior_audit build/roms/pokeyellow.gbc > build/qa/logs/interiors.csv
```

`unknown_graphics` detecta índices inválidos; `unclassified_flat_cells` registra
gráficos válidos que aún no tienen regla artística. Son medidas distintas.

Para repetir la integración de combates B1/B2:

```sh
tests/battles_qa.sh build/roms/pokeyellow.gbc \
  build/qa/interiors-fJDbZk/logs/town-route1.state \
  build/qa/firstperson-9cB3FJ/route.state
```

La primera fixture está en Ruta 1 (10,4), con las diez Poké Balls compradas por
`town`; la segunda en Ruta 1 (10,28). Se copian ROM, ejecutable y estados a
`build/qa/battles-XXXXXX/`. El modo `battle3d` encadena captura, cambios de equipo,
curación, aproximación al rival de Ruta 22, combate de entrenador y efectos.
El helper contrasta los retratos subidos a GPU con VRAM y comprueba GL y toda
la WRAM/VRAM/framebuffer después de cada presentación. No guarda batería.

`battle-effects` concede exclusivamente en sus copias privadas Placaje,
Impactrueno, Gruñido, Agilidad y Vuelo para probar las cuatro categorías y el
respaldo original. Los ataques se ejecutan mediante los menús del juego. La
captura y el combate del rival usan el equipo obtenido por el motor, sin
conceder Pokémon, cambiar el RNG ni decidir el resultado. Se guardan capturas,
trazas de efectos y estados para repetir los casos.

La tabla de 165 movimientos se lee de la ROM. Las pruebas unitarias comprueban
todos sus registros y la conservación del compositor durante la limpieza de
la animación; las pruebas jugables ejercitan las cuatro categorías y Vuelo,
con comparación exacta de los 23.040 píxeles del LCD para el respaldo opaco.
Esto no equivale a haber jugado 165 combates distintos.

## Datos y arquitectura

- `src/kanto_rom.h`: lector de bancos, headers, conexiones, bloques, tilesets,
  warps, señales y objetos, con límites y rechazo de datos inválidos. Calcula el
  grafo de 36 mapas: 34 Overworld y dos Plateau. Bosque Verde (Forest) y muelle
  (ShipPort) se añaden como componentes separados.
- `src/interior_scene.h`: clasificación de paredes, muebles y suelo. Los warps
  permanecen planos. Cada interior tiene componente propio, un atlas de su
  tileset y cámara independiente del exterior.
- `src/battle_state.h`: selección de combate, retratos por columnas desde
  `wTileMap` y VRAM, paletas de especie, HP, nivel, estado y experiencia.
  `wAnimationID=0xD07B` y el contador `0xD086` tienen alias: su valor aislado
  no indica una animación. Se verifican los retornos CALL vivos del motor y
  sus operandos ROM antes de usar los efectos o la composición original.
- `src/battle3d.h`: arena independiente, dos plataformas, billboards y marcadores
  ImGui. Usa su propia textura y cámara; no modifica las mallas del mundo.
  Mantiene los retratos durante efectos que borran temporalmente el tilemap;
  los invalida al cambiar de especie, salir del combate o retroceder el reloj.
  Relee el terreno al cargar un combate de otro mapa o posición. Agilidad
  añade una estela visual limitada a 0,55 s porque el destello original dura
  dos frames; no retrasa el motor. Las fases de captura siguen sus contadores.
- `src/world_scene.h`: catálogo, clasificación visual por tileset e inferencia
  de edificios. Hay 140 volúmenes; los patrones de borde de tejado separan
  edificios contiguos. Torre Pokémon y sede de la Liga tienen correcciones
  acotadas; Silph, el centro comercial y el gimnasio de Plateada tienen alturas
  particulares. Las puertas frontales se auditan contra el borde de fachada.
- `src/pallet_state.h`: lectura de WRAM, selección de vista, interpolación y
  actores reales. Comprueba dimensiones, tileset, estado visual y validez del
  búfer de bloques antes de activar 3D. Los objetos ocultos siguen ocultos.
- `src/pallet3d.cpp`: atlas compartido de 512×512 (1 MiB RGBA), cuatro tilesets
  con variantes de paleta y franja de sprites desde VRAM. Mallas estáticas por
  mapa; solo quedan residentes el actual y sus vecinos directos, como máximo
  cinco. Los actores se actualizan por separado cada frame.
- La ROM aporta la base del mundo; el mapa activo usa los bloques vivos de WRAM.
  Corte y los cambios de bloques invalidan su malla. Las fachadas también leen
  esos bloques. Una carga de estado se detecta por su contenido y reconstruye
  lo necesario; se espera un frame de estabilidad para no usar el búfer anterior
  durante un cambio de header.
- La cámara sigue coordenadas mundiales y conserva giro/zoom en conexiones;
  reajusta el foco al cambiar de componente o saltar una distancia grande.
  Una silueta dorada permite localizar al jugador detrás de un tejado.
- `src/firstperson.h`: matriz ortográfica equivalente y perspectiva de 70°,
  ojo a 1,1 casillas, planos 0,1–96 e interpolación de orientación de 150 ms.
  La posición sigue exactamente la interpolación del motor; al volver de 2D
  se adopta su orientación en el primer frame. El jugador y su silueta se
  omiten en primera persona; los demás sprites se orientan hacia el ojo y
  apoyan su último píxel visible en el suelo.
- El mapeador relativo dispone de su propia máscara activa a cero, combinada
  con mando manual y scripts mediante AND. Los giros esperan reposo del motor,
  emiten como máximo dos frames efectivos (140.448 ciclos) y se liberan cuando
  el motor confirma la orientación. Se ignoran órdenes repetidas mientras hay
  un giro pendiente. Diálogos, menús, pérdida de foco y F2 neutralizan la máscara.
  La grabación recibe las direcciones resultantes; los scripts mantienen su
  canal original. No se escriben coordenadas ni orientación en WRAM.
- Ambas cámaras comparten mallas y shader. Solo en primera persona hay cielo,
  niebla y repetición de tiles en laterales, hojas y rocas, sin aumentar vértices
  ni llamadas de dibujo. El salto añade hasta 0,14 unidades de balanceo a partir
  de `wMovementFlags` y del índice de salto del juego, sin cambiar el suelo.
- Los píxeles de sprites solo se transfieren a GPU cuando cambian. Cuando el
  renderer 3D ya está inicializado y va a cubrir un exterior, la adaptación SDL
  evita subir y dibujar el framebuffer 2D que se descartaría. El juego sigue
  produciendo ese framebuffer. Los diálogos, combates y warps soportados se
  componen en el renderer; inicialización, F2 y pantallas aún no reconocidas
  conservan la ruta original.
- `cmake/Pallet3D.cmake` comprueba la versión de `gb_presentation.h` y añade
  el renderer y su adaptador. El launcher registra los callbacks antes de
  arrancar SDL. No genera copias del frontend ni modifica fuentes descargadas.
  El runtime se fija a una revisión inmutable de `dobbygl/gb-recompiled` en `CMakeLists.txt`. La contribución a upstream está en [GB-Recomp/gb-recompiled#2](https://github.com/GB-Recomp/gb-recompiled/pull/2).

Las listas de colisión, hierba, cornisas y pares de tiles se leen de la ROM.
La colisión del juego sigue siendo la única autoridad. Las cornisas originales
solo se aplican a Overworld. Las alturas y los colores son una interpretación
artística; no existe una segunda simulación de física.

## Cobertura comprobada

| Prueba | Qué verifica |
| --- | --- |
| `kanto_rom_audit ROM` | 36+2 mapas, distribución 34/2, ciclos, offsets, recursos residentes y rechazo de corrupción |
| `kanto_geometry_audit ROM` | 140 edificios válidos, puertas alineadas y cobertura de terreno; CSV de gráficos sólidos conservados en plano |
| `pallet_state_test ROM` | Escenas, transiciones, bloques inválidos, movimiento, actores y memoria de solo lectura |
| `viridian` | Paleta → Ruta 1 → Ciudad Verde y vuelta, cornisas, cinco victorias reales, continuidad y caché estable |
| `prepare horizontal` | Ciudad Verde ↔ Ruta 22, conexión horizontal y continuidad de origen |
| `route` / `camera` | Menú, pared, entrada a casa, interior 3D, F2, giro y zoom |
| Corte y `reload` | Corte desde el menú original elimina el árbol; cargar el estado previo lo restaura y solo reconstruye una malla |
| Bicicleta / surf | Activación por menús originales, movimiento y desembarco automático al volver a tierra |
| Bosque / muelle | Bosque ↔ acceso 2D; muelle ↔ Carmín y entrada al barco en 2D |
| `catalog` | Construcción y presentación de las 38 mallas, expulsión de caché, capturas y rendimiento |
| `firstperson_test` | Matrices, giro suave, direcciones relativas, duración del toque y liberación del mando |
| `firstperson` / `journey` | Mismo recorrido y estado final en ambas cámaras; salto, combate y retorno inmediato a primera persona |
| `fp-controls` | Eventos SDL reales: W, giros, repetición rápida, flechas, Esc, menú, F2 y lectura del cartel con Z |
| `fp-house` / `fp-views` | Primer frame tras salir de una casa y cuatro orientaciones sin mover al jugador |
| `fp-benchmark` / `ortho-benchmark` | Siete rondas de 100 presentaciones con cinco mapas residentes, OpenGL y WRAM |

El recorrido a Ciudad Verde usa únicamente controles normales: no cambia equipo,
mapas, RNG ni colisiones. Resultado: 12.445 frames, 3.995 presentaciones 3D,
8.255 frames de combate y cuatro cruces de frontera. En cada frame se comprueba
la selección de vista, OpenGL, el límite de caché y que dibujar no modifica WRAM.

Las pruebas tardías usan **preparación explícita de fixtures exclusivamente en
el helper**: conceden los movimientos/medallas correspondientes, bicicleta o
billete; los escenarios separados se cargan mediante el warp por script del
motor. Después se realizan los movimientos, menús y cruces con el juego original.
No representan una partida completa conseguida jugando ni son funciones del
renderer. No se incorpora ninguno de estos controles al ejecutable del juego.

Se han revisado las capturas de todos los mapas y, con más detalle, Paleta,
Ciudad Verde, Plateada, Azulona, Azafrán, Meseta Añil, Bosque Verde y muelle.
El auditor distingue una puerta frontal de una cueva, entrada lateral o warp
entre escenas: los dos accesos de Ruta 23 son portales, no casas.

## Rendimiento medido

Intel UHD Graphics 620, Mesa, superficie 800×720, OpenGL real mediante SDL
`offscreen`. Treinta presentaciones sincronizadas con `glFinish` por escena,
tras calentarla; 1.140 muestras en total. Es coste de presentación de la vista
completa de cada mapa, sin avanzar la CPU del juego; no es FPS de la partida.

| Escena | Construcción de malla | Presentación media | Vértices en GPU, bytes |
| --- | ---: | ---: | ---: |
| Paleta | 2,44 ms | 1,72 ms | 545.832 |
| Ciudad Verde | 9,70 ms | 2,62 ms | 3.699.000 |
| Azulona | 2,86 ms | 2,53 ms | 3.972.456 |
| Ruta 17 | 2,38 ms | 1,94 ms | 3.323.160 |
| Bosque Verde | 5,43 ms | 2,91 ms | 4.846.392 |
| Muelle | 0,52 ms | 1,59 ms | 291.384 |

Medición inicial en `build/qa/logs/final-catalog.log`. El script completo repite
la medición y conserva su propio log. Los tiempos dependen del equipo y carga.

Primera persona, versión final: cinco mapas residentes (Azafrán y vecinos),
204.360 vértices y 7.356.960 bytes de mallas; siete rondas de 100 presentaciones
sin avanzar el motor, con `glFinish`. Mediana **3,639 ms** en primera persona y
**3,377 ms** en ortográfica. Son tiempos del renderer, no FPS de la partida.
Logs: `build/qa/firstperson-CWqk6R/logs/benchmark-*.log`.

Comparación con el ejecutable anterior a primera persona: cuatro pares alternados
de 200 presentaciones por cada una de las 38 escenas, sin otras pruebas gráficas
ejecutándose a la vez. La media ortográfica pasa de **2,104 a 1,423 ms**; todas
las medianas por mapa mejoran (17,74–44,89 %), sin cambiar vértices ni bytes de
geometría. Datos completos en
`build/qa/firstperson/logs/performance-release/comparison.json`.

## Diagnóstico y límites

Desde una carpeta QA que contenga una copia del helper:

```sh
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  SMOKE_CAPTURE=ciudad.ppm \
  ./pallet_render_smoke roms/pokeyellow.gbc viridian.state camera
```

`play SCRIPT FRAMES [OUTPUT_STATE]` reproduce entradas; `inspect` muestra el mapa,
modo de transporte y diferencias de bloques; `capture2d` captura un estado 2D.
`catalog` usa la misma geometría con una cámara de revisión y sin inventar NPC ni
escribir RAM. `CATALOG_CAPTURE_DIR` guarda cada mapa y `CATALOG_BENCH_FRAMES`
activa la medición. `PALLET3D_TRACE=1` registra cambios de vista y coordenadas;
`PALLET3D_CAPTURE=ruta.ppm` captura el juego normal después de 30 frames 3D.

- La ampliación de interiores y combates está validada con el alcance y las
  evidencias de `PLAN_INTERIORES_COMBATES.md`. No incluye cámara libre ni alturas transitables nuevas. Primera persona conserva movimiento por
  casillas y cuatro direcciones de interacción; el ratón no gira al jugador.
- Los cuadros inferiores con borde completo y mapa visible detrás conservan
  el 3D en ambas cámaras: `lcd_overlay.h` compone las filas 96–143 del
  framebuffer original, con escala entera y caché de las regiones subidas.
  Con una escena residente, Start y las ventanas reconocidas se superponen
  al mundo. El LCD completo sobre la escena atenuada y desenfocada sigue
  disponible como respaldo. Los perfiles de PC, equipo, mochila y Pokédex
  descritos más abajo añaden sus presentaciones especializadas; las pantallas
  no reconocidas conservan todo el contenido original.
  Las teclas son las originales mientras hay texto. Volver al exterior restaura
  la preferencia de cámara, sin pulsaciones relativas retenidas.
- Pikachu puede ocupar gran parte de la vista al estar justo al lado del jugador.
  Árboles, rocas y laterales tienen geometría sencilla; las alturas son visuales.
  Las cornisas no cambian de nivel real: se representan con un balanceo de cámara.
- Agua y flores siguen la animación original, según el apartado 5A. Las playas,
  detalles sin clasificación
  volumétrica, la cubierta del muelle y el barco conservan sus tiles originales
  sobre el plano; su cobertura se registra en el CSV. No se ocultan gráficos
  desconocidos ni se deducen colisiones de su aspecto.
- Los NPC de mapas vecinos no se simulan. Fuera de la pantalla original, los NPC
  activos usan su posición/orientación disponibles y la animación que actualice
  el motor. No se ha jugado toda la historia de principio a fin: hay auditoría
  completa de geometría y recorridos representativos del motor.
- La integración SDL usa la API versionada del fork. La contribución upstream
  y la validación de portabilidad siguen en `PLAN_API_CI.md`.

Véanse [PLAN_KANTO_3D.md](PLAN_KANTO_3D.md),
[PLAN_PRIMERA_PERSONA.md](PLAN_PRIMERA_PERSONA.md) y
[PLAN_INTERIORES_COMBATES.md](PLAN_INTERIORES_COMBATES.md) para criterios y evidencias. Fuentes:
[headers](https://github.com/pret/pokeyellow/blob/master/macros/scripts/maps.asm),
[memoria](https://github.com/pret/pokeyellow/blob/master/ram/wram.asm),
[Corte](https://github.com/pret/pokeyellow/blob/master/engine/overworld/cut.asm),
[movimiento](https://github.com/pret/pokeyellow/blob/master/home/overworld.asm),
[sprites](https://github.com/pret/pokeyellow/blob/master/engine/gfx/sprite_oam.asm).

### Fundidos de mapa (C1)

`src/fade_state.h` identifica las llamadas vivas de warp/carga en la pila y
comprueba sus instrucciones en la ROM. `WorldFrame` conserva la matriz de
cámara; las mallas residentes, los actores y su textura se dibujan sin releer
los bloques o sprites del destino mientras se carga. El HUD y el contorno del
jugador a través del tejado se ocultan durante la transición.

El shader aplica `color × multiplicador + suma`, también al cielo y al fondo.
Se usa la luminancia media de los cuatro tonos BGP, con peso 1/4 para cada uno:

| BGP | Multiplicador | Suma | Presentación |
| --- | ---: | ---: | --- |
| E4 | 1 | 0 | Escena normal |
| F9 | 1/2 | 0 | Fundido a negro |
| FE | 1/6 | 0 | Fundido a negro |
| FF | 0 | 0 | Negro completo |
| 90 | 1/2 | 1/2 | Fundido a blanco |
| 40 | 1/6 | 5/6 | Fundido a blanco |
| 00 | 0 | 1 | Blanco completo |

Los pasos F9/FE de las puertas y 40/90 de la vuelta desde blanco duran nueve
frames en las trazas del runtime. Se lee BGP en cada frame; no se programa
esa duración en el renderer. En puertas el motor restaura directamente E4
tras el negro. La carga de un savestate es distinta: no tiene una curva del
motor, por lo que el callback de carga correcta inicia dos mitades nominales
de 90 ms y cambia la escena en un frame completamente negro. La preparación
de texturas/mallas se añade a ese tiempo; el avance de la animación se limita
por presentación para que una espera del driver no suprima los pasos visibles
de la entrada. Las pruebas con caché fría tardan aproximadamente 300–350 ms.

Reproducir con fixtures privadas (Paleta 9,8; centro comercial 1F 2,7; Ruta 1
10,28 con equipo):

```sh
tests/ui_transitions_qa.sh build/roms/pokeyellow.gbc \
  build/qa/interiors-fJDbZk/house.state \
  build/qa/interiors-wt5ABt/mart-start.state \
  build/qa/firstperson-9cB3FJ/route.state
```

El script copia las entradas a una carpeta nueva, graba vídeos con `ffmpeg`,
compara el brillo de los píxeles y comprueba cámara, mallas, GL, cobertura y
memoria por frame. `check_ui_traces.py` comprueba las duraciones medidas.
Esto cubre fundidos de mapa y la vuelta desde blanco. La entrada en combate
se describe en C2 más abajo; los viajes de Vuelo, Teletransporte y Excavar se
comprueban en la batería específica de C3.

### Menús del mundo (A2/A3)

`menu_state.h` mantiene la escena mediante llamadas de texto vivas verificadas
en la ROM. `menu_layout.h` clasifica los bordes visibles, incluidos sus
solapamientos; lo desconocido conserva el framebuffer completo dentro del
marco. `scene_filter.h` aplica desenfoque y atenuación al fondo sin reconstruir
la malla ni leer los buffers de mapa que usan las pantallas de equipo y PC.
La escala del LCD es entera; a 800×720, las pantallas enmarcadas usan 3×.

La batería privada de menús se reproduce con:

```sh
tests/ui_menus_qa.sh build/roms/pokeyellow.gbc \
  build/qa/battles-eovzS8/logs/capture-complete.state
```

Requiere un estado del mundo con Pokédex, encargo entregado, Pikachu y otro
Pokémon capturado, y al menos 200 de dinero. Comprueba Start, resumen y orden
del equipo, mochila, ficha, opciones, Pokédex, guardado, curación, PC y compra
en ambas cámaras, además de renombrar un Pokémon mediante el inspector de
motes. Verifica cada píxel de las regiones compuestas y la ausencia
de escrituras en WRAM/VRAM/framebuffer, errores GL y reconstrucciones durante
pantallas completas. La partida original no se guarda ni sobrescribe.
El registro de `PLAN_MENUS_TITULO_TRANSICIONES.md` distingue las pruebas de
desarrollo de la validación final y sus regresiones.

La siguiente tabla describe la composición clásica. Las disposiciones que
añade el estilo integrado figuran en [Estilo de menús](#estilo-de-menús-clásico-e-integrado).

| Situación | Presentación |
| --- | --- |
| Mundo exterior e interiores compatibles | 3D, ortográfica o primera persona |
| Texto inferior, Start, guardar, curar y comprar | Ventanas originales sobre el mundo 3D |
| Equipo, resumen, mochila, ficha, opciones y nombres desde el mundo | LCD original enmarcado sobre la última escena atenuada y desenfocada |
| PC del Centro Pokémon y dormitorio | Acercamiento al monitor; menú principal sobre su pantalla y submenús originales sobre la escena atenuada |
| Ficha de datos de la Pokédex desde su lista | Dispositivo rojo 3D, texto original y retrato verificado; fase A1 completada |
| Lista de la Pokédex | Dispositivo 3D con retrato, silueta o pantalla vacía según capturado/visto/ausente; fase A2 completada |
| AREA de la Pokédex | Vista del catálogo de Kanto con nidos originales, parpadeo y texto del juego |
| Disposición de menú desconocida con escena residente | Mismo encuadre completo; no se descarta texto |
| Savestate de menú completo cargado sin escena previa | LCD original hasta disponer de una escena válida |
| Combate normal y presentación del entrenador | Arena 3D y compositor de combate existente |
| Animación compleja de combate | Animación LCD original sobre la arena |
| Puertas, escaleras y ascensor reconocidos | Fundido 3D derivado del BGP del motor |
| Cargar un savestate de otro mapa | Fundido breve a negro de la presentación |
| Título | Paleta sin actores, cámara lenta al atardecer, logo original y Pikachu decodificado de VRAM |
| Continuar, Nueva partida y nombres iniciales | LCD original sobre Paleta atenuada y desenfocada |
| Copyright, Game Freak, intro de Pikachu, link, tutorial y Safari | LCD original |
| Entrada en combate normal | Destellos BGP y acercamiento con desenfoque radial; arena desde el primer cuadro original |
| Equipo y mochila durante el combate | LCD enmarcado sobre la arena atenuada y desenfocada |
| Salida de combate normal | Fin de combate y fundido del mundo compuestos sin hueco 2D |
| F2 manual | Fundido de 200 ms entre frames completos, reversible y congelado durante la pausa |

### Entrada y salida de combate (C2)

El detector verifica las llamadas originales de entrada y fin de combate en
ROM y en la pila. Contra entrenadores empieza antes de `wIsInBattle`, que el
juego activa después del barrido. El zoom modifica una copia de la proyección;
la cámara y las mallas del mapa quedan retenidas para el regreso. El progreso
del barrido sigue los tiles que escribe el motor, sin un temporizador externo.

La primera arena coincide con el primer cuadro de texto visible del LCD,
incluido el frame de latencia después de la escritura de BGP. Los dos Pokémon
no tienen que estar listos para presentar esa arena. En estilo clásico, la
mochila y el equipo usan el mismo marco que los menús del mundo, también
mientras se desvanecen. Sus perfiles integrados conservan esa arena de fondo.

```sh
tests/ui_battles_qa.sh build/roms/pokeyellow.gbc \
  build/qa/firstperson-9cB3FJ/route.state \
  build/qa/battles-LojUdN/logs/route22-trainer.state
```

Las fixtures privadas sitúan al jugador en Ruta 1 `(10,28)` y ante el rival
en Ruta 22 `(30,5)`. El script copia sus entradas, graba ambos encuentros en
ambas cámaras y contrasta cobertura, tiempos, memoria, GL, controles y paridad
del motor. Conserva `original-intro.mp4`, `composed.mp4`, capturas y CSV dentro
de cada carpeta de prueba. La batería específica pasa en
`build/qa/ui-battles-0luMpy/`, junto a las regresiones generales y CTest 15/15
registrados en el cierre de C2 de `PLAN_MENUS_TITULO_TRANSICIONES.md`.

### Fundidos generales y viajes (C3)

F2 mezcla el frame completo, incluidos los menús del juego. Invertir el cambio
conserva la imagen intermedia; Esc y la pérdida de foco congelan su progreso.
La batería `tests/ui_crossfade_qa.sh ROM PALLET_9_7 READY_BATTLE` comprueba
continuidad de píxeles, inversión, pausa, menús, combate y puertas en ambas
cámaras. Los modos no soportados conservan el framebuffer original.

Vuelo, Teletransporte y Excavar se detectan mediante retornos vivos verificados
contra las llamadas de la ROM. La salida conserva la escena; al llegar al
punto blanco se carga el destino y se recoloca la cámara sin interpolar el
viaje. El ritmo sigue BGP y el motor original. Bicicleta y surf no añaden
transiciones. El fundido de carga de estado excluye todo el tiempo pasado con
Esc abierto o sin foco, incluida la primera imagen al reanudar.

Si el motor ha avanzado con F2 desactivado, la escena retenida ya no sirve de
fondo de entrada a un combate. Se conserva la transición 2D original hasta
disponer de una arena 3D válida. Esto también se comprueba al moverse dentro
del mismo mapa. Los controles relativos se neutralizan durante cada transición,
incluido cuando se mantenía W pulsada antes de iniciarla.

```sh
tests/ui_special_transitions_qa.sh build/roms/pokeyellow.gbc \
  build/qa/interiors-fJDbZk/pallet.state \
  build/qa/interiors-fJDbZk/city.state \
  build/qa/firstperson-9cB3FJ/route.state
```

Sus 16 escenarios usan copias privadas. La preparación concede los movimientos,
la medalla, los destinos y los objetos necesarios; el recorrido observado usa
menús e inputs del motor original. No equivale a obtener esos requisitos en una
partida completa. Cada frame comprueba WRAM, VRAM, RAM de cartucho, framebuffer
y GL. Evidencias y cierre de regresión en `PLAN_MENUS_TITULO_TRANSICIONES.md`.

### Pokédex y PC: plan en ejecución

La ficha de datos de la Pokédex tiene una presentación propia con dos
pantallas. La izquierda compone los píxeles originales del número, nombre,
categoría, medidas y descripción; la derecha muestra el retrato con su paleta
del cartucho. La detección verifica una llamada viva del motor y el rectángulo
de tiles ya transferido a VRAM. Una especie o transferencia no reconocida
mantiene la presentación de respaldo.

El descompresor de lectura se ha contrastado con los 151 retratos cargados
por el juego: 47 de 5×5, 46 de 6×6 y 58 de 7×7 tiles, los tres modos de
compresión, con 118.384 bytes idénticos. La prueba compara también con la ROM
parcial que utiliza el runtime. Los archivos de referencia permanecen privados
en `build/qa/`; no se añaden gráficos ni ROM al repositorio.

```sh
tests/dex_portraits_qa.sh build/roms/pokeyellow.gbc \
  build/qa/ui-menus-R28lDE/pallet.state dex-data
ctest --test-dir build --output-on-failure -R 'mon_pic|dex_state'
```

El script prepara una copia con los bits de Pokédex vistos/capturados para
recorrer las 151 fichas mediante los menús originales. No altera sus gráficos.
El modo `dex-screen` del helper abre una ficha privada para comprobar carga
independiente, texturas estables y respaldo ante una transferencia incompleta;
`dex-screen-fp` repite la comprobación con la preferencia de primera persona.

La lista también utiliza el dispositivo: conserva sus opciones originales,
muestra retratos de capturados, siluetas de vistos y deja vacía la pantalla
para especies ausentes. Los contadores del cuerpo son los glifos originales
del juego. Una caché LRU mantiene como máximo 32 retratos y precarga los
vecinos vistos. El retrato se sincroniza con el número y cursor visibles del
LCD, que pueden llegar hasta tres frames después del cambio de WRAM.

```sh
tests/dex_list_qa.sh build/roms/pokeyellow.gbc \
  build/qa/ui-menus-R28lDE/pallet.state
```

La prueba recorre los 151 números tres veces por cámara, distingue las tres
marcas de registro, comprueba los contadores, abre CRY y vuelve al mundo
con su cámara y mallas intactas. Capturas:
`build/qa/logs/dex-a2-list-final.png`.

La galería del Salón de la Fama está validada en B3. AREA ya tiene una vista del
mundo 3D; su validación se detalla abajo. La ficha
original usa A/B para salir; para cambiar de especie se vuelve a su lista,
sin añadir controles nuevos al juego. El estado de cada fase y sus pruebas
se registra en `PLAN_POKEDEX_PC.md`. A1/A2 quedan validadas con CTest 19/19,
las tres regresiones generales y la batería de menús aprobadas, y las
38 capturas exteriores sin cambios. Capturas de las fichas:
`build/qa/logs/dex-a1-data-review.png`.

El acceso al PC (B1) está validado en el Centro Pokémon de Ciudad Verde y
en el dormitorio, con ambas cámaras. La cámara se acerca al monitor en unos
400 ms y vuelve al cerrar el menú. El dormitorio conserva su almacenamiento
de objetos original. El menú principal aparece sobre el monitor; los textos
y submenús conservan los píxeles y controles del juego.

```sh
tests/pc_focus_qa.sh build/roms/pokeyellow.gbc \
  build/qa/ui-menus-eqZtuv/center.state build/qa/ui-menus-R28lDE/pallet.state
```

Evidencia: `build/qa/pc-focus-xiR0Sr/` y
`build/qa/logs/pc-b1-focus-review.png`. CTest 20/20 y las regresiones de mundo,
primera persona e interiores pasan; las 38 capturas exteriores no cambian.
La estantería de Bill (B2) muestra las doce cajas, hasta veinte retratos de la
caja activa y el equipo. Nombres, niveles y contadores usan la fuente de la ROM.
Los menús y sus controles siguen siendo los originales. Los bancos guardados
se verifican con sus checksums; una lectura inválida conserva el menú original
sobre el interior. El cursor resalta su retrato solo cuando el nombre y la flecha
ya aparecen en el LCD, y la geometría se reutiliza mientras no cambian los datos.

```sh
tests/pc_storage_qa.sh build/roms/pokeyellow.gbc \
  build/qa/battles-eovzS8/town.state
```

La prueba captura un Pidgey mediante encuentros reales, lo deposita, cambia de
caja entre bancos, lo retira y lo libera. Otra partida sintética prueba doce
cajas ocupadas y el scroll de veinte Pokémon. Ambas cámaras pasan, con memoria
intacta durante el renderizado y píxeles originales en los menús. CTest 22/22;
evidencia: `build/qa/pc-storage-JZqDwI/`, captura de la estantería:
`build/qa/logs/pc-b2-full-grid-final.png`.


### AREA de la Pokédex

AREA presenta los 36 mapas conectados de Kanto con cámara cenital, niebla
ligera y nombres de ciudades tomados de la ROM. Conserva una malla y un atlas
propios: al salir reaparecen las mallas y la cámara que estaban en el mundo.
Los nidos parpadean con el contador del juego. Los mapas aislados y las cuevas
se señalan en sus entradas; varias plantas pueden compartir una entrada.

Pidgey coincide con las 13 zonas del mapa original. Zubat coincide con sus
4 zonas; las entradas físicas pueden ocupar varios puntos de una misma zona.
Magikarp muestra el mensaje original `AREA UNKNOWN`, porque AREA consulta
encuentros terrestres y acuáticos, no las tablas de pesca. El nombre y ese
mensaje se componen desde los píxeles originales, sin cambiar sus controles.

```sh
tests/dex_area_qa.sh build/roms/pokeyellow.gbc \
  build/qa/ui-menus-R28lDE/pallet.state
```

El modo `dex` de `pallet_render_smoke` recorre lista, datos, CRY y AREA;
`dex-fp` repite con preferencia de primera persona. La batería compara los
nidos con los sprites del mapa original, todos los píxeles del texto, las dos
fases de parpadeo, las cargas directas y la conservación de cámara y cachés.
WRAM, VRAM, RAM de cartucho y framebuffer se comprueban en cada frame.
Capturas revisadas: `build/qa/logs/dex-a3-area-review.png`. El registro del plan
indica el estado de las regresiones posteriores antes de cerrar A3.


### Objetos, evaluación y Salón de la Fama (B3)

El PC de objetos muestra las posiciones ocupadas de sus 50 espacios en el
borde superior del monitor. Las listas de depósito, retirada y descarte
conservan el framebuffer original sobre el interior atenuado. La evaluación
de Oak mantiene sus textos en el monitor y añade los contadores originales
de Pokémon vistos y capturados.

El Salón de la Fama lee cada equipo de `sHallOfFame` en el banco cero de
SRAM. Muestra un pedestal con retrato, nombre y nivel por miembro; la cámara
sigue al Pokémon que selecciona el programa original. Los cuadros de datos
y número de equipo son píxeles del juego. La selección requiere llamadas
vivas verificadas, coincidencia del registro SRAM/WRAM y retrato idéntico a
VRAM; los estados no reconocidos conservan la presentación anterior.

La batería privada se ejecuta con:

```sh
tests/pc_details_qa.sh build/roms/pokeyellow.gbc \
  build/qa/pc-storage-4lUeyZ/center.state
```

Necesita un estado a la entrada del Centro de Ciudad Verde, dos Pokémon,
una caja vacía y Poké Balls en la mochila. Ejecuta `pc-details` y `pc`
en ambas cámaras. `pc` incluye depósito y retirada de un Pokémon, cambio de
caja, operaciones con objetos, Oak y el Salón de la Fama. La fixture de
campeón usa el equipo real de esa copia y la rutina recompilada original
`SaveHallOfFameTeams`; no representa una partida completada. La preparación
solo modifica la copia privada. Cada frame presentado comprueba que el
renderer no altera WRAM, VRAM, SRAM ni framebuffer. La aceptación completa
de B3 y sus resultados se registran en `PLAN_POKEDEX_PC.md`.


### Agua y flores (5A)

`src/tile_animation.h` reconstruye los tiles animados a partir de la ROM.
Busca la rotación del agua y el fotograma de flor que coinciden exactamente
con la VRAM residente del motor; no integra un reloj propio. Esto conserva
la fase al pausar, cargar estado, cambiar de mapa o volver desde F2.
Los tilesets vecinos usan esa fase y sus propios gráficos y flags de ROM.
Un tile no reconocido conserva su textura estática anterior.

En Yellow UE, `UpdateMovingBgTiles` está en `00:1c75`: el agua (`$14`,
VRAM `$9140`) rota en el VBlank 20. Los tilesets con flag 2 copian la flor
(`$03`, VRAM `$9030`) en el siguiente VBlank y reinician el contador;
con flag 1 lo reinician tras rotar el agua. El contador de pasos alterna
cuatro rotaciones a cada lado en un ciclo de ocho pasos. Las tres flores
se leen en `00:1cd5`, `00:1ce5` y `00:1cf5`. Los contadores vivos están en
`$FFD8` y `$D084`, y el flag en `$FFD7`, cotejados con
`pokeyellow_internal.h` y la ejecución del motor original.

Solo se actualizan rectángulos de 8×8 de la textura privada cuando sus
píxeles cambian. Los previews de catálogo conservan la fase estática de
ROM para comparar las 38 referencias; los recorridos jugables sí animan.
No se escribe en la ROM, WRAM, VRAM, SRAM ni framebuffer del juego.

```sh
ctest --test-dir build -R tile_animation_test --output-on-failure
tests/tile_animation_qa.sh build/roms/pokeyellow.gbc \
  build/qa/interiors-fJDbZk/pallet.state
```

La batería usa una copia privada en Paleta (9,7), prepara los demás mapas
con el warp del motor y comprueba todos los tilesets animados y un interior
sin animación en ambas cámaras. Registra dos ciclos completos, verifica
cada frame contra VRAM, lee los texels reales de la GPU y revisa pausa,
recarga y entrada/salida de la casa. Las secuencias quedan en
`build/qa/tile-animation-*/`, fuera de Git.

### NPC lejanos, viento y partículas (5B)

`src/npc_animation.h` lee dirección, estado y contadores del actor vivo. Los
NPC fuera del LCD usan la hoja de sprites de la ROM, incluida su mitad de
andar y los reflejos de la tabla de orientación. La posición interpola el
paso restante respecto a la casilla de destino. Un NPC que el motor detiene
fuera de pantalla permanece detenido; los objetos de cuatro tiles no andan.
La ROM compatible conserva 82 entradas en `05:42a9`; no se usan los IDs
renumerados de versiones posteriores del desensamblado.

`src/world_effects.h` observa posiciones y ciclos del motor. Deforma solo
las puntas de hierba y mantiene un máximo de 96 partículas; caminar sobre
hierba y surfear las emite, quedarse quieto no. Su variación es determinista
y no llama al RNG del juego. Las cargas y cambios de mapa reinician los
efectos; la pausa congela su reloj. El catálogo conserva la fase cero para
las 38 comparaciones exactas. `pallet3d_world_effects(bool)` permite comparar
recorridos con viento y partículas activados o desactivados.

```sh
ctest --test-dir build -R world_animation_test --output-on-failure
tests/world_animation_qa.sh build/roms/pokeyellow.gbc \
  build/qa/interiors-fJDbZk/pallet.state \
  build/qa/firstperson-9cB3FJ/route.state \
  build/qa/kanto-Eqy7Ry/surf-active.state
```

La batería necesita Paleta (9,7), Ruta 1 (10,28) y una partida con surf
activo en Paleta (6,14), preparada por `tests/world_qa.sh`. Usa copias
privadas. Comprueba ROM contra VRAM y la textura de GPU, un recorrido de
NPC que el motor ejecuta fuera del LCD y 300 snapshots completos por
recorrido y cámara con efectos activados/desactivados. No se publican las
partidas ni las capturas. El cierre y la regresión de 5B se registran en
`PLAN_MEJORAS.md`.

### Hora local e iluminación (4A/4B)

Pulsa **Esc** y usa **Hora del mundo**, al principio de los ajustes. Está
disponible en ambas cámaras: **Desactivada** conserva el aspecto anterior,
**Hora local** sigue el reloj y la zona horaria del sistema, y **Hora fija**
permite elegir un minuto del día. El ciclo viene desactivado por defecto.
La selección se guarda en `lighting.cfg` dentro del directorio que devuelve
`SDL_GetPrefPath("pokeyellow3d", "pokeyellow3d")`, aparte de la partida.

`src/daylight.h` convierte la hora en dirección del sol, colores del cielo y
la niebla, luz ambiental y directa, y emisión de ventanas. Los materiales
existentes reciben iluminación por sus normales; el tile de ventana original
`0x0a` del tileset exterior conserva el marco y enciende solo sus cristales.
Los interiores conservan su iluminación actual y el título su atardecer fijo.
No cambia el motor, sus encuentros, scripts ni RNG. Preferencias ausentes o
no reconocidas conservan el modo desactivado.

```sh
ctest --test-dir build -R 'daylight_test|render_preview_synthetic' --output-on-failure
tests/daylight_qa.sh build/roms/pokeyellow.gbc \
  build/qa/interiors-fJDbZk/pallet.state \
  build/qa/firstperson-9cB3FJ/route.state \
  build/qa/interiors-fJDbZk/city.state
```

La batería prepara 24 vistas: 06:00, 12:00, 18:00 y 00:00 en Paleta,
Ruta 1 y Ciudad Verde, en ambas cámaras. Comprueba la recuperación exacta
del modo desactivado y la carga de preferencias en procesos nuevos. Además
repite un encuentro salvaje, los menús originales y la huida a cada hora,
comparando todos los bytes de cada estado completo con el recorrido sin
ciclo. La referencia comprimida con gzip permite comparar los bytes sin
guardar miles de estados completos en memoria. Las evidencias privadas
quedan en `build/qa/daylight-*/`. La validación de 4A/4B completó las 20
baterías, 30/30 pruebas CTest, 13/13 sin ROM y 31.464 comparaciones de estados
completos. Las 38 referencias exteriores siguen idénticas. Las 24 vistas y
ambos paneles de ajustes revisados están en `build/qa/logs/daylight-final-*-review.png`;
`PLAN_MEJORAS.md` registra el cierre, la CI y las comparaciones adicionales.

### Título y menú principal

El título muestra Paleta del catálogo sin actores, con un travelling lento
al atardecer. El logo y el copyright conservan los píxeles originales a
escala entera; Pikachu se decodifica de la VRAM, los atributos de tiles, OAM
y las paletas vivas. Continuar, Nueva partida y los nombres se componen sobre
la misma escena atenuada y desenfocada. El motor mantiene toda la entrada,
la escritura de nombres y el temporizador que devuelve el título a la intro.
El copyright inicial, Game Freak y la intro de Pikachu conservan su LCD.
Esta animación usa objetos propios y no describe un mapa que convertir a
3D. Se deja al motor reproducirla completa y el compositor aplica su
fundido de 200 ms al entrar en el título, conservando el último frame
completo de la intro como origen.

La prueba local arranca una máquina nueva con ROM, tanto sin partida como con
una copia de batería. No escribe la partida proporcionada:

```sh
tests/title_qa.sh build/roms/pokeyellow.gbc /ruta/a/partida.sav
```

Las capturas, trazas y copias quedan en `build/qa/title-*/`. Se comparan los
píxeles del retrato contra el LCD original y se vigilan por frame la memoria
emulada, el framebuffer y los controles relativos. El estado de aceptación
y los recorridos observados se registran en `PLAN_MENUS_TITULO_TRANSICIONES.md`.

Para verificar el arranque completo, sin saltar la intro ni cargar un
savestate, el helper ofrece el modo público `boot`. Por defecto llega al
menú principal y termina después de 90 frames de menú:

```sh
cmake --build build --target pallet_render_smoke --parallel 4
mkdir -p build/qa/boot-manual/logs
(cd build/qa/boot-manual && \
  SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  ../../pallet_render_smoke ../../roms/pokeyellow.gbc boot)

# Arranque completo, nueva partida y Continuar, con vídeos privados.
tests/boot_qa.sh build/roms/pokeyellow.gbc /ruta/a/partida.sav
```

La sintaxis ampliada es `ROM boot [menu|new|continue] [MAX_FRAMES] [BATTERY]`.
`new` parte sin batería y escribe los nombres mediante botones originales;
`continue` exige una copia de batería válida. `BOOT_VIDEO=1` graba todos los
frames a 60 fps en `logs/boot-composed.mp4` y `logs/boot-original.mp4`, sin
audio. En Linux el helper usa un reloj virtual de presentación para que
los fundidos sean reproducibles, con 60 pasos por segundo.
`QA_FRAME_SECONDS=wall` recupera la temporización real. El reloj del motor
y el ejecutable jugable conservan su comportamiento original.

La batería de vídeos requiere `ffmpeg` y `ffprobe`. Deja vídeos, capturas, traza CSV y estado final en
`build/qa/boot-*/`. Cada frame anterior al título se compara con los píxeles
del LCD original, sin pulsaciones; la primera imagen del fundido coincide
con el último frame de la intro. También se comprueban la finalización del
fundido, la llegada al menú y al mundo, la cantidad de frames de los vídeos,
los hashes de entrada y los guardas de memoria y controles por frame.


### Estilo de menús: clásico e integrado

Esc incluye **Estilo de menus: Clasico / Integrado**. Integrado es el
valor inicial; la elección se guarda junto a la iluminación en
`lighting.cfg`, formato v2. Los archivos v1 siguen cargando su iluminación.
El estilo integrado presenta las regiones reconocidas del mundo con paneles
oscuros y los glifos originales de la ROM. A 800x720, Start y los cuadros
superiores usan escala 3; el diálogo inferior usa escala 4. El relleno es
14 píxeles y el radio 6. La referencia de la composición clásica es v0.3.0.
El cursor del ratón se oculta al jugar con foco y vuelve en Esc o al perderlo.

Los paneles integrados aparecen y desaparecen en unos 150 ms, con un fundido
y un desplazamiento de 8 píxeles. Solo se anima su fondo: los glifos conservan
su posición y su tinta completa desde el primer frame que los pinta el motor.
El tiempo procede de `ctx->cycles`; Esc y la pérdida de foco lo congelan.
Las regiones compartidas conservan su fase aunque otra ventana las tape.
Cargar una partida con un menú abierto lo presenta directamente completo.

La fuente compartida es `FontGraphics` de la ROM, sin reemplazar sus 128
glifos. Los atlas del PC conservan sus coordenadas y filtrado. Los tokens
visuales se encuentran en `src/ui_theme.h`.

| Disposición del motor | Regiones en tiles (x, y, ancho, alto) |
| --- | --- |
| Cuadro inferior | (0, 12, 20, 6) |
| Start | (10, 0, 10, 14) o (10, 0, 10, 16) |
| Sí / No | (14, 7, 6, 5), sobre el cuadro inferior |
| Guardado | Tarjeta (4, 0, 16, 10); confirmación (0, 7, 6, 5) |
| Centro | Heal/Cancel (11, 6, 9, 6), con cuadro inferior |
| Tienda | Buy (0, 0, 11, 7), dinero (11, 0, 9, 3), stock (4, 2, 16, 11), cantidad (7, 9, 13, 3) |
| Combate: mensajes y FIGHT | Inferior (0, 12, 20, 6), con controles (8, 12, 12, 6) superpuestos |
| Combate: movimientos | Inferior, lista (4, 12, 16, 6) y PP/tipo (0, 8, 11, 5), en ese orden |
| PC: selección de terminal | Principal (0, 0, 16, 8/10/12), con cuadro inferior y Sí / No según el original |
| PC de Bill | Principal (0, 0, 14, 14), caja actual (9, 14, 11, 4), lista (4, 2, 16, 11), acción (9, 10, 11, 8) |
| PC: cambio de caja | Número (0, 0, 11, 4), doce cajas (11, 0, 9, 14), sobre el menú y diálogo originales |
| PC del jugador | Principal (0, 0, 16, 10), lista (4, 2, 16, 11), cantidad (15, 9, 5, 3), cuadro inferior y confirmación |
| PC de Oak | Menú del terminal, cuadro inferior y Sí / No; conserva los diálogos y la evaluación originales |
| Equipo | Cuadrícula (0, 0, 20, 18), seis filas de dos tiles; cuadro inferior (0, 12, 20, 6); iconos originales y retrato de la fila seleccionada |
| Acciones de equipo | Sin movimientos de campo: (11, 11, 9, 7). Con ellos: borde izquierdo verificado, alto variable y una fila extra sobre la primera opción |
| Resumen: estadísticas | Retrato (1, 0, 7, 7), caja de estadísticas (0, 8, 10, 10), líneas originales derechas; HP con los tokens del HUD |
| Resumen: movimientos | Retrato (1, 0, 7, 7), movimientos/PP (0, 8, 20, 10), barra de experiencia en la fila vacía 2; conserva ambos textos de experiencia |
| Mochila del mundo | Start conservado, lista (4, 2, 16, 11), Use/Toss (13, 10, 7, 5), cantidad (15, 9, 5, 3), diálogo y confirmación |
| Tienda completa | Buy, dinero y stock en cuadrícula completa; cantidad, confirmaciones y mensaje de objeto no vendible conservan el orden de ventanas |
| Lista Pokédex | Lista (0, 0, 14, 18), lateral (15, 8, 5, 9), contadores (16, 1, 4, 2) y (16, 4, 4, 2), sobre el dispositivo |
| Mochila en combate | Cuadrícula completa, inferior y lista; texto y fragmentos de HUD/retratos originales fuera de las ventanas, verificados contra ROM y VRAM |
| Opciones | (0, 0, 20, 18), desde Start y desde el menú inicial; cursor propio CD3D y flecha original en filas 2/4/6/8/10/16 |
| Tarjeta de entrenador | Superior (0, 0, 20, 8), etiqueta central (0, 8, 20, 2), inferior (0, 10, 20, 8); conserva retrato, caras/medallas, números y colon propios |
| Nombres | Cabecera (0, 0, 20, 4), teclado (0, 4, 20, 11), cambio de caja (0, 15, 20, 3); cuadrícula de 5 × 9 teclas, flecha, ED, subrayados e icono animado originales |
| Pantalla desconocida | LCD completo enmarcado |

Las siguientes pantallas conservan su composición previa, fuera de los
perfiles completos de A1–A4:

| Pantalla | Presentación conservada |
| --- | --- |
| Impresión del PC | Página original; no se convierte en una lista integrada nueva |
| Salón de la Fama | Galería y retratos con sus ventanas de información originales |
| Pokédex DATA y CRY | Datos y retrato sobre el dispositivo existente |
| Pokédex AREA | Catálogo de Kanto, nidos y texto originales |
| Geometría, fuente o gráfico no reconocidos | LCD completo enmarcado, sin recomponer parcialmente la pantalla |


Cada celda pertenece a la última ventana que la cubre en el mapa original;
no se repite texto al separar el diálogo inferior de los cuadros superiores.
Un tile ajeno a la fuente o reemplazado en VRAM conserva los píxeles LCD de
su región, a escala entera. La tarjeta de guardado utiliza este respaldo por
su colon pequeño; las pantallas completas mantienen su marco. Ningún texto
ni selección se reconstruye desde nombres o estados del menú.

El orden de las regiones conserva los solapamientos originales. Las nuevas
pruebas se ejecutan con `tests/ui_style_qa.sh ROM WORLD ROUTE1 ROUTE22 PALLET
READY_BATTLE PC_WORLD SUCCESSFUL_CAPTURE_THROW`. `PC_WORLD` debe contener a Pikachu, un Pidgey capturado
y una caja vacía; los recorridos de almacenamiento verifican ese requisito.
El último estado debe proceder de un lanzamiento original que captura al
Pokémon y permite abrir su teclado de mote; no se modifica el resultado.
El helper acepta `menus-styled`, `menus-fp-styled`,
`battle3d-styled` y `crossfade-styled`, además de las variantes de los
recorridos. Los modos clásicos existentes siguen siendo la referencia.
La comparación byte a byte se hace con
`tests/compare_classic_captures.py BASE CANDIDATE --report REPORTE.json`;
`--settings-ui-changed` registra aparte las dos capturas del panel Esc
que contienen el control nuevo, sin excluir capturas del juego.

Los PC completos usan una cuadrícula común de 20 × 18 tiles a escala entera.
La estantería de Bill y el monitor del jugador/Oak conservan sus retratos y
contadores. Cada carácter procede de `wTileMap`; selección y desplazamiento
se leen de `wCurrentMenuItem` y `wListScrollOffset`. No se reconstruyen nombres,
cantidades ni listas. Los gráficos LV (`6E`) y caja ocupada (`78`) se contrastan
con ambos planos de VRAM y sus gráficos originales en ROM antes de presentarlos.
Un borde desconocido, selección inválida o glifo reemplazado conserva **todo**
el LCD enmarcado. Las páginas de impresión, Hall of Fame y otras disposiciones
que no figuran en los perfiles completos mantienen su presentación previa.
Opciones, tarjeta y nombres se incluyen en los perfiles de A4 descritos arriba.

La batería específica es `tests/ui_full_pc_qa.sh ROM PC_WORLD`, con
`QA_MENU_STYLE=integrated QA_GLYPH_ORACLE=1` para el estilo integrado.
Recorre ambas cámaras, veinte Pokémon, cincuenta objetos, cantidades,
confirmaciones y cancelaciones. Verifica por frame los píxeles de los glifos,
su cobertura sin omisiones ni duplicados, el cursor y la memoria; además altera
fuente, gráficos, borde y selección en un contexto de prueba desechable para
comprobar el respaldo LCD completo. Los savestates y ROM quedan fuera de Git.


La base A1 está validada: veinte baterías clásicas, 2.159 capturas de juego
idénticas a v0.2.0 y 1.831 estados completos pareados sin diferencias; las
38 vistas exteriores siguen idénticas al archivo original. La comparación
exacta usa el mismo renderizador Intel que ese archivo. CTest pasa 34/34
con ROM y 15/15 en el build independiente sin ROM, donde también se prueba
el renderizado por software.

`ui_style_qa.sh` hereda el renderizador elegido por el entorno, como las
otras baterías; `LIBGL_ALWAYS_SOFTWARE=1` permite ejecutarlo con llvmpipe.
La ejecución integrada de A1 comprobó 478.265 glifos en 15.817 frames
observados, además del cursor y la persistencia entre procesos. La prueba
de iluminación conserva explícitamente el estilo seleccionado al crear y
recargar sus preferencias. La evidencia y los comandos completos están en
el registro de `PLAN_ESTILO_MENUS.md` y en
`build/qa/logs/menu-style-a1-evidence.json`.


La presentación A2 está validada: las veinte baterías clásicas conservan
2.159 capturas y 1.831 estados idénticos a v0.2.0, incluidas las 38 vistas
exteriores. CTest pasa 35/35 y el build independiente sin ROM 16/16. Los
18 recorridos integrados comprobaron 514.558 glifos en 16.517 frames y el
cursor activo en 5.400 ocasiones. Los contactos revisados y la evidencia
se guardan en `build/qa/logs/menu-style-a2-*`. La tarjeta de guardado
conserva su región LCD por el colon adicional de la fuente; una carga fría
sin escena residente conserva el LCD completo hasta poder presentar la
escena con seguridad.

En B1, los mensajes y controles de combate usan la misma fuente y paneles,
alineados abajo a escala 4 a 800x720. Un cuadro sin tinta no dibuja panel;
el texto aparece cuando el motor escribe cada glifo. PP/tipo conserva su
solapamiento original y el retrato del jugador previamente decodificado,
si corresponde al luchador actual. Una carga fría sin retratos completos,
equipo y mochila conservan el LCD enmarcado. El respaldo de animaciones
permanece intacto; no se dibuja simultáneamente el menú integrado.
B1 está validada: CTest 36/36, sin ROM 17/17, veinte baterías clásicas
con 2.159 capturas y 1.831 estados idénticos a v0.2.0. Los 18 recorridos
integrados comprueban 659.726 glifos en 25.613 frames, y la cadena adicional
de combate verifica captura, cambios, rival y efectos. Evidencia y contactos
revisados en `build/qa/logs/menu-style-b1-*`.


B2 extiende la fuente original al HUD del mundo y a nombres, niveles, HP y
estado del combate. Los nombres conservan directamente los índices del
juego, incluidos acentos y símbolos. El rótulo del entrenador admite los
12 glifos originales de su campo; los nombres de Pokémon conservan su
límite de 10. Los paneles comparten tinta
RGB 248/244/219, fondo 17/29/33, relleno 14 y radio 6. A 800x720, el título
del mapa usa glifos de 24 píxeles, los nombres de combate 16 y los detalles
8; los controles ajustan sus líneas al ancho disponible. Las etiquetas
compactas del mapa de áreas usan relleno 3 y radio 3.

Los rótulos del PC y del Salón de la Fama comparten la tinta del tema; sus
mallas se actualizan al cambiar de estilo. Las regiones de texto compatibles
de Pokédex y título usan el atlas común solo cuando VRAM y el LCD mostrado
coinciden con sus glifos. Los logotipos, símbolos especiales, pantallas
completas y regiones incompatibles conservan los gráficos originales.
La tipografía ImGui queda reservada a los ajustes Esc en estilo integrado.

El observador de QA detecta glifos ImGui en los comandos de dibujo y compara
los píxeles visibles del atlas 2D con los bits de la ROM. En el mapa de áreas,
la ventana original AREA UNKNOWN puede cubrir etiquetas: esa parte se
compara con el último LCD completo observado; el resto de cada glifo sigue
comprobándose. La validación completa de B2 se registra en
`PLAN_ESTILO_MENUS.md` antes de acreditar sus tres criterios.


B2 está validada: CTest 37/37, sin ROM 18/18, veinte baterías clásicas con
2.159 capturas y 1.831 estados idénticos a v0.2.0 y las 38 vistas exteriores
sin diferencias. Los 18 recorridos integrados comprueban 659.726 glifos en
25.613 frames. La auditoría del HUD no detecta fuente ImGui durante el
juego; los 47 nombres de clase de entrenador pasan en ambas cámaras.
Los cinco contactos revisados y el informe completo se conservan en
`build/qa/logs/menu-style-b2-*`. C1 incorpora las animaciones de paneles;
sus tres criterios están acreditados en `PLAN_ESTILO_MENUS.md`.

La prueba de C1 `menu-motion-styled` (y `menu-motion-fp-styled`) comprueba
Esc, foco y cargas de estado con el motor real, y forma parte de
`ui_style_qa.sh`. Para comparar todas las entradas con animaciones activadas
y desactivadas, se reutilizan los fixtures privados preparados por
`ui_menus_qa.sh`:

```sh
env -u LIBGL_ALWAYS_SOFTWARE python3 tests/compare_menu_motion.py \
  build/roms/pokeyellow.gbc RUTA_QA/pallet.state \
  --center RUTA_QA/center.state --shop RUTA_QA/shop.state
```

El informe compara cada byte de cada estado completo del motor por frame,
incluidas las pulsaciones durante la aparición, y las capturas al terminar
el fundido. Los estados se comprimen durante la ejecución; las secuencias
visuales y las fases se guardan aparte bajo `build/qa/menu-motion-pair-*`.
Las opciones `QA_MENU_MOTION` y de auditoría solo pertenecen al helper.


C1 está validada: CTest 38/38 y sin ROM 19/19; veinte baterías clásicas
con 2.159 capturas y 1.831
estados idénticos a v0.2.0, más las 38 vistas exteriores originales.
Los dieciocho recorridos integrados mantienen la comprobación de glifos
por frame. Ocho recorridos comparan 20.520 frames
completos del motor con animaciones activadas y desactivadas; las
62 capturas a fase fija coinciden con B2.
Los contactos de apertura y cierre, la pausa/carga real en ambas cámaras
y el informe final se conservan en `build/qa/logs/menu-style-c1-*`.


### Pantallas completas: validación A1

Bill, jugador y Oak pasan doce recorridos en ambas cámaras, integrados en la
batería acumulativa de treinta recorridos. El oráculo PC comprueba 2.504.708
glifos y 49.454 gráficos con memoria intacta por frame. Sus 110 estados de
control son exactos entre clásico e integrado. Las veinte baterías clásicas
conservan 2.169 capturas y 1.831 estados frente a v0.3.0, y la batería PC
ampliada añade 110 capturas y estados exactos. Las 38 vistas exteriores
originales siguen intactas. CTest pasa 39/39 y el build independiente sin
ROM 20/20. Véanse `PLAN_PANTALLAS_COMPLETAS.md` y la evidencia privada
`build/qa/logs/fullscreen-a1-evidence.json`.


### Equipo y resumen integrados (A2)

`pokemon_menu_state.h` comprueba las llamadas originales activas, los bordes,
las dos páginas del resumen, la fuente y los gráficos de VRAM antes de sustituir
el LCD. Los símbolos especiales tienen perfiles distintos de los del PC: por
ejemplo, `78` es aquí un borde vertical y `6E` procede del HUD del resumen.
El retrato se descomprime de la ROM y se compara completo con VRAM; los iconos
animados conservan los sprites, posiciones y fases del motor original.

El retrato lateral del equipo sigue la flecha pintada, incluido el frame en
que el motor aún no ha repintado una nueva selección. Se muestra cuando cabe
a escala entera en el margen; los iconos originales del equipo permanecen en
la cuadrícula. Las barras de HP leen la longitud y el color pintados por el
juego. Las porciones ocultas por un menú de acciones conservan sus gráficos
originales visibles. La barra de experiencia lee el registro permanente del
Pokémon: `CalcExpToLevelUp` sustituye temporalmente `wLoadedMonExp` por la
cantidad que falta para subir de nivel. Ninguna etiqueta ni cifra se reconstruye.

`tests/ui_full_pokemon_qa.sh ROM TWO_POKEMON_WORLD READY_BATTLE` recorre equipo, acciones,
ambas páginas del resumen y los resúmenes de equipo/caja desde el PC, en ambas
cámaras, y las mismas pantallas desde un combate. Con
`QA_MENU_STYLE=integrated QA_GLYPH_ORACLE=1` comprueba glifos,
cursores, gráficos especiales y barras por frame, además de memoria intacta,
pausa/carga y cinco alteraciones que deben conservar el LCD completo.
La copia privada de seis miembros incluye HP cero y mínimo, todos los estados
alterados, nivel 100 y movimientos de campo. El mismo recorrido clásico sirve
para la comparación exacta con v0.3.0. La evidencia y el estado de aceptación
de esta fase se registran en `PLAN_PANTALLAS_COMPLETAS.md`.

### Mochila, tienda y lista Pokédex (A3)

La mochila del mundo y la tienda comparten la cuadrícula y la lectura de cursor
/scroll de A1. Conservan las partes visibles de Start y de las ventanas anteriores,
incluidos cantidades, mensajes de Use/Toss, compra, venta y objetos no vendibles.
`item_menu_state.h` exige las llamadas originales activas y rechaza los gráficos
de otros perfiles. La mochila de combate añade un perfil independiente: conserva
el HUD y los fragmentos de ambos retratos que deja visibles la lista. Los retratos
se descomprimen de la ROM y se cotejan completos con VRAM; el gráfico LV de combate
procede de `BattleHudTiles1`, diferente del que carga el PC. Una partida cargada
dentro de la mochila establece la escena de combate mediante su llamada original,
sin necesitar una imagen anterior del mundo.

La lista Pokédex presenta cuatro regiones sobre las pantallas y el cuerpo del
dispositivo existente. Usa `menu_text` con posiciones proyectadas y escala entera;
los nombres y contadores siguen siendo tiles originales. El marcador de captura
`72` se compara con los dos planos de VRAM y ROM `3AA28`. Los separadores `70/71`
también se validan. Una región, fuente, gráfico o selección no reconocidos mantiene
el LCD completo; nunca combina media lista integrada con media lista nativa.
El retrato sigue esperando a que el número y el cursor correspondientes sean
visibles en el LCD. DATA y AREA conservan sus composiciones previas.

Pruebas específicas: `tests/ui_full_items_qa.sh ROM WORLD_AFTER_PARCEL READY_BATTLE` y
`tests/ui_full_dex_qa.sh ROM WORLD_WITH_POKEDEX`, con
`QA_MENU_STYLE=integrated QA_GLYPH_ORACLE=1`. La Pokédex recorre listas de 5 y
151 entradas, ambos extremos, saltos de página, registros capturados/vistos/no
vistos, DATA y CRY en ambas cámaras. Comprueba cada glifo y marcador contra ROM,
VRAM y la superficie final, además de cobertura, cursor, memoria, pausa/carga
y cinco alteraciones de prueba. Ambos recorridos se incorporan a `ui_style_qa.sh`.
Los 56 recorridos integrados acumulados y las comprobaciones de pausa/carga
en ambas cámaras pasan; los contactos y las capturas nuevas del README están
revisados. Las 24 baterías clásicas conservan 2.497 capturas y 2.159 estados
exactos frente a v0.3.0, incluidas las 38 vistas exteriores originales.
CTest pasa 43/43 y el build independiente sin ROM 24/24. La evidencia y el
estado de CI/fusión están registrados en `PLAN_PANTALLAS_COMPLETAS.md`.

### Opciones, tarjeta de entrenador y nombres

Opciones lee `wOptionsCursorLocation` (CD3D, alias `wWhichTrade` en el C
generado), porque `wCurrentMenuItem` puede conservar la selección de Start.
La flecha de `wTileMap` sigue siendo la autoridad visual durante un repintado.
El perfil reconoce la llamada original compartida por Start y el menú inicial.
Sus cinco ajustes, sus ciclos y Cancel siguen ejecutándose en el motor.

La tarjeta valida el recorte del retrato de Red contra la ROM descomprimida,
los bordes propios y cada gráfico visible. Los tiles D6–DF contienen el colon,
fondo y números de medalla; no son caracteres de la fuente. Las caras y
medallas se cotejan además con los bits de `wObtainedBadges`. Un gráfico,
fuente, borde o disposición desconocidos conserva el LCD completo enmarcado.

`tests/ui_full_options_qa.sh ROM WORLD_WITH_POKEDEX [INITIAL_MENU_STATE]`
recorre todos los valores en ambas direcciones, el salto de filas vacías,
wrap y salidas por Cancel, Start y B, desde ambos menús y en ambas cámaras.
Sin el tercer argumento obtiene el estado inicial mediante el arranque original.
`tests/ui_full_trainer_qa.sh ROM WORLD_WITH_POKEDEX` recorre medallas ausentes,
mixtas y completas, nombre de siete caracteres y extremos de dinero/tiempo.
Estas baterías están incluidas en `ui_style_qa.sh`; con estilo integrado
comprueban fuente/gráficos/píxeles por frame, memoria intacta, pausa y carga.

El teclado reconoce las llamadas originales al nombrar al jugador, al rival,
en el inspector de motes y después de una captura. Lee el alfabeto y el texto
de `wTileMap`; valida cada tecla y el cambio de caja contra las tablas de la
ROM. ED y los subrayados se comprueban contra ROM/VRAM. El icono de Pokémon
conserva los cuatro sprites OAM, sus fases, reflejos y paleta originales.
El cursor pintado manda durante la llamada de animación, que reutiliza
temporalmente `wCurrentMenuItem`. La rejilla añade fondos a las teclas sin
mover ni sustituir caracteres. Cualquier gráfico, fuente o posición desconocidos
devuelve atómicamente la pantalla LCD completa.

`tests/ui_full_naming_qa.sh ROM TWO_POKEMON_WORLD SUCCESSFUL_CAPTURE_THROW
[INITIAL_MENU_STATE]` obtiene los teclados con diálogos e inputs originales
y recorre dieciséis combinaciones de entrada, cámara y envío por Start/ED.
Comprueba mayúsculas/minúsculas, wrap, borrado, límites de 7/10 caracteres,
nombre aceptado, icono animado, pausa, carga y controles negativos. También
está incluida en `ui_style_qa.sh`. La invariante por frame incluye OAM,
HRAM e IO además de WRAM, VRAM, RAM del cartucho y framebuffer.

La validación inicial suma cuatro recorridos de opciones y seis de tarjeta,
45/45 CTest y 26/26 pruebas en el build independiente sin ROM. Los contactos
están revisados en `build/qa/logs/fullscreen-a4-*-contact-review.json`.
Con el teclado añadido pasan dieciséis recorridos más, 46/46 CTest y 27/27
pruebas sin ROM. La batería de nombres comprueba 21.172 frames, 1.460.496
glifos y 20.960 cursores; el contacto incluye jugador, rival, inspector de
motes y captura, con ambos alfabetos y nombres de longitud máxima.
La batería acumulativa final pasa 82 recorridos, además de pausa/carga en
ambas cámaras. Los tres contactos finales de A4 están revisados y CI pasa
GCC, Clang, Linux 2D, MSVC y formato; Windows verifica también 27/27 pruebas
sin ROM con ANGLE. Las 27 baterías clásicas conservan 2.765 capturas y
2.427 estados idénticos a v0.3.0, incluidas las 38 vistas exteriores originales.
La evidencia final está en `fullscreen-a4-evidence.json`. La PR #26 está
fusionada en main; `PLAN_PANTALLAS_COMPLETAS.md` registra los 24 criterios
completos y el contrato de publicación de v0.4.0.

### Fixtures y comprobaciones del pase artístico

Con una partida privada de Paleta en (9,7), con un equipo preparado para los
recorridos existentes, se puede generar la fixture del benchmark de cinco mapas
mediante el warp de pruebas que ejecuta el motor original:

```sh
mkdir -p build/qa/art-fixtures
cp "$PALLET_STATE" build/qa/art-fixtures/pallet.state
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  build/pallet_render_smoke "$ROM" build/qa/art-fixtures/pallet.state \
  warp 10 0 build/qa/art-fixtures/five-maps.state
ctest --test-dir build --output-on-failure -R 'art_benchmark_fixture|exterior_geometry_rom'
```

`QA_ART_PASS` queda desactivado por defecto en las herramientas de QA; usa
`QA_ART_PASS=on` para probar las nuevas mallas. El juego conserva su preferencia
independiente. `exterior_geometry_rom` audita también los exteriores de Safari
que no pertenecen al catálogo inicial de 38 mapas.

El gate visual se ejecuta contra un helper de referencia construido con el
renderer congelado de `7955aaa`, adaptado al mismo reloj determinista de QA:

```sh
python3 tests/art_qa.py \
  build/pallet_render_smoke build/art_benchmark "$ROM" \
  build/qa/art-fixtures/pallet.state build/qa/art-fixtures/five-maps.state \
  "$BENCHMARK_7955AAA" build/qa C1 \
  --reference-smoke "$SMOKE_7955AAA" --captures-only --compress-captures
```

Ese comando declara `CAPTURES_PASS`, no aceptación de rendimiento. Sin
`--captures-only` mide también diez escenarios contra el benchmark congelado,
sin otras baterías ni juegos utilizando la GPU. El catálogo FP diagnóstico no
existía en v0.4.1: el gate utiliza sus cuatro orientaciones reales de cámara y
el recorrido original, además de los catálogos ortográficos. ROM, partidas y
capturas permanecen fuera de git y de los paquetes.

Las pruebas de fases de combate incorporadas en PR #33 necesitan estados ya
dentro del encuentro. Las fixtures históricas de `tests/battles_qa.sh` sirven
para reproducirlas; `BATTLE_QA_DIR` es el directorio privado que imprime esa
batería:

```sh
mkdir -p build/qa/battle-phases
cp "$BATTLE_QA_DIR/logs/battle-fight.state" build/qa/battle-phases/wild.state
cp "$BATTLE_QA_DIR/logs/trainer-intro.state" build/qa/battle-phases/trainer.state
ctest --test-dir build --output-on-failure -R '^battle_phases_'
```

`route22-trainer.state` es una aproximación anterior al diálogo del rival y
no sustituye a `trainer-intro.state` en esta prueba de fases y tiempos.
