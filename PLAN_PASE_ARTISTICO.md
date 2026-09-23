# Plan: pase artístico e iluminación

Fecha: 2026-09-22; actualización: 2026-09-23. Estado: A1 validada
(3/24 criterios); siguientes fases pendientes. Desarrolla el punto 3 de
`PLAN_MEJORAS.md`.

## Análisis del estado actual

Punto de partida verificado en `src/pallet3d.cpp`, `src/world_scene.h`,
`src/interior_scene.h`, `src/daylight.h` y `PALLET3D.md`, sobre el commit
`7955aaa0ddbba3d9e2fd78085902be167faccf23` (base denominada v0.4.1):

| Elemento | Presentación actual | Mecanismo |
| --- | --- | --- |
| Suelo, caminos, arena | Plano con el tile original de 8 píxeles | `create_map` emite un quad por casilla con UV del atlas de 512×512 |
| Árboles, rocas, vallas, carteles | Árboles con tronco y tres capas de cajas; rocas escalonadas y decoración por familia | `box` y `quad` por familia de `pallet::Terrain`; C1 mejora sus siluetas |
| Hierba alta | Quads elevados con viento existente | Rama `Terrain::Grass` en `create_map` |
| Casas y edificios | Prisma con tejado plano y laterales de tiles repetidos | `make_house` sobre las 140 regiones de `pallet::houses` |
| Cornisas | Borde visual de cajas de hasta 0,32 unidades; suelo transitable plano | `Terrain::Ledge`; el salto añade balanceo de cámara |
| Agua y flores | Animadas a la cadencia original desde 5A | `tile_animation.h` sube solo las celdas cambiadas del atlas |
| Interiores | Paredes de dos unidades, mobiliario como cajas con altura por familia | `interior::classify`, `Kind::{Floor, Warp, Wall, Furniture, Counter, Water}` |
| Casillas de interior sin regla | Planas con su tile original; 2.960 en 104 interiores | Columna `unclassified_flat_cells` de `build/qa/logs/interiors.csv` |
| Iluminación | Lambert por vértice con sol, ambiente, niebla y ventanas emisivas | `daynight::Light`; `Vertex::normal` viaja como `material.xyz` |
| Sombras y oclusión | Ninguna; solo una sombra plana bajo actores | Función `shadow` en `pallet3d.cpp` |
| Actores | Billboards con los sprites de la ROM | `sprite_image` desde VRAM y hojas de la ROM |
| Pantallas completas | PC, equipo/resumen, mochila/tienda/Pokédex, opciones, entrenador y nombres integrados; ajuste clásico conservado | A1–A4 de `PLAN_PANTALLAS_COMPLETAS.md`; detección y fallback atómico |

Hechos que condicionan el diseño:

- Base de ejecución: `main` en `7955aaa`. El remoto no tiene aún un tag
  `v0.4.1`; se fija el SHA completo anterior como referencia inmutable,
  sin crear ni mover etiquetas ajenas a este plan.
- v0.4.0 añadió las pantallas completas integradas. v0.4.1 corrige la lista
  Pokédex sobre Esc, el contexto de UseItem al volver al mundo, el resumen
  durante descompresión/PlayCry y las cargas en frío dentro de equipo/mochila.
  `can_compose_menu` admite esas pantallas sin escena retenida si valida el
  mapa; el equipo puede usar fondo neutro porque sus iconos sustituyen VRAM.
  Se preservan estas rutas y el guard `menu_open`: ningún elemento nuevo se
  añade al foreground de ImGui mientras Esc está abierto.
- La comparación desactivada se ejecuta contra `7955aaa` en ambos estilos
  de menú y ambas cámaras antes y después de **cada** fase. Para clásico
  se contrasta además con las 2.765 capturas de v0.3.0 y las 38 exteriores
  originales. Una diferencia previa a la implementación se investiga y
  registra; nunca se atribuye al pase ni se acepta silenciosamente.

- El vértice ya lleva posición, UV, color, viento, normal y emisivo, y la
  normal se calcula por cara en `surface_normal`. La luz direccional existe;
  lo que falta es que la geometría la aproveche: sin sombras proyectadas ni
  oclusión, el sol solo tiñe cada cara por su orientación.
- El contexto es OpenGL ES 2. No hay garantía de textura de profundidad ni
  de múltiples destinos de render. Un mapa de sombras debe escribir la
  profundidad codificada en RGBA8 dentro de un framebuffer propio y leerla
  con un muestreo de 2×2. Es una técnica conocida y cabe en el presupuesto.
- Un único programa dibuja mundo, interiores, combate y título. La
  documentación histórica mide 1,42 ms en ortográfica y 3,64 ms en primera
  persona con cinco mapas sobre Intel UHD 620 a 800×720. Esos datos no
  sustituyen la nueva medición de `7955aaa` en este equipo. El límite es 2,0×.
- Las 38 capturas exteriores y las 179 de interiores son la puerta de
  regresión de todos los planes anteriores. Este plan cambia el aspecto por
  definición; necesita un ajuste que conserve la imagen actual y una
  referencia nueva para el aspecto activado.
- Toda textura reescalada o retocada a partir de los tiles de la ROM es obra
  derivada y no puede incorporarse al repositorio. Las mallas y materiales
  generados por código sí; los assets externos solo como carpeta opcional
  del usuario.
- El ajuste de iluminación y el de estilo de menús ya se guardan en
  `ui_preferences.h` con lectura compatible hacia atrás; el ajuste nuevo
  sigue ese mismo mecanismo.

## Objetivo y alcance

Que el mundo deje de ser un diorama de cajas: volumen por familia de tile,
sombras proyectadas por el sol, oclusión donde se juntan sólidos, materiales
que distinguen agua, cristal, tejado y tierra, e interiores con mobiliario
completo y luces de sala. Todo generado desde los datos de la ROM y por
código, conservando cada píxel de las texturas originales. El motor sigue
siendo la única autoridad; nada de este plan escribe en memoria ni cambia
colisiones, alturas transitables o lógica del juego.

Quedan fuera: modelos 3D de Pokémon o personajes, texturas de alta
resolución dentro del repositorio, cambios en el runtime descargado o en el
C generado, y cualquier alteración del movimiento por casillas.

## Decisiones de arquitectura

1. Un ajuste persistente "Pase artístico" con dos valores: desactivado, que
   produce la imagen de v0.4.1 byte a byte salvo las excepciones autorizadas
   de Esc y del panel residual de transición descritas en Validación, y
   activado, que es el objeto de este plan. Valor por defecto activado.
   La referencia clásica se conserva
   para las pruebas; el modo activado obtiene su propia referencia archivada
   tras revisión.
2. Un manifiesto de familias en `src/art_manifest.h`: para cada familia de
   terreno exterior y cada `Kind` de interior, la malla procedural, el
   material y el tratamiento de oclusión. El manifiesto es una tabla de
   datos, no código disperso; añadir una familia es añadir una fila.
3. Toda malla nueva se genera por código a partir del tile y de sus vecinos,
   dentro de `create_map` e `interior_map`, sin ficheros de modelo. Las
   texturas siguen siendo las del atlas de la ROM, incluidas las animadas.
4. Sombras por mapa de sombras desde `sun_direction`: un segundo programa
   escribe la profundidad en RGBA8 en un FBO de 1024×1024 ortográfico que
   cubre los mapas residentes; el programa principal muestrea 2×2 con sesgo
   por pendiente. Sin sol, por la noche o con el ajuste desactivado, no se
   dibuja el paso.
5. Oclusión ambiental precalculada por vértice al generar cada malla, según
   los sólidos vecinos en un radio de una casilla. Va en el canal de color
   existente; no añade atributos ni pasos de render.
6. Materiales por familia con parámetros explícitos. En la base,
   `material.xyz` contiene la normal y `.w` el emisivo: **no hay canales
   libres**. Se conserva ese contrato y se añaden parámetros dedicados
   medidos dentro del límite GLES2; no se sobrescriben normales/emisivo.
   Un único programa principal con selección uniforme del pase.
7. Las cornisas ganan un escalón visual de medio tile en la cara sur de la
   casilla de cornisa. El suelo transitable no cambia de altura; el jugador,
   los NPC y la cámara mantienen su cota actual.
8. Luces de sala en interiores: hasta cuatro luces puntuales por escena,
   colocadas en lámparas y ventanas clasificadas, con atenuación en el
   fragment shader. Solo en interiores y solo con el pase activado.
9. Presupuesto medido, no estimado: cada fase repite la medición de catálogo
   y de primera persona y registra el múltiplo respecto a v0.4.1. Ninguna
   fase se cierra por encima de 2,0×.
10. Cualquier familia sin fila en el manifiesto, cualquier asset opcional
    ausente y cualquier fallo del FBO de sombras cae a la presentación
    actual completa, sin mezcla parcial de geometría o materiales. La
    preparación valida todos los recursos antes de activar el pase para la
    escena; un FBO incompleto o una familia no soportada invalida el conjunto.
    En E1 la ausencia de carpeta o un fichero inválido conserva íntegra la
    referencia del pase activado sin sustituciones parciales.

## Bloque A: datos y referencia

### Fase A1: ajuste, manifiesto y referencias

Trabajo:

- Añadir el ajuste "Pase artístico" al bloque de ajustes y a
  `ui_preferences.h`, con test de carga de versiones anteriores.
- Crear `src/art_manifest.h` con una fila por familia de `pallet::Terrain` y
  por `interior::Kind`, inicialmente con los valores que reproducen la
  geometría actual. Un test comprueba que ninguna familia queda sin fila.
- Añadir a `pallet_render_smoke` la variante `-art` de los modos `catalog`,
  `journey`, `interior-catalog` y `firstperson`, y `tests/art_qa.sh` que las
  ejecuta en ambas cámaras, mide rendimiento y archiva capturas.
- Establecer la referencia del modo activado: las 38 exteriores y las 179
  de interiores con el pase activado, revisadas y archivadas bajo
  `build/qa/art-reference/` fuera del repositorio.

Criterios de aceptación:

- [x] Con el pase desactivado, las 38 capturas exteriores, las 179 de interiores y las baterías de menús, combate, PC, Pokédex y título son idénticas byte a byte a v0.4.1, salvo el panel de Esc y la eliminación autorizada de paneles residuales durante transiciones.
- [x] Un fichero de preferencias de cualquier versión anterior sigue cargando y el ajuste se conserva tras reiniciar.
- [x] El manifiesto cubre todas las familias y `tests/art_qa.sh` produce las capturas y las medidas de rendimiento de ambos modos.

### Fase A2: clasificación completa de interiores

Trabajo:

- Recorrer las 2.960 casillas de `unclassified_flat_cells` agrupadas por
  tileset y por gráfico, y asignar familia con altura y tratamiento:
  estanterías, camas, mesas, sillas, plantas, mostradores, máquinas,
  ordenadores, alfombras, cuadros, escaleras, rocas y agua interior.
- Añadir familias nuevas a `interior::Kind` solo cuando ninguna
  existente encaje; documentar cada regla con el tileset y el índice.
- Repetir `interior_audit` y la batería de interiores; el CSV debe dar cero
  en `unclassified_flat_cells` para los 179 interiores.
- Revisar contactos de los 25 tilesets con el pase activado: el audit de
  `7955aaa` confirma 21 usados en los 179 interiores y cuatro solo exteriores;
  los contactos identifican esa distinción sin inventar interiores.

Criterios de aceptación:

- [ ] `build/qa/logs/interiors.csv` registra cero casillas sin clasificar en los 179 interiores y cero gráficos desconocidos.
- [ ] Ningún mueble oculta permanentemente al jugador ni bloquea la lectura de un cuadro de texto en ambas cámaras.
- [ ] Contactos de los 25 tilesets revisados (21 de interiores y cuatro exteriores), y la batería de los 179 interiores en PASS en ambos modos.

## Bloque B: iluminación

### Fase B1: mapa de sombras

Trabajo:

- Programa de profundidad y FBO RGBA8 de 1024×1024 con proyección
  ortográfica alineada a `sun_direction`, que cubre los mapas residentes;
  recorte a la ventana visible en ortográfica y al cono de la cámara en
  primera persona.
- Muestreo 2×2 con sesgo por pendiente en el programa principal; sombra
  aplicada solo a la componente directa, nunca a la ambiente.
- Los billboards de actores proyectan sombra con su silueta alfa; el
  jugador la proyecta también en primera persona aunque no se dibuje.
- Verificación de errores del FBO en la inicialización: si falla, se
  invalida el pase artístico completo de la escena y se registra; se dibuja
  la presentación de referencia sin resultados parciales.

Criterios de aceptación:

- [ ] Árboles, casas, rocas, vallas y actores proyectan sombra coherente con la hora en Paleta, Ruta 1 y Ciudad Verde, revisada en amanecer, mediodía y atardecer en ambas cámaras.
- [ ] Sin sol, de noche o con el pase desactivado no se ejecuta el paso de sombras y la imagen coincide con la referencia correspondiente.
- [ ] Presentación media de catálogo y de primera persona por debajo de 2,0× respecto a v0.4.1 en el mismo equipo.

### Fase B2: oclusión ambiental y sombra de contacto

Trabajo:

- Oclusión por vértice calculada en `create_map` e `interior_map` a partir
  de los sólidos vecinos: esquinas interiores, base de paredes, bajo aleros
  y entre árboles contiguos.
- Sombra de contacto suave bajo actores y mobiliario, sustituyendo a la
  sombra plana actual cuando el pase está activado.
- El cálculo se hace una vez por malla y se cachea con ella; no añade coste
  por frame.

Criterios de aceptación:

- [ ] Las bases de paredes, las esquinas de habitaciones y los bosques densos muestran oclusión visible en los contactos revisados.
- [ ] El tiempo de construcción de malla por mapa no supera 1,5× el de v0.4.1.
- [ ] Cero errores OpenGL y memoria intacta por frame en todas las baterías.

## Bloque C: exteriores

### Fase C1: mallas procedurales por familia

Trabajo:

- Árboles: copa en dos o tres capas con silueta redondeada y tronco
  visible; variación de escala del 10 % por semilla derivada de la posición,
  determinista. Árboles altos con una capa más. Árbol de Corte con la misma
  silueta y su tocón.
- Edificios: tejado a dos aguas o a cuatro según el ancho de la región,
  alero, marco de puerta y ventanas con relieve, chimenea donde el tile la
  tiene. Los tejados planos de Silph, el centro comercial y el gimnasio de
  Plateada se conservan por manifiesto.
- Rocas facetadas, vallas con postes y travesaños, carteles con soporte,
  farolas con luz emisiva de noche.
- Cornisas con escalón visual de medio tile en la cara sur, sin cambiar la
  cota del suelo transitable.
- Hierba alta con relieve corto y viento existente; bordes de camino
  suavizados con una franja de transición.

Criterios de aceptación:

- [ ] Contactos revisados de Paleta, Ciudad Verde, Plateada, Azulona, Azafrán, Bosque Verde y el muelle en ambas cámaras y en tres horas del día.
- [ ] Ninguna malla nueva invade una casilla transitable ni oculta permanentemente al jugador; los recorridos de Kanto, primera persona y Corte pasan sin cambios en el estado del motor.
- [ ] Vértices por mapa por debajo de 2,0× respecto a v0.4.1 y presentación por debajo de 2,0×.

### Fase C2: materiales

Trabajo:

- Rugosidad y especular por familia en el manifiesto: agua y cristal
  reflejan el sol y el cielo; tejado, tierra y hierba no.
- Agua con normales animadas derivadas de la fase de `tile_animation.h`,
  sin reloj propio; reflejo del color del cielo de `daynight::Light`.
- Ventanas: el brillo de noche ya existe; se añade halo suave alrededor del
  cristal emisivo.
- Cielo con sol visible en primera persona a la posición que indica
  `sun_direction`.

Criterios de aceptación:

- [ ] El agua muestra reflejo especular coherente con la hora y su animación sigue idéntica a la VRAM original frame a frame.
- [ ] Cristal, agua y tejado se distinguen en los contactos por su respuesta a la luz, no solo por su textura.
- [ ] La batería de animación de tiles y la de iluminación pasan en ambos modos.

## Bloque D: interiores

### Fase D1: mobiliario, zócalo, techo y luces de sala

Trabajo:

- Mallas por familia de interior según el manifiesto y la clasificación de
  A2: estanterías con baldas, camas con cabecero, mesas con patas, plantas
  con maceta, mostradores con frente, máquinas y ordenadores con pantalla
  emisiva.
- Zócalo en la base de las paredes, techo con el color del tileset y
  oclusión en las esquinas.
- Hasta cuatro luces puntuales por escena, en lámparas, ventanas y
  pantallas, con atenuación; por la noche las ventanas de interior dejan de
  emitir luz de día.
- Cuevas: rocas facetadas, techo bajo con niebla existente y agua interior
  con la geometría y el material base; C2 incorpora después su material
  definitivo y repite los recorridos de cuevas (D1 precede a C2).

Criterios de aceptación:

- [ ] La casa del jugador, el laboratorio, el centro Pokémon, la tienda, el gimnasio y una cueva revisados en ambas cámaras con el pase activado.
- [ ] Las baterías de interiores, PC y menús pasan en ambos modos sin cambios en el estado del motor.
- [ ] Presentación de interiores por debajo de 2,0× respecto a v0.4.1.

## Bloque E: assets opcionales

### Fase E1: carpeta de assets del usuario

Trabajo:

- Carpeta opcional junto a las preferencias con texturas por familia y por
  índice de tile, cargadas al iniciar si existen y verificadas por
  dimensiones múltiplo de 8. Nunca se distribuyen con el repositorio ni con
  la release.
- Sustitución en el atlas con mipmaps y filtrado configurable; las
  animaciones originales siguen marcando la fase.
- Documentar el formato en `PALLET3D.md` con un ejemplo generado por código,
  no derivado de la ROM.

Criterios de aceptación:

- [ ] Con la carpeta ausente o con un fichero inválido, la imagen coincide con la referencia del pase activado.
- [ ] Con una textura de prueba generada por código, la familia correspondiente la muestra y el resto no cambia.
- [ ] La release no contiene ningún asset y el manifiesto lo declara.

## Limitaciones asumidas

- Los Pokémon, el jugador y los NPC siguen siendo billboards con los sprites
  originales; ganan sombra y contacto, no volumen.
- Las alturas siguen siendo interpretaciones visuales. Las cornisas no
  cambian de nivel real.
- El mapa de sombras cubre los mapas residentes; en primera persona, los
  objetos más allá de la niebla no proyectan sombra.
- Cuatro luces puntuales por escena como máximo; salas con más lámparas
  agrupan las cercanas.
- Sin assets externos, el detalle de textura es el de la ROM: 8 píxeles por
  casilla. El plan mejora volumen, luz y material, no resolución.

## Validación y entrega

- Una rama y un PR por fase, en el orden A1, B1, B2, A2, D1, C1, C2, E1.
  Ninguna fase posterior comienza antes de fusionar la anterior con CI Linux
  y Windows/MSVC verde. El plan se incorpora primero mediante PR.
- CTest completo, `ctest -LE rom` sin ROM, `clang-format` y todas las
  baterías `tests/*_qa.sh` con el pase desactivado antes y después de cada
  fase; capturas idénticas a `7955aaa` en ambos estilos y cámaras; además,
  clásico idéntico a v0.3.0 (2.765 capturas y 38 exteriores).
  Las únicas excepciones autorizadas son la interfaz del panel de Esc y la
  eliminación de la decoración de menús residuales durante las transiciones
  del juego. Esta última se limita a los paneles de cierre integrados que
  quedaban sobre el fundido; no permite diferencias del mundo ni del estado
  del motor. Cada diferencia se registra y verifica, sin tolerancias generales.
- `tests/art_qa.sh` con el pase activado tras cada fase: catálogo exterior,
  catálogo de interiores, recorridos de Kanto y primera persona, tres horas
  del día, medición de rendimiento y comparación con la referencia
  archivada de la fase anterior. Las diferencias esperadas de cada fase se
  revisan y se archivan como referencia nueva.
- Pruebas unitarias del manifiesto, de la oclusión por vértice con mapas
  sintéticos, del empaquetado de profundidad y de la carga de preferencias.
- Fixtures privadas bajo `build/qa/`; ninguna en el repositorio. Tests con
  ROM registrados en CTest con etiqueta `rom` y `SKIP_RETURN_CODE 77`;
  ejecución con ROM sin saltos, ejecución sin ROM con exclusión explícita.
- Oráculos independientes, controles negativos y comprobación por frame de
  memoria intacta y cero errores GL. Animación por `ctx->cycles` o fase
  original de `tile_animation.h`; Esc, foco y carga en frío/a mitad de fase.
- Rendimiento antes/después en el mismo equipo contra el binario congelado
  `7955aaa`: presentación <=2,0× en cada fase y malla <=1,5× en B2. Archivar
  muestras, entorno, comandos y hashes; no cerrar por estimaciones ni por
  comparaciones contra una fase intermedia.
- Ninguna ROM, partida, savestate ni asset derivado entra en git, paquetes o
  capturas públicas. Los contactos del juego permanecen privados. Las nuevas
  capturas públicas de `docs/screenshots/` usan fixtures sintéticas generadas
  por código con el binario final y se identifican como tales.
- Entregar `build/pokeyellow3d`, `PALLET3D.md` con el manifiesto, las
  medidas y los límites, `README.md` con capturas nuevas en
  `docs/screenshots/`, y contactos revisados en `build/qa/logs/`.
- Marcar las casillas solo con evidencia registrada, contactos revisados,
  hashes de binarios y comandos reproducibles por fase.
- Preparar v0.5.0 y sus paquetes Linux/Windows, inspeccionar contenido y
  checksums. **No publicar ni enviar un tag `v*`** (dispara publicación
  automática) hasta recibir confirmación explícita del usuario sobre la
  entrega terminada. No modificar protección de `main` sin petición.

## Orden recomendado

1. A1, porque el ajuste y la referencia hacen reversible todo lo demás.
2. B1 y B2, que son el cambio más visible y no necesitan geometría nueva.
3. A2 y luego D1, para que los interiores reciban las luces sobre la
   clasificación completa.
4. C1 y C2, que cambian más vértices y se miden con las sombras ya activas.
5. E1 al final, porque solo añade una vía de carga.

## Referencias técnicas

- `src/pallet3d.cpp`: `create_map`, `interior_map`, `make_house`, `box`,
  `surface_normal`, `shadow`, y los dos shaders actuales.
- `src/world_scene.h`: `Terrain` y `houses`; `src/interior_scene.h`: `Kind`.
- `src/daylight.h`: `Light` con sol, ambiente, directa, niebla y ventanas.
- `src/tile_animation.h`: fase de agua y flores para las normales animadas.
- `PALLET3D.md`, sección "Rendimiento medido": referencia de tiempos.
- [Mapas de sombras en OpenGL ES 2 con profundidad en RGBA](https://registry.khronos.org/OpenGL-Refpages/es2.0/): sin extensión de textura de profundidad garantizada.
- [Tilesets y colisiones](https://github.com/pret/pokeyellow/blob/master/data/tilesets/tileset_headers.asm) para la clasificación de interiores.

## Ejecución como goal

Objetivo vigente: ejecutar las ocho fases en el orden indicado partiendo de
`7955aaa`, con los gates de validación y publicación anteriores. El texto
completo del goal queda en el adjunto privado de la conversación; este plan
es el registro versionado de alcance, decisiones y evidencia.

## Registro de ejecución

### Preparación del plan — 2026-09-22

- Verificado `git status --short --branch`, `git log -5 --oneline` y
  `git show --stat 7955aaa`: main coincide con la base solicitada, el plan
  todavía no estaba versionado. `PLAN_CRYSTAL_3D.md` se mantiene ajeno.
- Leídas las instrucciones AGENTS.md aplicables y consultado CodeGraph para
  `can_compose_menu`, `menu_open`, preferencias y vértices. El índice avisó
  de cambios en `pallet3d.cpp`; se leyó el archivo actual según ese aviso.
- Corregidos análisis, referencia v0.4.1, campos de material ocupados,
  namespace `interior`, dependencia D1/C2, fallback completo, publicación
  pendiente de confirmación y privacidad de las capturas.
- `git ls-remote --tags origin 'refs/tags/v0.4.*'` solo devuelve v0.4.0;
  referencia efectiva fijada a `7955aaa0ddbba3d9e2fd78085902be167faccf23`.
- `build/interior_audit build/roms/pokeyellow.gbc > build/qa/logs/art-baseline-interiors.csv`
  confirma 179 interiores, 2.960 casillas pendientes en 104 mapas, cero
  gráficos desconocidos y 21 tilesets interiores de 25 totales. Resumen en
  `build/qa/logs/art-baseline-interior-summary.json`.
- Corregida la descripción de geometría existente: ya hay troncos/copas de
  cajas, hierba elevada y bordes de cornisa; C1 mejora esa geometría.
- Sin criterios marcados: faltan todavía las ocho fases y su evidencia.

### A1 en curso — ajuste y referencias

- El usuario autoriza cambios para mejorar la presentación visual. La nueva
  opción de Esc se registra como diferencia esperada del panel de ajustes;
  la comparación byte a byte de las imágenes del juego con el pase
  desactivado sigue siendo estricta en ambos estilos y cámaras.
- Plan incorporado por PR #28 con cinco checks verdes antes de comenzar A1.
  Implementación en una worktree aislada mientras `7955aaa` genera las
  referencias privadas; no se recompila su binario con código de A1.
- Ningún criterio de A1 acreditado todavía.

### A1 — recuperación de evidencia y medición reproducible

- Se borró permanentemente `build/qa` durante las regresiones. El usuario
  confirmó el borrado y pidió continuar. Diez archivos privados externos
  conservan 1.238 capturas, sus fixtures y el helper de referencia; el
  proceso interrumpido terminó con error y no acredita el gate completo.
- `build/qa/logs/art-recovery.json` registra los hashes recuperados y las
  rutas privadas. Hay que reconstruir las referencias perdidas de v0.3.0
  y repetir las comparaciones; no se rebajan sus requisitos.
- `tests/art_benchmark.cpp` mide la media de lotes sincronizados con
  `glFinish`, excluyendo del tiempo los controles de memoria y GL por frame.
  El mismo driver se enlaza con las bibliotecas congeladas de `7955aaa` y
  con A1; alterna el orden de ejecución sobre el mismo dispositivo.
  Incluye los catálogos completos y ambas cámaras con cinco mapas residentes.
- La nueva fixture del benchmark queda registrada en CTest con etiqueta
  `rom` y salto explícito 77 cuando falta el material privado.
- La PR #29 sigue en borrador; ninguna fase ni criterio queda cerrado.

- Recuperadas las fixtures de Pokédex, PC y fuente mediante el motor original;
  la base vuelve a pasar 46/46 pruebas. A1 pasa 49/49 y el build independiente
  sin ROM 28/28, sin saltos (`art-a1-recovered-ctest.log` y
  `art-a1-recovered-no-rom-ctest.log`).
- La comparación A1 activado/desactivado de los catálogos conserva 1.736
  imágenes en ambos estilos y cámaras. La revisión detectó encuadres de
  diagnóstico pegados a paredes: el preview ahora busca suelo transitable
  libre de sólidos visuales y cuatro casillas de visión, una vez por mapa.
  La cámara del jugador no cambia. Se regeneran las referencias tras esta
  mejora; el primer pase de contactos no se acredita como definitivo.
- La batería de horas conserva el ajuste artístico al recargar preferencias.
  El benchmark escribe líneas completas para impedir que los mensajes del
  renderer corrompan su formato; las mediciones incompletas se descartaron.

### A1 — referencias y rendimiento anteriores a la corrección (histórico)

- El pase anterior, `build/qa/art-a1-4mwrsvda`, conserva las 1.736 imágenes de los 38 exteriores
  y 179 interiores en ambas cámaras, estilos y estados del ajuste. Activado y
  desactivado son idénticos; los 28 contactos están revisados. También se
  revisaron las cuatro horas y los dos estados de la casilla de Esc.
- Los 217 catálogos ortográficos clásicos y sus 217 estados completos
  coinciden con los archivos recuperados de `7955aaa`; informe
  `art-a1-recovered-catalog-comparison.json`.
- Medias en Intel UHD 620, referencia → A1: exteriores 2,057 → 2,090 ms
  (1,016×), interiores 1,679 → 1,681 ms (1,001×), cinco mapas ortográficos
  4,322 → 4,137 ms (0,957×), cinco mapas en primera persona 4,987 → 4,958 ms
  (0,994×). Tres repeticiones con orden alternado, sincronización GPU y
  controles de memoria/GL fuera del intervalo medido; todas bajo 2×.
- La prueba de persistencia añade once procesos nuevos: escritura y lectura
  de las cuatro combinaciones estilo/pase, más migración real de v1 y v2.
  Se arranca cada lector con el ajuste contrario para comprobar la carga.
  Evidencia: `art-a1-preferences-restart.json`; la ampliación del driver de QA
  no cambia el renderer ni los binarios de aplicación/benchmark ya medidos.
- Aplicación: SHA-256 `6502dbbe0ca27fa8fc9fd7eef9faa8d37861a5f308945eefca6e958145d6f754`.
  Benchmark: `32fe7ce2fbd7ed491c3d7bb5a06a993e0dc3fe1f3d15107be0cc997fc32ae577`.
  Informe completo: `build/qa/logs/art-a1-reference-summary.json`; los
  archivos privados se comprimen y verifican byte a byte antes de retirar
  únicamente los directorios de ejecuciones sustituidas.
- El driver ampliado enlazado con la producción inalterada de v0.3.0 conserva
  las 38 capturas exteriores y las once de PC, junto con todos sus estados,
  frente al driver original. Las baterías completas clásica/integrada están
  en curso. El primer criterio de A1, su fusión y las siete fases siguientes
  permanecen pendientes; la PR #29 sigue en borrador.

Comandos del gate artístico (build y CTest ejecutados por separado):

```sh
cmake --build build --target all pallet_render_smoke art_benchmark --parallel 4
ctest --test-dir build --output-on-failure
ctest --test-dir build/qa/no-rom -LE rom --output-on-failure
python3 tests/art_qa.py build/pallet_render_smoke build/art_benchmark \
  "$ROM" "$PALLET_STATE" "$FIVE_MAP_STATE" "$V041_BENCHMARK" "$QA_OUTPUT"
```

Los caminos absolutos, hashes y comandos concretos de las fixtures privadas
figuran en `inputs.json`, los informes de procedencia del benchmark y
`art-a1-preferences-restart.json`, fuera del repositorio.

### A1 — cobertura reconstruida y defecto previo de la referencia

- Las 27 baterías clásicas reconstruidas alcanzan las 2.765 capturas y los
  2.427 estados del motor: v0.3.0 y `7955aaa` coinciden byte a byte. Se
  conservan los archivos anteriores a cada ampliación de evidencia. El
  inventario está en `build/qa/logs/art-full-regression-summary.json`.
- La batería de retratos ejecuta ahora por defecto el observador DATA por
  fotograma y las dos cargas en frío ya documentadas: `dex-screen` y
  `dex-screen-fp`, incluida la detección de especie/VRAM incoherentes. Estas
  dos comprobaciones faltaban en el ejecutor reconstruido. Se descartó la
  hipótesis inicial sobre la preparación del menú: genera 52 capturas y no
  explicaba el desfase de dos. No se incorporaron esas capturas al recuento.
- Las 38 vistas exteriores de A1 y sus 38 estados coinciden directamente con
  el driver original de v0.3.0; `art-a1-original-v030-catalog.json`. Las pruebas
  adicionales de pausa/carga de paneles integrados también coinciden con
  `7955aaa` en ambas cámaras; `art-menu-motion/result.json`.
- La referencia integrada `7955aaa` falla en `ui_transitions`, salida blanca
  de combate, fotograma 3883: la decoración de cierre deja un panel vacío
  sobre el blanco. Es un fallo reproducido en la producción original. Una
  copia privada que suprime esa decoración durante `warp_overlay` pasa el
  recorrido completo y conserva el estado del motor del mismo fotograma.
  Evidencia privada: `art-transition-probe/result.json` y capturas antes/después.
- Aplicar esa corrección al modo desactivado requería una excepción acotada a
  la identidad visual solicitada. Se preparó y verificó primero en una copia
  privada; la incidencia original no se cuenta como una prueba aprobada.
- Un recorrido diagnóstico completo confirma que A1 desactivado conserva
  exactamente las 18 capturas y los 18 estados de `7955aaa`, incluidos los
  nueve fotogramas rechazados, 3883–3891. El diagnóstico continúa para recoger
  evidencia, pero termina con código 41: no sustituye a una prueba aprobada.
  Informe: `build/qa/logs/art-a1-inherited-transition-comparison.json`.
- Frente a la copia corregida, los nueve estados de las capturas comunes
  permanecen idénticos y solo cambia `warp-01-map-012-bgp-00.ppm`;
  `art-transition-fix-scope.json` delimita esa diferencia. Las 139 piezas de
  evidencia del diagnóstico y de la corrección aislada tienen una copia
  privada fuera de `build/qa`, verificada byte a byte al leer el archivo.

### A1 — corrección de transición autorizada

- El usuario autoriza corregir y documentar la excepción visual acotada de
  los paneles residuales durante transiciones, también con el pase apagado.
  Se suprime únicamente su decoración de cierre mientras `warp_overlay`
  presenta el fundido original. No se modifica el estado del motor.
- Antes de esta corrección, A1 completó las 27 baterías clásicas: 2.755
  imágenes del juego y los 2.427 estados son idénticos a ambas referencias;
  las diez imágenes de Esc son la excepción de interfaz ya autorizada.
  `art-a1-classic-complete.json` registra la auditoría de todos los archivos.
- Los binarios anteriores se conservan mientras terminan sus recorridos.
  La versión corregida requiere su propia validación; las pruebas anteriores
  no acreditan automáticamente el cambio. La fase sigue sin cerrar.
- En `82c039e`, la corrección solo suprime el dibujo de la decoración de
  cierre durante `warp_overlay`; la evolución de la animación continúa.
  Se comprueban ocho recorridos de salida blanca: ambos estilos, ambas
  cámaras y ambos estados del pase, sin relajar los oráculos de transición.
- Frente a la producción original, el estilo clásico conserva todas las
  imágenes y los estados en ambas cámaras. En integrado solo cambia la
  captura `white/logs/warp-01-map-012-bgp-00.ppm` del inventario histórico;
  el recorrido adicional `white-fp` verifica la misma corrección en primera
  persona. Todos los estados siguen siendo idénticos. La imagen corregida
  debe ser blanca en sus 800×720 píxeles; en la original, los píxeles del
  panel están limitados al rectángulo inclusivo (95, 549)–(704, 710).
  El diagnóstico original conserva su salida 41 y sus nueve fotogramas
  rechazados, 3883–3891; nunca se registra como prueba aprobada.
- El verificador de la excepción rechaza un byte distinto del motor, la
  imagen en otra batería, el mismo nombre en otra ruta y un solo píxel no
  blanco en el resultado. No hay tolerancia visual general ni excepción
  para estados. Evidencia: `art-transition-exception-validator.json`,
  `art-a1-fixed-white.json` y `art-original-white-fp.json`.
- El script completo de transiciones, ampliado con `transitions-white-fp`,
  pasa con 132 capturas y 132 estados por estilo; los archivos privados se
  verifican byte a byte tras comprimirlos. Informe:
  `build/qa/logs/art-final-transition-suites.json`. También pasan los 49
  tests con ROM, los 28 sin ROM y los cinco trabajos requeridos de CI de
  Linux/Windows para `82c039e` (ejecución `35791107179`).
- Las regresiones completas, los recorridos acumulados y el gate artístico
  del binario corregido siguen pendientes de cierre. Estos resultados
  parciales no acreditan todavía la fase A1.

### A1 — validación final del binario corregido — 2026-09-23 (3/24 criterios)

La referencia vigente es `build/qa/art-reference/A1`, que apunta al pase
`art-a1-evh9hy4g`. Las cifras anteriores pertenecen a sus respectivos
binarios históricos; no sustituyen las pruebas del binario corregido.

- Clásico: 27/27 baterías, 2.765 capturas y 2.427 estados. Las 2.755 imágenes
  del juego y todos los estados coinciden exactamente con v0.3.0 y
  `7955aaa`; las diez capturas restantes son el panel de Esc autorizado.
- Integrado: 27/27 baterías, 3.022 capturas y 2.684 estados. Coinciden
  exactamente 3.011 imágenes y todos los estados con `7955aaa`. Solo
  difieren las diez capturas de Esc y el extremo blanco delimitado antes.
  La referencia original conserva su fallo; no se cuenta como prueba aprobada.
- Pasan los 82 recorridos acumulados y los dos de animación de menús.
  La batería actual de transiciones añade 132 capturas y 132 estados por
  estilo, incluida primera persona. Los cinco controles negativos de la
  excepción impiden ampliar su alcance.
- Pasan 49/49 tests con ROM y 28/28 sin ROM, sin saltos silenciosos.
  Las tres cargas de preferencias v1/v2 se repiten con el binario corregido;
  el pase final añade ocho procesos de escritura/lectura de v3. Empiezan
  con el valor artístico contrario al esperado.
- Las 1.736 capturas de catálogo y sus estados coinciden entre pase activado
  y desactivado. Los 217 catálogos ortográficos clásicos coinciden con
  v0.4.1; los 38 exteriores también se comparan directamente con el driver
  original de v0.3.0.
- Las 28 hojas y todas sus fuentes a resolución completa son idénticas a
  las previamente revisadas. Se inspeccionan otra vez seis hojas del binario
  final; las otras 22 conservan su revisión mediante esa identidad exacta.
  Se revisan los 16 encuadres de cuatro horas en ambos estilos/cámaras y
  los dos estados de Esc. A1 conserva la geometría existente; la clasificación
  y el mobiliario restantes corresponden a A2/D1.
- Los archivos privados se verifican al leer su contenido descomprimido.
  Una primera copia final se interrumpió con salida 143; se conserva el
  parcial y se completa una copia independiente de 3.775 archivos, incluida
  la aplicación. Los metadatos de revisión se guardan aparte con hashes;
  no se modifican los píxeles ni las fixtures.

Medias de presentación sincronizada en Mesa Intel UHD Graphics 620, tres
repeticiones con referencia/candidato alternados y controles de memoria/GL
fuera del intervalo medido:

| Escenario | v0.4.1 (ms) | A1 (ms) | A1 / v0.4.1 |
| --- | ---: | ---: | ---: |
| 38 exteriores | 2,045 | 2,071 | 1,013× |
| 179 interiores | 2,211 | 2,403 | 1,087× |
| Cinco mapas, ortográfica | 3,416 | 3,198 | 0,936× |
| Cinco mapas, primera persona | 3,521 | 3,425 | 0,973× |

Todos los escenarios están bajo 2,0×. SHA-256 de los binarios finales:

- Aplicación: `59fe9d1d0c00a19b90ac3866de93679813224a11d8fc72af35f7d1f63222b1f5`.
- Driver: `4593eb513d6c6b778c4f4f6a3cd0f77c9a994558ace3886de41cc1099cbf0a0d`.
- Benchmark A1: `753776f6192ac7f97da90fb3785224cb190d63fcc3ed62ba82696c3d6659aea0`.
- Benchmark v0.4.1: `8c21f81a0407c7c0014fb3a3445396096a09772fbd919c1398e9690e4c31d524`.

La producción corresponde a `82c039e`; los commits posteriores hasta esta
validación solo documentan evidencia. Los cinco trabajos requeridos de
Linux/Windows y formato pasan en `80f9b46` (ejecución `35796519524`); el
commit de este cierre debe superar también la CI antes de fusionar la PR
#29. B1 no comienza hasta esa fusión.

Informes privados bajo `build/qa/logs/`: `art-final-regression-summary.json`,
`art-cumulative-a1-fixed-archive.json`, `art-a1-fixed-legacy-preferences.json`,
`art-a1-corrected-catalog-comparisons.json`, `art-a1-final-visual-review.json`
y `art-a1-backup-recovery.json`. El `result.json` del pase final conserva
entradas, muestras completas, medidas y la base de la revisión.

Además del build/CTest y `tests/art_qa.py` documentados antes, los ejecutores
privados conservan los comandos y las adaptaciones del inventario histórico:

```sh
python3 "$EVIDENCE/run-final-a1-regressions.py" a1 classic
python3 "$EVIDENCE/run-final-a1-regressions.py" a1 integrated
python3 "$EVIDENCE/run-cumulative-a1-fixed.py"
python3 "$EVIDENCE/check-fixed-legacy-preferences.py"
python3 "$EVIDENCE/audit-final-a1-regressions.py" --verify-archives --require-complete
```

`EVIDENCE` es la carpeta privada registrada en los informes. Una nueva fase
debe usar salidas nuevas y conservar las referencias. No se incorporan ROM,
partidas ni capturas del juego al repositorio.
