# Plan: pantallas completas con el estilo integrado

Fecha: 2026-09-22. Estado: A1–A2 fusionadas; A3 en desarrollo; A4 pendiente.
12/24 criterios acreditados. Base: release v0.3.0.

## Análisis del estado actual

Inspección sobre `main`, commit
`f6ce1f3e0cf0023daa0020ec852e8c3269fba858`, idéntico al tag `v0.3.0`.
`PLAN_ESTILO_MENUS.md` está cerrado con sus 19/19 criterios. Se conservan
sus implementaciones, pruebas y evidencias; este plan amplía su alcance.

| Situación | Presentación verificada en v0.3.0 | Punto de extensión |
| --- | --- | --- |
| Start, diálogo, tienda parcial, sí/no, guardado | Regiones reconocidas por sus bordes, glifos originales y paneles animados | `menu_layout.h`, `menu_text.h`, `menu_text_gl.h` |
| Equipo, mochila, opciones, tarjeta y nombres | Pantalla LCD completa enmarcada sobre la escena atenuada cuando el compositor conserva el mundo | Rama `menu_full` de `pallet3d.cpp`, `lcd_overlay::framed` |
| PC del centro y del jugador | Monitor con LCD para los menús principales; listas completas enmarcadas en otros estados | `pc_state.h`, `pc3d.h` |
| PC de Bill | Estantería 3D y composición específica de almacenamiento | `pc_boxes.h`, seleccionado desde `pc3d::draw` |
| PC de Oak | Monitor y contadores de Pokédex; el motor ofrece diálogo y elección sí/no, no una lista de inventario | `pc_state::Mode::Oak`, `pc3d.h`, rutina original `OpenOaksPC` |
| Lista de Pokédex | Dispositivo de dos pantallas, retrato y recortes del LCD; algunas regiones de texto ya admiten el tema | `dex3d.h`, `dex_state.h`, `rom_text_region.h` |
| HUD de combate | Tipografía ROM, tokens comunes para paneles, vida y experiencia | `battle3d.h`, `ui_theme.h`, `rom_font.h` |

Hechos que condicionan el diseño:

- `menu_layout::classify` reconoce disposiciones parciales mediante bordes
  visibles y devuelve `Kind::Full` para el resto. Un nuevo detector no puede
  confundir cualquier fondo vacío con una pantalla soportada.
- `menu_text::prepare` conserva la propiedad de cada celda en solapamientos,
  valida los glifos contra VRAM y rechaza gráficos ajenos a la fuente.
  Actualmente solo admite disposiciones parciales y posiciones mundo/combate.
- `pc_state::sample` verifica terminal, tileset y llamadas vivas originales;
  distingue Center, Items, Bill, Oak y Hall. Detectar PC no identifica por sí
  solo todos sus submenús ni acredita su geometría.
- El LCD puede tardar varios frames en mostrar el nuevo `wTileMap`. Los
  retratos, selecciones y textos deben pertenecer al mismo estado visible;
  no se conserva contenido antiguo sobre una pantalla nueva.
- La lista de Pokédex ocupa regiones separadas del dispositivo. Reemplazar
  todo por un panel flotante perdería ese fondo existente.
- La fuente admite índices 80–FF validados contra sus patrones de VRAM.
  Existen gráficos especiales fuera de ese rango —por ejemplo el indicador
  de nivel— y retratos, iconos o barras que no son caracteres. Requieren un
  contrato gráfico explícito; no se aceptan como glifos inventados.
- Las animaciones de paneles existentes usan `ctx->cycles`, congelan con
  Esc o sin foco y separan decoración de texto. El texto se presenta completo
  desde el primer frame en que lo muestra el motor.
- Hay veinte baterías clásicas y `tests/ui_style_qa.sh` con dieciocho
  recorridos integrados, además de pausa/carga en ambas cámaras. Las pruebas
  completas actuales son 38, de ellas 19 ejecutables sin ROM.

## Objetivo y alcance

Extender el estilo integrado a las pantallas completas del PC de Bill,
del jugador y de Oak; equipo y resumen de Pokémon; mochila, tienda completa
y lista de Pokédex; opciones, tarjeta de entrenador y nombres. Conservar
texto, orden, contenido, controles, cursor, desplazamiento y decisiones del
motor original. Presentar cada pantalla sobre su escena apropiada, con el
mismo tema y fuente de la ROM que el resto de la aplicación.

El módulo 3D solo lee `wTileMap`, VRAM, ROM, WRAM, RAM de cartucho y
framebuffer. No reimplementa menús ni escribe en el estado del juego.
El estilo clásico continúa como ajuste persistente y reproduce v0.3.0
byte a byte. No se modifican el runtime descargado ni el C generado.

No se crean opciones, etiquetas, traducciones ni filas que el juego no
muestre. Los casos de este alcance deben implementarse; el respaldo clásico
protege estados desconocidos o transitorios, no sustituye su implementación.
Otras pantallas especiales, como el mapa AREA y el Salón de la Fama,
conservan su composición existente y se documentan expresamente.

## Decisiones de arquitectura

1. Añadir un detector puro de disposiciones completas con identificadores
   de pantalla, rectángulos de texto, regiones gráficas y orden de propiedad.
   Validar bordes y regiones fijas contra savestates privados, siguiendo el
   contrato de `menu_layout.h`. Combinar geometría con el contexto original
   comprobado cuando varias pantallas comparten bordes.
2. Reutilizar `menu_text.h` para preparar y presentar listas y regiones.
   Extenderlo sin alterar las posiciones ni los resultados de los modos
   existentes. El contenido de cada celda procede del `wTileMap` original.
3. El mecanismo común de lista relaciona las filas visibles, el cursor,
   `wCurrentMenuItem` y `wListScrollOffset`; no genera la lista a partir del
   inventario ni aplica su propio scroll. Cantidades y marcadores conservan
   los glifos que el juego dibuja. Verificar direcciones y semántica contra
   las rutinas originales, no asumir que todas las pantallas usan igual RAM.
4. Separar texto validado y gráficos verificados. Retratos e iconos se leen
   de sus datos originales, contrastados con VRAM y selección visible.
   Vida y experiencia usan los tokens del HUD y valores originales con
   unidades/rangos verificados. No reemplazar un símbolo especial por una
   letra parecida ni introducir texto de ImGui.
5. Mantener estantería y monitor como fondo del PC; el dispositivo de
   `dex3d.h` como fondo de su lista; y la escena retenida del juego para las
   demás pantallas. Evitar duplicar los mismos glifos en el LCD y el panel.
6. Una disposición desconocida, geometría incompleta, gráfico no validado,
   tile ajeno a la fuente o fallo de detección usa `lcd_overlay::framed`
   completo en el camino integrado. Validar antes de emitir quads: no hay
   pantallas vacías ni mitad de un menú recompuesto. El camino clásico
   conserva exactamente la composición de v0.3.0, incluidos sus monitores.
7. Reutilizar `menu_motion` para decoración cuando corresponda. Reloj
   `ctx->cycles`, pausa con Esc/pérdida de foco, texto completo desde el
   primer frame, regiones compartidas sin reinicio y carga de menú abierto
   sin animación de entrada. Ninguna animación consume o demora una entrada.
8. Verificar en `pret/pokeyellow`, fijado al commit
   `e89ead154b9968aa50eed9328ff2b38b6c194382`, `charmap.asm` y las rutinas
   pertinentes cada índice especial antes de usarlo. Registrar archivo,
   símbolo, valor, SHA y contraste con los fixtures de esta ROM.
9. Cada disposición tendrá pruebas negativas además de fixtures válidos:
   borde alterado, fuente reemplazada, transición parcial y selección fuera
   de rango. El oráculo de glifos debe verificar el resultado presentado,
   su cobertura y su relación con la fuente; no solo repetir el detector.

## Bloque A: pantallas completas

### Fase A1: listas completas de PC

Trabajo:

- Inventariar y capturar las disposiciones de Bill, jugador y Oak, incluidos
  selección de terminal, almacenar/retirar/liberar, cambio de caja, objetos,
  cantidades, confirmaciones y evaluación de Pokédex.
- Verificar llamadas, variables de selección/scroll y gráficos especiales
  contra las rutinas originales. Oak conserva sus diálogos y decisiones.
- Crear el detector completo y el mecanismo de listas reutilizable;
  integrar `menu_text` sobre estantería y monitor sin duplicar contenido.
- Añadir recorridos PC integrados a `ui_style_qa.sh` en ambas cámaras,
  cubriendo listas largas, extremos, cantidades, cancelación y respaldos.

Criterios de aceptación:

- [x] Bill, jugador y Oak presentan todas las disposiciones inventariadas de esta fase con paneles y glifos del tema sobre sus fondos de PC, y completan sus operaciones originales en ambas cámaras.
- [x] Cursor, scroll, cantidades, cambio de caja y confirmaciones coinciden por frame con los tiles y las variables originales verificadas; no hay filas omitidas, reordenadas o duplicadas.
- [x] Las disposiciones y gráficos no reconocidos vuelven al LCD completo enmarcado, con pruebas negativas y transiciones sin pérdida de contenido.
- [x] Los recorridos PC añadidos a `ui_style_qa.sh` pasan el oráculo glifo a glifo, cursor, OpenGL y memoria intacta por frame en ambas cámaras.
- [x] CTest completo, build independiente sin ROM con `ctest -LE rom`, formato y todas las baterías clásicas pasan; sus capturas y las 38 exteriores son idénticas a v0.3.0.
- [x] Contactos PC revisados y comandos reproducibles están registrados; la PR de A1 supera CI y se fusiona antes de comenzar A2.

### Fase A2: equipo y resumen de Pokémon

Trabajo:

- Verificar las disposiciones de equipo y páginas del resumen: selección,
  datos, estadísticas y movimientos, incluidas variantes de estado, nivel y
  cambio de Pokémon. Reutilizar la lista cuando proceda.
- Presentar retratos originales y barras de vida/experiencia con los tokens
  del HUD de combate, conservando todos los textos y números originales.
  Verificar especie, HP, experiencia y relación con la selección visible.
- Añadir recorridos de entrada, navegación, cambio de página y salida en
  ambas cámaras, con comprobaciones de gráficos y de texto independientes.

Criterios de aceptación:

- [x] Equipo y todas las páginas del resumen inventariadas presentan su contenido original completo y permiten las acciones y navegación originales en ambas cámaras.
- [x] Retratos, HP y experiencia corresponden al Pokémon seleccionado, usan los tokens del HUD y coinciden con los valores y gráficos originales verificados, incluidos límites y estados alterados.
- [x] Cada carácter y cursor coincide por frame con su tile original; los índices especiales están contrastados con charmap y rutinas pret antes de su uso.
- [x] Los recorridos de A2 añadidos a `ui_style_qa.sh` verifican glifos, cursor, memoria intacta, cambios de página y respaldo LCD ante datos o gráficos no reconocidos.
- [x] CTest completo, pruebas independientes sin ROM, formato y todas las baterías clásicas pasan; no cambia ninguna captura clásica respecto a v0.3.0, incluidas las 38 exteriores.
- [x] Contactos de equipo y resumen están revisados y registrados; A2 se fusiona con CI verde antes de iniciar A3.

### Fase A3: mochila, tienda completa y lista de Pokédex

Trabajo:

- Aplicar el mecanismo de listas de A1 a mochila y tienda completa,
  conservando objetos, cantidades, dinero, indicadores, menús de acción y
  confirmaciones del juego sin reconstruir sus cadenas.
- Integrar la lista de Pokédex sobre el dispositivo de `dex3d.h`, incluyendo
  selección, scroll, registros no vistos/vistos/capturados y contadores.
  Conservar la correspondencia de cada nombre con su retrato visible.
- Añadir recorridos en ambas cámaras que crucen límites de scroll y
  alternen listas y subpantallas, con inventarios cortos, largos y vacíos.

Criterios de aceptación:

- [ ] Mochila y tienda completa usan la lista integrada con sus cantidades, dinero, opciones y confirmaciones originales, sin omitir contenido ni modificar operaciones.
- [ ] La Pokédex presenta la lista integrada sobre su dispositivo, con números, nombres, registros, contadores y retrato coherentes con la selección original.
- [ ] Scroll, cursor, extremos y cambios de lista mantienen el orden y la selección del motor por frame; toda disposición o gráfico desconocido conserva el LCD enmarcado.
- [ ] Los recorridos de A3 añadidos a `ui_style_qa.sh` pasan glifo a glifo, cursor, memoria intacta y transiciones en ambas cámaras.
- [ ] CTest completo, pruebas independientes sin ROM, formato y todas las baterías clásicas pasan con capturas idénticas a v0.3.0, incluidas las 38 exteriores.
- [ ] Contactos de mochila, tienda y Pokédex están revisados y registrados; A3 se fusiona con CI verde antes de comenzar A4.

### Fase A4: opciones, tarjeta de entrenador y nombres

Trabajo:

- Verificar e integrar opciones y tarjeta de entrenador, incluidos cursores,
  números, insignias y demás regiones gráficas que realmente muestre la ROM.
- Presentar nombres como cuadrícula del tema, con las celdas, caracteres,
  selección, edición, borrado y finalización originales. Comprobar las
  variantes de nombres de jugador, rival y Pokémon que use el motor.
- Añadir recorridos de ajustes, tarjeta y teclado en ambas cámaras;
  probar pausa con Esc, pérdida de foco, cargas y cambios rápidos de pantalla.
- Completar la tabla de disposiciones y respaldos en `PALLET3D.md`, capturas
  nuevas del README bajo `docs/screenshots/` y contactos privados revisados.

Criterios de aceptación:

- [ ] Opciones y tarjeta muestran todos sus datos y gráficos originales con el tema; los ajustes siguen siendo decisiones del motor y las variantes no reconocidas conservan el LCD enmarcado.
- [ ] Las pantallas de nombres usan la cuadrícula del tema con el cursor original, conservando caracteres, orden, edición, borrado y aceptación en las variantes de jugador, rival y Pokémon.
- [ ] Los recorridos de A4 añadidos a `ui_style_qa.sh` pasan glifo a glifo, cursor y memoria intacta por frame en ambas cámaras, con respaldo ante detección o fuente inválida.
- [ ] Las animaciones de las nuevas pantallas usan ciclos del motor, se congelan con Esc o sin foco, no retrasan texto ni entradas y respetan cargas y regiones compartidas; hay evidencia reproducible.
- [ ] CTest completo, pruebas independientes sin ROM, formato, todas las baterías clásicas e integradas pasan; todas las referencias clásicas y las 38 exteriores siguen idénticas a v0.3.0.
- [ ] Ejecutable compilado, PALLET3D con tabla completa y casos clásicos, README con capturas nuevas y contactos revisados están entregados; A4 pasa CI y su fusión deja las cuatro fases y todos los criterios en main.

## Limitaciones asumidas

- La referencia es la ROM Yellow de 1 MiB que soporta el proyecto, con sus
  símbolos verificados; no se promete compatibilidad con traducciones o
  ROM modificadas. Un dato que no cumple el contrato cae al LCD.
- Se conserva la fuente de la ROM a escala entera. Los gráficos no son
  caracteres: cada excepción necesita validación propia, nunca una
  tolerancia general que deje pasar tiles desconocidos.
- No se convierte el compositor en un segundo motor de interfaces. No
  añade navegación, entrada de texto nativa ni inventarios independientes.
- Fixtures, savestates y ROM quedan privados en `build/qa/`. Solo se
  publican las capturas seleccionadas para la documentación solicitada.
- El oráculo de glifos integrado mide tinta y cobertura por frame; la
  equivalencia clásica sigue siendo de píxeles exactos, no perceptual.
- El fallback es parte comprobada de cada fase. No permite marcar como
  terminado un tipo de pantalla pedido que siga siempre en clásico.

## Validación y entrega

Base inmutable: tag `v0.3.0`. Los binarios y manifiesto inicial están en
`build/qa/fullscreen-v030/` y `build/qa/logs/fullscreen-v030-baseline.json`.
Las veinte referencias clásicas de C1 son válidas para esta base porque su
código y binarios coinciden con la release; se conservan sus SHA y rutas.
Los recorridos nuevos tendrán su propia referencia ejecutada con v0.3.0,
con el mismo reloj de pruebas y los mismos fixtures. No se regeneran
referencias a partir de una implementación nueva para hacerla pasar.

Tras cada fase:

1. Compilar `build/pokeyellow3d` y el helper de recorridos.
2. Ejecutar CTest completo y `ctest -LE rom` en un build independiente sin
   acceso a ROM/fixtures; registrar cantidad, omisiones y renderizador.
3. Ejecutar clang-format 18.1.8 sobre C++ propio y `git diff --check`.
4. Ejecutar las veinte baterías clásicas `tests/*_qa.sh` y cualquiera nueva
   que se añada; comparar todas las capturas y estados canónicos contra
   v0.3.0, con los ajustes Esc registrados aparte y las 38 exteriores exactas.
5. Ejecutar `tests/ui_style_qa.sh`, ampliado acumulativamente: recorridos
   anteriores y todos los nuevos en ambas cámaras. Cada frame comprueba
   glifos, cursor, WRAM, VRAM, RAM de cartucho, framebuffer y errores GL.
6. Revisar contactos estables y de transición; registrar origen, cámara,
   hash, fixture, disposición, cobertura y observaciones en `build/qa/logs/`.
   Los negativos deben demostrar el LCD de respaldo, no solo ausencia de crash.
7. Registrar comandos, código probado, evidencia y limitaciones; abrir PR,
   esperar todos los checks de CI requeridos y fusionar antes de iniciar
   la siguiente fase. Las casillas se marcan solo con evidencia registrada;
   la comprobación externa de la fusión completa los criterios de entrega.

Entregables finales: `build/pokeyellow3d`, `PALLET3D.md` con disposiciones y
lo que sigue en clásico, `README.md` y nuevas imágenes en
`docs/screenshots/`, contactos revisados bajo `build/qa/logs/`, cuatro fases
en `main` y release `v0.4.0` tras A4. Publicar paquetes Linux y Windows con
sus checksums, verificar el workflow y los archivos publicados, sin ROM ni
partidas. Auditar los 24 criterios y todos estos entregables antes de cerrar
el goal. La publicación no se considera cumplida por crear solamente el tag.

## Orden recomendado

1. Confirmar este plan mediante PR a `main` con CI verde, sin implementación.
2. A1, PC y mecanismo común de listas, y fusión verificada.
3. A2, equipo y resumen, y fusión verificada.
4. A3, mochila, tienda completa y lista Pokédex, y fusión verificada.
5. A4, opciones, tarjeta y nombres; cierre de documentación, fusión y v0.4.0.

Si aparece un bloqueo se documentan evidencia, causa y siguiente acción en
el registro. Se continúa con trabajo independiente de la misma fase sin
saltar fases ni simular su aceptación. El objetivo no se pausa por decisión
del agente y solo se cierra cuando alcance y entregables estén verificados.

## Referencias técnicas

- `PLAN_ESTILO_MENUS.md`: alcance anterior y registro de los 19 criterios.
- `src/menu_layout.h`, `src/menu_text.h`, `src/menu_text_gl.h`: geometría,
  validación de fuente, propiedad de celdas y dibujo actual de regiones.
- `src/menu_motion.h`, `src/ui_theme.h`, `src/rom_font.h`: animación,
  tokens comunes y fuente de la ROM.
- `src/pc_state.h`, `src/pc3d.h`, `src/pc_boxes.h`, `src/pc_details.h`:
  detección de PC, monitor, estantería y datos originales.
- `src/dex_state.h`, `src/dex3d.h`, `src/mon_pic.h`: selección de Pokédex,
  dispositivo y retratos; `src/battle3d.h`: HP y experiencia del HUD.
- `src/pallet3d.cpp`, `src/lcd_overlay.h`: composición y respaldo enmarcado.
- `tests/ui_style_qa.sh`, `tests/ui_style_oracle.h`, pruebas de PC y Pokédex:
  recorridos y oráculos existentes, a ampliar por pantalla.
- Fuente original fijada: `pret/pokeyellow` en
  `e89ead154b9968aa50eed9328ff2b38b6c194382`; `constants/charmap.asm`,
  `ram/wram.asm`, rutinas de PC bajo `engine/menus/` y `engine/pokemon/`,
  y rutinas originales de equipo, resumen, inventario, Pokédex, opciones,
  entrenador y nombres. Registrar sus rutas/símbolos exactos al verificarlos.

## Registro de ejecución

### Confirmación del plan — 2026-09-22

Base verificada: `main` y `v0.3.0` apuntan a
`f6ce1f3e0cf0023daa0020ec852e8c3269fba858`; árbol limpio antes de crear
`fullscreen-plan`. El plan anterior tiene sus 19 casillas marcadas.
Inspección CodeGraph de `pc_state.h`, `menu_layout.h`, `menu_text.h`,
`pc3d.h`, `dex3d.h` y `ui_theme.h`, seguida de lectura de la integración
actual y los drivers de pruebas. No se ha modificado código.

El manifiesto `build/qa/logs/fullscreen-v030-baseline.json` fija los binarios
de v0.3.0 y las veinte referencias clásicas, reutilizando únicamente evidencia
cuyo SHA coincide con la release. La batería C1 cerró con 38/38 y 19/19 sin
ROM; esas cifras describen la base, no acreditan ninguna fase nueva.

Comandos de comprobación inicial:

```sh
git status --short --branch
git rev-parse HEAD 'v0.3.0^{}'
git switch -c fullscreen-plan
git diff --check
```

Esta primera PR solo añade el plan. A1 comenzará después de su fusión con
CI verde; todos los criterios permanecen sin marcar hasta reunir evidencia.


### A1 — implementación y primeras verificaciones, 2026-09-22

El plan se fusionó antes de implementar: PR #22, merge
`22519197a55391c53c74e090bb32020e626a0d83`, cinco comprobaciones obligatorias
verdes en el run `35670637409`. A1 trabaja en `fullscreen-a1` desde ese merge.

`full_menu_layout.h` reconoce las ventanas originales superpuestas y conserva
sus coordenadas. `menu_text` añade una cuadrícula completa compartida y valida
la fuente antes de dibujar; `full_menu_gl.h` conecta Bill, jugador y Oak a sus
fondos existentes. Un fallo de disposición, selección, fuente o gráfico vuelve
al LCD completo enmarcado. No cambia el runtime ni el C generado del juego.

El inventario privado contiene cincuenta tilemaps cotejados con sus savestates.
Los gráficos LV y caja ocupada se contrastaron con los PNG originales de pret,
la ROM y ambos planos de VRAM antes de incorporarlos. Referencias y SHA:
`fullscreen-pc-source-inventory.json`, `fullscreen-pc-graphics-proof.json` y
`fullscreen-pc-cursor-address-proof.json`, bajo `build/qa/logs/`. Los offsets son
`10AE0` y `3AA28`; TM01–TM50 son `C9`–`FA`, y la cantidad original se lee en
`CF95`. La lista reutilizable lee selección y scroll en `CC26` y `CC36`.

Las pruebas añaden doce recorridos PC en ambas cámaras a `ui_style_qa.sh`.
Incluyen veinte Pokémon, cincuenta objetos, cantidades, cancelaciones y controles
negativos de borde, fuente, gráficos y selección. El oráculo compara todos los
bits presentados con la ROM y VRAM, cuenta cada celda para detectar omisiones o
duplicados y verifica el cursor; el guardián por frame conserva WRAM, VRAM,
ERAM y framebuffer. El retorno de ChangeBox requiere respetar el cursor de doce
filas que sigue pintado después de que el motor borre su flag de espaciado.

Resultados parciales, que todavía no cierran la fase:

- CTest completo 39/39 y build independiente sin ROM 20/20; formato 18.1.8.
- Los doce recorridos PC clásicos mantienen 110 capturas y 110 estados exactos
  frente al renderizador de v0.3.0 con el nuevo driver de entradas. Sus archivos
  de producción se compararon con el tag; un recorrido previo conserva además
  11/11 capturas y estados frente al helper original. Evidencia:
  `fullscreen-v030-new-driver-proof.json`,
  `fullscreen-v030-driver-equivalence-storage.json` y
  `fullscreen-a1-compare-full-pc.json`.
- Contactos de Bill, jugador y Oak revisados en ambas cámaras, con manifiestos
  `fullscreen-a1-bill-contact-review.json` y
  `fullscreen-a1-player-oak-contact-review.json`. README incorpora esas capturas.
- La batería PC integrada pasa 12/12: 46.834 frames, 2.504.708 glifos,
  49.454 gráficos y 29.862 comprobaciones de cursor. Las 2.305 presentaciones
  de respaldo conservan el LCD completo; cifras en
  `fullscreen-a1-full-pc-totals.json`. Los contactos de scroll y respaldo
  también están revisados.
- Continúan la batería integrada acumulativa y las veinte regresiones clásicas.
  No se ha abierto ni fusionado la PR A1; sus casillas siguen pendientes.

Se conservaron los primeros fallos de prueba: fixture general inadecuado para
el recorrido que exige Pidgey y caja vacía; cursor de ChangeBox tras borrar el
flag; contador del monitor afectado por desenfoque. La batería PC recibe ahora
su fixture explícito y el monitor integrado conserva nítidos sus contadores.

Comandos ejecutados (las rutas de savestate son evidencia privada local):

```sh
cmake --build build --target all pallet_render_smoke --parallel 2
ctest --test-dir build --output-on-failure
cmake --build build/qa/no-rom --parallel 2
LIBGL_ALWAYS_SOFTWARE=1 ctest --test-dir build/qa/no-rom -LE rom --output-on-failure
env -u LIBGL_ALWAYS_SOFTWARE QA_MENU_STYLE=integrated QA_GLYPH_ORACLE=1 QA_CAPTURE_STATES=1 \
  tests/ui_full_pc_qa.sh build/roms/pokeyellow.gbc \
  build/qa/pc-storage-fJ2Oob/capture/logs/capture-complete.state
env -u LIBGL_ALWAYS_SOFTWARE QA_CAPTURE_STATES=1 tests/ui_style_qa.sh \
  build/roms/pokeyellow.gbc build/qa/battles-eovzS8/logs/capture-complete.state \
  build/qa/firstperson-9cB3FJ/route.state build/qa/battles-LojUdN/logs/route22-trainer.state \
  build/qa/ui-crossfade-5bOG41/world.state build/qa/ui-crossfade-5bOG41/battle.state \
  build/qa/pc-storage-fJ2Oob/capture/logs/capture-complete.state
bash build/qa/logs/run-fullscreen-a1-regressions.sh
python3 build/qa/logs/collect-fullscreen-a1-classic.py
```


### A1 — batería integrada y CI completos, 2026-09-22

Commit `c4a0be9c3c948b3630bcc21a217011b88ae2cc4a`, publicado en
`fullscreen-a1`; PR #23 abierta como borrador. CI `35673667245` pasa las cinco
comprobaciones obligatorias para ese commit: formato, Linux 2D, Linux GCC,
Linux Clang y Windows MSVC. macOS permanece desactivado explícitamente como
en la base; no se cuenta como plataforma validada.

`build/qa/ui-style-zhkm0d/logs/glyphs.json` confirma los treinta recorridos
integrados y ambas pruebas de pausa/carga. El PC aporta los doce recorridos
previstos, con controles negativos activos. La comparación de sus estados
completos entre clásico e integrado comprueba 110 pares y 26.950.880 bytes,
sin diferencias (`fullscreen-a1-style-engine-parity.json`). Las 38 capturas
exteriores permanecen exactas (`fullscreen-a1-original-exteriors.json`).

Esta evidencia acredita los cuatro primeros criterios de A1. Los dos últimos
siguen pendientes hasta terminar las veinte regresiones clásicas y fusionar
la PR. No se ha iniciado A2.


### A1 — cierre de validación local, 2026-09-22

Las veinte baterías clásicas terminan con código 0. Las 2.169 capturas,
incluidas las diez vistas de ajustes antes registradas aparte, y 1.831 estados
completos son idénticos a v0.3.0. La batería PC nueva añade 110 capturas y
110 estados exactos contra el renderizador original con el nuevo driver;
también conserva esos 110 estados entre clásico e integrado. Las 38 vistas
exteriores originales no cambian. Ninguna comparación excluye capturas de
ajustes ni acepta diferencias.

El recopilador `build/qa/logs/collect-fullscreen-a1-evidence.py` termina con
código 0 y genera `fullscreen-a1-evidence.json`. Exige fuentes congeladas,
binarios coincidentes en cada batería, veinte scripts clásicos más el PC
nuevo, treinta recorridos integrados, pausa/carga en ambas cámaras, CTest
39/39, pruebas independientes sin ROM 20/20, formato 18.1.8, renderizado
llvmpipe, contactos revisados, capturas README exactas y runtime sin cambios.

```sh
python3 build/qa/logs/collect-fullscreen-a1-evidence.py
python3 tests/compare_classic_captures.py \
  build/qa/ui-full-pc-v030-JXA8p8 build/qa/ui-full-pc-D03k7R \
  --report build/qa/logs/fullscreen-a1-compare-full-pc.json
gh run view 35673667245 --repo dobbygl/pokeyellow3d \
  --json headSha,status,conclusion,jobs
```

Quedan acreditados cinco de los seis criterios de A1. El último se marcará
al constatar la fusión de la PR #23 con CI verde, antes de implementar A2.


### A1 fusionada; inicio de A2 — 2026-09-22

PR #23 fusionada a las 01:16:09 UTC, merge
`33561771b3d07a994e0c4d0e2f197b1be930fbd2`, después del run final
`35674984372`: cinco comprobaciones obligatorias verdes sobre
`c28dfe5efbb14901daedf26c06cf8d03f6c2e9cd`. Evidencia de fusión en
`build/qa/logs/fullscreen-a1-merge.json`. A1 queda acreditada 6/6.

`fullscreen-a2` parte de ese merge; se inicia el inventario de equipo y resumen,
sin modificar todavía su renderizado. La fuente original y las referencias
clásicas continúan fijadas a los mismos commits de pret y v0.3.0.

### A2: implementación y validación inicial — 2026-09-22

`pokemon_menu_state.h` reconoce equipo, acciones y las dos páginas de resumen
por llamadas originales activas y geometría verificada. Los gráficos tienen
perfiles propios para equipo y resumen; se comprueban ambos planos de VRAM,
la fuente completa y el retrato descomprimido antes de presentar cualquier
contenido integrado. Los iconos mantienen las dos fases del motor original.
La selección del retrato lateral sigue la flecha pintada durante los frames
pendientes de repintado. `DisplayFieldMoveMonMenu` añade una fila vacía sobre
los movimientos de campo; esa variante está incluida.

La experiencia procede del registro permanente de equipo, rival, caja o
guardería. Se verificó que `CalcExpToLevelUp` sobrescribe `wLoadedMonExp`
con la cantidad que falta, de modo que ese temporal no alimenta la barra.
Evidencias privadas: `fullscreen-a2-source-inventory.json` (14 parejas de
tilemap/estado), `fullscreen-a2-graphics-proof.json` (assets y patrones vivos)
y `fullscreen-a2-stack-proof.json` (CALL de ROM y retornos activos).

Validación inicial completada, sin cerrar todavía criterios de la fase:

- CTest 40/40 y build independiente sin ROM 21/21; logs
  `fullscreen-a2-ctest-initial.log` y `fullscreen-a2-no-rom-initial.log`.
- Seis recorridos integrados en `build/qa/ui-full-pokemon-dwOSKY`: dos y seis
  miembros, HP cero/mínimo/lleno, estados alterados, nivel 100, Corte/Surf,
  pausa/carga, cinco respaldos negativos y resúmenes desde equipo/caja del PC,
  en ambas cámaras. Los oráculos comprueban glifos, cobertura, cursor,
  gráficos especiales y HP sobre la imagen final; la memoria se compara por
  frame. El recorrido está incorporado a `tests/ui_style_qa.sh`.
- Las 80 capturas y 80 estados de esos seis recorridos clásicos coinciden
  exactamente con v0.3.0: `fullscreen-a2-compare-new-pokemon.json`. El renderer
  de referencia permanece intacto; `fullscreen-a2-v030-new-driver-proof.json`
  registra los hashes y el adaptador de diagnóstico exclusivo de pruebas.
- Contactos de equipo, seis miembros y PC revisados en
  `fullscreen-a2-{team,six,pc}-contact-review.json`. Tres imágenes del README
  conservan todos los píxeles de las capturas, según
  `fullscreen-a2-readme-images.json`.

```sh
env -u LIBGL_ALWAYS_SOFTWARE QA_MENU_STYLE=integrated QA_GLYPH_ORACLE=1 \
  tests/ui_full_pokemon_qa.sh build/roms/pokeyellow.gbc \
  build/qa/ui-menus-KfpanN/pallet.state
env -u LIBGL_ALWAYS_SOFTWARE QA_MENU_STYLE=classic QA_CAPTURE_STATES=1 \
  tests/ui_full_pokemon_qa.sh build/roms/pokeyellow.gbc \
  build/qa/ui-menus-KfpanN/pallet.state
python3 build/qa/logs/run-fullscreen-a2-v030.py
python3 tests/compare_classic_captures.py \
  build/qa/ui-full-pokemon-v030-qdzd_4yu build/qa/ui-full-pokemon-vQsPXJ \
  --report build/qa/logs/fullscreen-a2-compare-new-pokemon.json
```

Quedan pendientes la batería integrada acumulada, todas las regresiones
clásicas y exteriores, el cierre de evidencia y la PR con CI verde.


La revisión de cobertura posterior añadió `PartyMenuOrRockOrRun`: su llamada
`0F:5236 → HandleMenuInput` abre las mismas acciones con el orden Switch,
Stats, Cancel. Se comprobó contra `engine/battle/core.asm`, el C generado,
los bytes `CD AB 3A` y un savestate privado. Equipo, acciones y ambas páginas
del resumen también se recorren desde un combate en ambas cámaras. La nueva
presentación no espera al fundido decorativo para mostrar texto validado.
La batería específica pasa a ocho recorridos y recibe un tercer argumento:
`tests/ui_full_pokemon_qa.sh ROM TWO_POKEMON_WORLD READY_BATTLE`.
La primera CI de #24 (`35677979317`) terminó con las cinco comprobaciones
obligatorias verdes sobre `8f74f5f`; las regresiones en curso se detuvieron
al ampliar este caso y se reinician sobre la revisión final.


### A2 — revisión final y recorridos específicos, 2026-09-22

La revisión `5a49174295861667fac4d8b4458d15e2ae37ffbc` supera las cinco
comprobaciones obligatorias de CI en el run `35678666460`. La PR #24 sigue
como borrador mientras terminan la batería integrada acumulativa y las veinte
regresiones clásicas. No se ha iniciado A3 ni se cierran criterios de A2 todavía.

Sobre ese mismo código pasan CTest 40/40, el build independiente sin ROM 21/21
y clang-format 18.1.8. Los ocho recorridos específicos integrados comprueban
6.869 frames, 547.075 glifos, 35.012.800 bits y 3.996 cursores; los veinte
controles negativos conservan el LCD completo. La batería incluye equipo,
acciones y resumen desde mundo, PC y combate, en ambas cámaras.

Los ocho recorridos clásicos nuevos conservan 88 capturas y 88 estados exactos
contra v0.3.0; el PC conserva sus 110 capturas y 110 estados exactos. Los cien
archivos de captura compartidos con los tres contactos revisados previamente
siguen idénticos sobre esta revisión; se añade el contacto de combate revisado.
Evidencias bajo `build/qa/logs/`:
`fullscreen-a2-pokemon-totals-final.json`,
`fullscreen-a2-compare-new-pokemon-final.json`,
`fullscreen-a2-compare-full-pc-final.json`,
`fullscreen-a2-reviewed-captures-final-parity.json` y
`fullscreen-a2-battle-contact-review.json`.

Los comandos específicos actuales incluyen el estado de combate como tercer
argumento; sustituyen los comandos de dos argumentos del registro inicial:

```sh
env -u LIBGL_ALWAYS_SOFTWARE QA_MENU_STYLE=integrated QA_GLYPH_ORACLE=1 \
  tests/ui_full_pokemon_qa.sh build/roms/pokeyellow.gbc \
  build/qa/ui-menus-KfpanN/pallet.state build/qa/ui-crossfade-5bOG41/battle.state
env -u LIBGL_ALWAYS_SOFTWARE QA_MENU_STYLE=classic QA_CAPTURE_STATES=1 \
  tests/ui_full_pokemon_qa.sh build/roms/pokeyellow.gbc \
  build/qa/ui-menus-KfpanN/pallet.state build/qa/ui-crossfade-5bOG41/battle.state
python3 build/qa/logs/run-fullscreen-a2-v030.py
python3 tests/compare_classic_captures.py \
  build/qa/ui-full-pokemon-v030-1ut0ba_s build/qa/ui-full-pokemon-QfkfzU \
  --report build/qa/logs/fullscreen-a2-compare-new-pokemon-final.json
python3 tests/compare_classic_captures.py \
  build/qa/ui-full-pc-v030-JXA8p8 build/qa/ui-full-pc-oNcFUK \
  --report build/qa/logs/fullscreen-a2-compare-full-pc-final.json
```


### A2 — cierre de validación local, 2026-09-22

Las veinte baterías clásicas finalizan con código 0. Sus 2.169 capturas y
1.831 estados completos son idénticos a v0.3.0, sin excluir vistas de ajustes.
Los recorridos nuevos conservan además las 110 capturas/estados del PC y las
88 capturas/estados de equipo y resumen. Las 38 vistas exteriores originales
permanecen exactas. Las entradas de cada comparación clásica son las mismas
que ejecuta su referencia v0.3.0; las comprobaciones adicionales de pausa y
carga pertenecen a los recorridos integrados.

`build/qa/ui-style-kAi1Im/logs/glyphs.json` acredita los 38 recorridos integrados:
ocho de menús, cuatro de combate, seis de fundidos, doce de PC y ocho de equipo
y resumen. También pasan pausa/carga en ambas cámaras, sin glifos de sustitución,
errores GL ni cambios de memoria por el compositor.

El recopilador `build/qa/logs/collect-fullscreen-a2-evidence.py` pasa y genera
`fullscreen-a2-evidence.json` sobre el código `5a49174`. Comprueba las fuentes
congeladas, binarios coincidentes en todas las baterías, CTest 40/40, build
independiente sin ROM 21/21 con llvmpipe, formato 18.1.8, cuatro contactos
revisados y las tres imágenes del README exactas. El runtime sigue limpio en
`00cc26dafb9a41ea9d935508e9fbe9e25b5f5a6e`; el C generado no cambia.

```sh
bash build/qa/logs/run-fullscreen-a2-regressions.sh
python3 build/qa/logs/collect-fullscreen-a2-classic.py
python3 build/qa/logs/collect-fullscreen-a2-evidence.py
ctest --test-dir build --output-on-failure
LIBGL_ALWAYS_SOFTWARE=1 ctest --test-dir build/qa/no-rom -LE rom --output-on-failure
env -u LIBGL_ALWAYS_SOFTWARE QA_CAPTURE_STATES=1 tests/ui_style_qa.sh \
  build/roms/pokeyellow.gbc build/qa/battles-eovzS8/logs/capture-complete.state \
  build/qa/firstperson-9cB3FJ/route.state build/qa/battles-LojUdN/logs/route22-trainer.state \
  build/qa/ui-crossfade-5bOG41/world.state build/qa/ui-crossfade-5bOG41/battle.state \
  build/qa/pc-storage-fJ2Oob/capture/logs/capture-complete.state
```

Quedan acreditados cinco de los seis criterios de A2. El último se marcará
tras verificar la fusión de la PR #24 con CI verde, antes de comenzar A3.


### A2 fusionada; inicio de A3 — 2026-09-22

PR #24 fusionada a las 02:47:33 UTC, merge
`c802b33ad9203644a9f69d724bf3657012c53ba4`, después del run final
`35680587598`: cinco comprobaciones obligatorias verdes sobre
`fd9b228d46061f5960be086cedc5dd29215a6cf0`. Evidencias de CI y fusión en
`build/qa/logs/fullscreen-a2-ci-final.json` y `fullscreen-a2-merge.json`.
A2 queda acreditada 6/6 y el plan alcanza 12/24.

`fullscreen-a3` parte de esa fusión. Se inicia el inventario de mochila,
tienda completa y lista de Pokédex, manteniendo el motor y las referencias
clásicas fijadas a v0.3.0. Todavía no hay criterios acreditados de A3.
