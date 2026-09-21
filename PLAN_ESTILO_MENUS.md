# Plan: estilo integrado de menús y HUD

Fecha: 2026-09-21. Estado: A1 fusionada; A2 en validación; B1, B2 y C1 pendientes.

## Análisis del estado actual

Punto de partida verificado en `src/menu_layout.h`, `src/lcd_overlay.h`,
`src/battle3d.h`, `src/pc_boxes.h` y `src/pallet3d.cpp`, tras el cierre de
`PLAN_MENUS_TITULO_TRANSICIONES.md` y la release v0.2.0:

| Situación | Presentación actual | Mecanismo |
| --- | --- | --- |
| Cuadro inferior, Start, sí/no, tienda, guardado, centro | Imagen LCD recortada a escala entera, opaca, blanco puro y borde de 8 bits | `menu_layout::classify` + `lcd_overlay::regions` con sombra de 35/255 |
| Pantallas completas (equipo, mochila, Pokédex, nombres) | Imagen LCD entera a 3x con marco fino sobre el mundo atenuado | `lcd_overlay::framed` |
| Marcadores de combate | Paneles oscuros redondeados, fuente ImGui por defecto, barras de color | `battle3d::panel` |
| Menú FIGHT/ITEM, mensajes y lista de movimientos en combate | Seis filas inferiores del LCD a escala entera, opacas | `lcd_overlay::draw(Bottom)` desde `battle3d.h` |
| Nombres y niveles en la estantería del PC | Glifos de la fuente de la ROM como quads en 3D | `pc_boxes.h` decodifica los 128 glifos de 1 bpp en ROM 0x10600 |
| Cursor del ratón | Visible sobre el juego en todo momento | El runtime nunca llama a `SDL_ShowCursor` |

Hechos que condicionan el diseño:

- A 800x720 el compositor usa escala 5: `floor(min(800/160, 720/144))`. Un
  menú Start de 10 tiles de ancho ocupa 400 píxeles, la mitad de la ventana.
  Las pantallas completas usan 3x y sí dejan ver el mundo.
- Los cuadros del juego son estrictamente de dos tonos: tinta y fondo. Los
  bordes son los tiles 79–7E, el espacio en blanco es 7F y el texto usa
  índices 80–FF que corresponden uno a uno con los 128 glifos de
  `FontGraphics`. El cursor de selección es un tile de flecha; su índice se
  verifica en `charmap.asm` antes de usarlo.
- `menu_layout.h` ya devuelve rectángulos por disposición reconocida y
  declara pantalla completa todo lo que no reconoce. Ese contrato no cambia.
- Los marcadores de combate ya definen un lenguaje visual: panel
  `IM_COL32(17, 29, 33, 238)` con radio 6, texto `248, 244, 219`, barras verde,
  ámbar y rojo según `wEnemyHPBarColor`. Falta aplicarlo al resto.
- Las preferencias de iluminación se guardan en `preferences_path` con un
  formato de versión 1 cuya carga rechaza cualquier campo extra. Un ajuste
  nuevo necesita fichero propio o versión 2 con lectura compatible.
- Las pruebas de `tests/menu_integration.h` y afines comparan píxel a píxel
  la región original con lo presentado. Cualquier reestilizado rompe esa
  comparación si no se cambia lo que se compara.
- Las animaciones nuevas de este plan derivarán su tiempo de `ctx->cycles`.
  En v0.2.0, los fundidos de composición y de carga aún usan `SDL_GetTicks`;
  el helper fija ImGui DeltaTime pero no ese reloj. La referencia clásica
  debe controlar también esa temporización en las pruebas, sin cambiar
  la conducta del ejecutable jugable, para exigir igualdad byte a byte.

## Objetivo y alcance

Que los menús y el texto del juego se vean como parte de la presentación 3D
y compartan un único lenguaje visual con los marcadores de combate, la
Pokédex, el PC y el título, sin que el juego deje de pintar el contenido. El
motor sigue siendo la única autoridad sobre qué texto hay, dónde está el
cursor y qué opción está seleccionada. Ningún módulo escribe en memoria. El
usuario puede volver en cualquier momento a la composición actual.

Quedan fuera: traducir texto, cambiar el orden o contenido de los menús,
reimplementar cualquier lógica de menú, sustituir la tipografía de la ROM
por otra, redibujar las pantallas completas con disposición libre y cualquier
cambio en el runtime descargado o en el C generado.

## Decisiones de arquitectura

1. Un tema único en `src/ui_theme.h`: colores de panel, tinta, tinta atenuada,
   acento, radio, relleno, sombra y escala de glifo. Lo consumen
   `lcd_overlay.h`, `battle3d.h`, `pc_boxes.h`, `dex3d.h`, `pc3d.h` y
   `title3d.h`. Ningún módulo define colores propios fuera del tema.
2. Un ajuste persistente "Estilo de menús" con dos valores: clásico, que es
   la composición actual sin cambios, e integrado, que es el objeto de este
   plan. Se guarda junto a las preferencias de iluminación con lectura
   compatible hacia atrás. El valor por defecto es integrado.
3. La fuente sigue siendo la de la ROM. Un renderizador de glifos en
   `src/menu_text.h` lee los índices de `wTileMap` dentro de cada región
   reconocida y dibuja cada glifo desde el atlas de 1 bpp ya construido en
   `pc_boxes.h`, extraído a un módulo común. La escala del glifo es
   independiente de la escala del LCD.
4. La fuente de verdad de cada región es `wTileMap`, no el framebuffer. Para
   detectar tinta no se umbraliza color: se usa el patrón de bits del glifo.
   Así el resultado no depende de la paleta CGB ni de los fundidos.
5. El panel sustituye al borde: los tiles 79–7E y 7F no se dibujan; en su
   lugar se dibuja un panel del tema con el rectángulo de la región. El
   cursor de selección se dibuja como glifo del tema en la misma posición
   que su tile.
6. Toda región que el clasificador declara pantalla completa mantiene la
   presentación actual de `lcd_overlay::framed`, que ya es coherente con el
   tema. No se reinterpretan pantallas de disposición libre.
7. Las animaciones de apertura y cierre derivan de `ctx->cycles`, se
   congelan con el menú Esc o sin foco, y nunca retrasan la entrada del
   juego: el menú es operable desde el primer frame aunque siga apareciendo.
8. Cualquier fallo de detección, tile fuera del rango de la fuente o región
   no reconocida cae a la conducta clásica para esa región. Es la conducta
   segura, no un error.

## Bloque A: menús del mundo

### Fase A1: tema, ajuste y cursor

Trabajo:

- Crear `src/ui_theme.h` con los tokens del tema, tomando como base los
  colores que ya usan los marcadores de combate. Refactorizar `battle3d.h`,
  `pc_boxes.h`, `dex3d.h`, `pc3d.h`, `title3d.h` y `lcd_overlay::framed`
  para leerlos del tema sin cambiar su aspecto actual.
- Añadir el ajuste "Estilo de menús" al bloque de ajustes de `pallet3d.cpp`,
  con persistencia compatible con el fichero de preferencias actual y un
  test de carga de ficheros de versión 1 y 2.
- Ocultar el cursor del ratón mientras la ventana tiene foco y el menú Esc
  está cerrado; mostrarlo al abrir Esc o perder foco.
- Extraer el atlas de glifos de `pc_boxes.h` a `src/rom_font.h`, con
  inicialización única, y hacer que `pc_boxes.h` lo consuma.

Criterios de aceptación:

- [x] Con el estilo clásico, las 60 capturas de menús y las capturas de combate, PC, Pokédex y título son idénticas byte a byte a v0.2.0.
- [x] El ajuste se conserva tras reiniciar la aplicación y un fichero de preferencias de versión 1 sigue cargando.
- [x] El cursor del ratón no aparece en ninguna captura de juego y sí con el menú Esc abierto.
- [x] `rom_font` decodifica los 128 glifos y cada uno coincide bit a bit con el tile de VRAM correspondiente en un savestate con texto en pantalla.

### Fase A2: renderizado de regiones desde `wTileMap`

Trabajo:

- `src/menu_text.h`: dado un rectángulo de `menu_layout`, leer sus índices
  de `wTileMap`, clasificar cada tile en borde, blanco, glifo o cursor, y
  emitir un panel del tema más los glifos como quads del atlas. Escala de
  glifo configurable; a 800x720 el objetivo es 3x para Start y 4x para el
  cuadro inferior, con relleno del tema.
- Verificar en `charmap.asm` el índice del cursor de selección y de los
  caracteres especiales que usan los menús: flechas, "é", "'d", "'s",
  "PK", "MN", el símbolo de Poké Dólar y los dígitos de nivel. Todo índice
  fuera de 80–FF que aparezca en una región reconocida se registra y hace
  caer esa región al estilo clásico.
- Aplicar a las disposiciones ya reconocidas: cuadro inferior en ambas
  cámaras, Start de 14 y 16 filas, sí/no, tarjeta de guardado, Heal/Cancel
  del centro y las cinco disposiciones de tienda con sus solapamientos, que
  se resuelven en el mismo orden que hoy.
- Mantener la sombra suave y el HUD oculto durante el menú; los controles
  relativos siguen neutros como ahora.

Criterios de aceptación:

- [ ] Cada glifo presentado coincide bit a bit con el patrón de tinta del tile de `wTileMap` que representa, comprobado por frame en las ocho variantes de `ui_menus_qa.sh`.
- [ ] El cursor de selección se presenta en la fila que indica `wCurrentMenuItem` y sigue al cursor original frame a frame al recorrer Start, tienda y sí/no.
- [ ] Start, guardar con confirmación, comprar en la tienda y curar en el centro mantienen la escena y son operables con los controles originales.
- [ ] Las regiones no reconocidas y las pantallas completas conservan su presentación actual sin frames en blanco.
- [ ] Cero errores OpenGL y WRAM, VRAM, RAM de cartucho y framebuffer intactos por frame.

## Bloque B: combate y HUD

### Fase B1: menú de combate y mensajes en el lenguaje del HUD

Trabajo:

- Presentar las seis filas inferiores del combate con `menu_text.h`: el
  cuadro de mensajes a la izquierda y el menú FIGHT/PKMN/ITEM/RUN a la
  derecha como dos paneles del tema. La lista de movimientos, con PP y tipo,
  usa el mismo mecanismo cuando el juego la pinta en esas filas.
- Cuando el cuadro de mensajes no contiene glifos, no se dibuja su panel.
  Cuando el texto se escribe letra a letra, aparece a la velocidad del juego
  porque se lee de `wTileMap` en cada frame.
- Las pantallas completas del combate, equipo y mochila, siguen enmarcadas
  sobre la arena atenuada.
- El respaldo de animación, que compone el LCD entero, no cambia.

Criterios de aceptación:

- [ ] Un encuentro salvaje en Ruta 1 y el combate contra el rival de Ruta 22 se juegan hasta el final con el menú integrado: luchar, elegir movimiento, mochila, equipo y huir.
- [ ] Los glifos del menú, la lista de movimientos y los mensajes coinciden bit a bit con `wTileMap` en cada frame presentado.
- [ ] Ningún frame muestra un panel vacío ni texto duplicado entre el panel y el respaldo de animación.
- [ ] `ui_battles_qa.sh` y `battles_qa.sh` pasan con el estilo integrado y con el clásico.

### Fase B2: coherencia de todo el HUD

Trabajo:

- Los nombres y niveles de los marcadores de combate, los rótulos de la
  estantería del PC, la Pokédex y el título usan el atlas de la ROM a través
  de `rom_font`, con los mismos tamaños y colores del tema. La fuente ImGui
  por defecto deja de aparecer en cualquier elemento de juego.
- Los marcadores de combate reciben las barras de estado y experiencia con
  los colores del tema y el mismo radio y relleno que los paneles de menú.
- Revisar con capturas las cuatro escenas, mundo, combate, PC y Pokédex, en
  ambas cámaras, y corregir las desviaciones de tamaño, relleno o color.

Criterios de aceptación:

- [ ] Ninguna escena de juego dibuja texto con la fuente ImGui por defecto; solo el menú Esc la conserva.
- [ ] Un contacto revisado con las cuatro escenas en ambas cámaras muestra un único conjunto de colores, radios y tamaños de glifo.
- [ ] Las baterías de combate, PC, Pokédex y título pasan sin cambios en el estado del motor.

## Bloque C: transiciones de menú

### Fase C1: aparición y cierre

Trabajo:

- Al reconocerse una disposición nueva, el panel aparece con un fundido y
  un desplazamiento de unos 150 ms derivados de `ctx->cycles`; al dejar de
  reconocerse, desaparece igual. Los glifos siempre se dibujan completos
  desde el primer frame para no ocultar texto que el juego ya muestra.
- Cambios entre disposiciones que comparten regiones, como Start a tarjeta
  de guardado o tienda a cantidad, animan solo la región nueva.
- Pausa con el menú Esc y sin foco, como los fundidos de C3 del plan de
  menús. Cargar un savestate con menú abierto lo muestra sin animar.

Criterios de aceptación:

- [ ] Abrir y cerrar Start, sí/no y la tienda animan sin retrasar ninguna pulsación: el estado del motor es idéntico con animación y sin ella para la misma entrada.
- [ ] Ninguna animación deja un frame sin el texto que `wTileMap` contiene.
- [ ] Las capturas de referencia a fase fija siguen idénticas y una secuencia de frames aparte demuestra la animación.

## Limitaciones asumidas

- La tipografía es la de la ROM. Los glifos se escalan a múltiplos enteros;
  no hay suavizado ni fuentes vectoriales.
- Las pantallas completas se enmarcan sobre el fondo atenuado, no se
  rediseñan. Reinterpretar su disposición queda para un plan posterior.
- Solo se reestilizan las disposiciones que `menu_layout.h` reconoce. Una
  disposición nueva del juego se presenta en clásico hasta que se añada.
- La detección de glifos es por índice de tile. Un tile de texto que el
  juego dibuje con gráficos fuera de la fuente cae al clásico para esa
  región, y el caso se registra.
- Los tests de píxeles exactos del clásico se conservan. En el estilo
  integrado la equivalencia se comprueba por patrón de glifo, no por píxel.

## Validación y entrega

- CTest completo, `ctest -LE rom` sin ROM, `clang-format` y las veinte
  baterías `tests/*_qa.sh` antes y después de cada fase, con el estilo
  clásico. Las 38 capturas exteriores y las capturas de menús, combate, PC,
  Pokédex y título en clásico siguen idénticas a v0.2.0.
- Nuevos modos de `pallet_render_smoke` con sufijo `-styled`: `menus`,
  `battle3d` y `crossfade` con el estilo integrado. Cada uno comprueba por
  frame la equivalencia glifo a glifo con `wTileMap`, el modo presentado,
  GL y la memoria intacta. Nuevo `tests/ui_style_qa.sh` que los ejecuta en
  ambas cámaras.
- Pruebas unitarias de `rom_font` contra VRAM, de `menu_text` con mapas de
  tiles privados exportados de savestates, incluidos los solapamientos de
  la tienda, y de la carga de preferencias de versión 1 y 2.
- Fixtures privadas bajo `build/qa/`, las mismas que usan `ui_menus_qa.sh`,
  `ui_battles_qa.sh` y `ui_crossfade_qa.sh`. No se incorporan al repositorio.
- Entregar `build/pokeyellow3d`, `PALLET3D.md` con la tabla de disposiciones
  y el ajuste de estilo, `README.md` con capturas nuevas, y contactos
  revisados en `build/qa/logs/`.
- Marcar las casillas solo con evidencia registrada.

## Orden recomendado

1. A1, porque el ajuste y el tema permiten que todo lo demás sea reversible
   y que las pruebas del clásico sigan siendo la referencia.
2. A2, que es el cambio visible en el mundo y valida `menu_text.h`.
3. B1, que reutiliza `menu_text.h` en combate.
4. B2 y después C1, siguiendo el orden fijado para este objetivo.

## Referencias técnicas

- [Fuente del juego](https://github.com/pret/pokeyellow/blob/master/gfx/font/font.png) y [mapa de caracteres](https://github.com/pret/pokeyellow/blob/e89ead154b9968aa50eed9328ff2b38b6c194382/constants/charmap.asm).
- [Dibujo de cuadros de texto](https://github.com/pret/pokeyellow/blob/master/home/text.asm) y [menús de lista](https://github.com/pret/pokeyellow/blob/master/engine/menus/menu.asm).
- [Menú de combate](https://github.com/pret/pokeyellow/blob/master/engine/battle/core.asm).
- `src/menu_layout.h`: rectángulos y bordes por disposición.
- `src/lcd_overlay.h`: compositor actual, referencia del estilo clásico.
- `src/pc_boxes.h`: decodificación de la fuente de 1 bpp en ROM 0x10600.
- `src/battle3d.h`, función `panel`: colores de partida del tema.
- `src/daylight.h`: formato de preferencias de versión 1.

## Ejecución como goal

Comando para la conversación de Codex del proyecto:

```text
/goal Ejecuta las cinco fases de /home/jgomez/Projects/jgomez/pokemon2/pokeyellow/PLAN_ESTILO_MENUS.md en el orden A1, A2, B1, B2, C1. El juego sigue pintando el contenido y el módulo 3D solo lee wTileMap, VRAM, ROM y el framebuffer; no reimplementes ningún menú ni cambies la tipografía de la ROM. Conserva el estilo clásico como ajuste con capturas idénticas a v0.2.0 y haz que el integrado sea el predeterminado. Marca las casillas solo con evidencia y actualiza el registro de ejecución con comandos reproducibles.
```

## Registro de ejecución

### Confirmación del plan — 2026-09-21

Objetivo autorizado: A1, A2, B1, B2 y C1, cada fase en su rama y fusionada
con CI verde antes de empezar la siguiente. Se conservan los 19 criterios
sin marcar hasta registrar evidencia. Esta primera PR solo confirma el plan.

Base: tag `v0.2.0`, commit `2544f9fe6895213965439162511f4a87b9a87526`.
El `main` inicial es `3e7419207b3bd1ea32fd88dd23ae826223f753c7`; únicamente
añade documentación a ese tag. Código de juego y runtime idénticos a la
release. No se reinician las fases ya entregadas del objetivo anterior.

La comparación clásica exige referencias de v0.2.0 con el mismo reloj de
pruebas: no se aceptan como referencia las variaciones de opacidad por tiempo
real registradas en el objetivo anterior. Los fixtures, capturas y contactos
permanecen bajo `build/qa/`; las capturas integradas seleccionadas para el
README se publicarán como parte de la entrega expresamente solicitada.

La fase A1 aún no está iniciada. Validación documental: 19 casillas abiertas,
`git diff --check` y CI de esta PR antes de fusionar. Las veinte baterías
clásicas y la nueva integrada se ejecutarán tras cada fase; este commit no
modifica código ni cambia el binario de v0.2.0.

### A1 — implementación y referencia, 2026-09-21

Plan confirmado y fusionado en PR #16, merge `f2f6d75`, con CI
`35612560495` verde. Rama de implementación: `menu-style-a1`. Las 19
casillas siguen abiertas hasta terminar la evidencia de aceptación.

- `ui_theme.h` conserva los valores previos y centraliza paneles, barras,
  marcos y paletas. `rom_font.h` decodifica una vez los 128 glifos y copia
  el atlas compartido en los rectángulos originales de PC, cajas y Hall.
- `ui_preferences.h` lee v1 y v2. V2 conserva iluminación y estilo; el
  valor inicial del estilo es integrado. Esc permite cambiarlo. El cursor
  del sistema se oculta con foco durante el juego y se muestra en Esc o
  al perder foco.
- Referencia independiente: worktree privado `build/qa/menu-style-v020`,
  tag `2544f9fe6895213965439162511f4a87b9a87526`, con **solo** el adaptador
  de reloj de QA y su enlace. Ningún archivo de `src/` de v0.2.0 se modifica.
  En Linux, el helper intercepta SDL_GetTicks/SDL_Delay y avanza un reloj
  virtual por frame. El ejecutable jugable y el reloj del motor no cambian.
  La batería de fundidos de esa referencia ya pasa, incluida inversión y
  pausa de Esc/foco. Las veinte baterías completas están en ejecución.
- Las capturas del juego se compararán sin tolerancia. Las dos vistas del
  **panel de ajustes del runtime** (`settings.ppm` y `settings-paused.ppm`)
  se registran aparte porque ahora incluyen el nuevo control autorizado;
  esa separación no excluye ningún menú original del juego. Sus estados
  del motor también se comparan.
- `ui_style_qa.sh` reutiliza los recorridos de menús, combate y fundidos
  en ambas cámaras. A1 aún conserva la composición clásica en ambos
  estilos; su observador compara por frame los bits de los glifos y sus
  píxeles LCD presentados. A2 añadirá la comparación de los glifos
  reestilizados, y B1 la del menú de combate integrado.

Primeras comprobaciones: compilación correcta; CTest inicial 32/32;
128 glifos iguales a ambos planos de VRAM del estado privado
`build/qa/ui-menus-YdVuWy/menus/logs/start.state`. La nueva configuración
registra 34 pruebas con ROM y 15 sin ROM; su ejecución completa está
pendiente junto con las baterías. No se da por cerrada A1.

Comandos y evidencia reproducibles:

```sh
# La referencia usa los mismos tests/qa_sdl_clock.cpp y adaptador de reloj,
# sin los observadores posteriores ni cambios de presentación de A1.
bash build/qa/logs/run-menu-style-baseline.sh
bash build/qa/logs/run-menu-style-a1-regressions.sh
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy build/pallet_render_smoke \
  build/roms/pokeyellow.gbc build/qa/ui-menus-YdVuWy/menus/logs/start.state \
  font-export build/qa/ui/font.vram
build/rom_font_test build/roms/pokeyellow.gbc build/qa/ui/font.vram
ctest --test-dir build --output-on-failure
ctest --test-dir build/qa/no-rom -LE rom --output-on-failure
```

Los drivers, logs y datos de comparación se guardan bajo
`build/qa/logs/menu-style-*`. Para conservar espacio se aplicó compresión
Btrfs zstd transparente a 32.461 capturas existentes: SHA-256 idéntico antes
y después de cada archivo, rutas conservadas. Registro en
`private-captures-compression-summary.json` y su manifiesto JSONL privado.


### Cierre de A1 — 2026-09-21

Código validado: `82ee5b6f3eda36c42a70fcf3f0b96a463fdaff7c`, PR #17.
CI `35628041008`: Linux GCC, Clang, 2D, Windows MSVC y formato aprobados.
Este cierre documental no modifica el código probado. La PR se fusiona
solo después de comprobar también la CI de su último commit.

Evidencia consolidada: `build/qa/logs/menu-style-a1-evidence.json`.
Las veinte baterías clásicas terminaron con código 0 en la referencia
independiente de v0.2.0 y en A1: **2.159 capturas de juego y 1.831 estados
completos pareados idénticos**, sin archivos ausentes ni adicionales.
Incluyen las 72 capturas de menús y las de combate, PC, Pokédex y título.
Las 38 vistas exteriores coinciden también con el archivo original
`build/qa/kanto-hvoPvo/logs/catalog`, sin tolerancia.

La comparación usa **Mesa Intel UHD Graphics 620**, el renderizador de esas
capturas originales. La prueba inicial con llvmpipe evidenció diferencias
de rasterizado incluso al ejecutar el código intacto de v0.2.0; no se
aceptaron como regresiones del candidato ni se relajó la igualdad. Ambas
versiones se ejecutaron de nuevo con Intel. Los ensayos de software se
conservan en `build/qa/logs/menu-style-software-before-intel/`; la prueba
independiente sin ROM sigue verificando llvmpipe. El parche completo del
reloj de referencia, sus hashes y los comandos de preparación quedan en
`menu-style-v020-qa-clock.patch` y
`menu-style-v020-reference-reproduction.{json,md}` dentro de ese directorio
de logs. Ningún archivo `src/` de la referencia se modificó.

`ui_style_qa.sh` completó sus 18 recorridos de menús, combate y fundidos:
15.817 frames observados, 478.265 glifos y 30.608.960 bits comprobados.
Los tiles cuya fuente no está disponible en VRAM se registran conservando
la composición clásica. A1 todavía no rediseña los paneles. Las guardas
de cursor, OpenGL, memoria y controles originales pasan. El estilo clásico
y el integrado se guardan y restauran en procesos separados; un fichero
v1 conserva la iluminación. Se corrigió la preparación de iluminación para
que un fichero vacío no sustituyera el estilo clásico de QA por el valor
integrado predeterminado: los seis escenarios lo comprueban explícitamente.

CTest completo: **34/34**, sin saltos. Build independiente sin ROM:
**15/15**, sin saltos. `clang-format` **18.1.8** aprobado. Los 128 glifos
coinciden con ambos planos de VRAM; el atlas común conserva las coordenadas
y el filtrado clásicos. Se revisaron los contactos privados de las ocho
variantes de menús, título, ajustes, detalles del PC y combate, con
manifiestos `menu-style-a1-*-review.json` bajo `build/qa/logs/`.

Comandos de la ejecución definitiva, desde la raíz del proyecto:

```sh
# Los drivers desactivan la imposición de software para usar la GPU original.
bash build/qa/logs/run-menu-style-baseline.sh
bash build/qa/logs/run-menu-style-a1-regressions.sh
python3 build/qa/logs/collect-menu-style-a1-evidence.py
```

Los drivers enumeran las veinte baterías y sus fixtures privados, ejecutan
CTest completo y sin ROM, y terminan con `tests/ui_style_qa.sh` en integrado.
El comparador separa diez vistas de ajustes de Esc que contienen el control
nuevo, pero compara sus estados del motor; no excluye ningún menú del juego.
Los cuatro criterios de A1 quedan acreditados. Los quince criterios de
A2, B1, B2 y C1 permanecen abiertos; A2 empieza tras la fusión de esta PR.


### A2 — regiones integradas, 2026-09-21

A1 se fusionó en PR #17, merge `ad1b2c3`, después de la CI final
`35633075199` aprobada. A2 parte de ese merge en `menu-style-a2`.
Las cinco casillas de A2 permanecen abiertas hasta terminar su validación.

Se verificó `constants/charmap.asm` de pret/pokeyellow, commit
`e89ead154b9968aa50eed9328ff2b38b6c194382`, antes de implementar los glifos:
cursor lleno ED, vacío EC, flecha inferior EE, é BA, 'd BB, 's BD,
PK/MN E1/E2, dinero F0 y dígitos F6–FF. El símbolo de nivel 6E y el
colon pequeño 6D pertenecen a gráficos adicionales: obligan a conservar
los píxeles LCD de su región. La copia consultada y su SHA-256 quedan
bajo `build/qa/menu-style-a2/`; no se incorpora ROM al repositorio.

`menu_text.h` prepara la propiedad de cada celda desde el orden original
de ventanas. `menu_text_gl.h` sube un atlas GPU común con filtrado nearest,
dibuja paneles del tema y emite cada glifo visible una sola vez. A 800x720
Start y los cuadros superiores usan escala 3, el diálogo inferior escala 4,
y el relleno es 14 píxeles. Los tamaños se reducen por múltiplos enteros
cuando la ventana es menor. El motor conserva todos los controles.

Las regiones con tiles fuera del atlas o con gráficos VRAM sustituidos
usan sus píxeles LCD originales, a escala entera en la disposición
integrada; las celdas cubiertas por ventanas posteriores siguen ocultas.
Por ejemplo, la tarjeta de guardado conserva su aspecto clásico por el
colon 6D, mientras Start, diálogo y confirmación se integran. Las pantallas
completas y disposiciones desconocidas siguen enmarcadas sin reinterpretar.
El estilo clásico no entra en el nuevo renderizador.

Primer ensayo: 35/35 pruebas CTest y las ocho variantes de
`ui_menus_qa.sh` integradas aprobadas, con lectura por frame del resultado
OpenGL y comparación de sus glifos con la ROM. Evidencia inicial:
`build/qa/ui-menus-MD83vI`, log
`build/qa/logs/menu-style-a2-menu-probe.log`. Se revisaron capturas de
Start, guardado y tienda; los solapamientos conservan la propiedad original.
La comprobación posterior añade correspondencia del cursor activo con
`wCurrentMenuItem`, `wTopMenuItemX/Y` y `hUILayoutFlags`, verificados contra
`pokeyellow_internal.h` y `PlaceMenuCursor`; recorre también las opciones
de tienda, curación y sí/no mediante pulsaciones reales.

Validación completa pendiente: veinte baterías clásicas frente a la
referencia inmutable de v0.2.0, `ui_style_qa.sh` integrado, CTest con y sin
ROM, formato y CI de la PR. No se inicia B1 antes de fusionar A2.
