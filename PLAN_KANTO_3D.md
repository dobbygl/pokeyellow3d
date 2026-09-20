# Plan: ampliar Pokémon Amarillo 3D desde los datos de la ROM

Fecha: 2026-09-19. Estado: fases 1–4 completadas y verificadas con la ROM local.

## Objetivo y alcance

Sustituir la definición manual de Pueblo Paleta y Ruta 1 por un sistema de
escenarios derivado de la ROM. Conservar el motor recompilado como autoridad
para movimiento, colisiones, saltos, encuentros, combates, historia y guardado.

El plan completo comprende las fases 1–4: los 36 mapas unidos por conexiones
desde Paleta, edificios con volumen y dos escenarios exteriores separados,
Bosque Verde y muelle de Carmín. Ciudad Verde es el primer hito verificable,
no la condición de finalización del plan completo.

Los demás escenarios separados, los interiores y los combates 3D quedan como
ampliaciones posteriores. La arquitectura debe admitirlos sin exigir que se
implementen para terminar este plan. El traslado a otro runtime tampoco es
requisito de finalización.

## Punto de partida verificado

- Ejecutable actual: `build/pokeyellow3d`, SDL2 y OpenGL ES 2.
- ROM inglesa UE, SHA-1 `cc7d03262ebfaf2f06772c1a480c7d9d5f4a38e1`.
- `src/world_scene.h` define dos escenas, dimensiones, orígenes, casas, señales,
  agua, vallas y clasificación parcial de terreno.
- `src/pallet3d.cpp` genera ambos mapas en un atlas de 1024×1024; la cámara y
  parte de la geometría conservan constantes del mundo actual.
- `src/pallet_state.h` limita la vista a esos mapas y al tileset Overworld.
- Las pruebas existentes cubren movimiento, actores, transición entre los dos
  mapas, colisiones, salto, combate en 2D, regreso al 3D y lectura sin modificar
  WRAM. Consultar `PALLET3D.md` para reproducirlas.
- La auditoría de la ROM encontró 36 mapas conectados desde Paleta: 34 Overworld
  y 2 Plateau. No encontró contradicciones de origen en sus ciclos. Bosque Verde
  y el muelle no pertenecen a ese grafo. Reproducir esta auditoría con una
  herramienta del repositorio; no depender del script temporal de evaluación.
- Runtime actual fijado a `6581880fce60e6f139901a5942fc984e9c1db8ab`.

## Decisiones de arquitectura

1. Conservar una estructura `Scene` o equivalente. Generar su catálogo leyendo
   la ROM, y separar los retoques artísticos de los datos originales.
2. Separar el lector de ROM, el catálogo de mapas y conexiones, las reglas de
   terreno, la generación de mallas, la caché y la lectura del estado vivo.
3. Usar ROM para la base estática y WRAM para los bloques modificados del mapa
   activo. Invalidar datos derivados al cambiar de mapa o cargar un savestate.
4. Mantener la autoridad de las reglas originales. Una lista de colisiones no
   basta para deducir alturas, edificios o toda la transitabilidad.
5. Mantener un comportamiento visible y correcto para gráficos aún no
   clasificados: dibujar el tile original en plano y registrar su cobertura.
6. Mantener los actores del mapa activo ligados al estado real. Los datos de NPC
   en ROM son condiciones iniciales; no representan su estado actual ni
   justifican inventar NPC activos en mapas vecinos.
7. Reutilizar los módulos actuales donde corresponda, pero revisar sus supuestos
   sobre sprites, coordenadas, VRAM y detección de menús antes de generalizarlos.

## Fase 1: lector de ROM y auditor del mundo

Trabajo:

- Leer `MapHeaderBanks`, `MapHeaderPointers` y `Tilesets` con conversión explícita
  de banco/dirección a offset de ROM y comprobaciones de rango.
- Decodificar dimensiones, bloques, tileset, conexiones y objetos. Respetar
  unidades: tile gráfico de 8 px, casilla de movimiento de 16 px y bloque de 32 px.
- Distinguir conexiones de borde y warps. No todos los warps son puertas, ni todos
  los exteriores se descubren con un BFS desde Paleta.
- Asignar orígenes mediante conexiones, con desplazamientos con signo; verificar
  los ciclos y la coherencia espacial.
- Leer los punteros y atributos de cada tileset: gráficos, blockset, colisiones,
  hierba y animaciones. Comprobar disponibilidad en los recursos del runtime.
- Producir un informe reproducible con mapas, tamaños, tilesets, conexiones,
  orígenes, warps y errores. Excluir entradas no utilizadas.

Criterios de aceptación:

- [x] El auditor reproduce los 36 mapas, su distribución 34/2 y ciclos coherentes.
- [x] Paleta, Ruta 1 y Ciudad Verde se decodifican sin offsets individuales escritos a mano.
- [x] Lecturas inválidas o truncadas se rechazan con diagnósticos, sin acceder fuera de rango.
- [x] Las tablas y direcciones se verifican contra los símbolos y `pokeyellow_internal.h`.

## Fase 2: atlas por tileset y primer hito en Ciudad Verde

Trabajo:

- Reemplazar la imagen completa del mundo por atlas de tiles reutilizables y UV
  por tile; 128×128 es una propuesta que debe contrastarse con los datos reales.
- Separar texturas y mallas estáticas de los sprites que se actualizan desde VRAM.
- Generar mallas por mapa bajo demanda; mantener el mapa activo y sus vecinos
  necesarios para la cámara. Establecer límites de caché y liberar recursos.
- Eliminar de cámara, atlas y geometría las dimensiones exclusivas de Paleta/Ruta 1.
- Incorporar Ciudad Verde desde su header y blockset, sin añadir una tercera
  entrada manual a `Scenes`. Admitir temporalmente edificios en plano en este hito.
- Preservar giro/zoom en conexiones y reajustar cámara de forma explícita en warps.

Criterios de aceptación:

- [x] Se puede recorrer Paleta → Ruta 1 → Ciudad Verde y volver sin saltos de origen.
- [x] Ciudad Verde usa sus dimensiones reales y se ve correctamente con giro y zoom.
- [x] F2, menús, combate, entrada a interiores y vuelta al 3D siguen funcionando.
- [x] Capturas revisadas, cero errores OpenGL y ausencia de escrituras del renderer en WRAM.
- [x] La caché no crece indefinidamente al repetir transiciones.

## Fase 3: cobertura de los 36 mapas y estado vivo

Trabajo:

- Completar reglas visuales de Overworld y Plateau: suelo, árboles, rocas, agua,
  hierba, vallas y bordes de desnivel. Usar la hierba indicada por cada tileset.
- Interpretar `LedgeTiles` con su restricción original a Overworld. Considerar
  también las tablas de colisión entre pares de tiles al interpretar límites.
- Conservar las colisiones y los saltos en el motor original; no reconstruirlos
  como una segunda simulación de juego.
- Reflejar las sustituciones de bloques de WRAM, incluyendo Corte y cambios por
  scripts. Actualizar solo las mallas afectadas e invalidar la caché tras cargas.
- Revisar `view()`: mapa compatible y cargado, tileset esperado, batalla, menús,
  transiciones y validez de buffers. Tener origen mundial no basta para activar 3D.
- Construir y auditar las mallas de los 36 mapas; registrar tiles sin clasificación
  artística y usar su representación original hasta clasificarlos.
- Medir tiempo de generación, memoria y presentación en mapas pequeños y grandes.

Criterios de aceptación:

- [x] Los 36 mapas tienen suelo completo y mallas válidas, con decoración del terreno.
- [x] Se verifican conexiones horizontales, verticales y con desplazamiento lateral.
- [x] Cortar un árbol o hierba cambia su representación; cargar un estado restaura la escena correcta.
- [x] Combate, diálogo, surf y bicicleta no producen una presentación incompatible con el estado real.
- [x] Las regresiones anteriores pasan y existen medidas de rendimiento reproducibles.

## Fase 4: edificios y exteriores separados

Trabajo:

- Inferir edificios combinando patrones de tejado/fachada, tiles de puerta,
  warps y límites de región. El flood fill de sólidos es una ayuda, no la única regla.
- Evitar unir edificios con acantilados o paredes cercanas. Conservar la textura
  original cuando una inferencia no sea fiable y registrar el caso para corregirlo.
- Añadir retoques por mapa/coordenada para edificios singulares: Torre Pokémon,
  Silph, centro comercial de Azulona y Gimnasio de Plateada, entre los casos a revisar.
- Completar los casos ambiguos de los 36 mapas para que sus edificios tengan
  volumen coherente, entradas alineadas y no oculten permanentemente al jugador.
- Incorporar Bosque Verde y muelle de Carmín con reglas Forest y ShipPort y
  coordenadas de escena propias. No forzar su colocación por un BFS que no los alcanza.
- Mantener en 2D las puertas/interiores intermedios todavía no soportados.
- Documentar la cobertura y los límites; evaluar por separado una extensión
  formal del runtime, sin convertir un cambio de fork en dependencia del plan.

Criterios de aceptación:

- [x] Edificios de los 36 mapas revisados; regiones ambiguas corregidas o resueltas explícitamente.
- [x] Capturas de Paleta, Ciudad Verde, Plateada, Azulona y Azafrán revisadas.
- [x] Bosque Verde y muelle se muestran en 3D y sus entradas/salidas conservan el juego original.
- [x] Ninguna escena depende de escribir el estado de la máquina para funcionar.

## Validación y entrega final

- Ejecutar las pruebas existentes y añadir pruebas del lector, conexiones,
  caché, cambios de bloques y selección de vista. Un mapa sin edificios puede
  tener cero regiones de edificios: eso no es un fallo.
- Usar copias aisladas de ROM y partida para pruebas de integración. No sobrescribir
  las partidas del usuario ni incorporar ROM, gráficos extraídos o savestates al repositorio.
- Combinar auditorías de todos los mapas con recorridos reales representativos;
  generar mallas no prueba por sí solo la jugabilidad.
- Guardar comandos, resultados y capturas locales bajo `build/qa/`; documentar
  los requisitos de cada fixture para poder repetir las pruebas.
- Entregar `build/pokeyellow3d` compilado, documentación actualizada e informe
  de cobertura, rendimiento y limitaciones conocidas.
- Actualizar las casillas de este plan solo con evidencia. Completar Ciudad Verde
  o pasar únicamente CTest no equivale a terminar las cuatro fases.

## Ejecución como goal

Este archivo conserva el plan y su registro de ejecución. Si se ejecuta sobre
una copia que ya contiene avances, comprobar primero las evidencias registradas,
conservar lo que funciona y completar únicamente los criterios pendientes.
Las casillas marcadas no sustituyen la validación del estado actual.

Comando breve para ejecutar o verificar este plan desde la conversación del
proyecto (no desde Bash):

```text
/goal Lee pokeyellow/PLAN_KANTO_3D.md, contrasta el estado actual con su registro y completa los criterios pendientes de las cuatro fases. Conserva el motor original y las partidas. Entrega pokeyellow3d compilado, pruebas y capturas verificadas, y actualiza el plan con evidencia. Si todo está implementado, verifica la entrega sin rehacerla.
```

Si la conversación ya tiene otro objetivo activo, consultar `/goal` antes de
lanzarlo; este documento no sustituye por sí mismo el objetivo de la sesión.

En la conversación de Codex del proyecto, usar este objetivo para el plan completo:

```text
/goal Ejecuta las fases 1–4 de /home/jgomez/Projects/jgomez/pokemon2/pokeyellow/PLAN_KANTO_3D.md. Completa los 36 mapas conectados, edificios, Bosque Verde y muelle de Carmín. Usa Ciudad Verde como primer hito, conserva el motor original y las partidas, y continúa hasta cumplir todos los criterios de aceptación, con pokeyellow3d compilado, pruebas y capturas verificadas. Actualiza el plan con evidencia; no marques el objetivo completo al terminar solo el primer hito.
```

Si se desea ejecutar únicamente el primer hito, usar un objetivo distinto:

```text
/goal Completa solo las fases 1 y 2 de /home/jgomez/Projects/jgomez/pokemon2/pokeyellow/PLAN_KANTO_3D.md: lector y auditor de ROM, atlas por tileset, mallas por mapa y recorrido Paleta–Ruta 1–Ciudad Verde. Entrega pokeyellow3d compilado y verifica todos los criterios de esas dos fases. Deja las fases 3 y 4 pendientes en el plan.
```

Consultar con `/goal`, pausar con `/goal pause` y reanudar con `/goal resume`.
Fuente: [documentación oficial de objetivos de Codex](https://developers.openai.com/es-419/use-cases/follow-goals).

## Referencias técnicas

- [Headers, conexiones y objetos](https://github.com/pret/pokeyellow/blob/master/macros/scripts/maps.asm).
- [Catálogo de mapas](https://github.com/pret/pokeyellow/blob/master/constants/map_constants.asm).
- [Headers de tilesets](https://github.com/pret/pokeyellow/blob/master/data/tilesets/tileset_headers.asm).
- [Lógica de cornisas](https://github.com/pret/pokeyellow/blob/master/engine/overworld/ledges.asm).
- [Colisiones entre tiles](https://github.com/pret/pokeyellow/blob/master/data/tilesets/pair_collision_tile_ids.asm).
- [Cambios de bloques mediante Corte](https://github.com/pret/pokeyellow/blob/master/engine/overworld/cut.asm).
- [Memoria del juego](https://github.com/pret/pokeyellow/blob/master/ram/wram.asm).
- [Interfaz experimental de mstan](https://github.com/mstan/gbrecompiled/blob/master/runtime/include/gb_custom_view.h): compositor de píxeles, no sustituto directo del hook OpenGL actual.

## Registro de ejecución

### 2026-09-19: lector y primer recorrido a Ciudad Verde

- `src/kanto_rom.h`: lector con comprobación de rangos, bancos, dimensiones,
  objetos, conexiones, tilesets, cornisas y pares de colisión. Catálogo de 38
  escenas: 36 conectadas más Bosque Verde y muelle.
- `build/kanto_rom_audit build/roms/pokeyellow.gbc` pasa: distribución 34/2,
  ciclos, coordenadas de Ciudad Verde, datos disponibles en el manifiesto del
  runtime y rechazo de entradas corruptas. Informe: `build/qa/logs/kanto-audit.csv`.
- `src/world_scene.h` obtiene mapas del lector. `src/pallet3d.cpp` usa un atlas
  de tilesets de 512×512 con variantes de paleta y una malla estática en GPU por
  mapa, conservando solo el mapa activo y sus vecinos. Espera estabilidad del
  búfer vivo durante cambios de mapa.
- CTest: `pallet_state` y `kanto_rom` pasan con la ROM local.
- Recorrido real: ejecutar desde `build/qa` el helper actualizado con
  `SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy JOURNEY_CITY_STATE=viridian.state
  ./pallet_render_smoke roms/pokeyellow.gbc progress.state10 viridian`.
  Resultado en `logs/viridian-journey.log`: 12.410 frames, cuatro cruces de
  frontera, cinco victorias, regreso a Paleta; comprobación de GL, modo de
  presentación y WRAM en cada frame. Se obtiene una fixture local en Ciudad Verde.
- Captura del primer hito: `build/qa/logs/viridian-phase2.png`.
- Pendiente antes de cerrar fase 2: prueba explícita del límite de caché y
  repetición de transiciones, y regresión de menú/interior con el renderer nuevo.
- Fases 3–4 siguen pendientes: ampliar clasificación artística, verificar cambios
  vivos/Corte/carga, cobertura y rendimiento, edificios automáticos, escenarios
  separados y pruebas de entradas/salidas. El goal completo sigue activo.


### 2026-09-19: cierre de las cuatro fases

Ejecución completa reproducible:

```sh
tests/world_qa.sh build/roms/pokeyellow.gbc \
  build/qa/progress.state10 build/qa/fixture.state1
```

Resultado: **PASS**, carpeta `build/qa/kanto-UhdZAc/`; resumen exterior en
`build/qa/logs/full-suite.log`. Las notas anteriores de tareas pendientes son
históricas y quedan resueltas por esta ejecución.

- Compilado `build/pokeyellow3d` y las tres pruebas CTest pasan. El script construye
  también el helper, copia ROM/estados/ejecutable a una carpeta nueva y nunca
  invoca guardado de batería. Los hashes de las entradas privadas se conservan
  en `logs/inputs.sha256` dentro de la carpeta QA.
- `logs/maps.csv`: 36 mapas conectados, 34 Overworld + 2 Plateau, más Bosque Verde
  y muelle. Cero ciclos contradictorios. Pruebas de bancos, rangos, dimensiones,
  entradas corruptas y disponibilidad de los recursos en el manifiesto.
- `logs/journey.log`: Paleta–Ruta 1–Ciudad Verde y regreso, cuatro cruces de mapa,
  cinco victorias, 12.445 frames. Incluye el desplazamiento lateral de Ruta 1 a
  Ciudad Verde, cornisas, combate 2D, vuelta a 3D, caché de hasta cinco mapas y
  ausencia de reconstrucciones mientras se está quieto.
- `logs/horizontal.log`: Ciudad Verde–Ruta 22 y regreso por controles normales;
  continuidad de origen comprobada en ambos sentidos.
- `logs/camera.log`, `logs/viridian-camera.ppm` y `logs/house.log`: F2, cámara,
  menús, colisiones, marcha continua y entrada a una casa en 2D.
- `logs/cut-action.log` y `logs/cut-reload.log`: el motor original ejecuta Corte
  desde su menú, elimina un árbol de Ciudad Verde y reduce su malla. Cargar el
  estado anterior restaura la geometría exacta; solo se reconstruye el mapa
  afectado. Los bloques inválidos se rechazan antes de dibujar.
- `logs/bike-movement.log` y `logs/surf-movement.log`: uso y movimiento con la
  bicicleta y surf; al llegar a tierra el motor abandona surf. Las fixtures
  conceden las habilidades/objetos y medallas necesarios exclusivamente en el
  helper; las escenas de producción no escriben memoria para funcionar.
- `logs/forest-return.log`: Bosque Verde → acceso interior 2D → Bosque Verde.
  `logs/dock-return.log`: muelle → Carmín → muelle → barco interior 2D. Para
  preparar estas fixtures se solicita al motor un warp por script; los cruces
  posteriores son movimiento normal con los datos cargados por el motor.
- `logs/geometry.csv`: 140 edificios con dimensiones válidas; cero puertas
  frontales descubiertas o desalineadas. Las dos entradas de cueva de Ruta 23
  se resuelven como portales. Se registran terreno clasificado y sólidos que
  conservan el gráfico original en plano.
- `logs/catalog.log` y `logs/catalog/map-ID.ppm`: se construyen y presentan las
  38 escenas, expulsando cada malla anterior. Se han revisado sus capturas,
  con detalle adicional en los cinco pueblos exigidos, Plateau, bosque y muelle.
  Ningún error OpenGL ni escritura del renderer en WRAM en las comprobaciones.
- Rendimiento reproducible con 30 frames medidos por escena, tras calentamiento,
  GPU Intel UHD 620 y `glFinish`. Medición inicial: 1,59–2,91 ms por presentación
  de un mapa completo; mayor malla 4.846.392 bytes (Bosque Verde). Es coste del
  renderer, no rendimiento total de la CPU del juego. El script conserva la
  repetición completa y la identificación de GPU.
- `PALLET3D.md` y `README.md` actualizados con cobertura, arquitectura, controles,
  ejecución, pruebas y límites. El runtime sigue fijado al commit original;
  no se modifican sus fuentes descargadas ni el C recompilado del juego.

Límites explícitos del entregable: agua y vegetación estáticas; detalles sin
regla volumétrica, muelle y barco mantienen gráficos originales en plano.
Interiores y combates continúan en 2D. Hay auditoría completa de geometría y
recorridos reales representativos; no se afirma haber completado toda la historia.
Una interfaz de extensión formal del runtime queda como mejora futura.
