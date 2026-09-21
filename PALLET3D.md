# Kanto en 3D

Presentación 3D para la ROM inglesa UE de Pokémon Amarillo, SHA-1
`cc7d03262ebfaf2f06772c1a480c7d9d5f4a38e1`. El juego recompilado conserva
movimiento, colisiones, encuentros, combates, historia y guardado.

## Ejecutar

Está disponible el [paquete Linux x86_64 v0.1.0](https://github.com/dobbygl/pokeyellow3d/releases/tag/v0.1.0), compilado en Ubuntu 24.04. Descomprímelo, coloca tu ROM en `roms/pokeyellow.gbc` junto al ejecutable y sigue `RUN.md`; las bibliotecas necesarias están en `DEPENDENCIES.txt`. No incluye ROM ni partida. Para la compilación local:

```sh
cd build
./pokeyellow3d
```

Usa `build/roms/pokeyellow.gbc`, los recursos extraídos y `build/pokeyellow.sav`.
Cierra otra instancia del juego antes de jugar para evitar escrituras simultáneas
sobre la misma partida.

El 3D aparece automáticamente en los **36 mapas exteriores conectados** de Kanto,
**Bosque Verde** y **muelle de Carmín**. Las conexiones de borde mantienen una
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
savestate en otro mapa hace un fundido breve a negro. El resto de la
ampliación de menús, título y transiciones está en curso en
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

## Compilar y probar

```sh
cmake -S . -B build -G Ninja -DPOKEYELLOW_3D=ON \
  -DGBRT_REF=6581880fce60e6f139901a5942fc984e9c1db8ab
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

Se necesitan las dependencias habituales del proyecto, SDL2 y OpenGL ES 2.
`POKEYELLOW_3D=OFF` produce el ejecutable original `pokeyellow`.
CTest registra pruebas de controles, menús, fundidos y escenarios sintéticos.
Al configurar con la ROM local en `build/roms/pokeyellow.gbc` añade las
comprobaciones del cartucho. La equivalencia de retratos con VRAM requiere
generar primero la evidencia privada con `tests/dex_portraits_qa.sh`.
`ctest --test-dir build -LE rom --output-on-failure` ejecuta el grupo sin ROM;
no se distribuye esa ROM.

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
  El runtime se fija al tag `presentation-api-v1` de `dobbygl/gb-recompiled`.

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
  al mundo. Equipo, mochila, PC, ficha, opciones, Pokédex y disposiciones no
  reconocidas se enmarcan sobre esa escena atenuada y desenfocada. Cargar
  directamente un estado de menú completo sin escena previa conserva el LCD.
  Las teclas son las originales mientras hay texto. Volver al exterior restaura
  la preferencia de cámara, sin pulsaciones relativas retenidas.
- Pikachu puede ocupar gran parte de la vista al estar justo al lado del jugador.
  Árboles, rocas y laterales tienen geometría sencilla; las alturas son visuales.
  Las cornisas no cambian de nivel real: se representan con un balanceo de cámara.
- Agua y vegetación son estáticas. Las playas, detalles sin clasificación
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
| Intro, título, link, tutorial y Safari | LCD original; fases B y C pendientes |
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
no tienen que estar listos para presentar esa arena. La mochila y el equipo
usan el mismo marco que los menús del mundo, también mientras se desvanecen.

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
