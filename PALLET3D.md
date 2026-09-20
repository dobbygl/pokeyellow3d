# Kanto en 3D

Presentación 3D para la ROM inglesa UE de Pokémon Amarillo, SHA-1
`cc7d03262ebfaf2f06772c1a480c7d9d5f4a38e1`. El juego recompilado conserva
movimiento, colisiones, encuentros, combates, historia y guardado.

## Ejecutar

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

Los combates normales tienen una primera escena 3D con retratos de VRAM,
paletas originales y marcadores de vida. B1 sigue en validación: no se afirma
todavía cobertura completa de cambios de Pokémon ni combates de entrenador.
El texto y los menús son el framebuffer original; las animaciones conservan
la imagen completa. Link, tutorial, Safari y estados no reconocidos siguen en
2D. En primera persona los cuadros de diálogo inferiores reconocidos se
superponen al mundo 3D.

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
Las siete pruebas CTest se registran al configurar CMake con la ROM local en
`build/roms/pokeyellow.gbc`; no se distribuye esa ROM.

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
- `src/battle3d.h`: arena independiente, dos plataformas, billboards y marcadores
  ImGui. Usa su propia textura y cámara; no modifica las mallas del mundo.
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
  produciendo ese framebuffer; inicialización, F2, menús, transiciones y combate
  conservan su ruta de presentación original.
- `cmake/Pallet3D.cmake` genera una copia adaptada del frontend SDL dentro de
  `build/`. No edita el runtime descargado ni el C generado del juego. Los
  puntos de inserción se comprueban y fallan explícitamente si cambian.

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

- La ampliación de interiores y combates está en curso, según
  `PLAN_INTERIORES_COMBATES.md`. No incluye cámara libre ni alturas transitables nuevas. Primera persona conserva movimiento por
  casillas y cuatro direcciones de interacción; el ratón no gira al jugador.
- Los cuadros inferiores con borde completo y mapa visible detrás conservan
  el 3D en primera persona: se copian las filas 96–143 del framebuffer original.
  Start, pantallas completas y disposiciones de texto no reconocidas pasan a 2D.
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
- El hook SDL depende del commit fijado. Una interfaz formal de renderizado
  upstream sería preferible para futuras actualizaciones; cambiar de fork no
  es necesario para esta entrega.

Véanse [PLAN_KANTO_3D.md](PLAN_KANTO_3D.md),
[PLAN_PRIMERA_PERSONA.md](PLAN_PRIMERA_PERSONA.md) y
[PLAN_INTERIORES_COMBATES.md](PLAN_INTERIORES_COMBATES.md) para criterios y evidencias. Fuentes:
[headers](https://github.com/pret/pokeyellow/blob/master/macros/scripts/maps.asm),
[memoria](https://github.com/pret/pokeyellow/blob/master/ram/wram.asm),
[Corte](https://github.com/pret/pokeyellow/blob/master/engine/overworld/cut.asm),
[movimiento](https://github.com/pret/pokeyellow/blob/master/home/overworld.asm),
[sprites](https://github.com/pret/pokeyellow/blob/master/engine/gfx/sprite_oam.asm).
