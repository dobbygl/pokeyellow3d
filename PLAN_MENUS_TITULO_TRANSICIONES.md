# Plan: menús, pantalla de título y transiciones

Fecha: 2026-09-20. Estado: goal pausado por petición del usuario. A1, A2, A3, C1 y C2 completadas y verificadas; C3 parcialmente implementada; B1 y B2 pendientes.

## Análisis del estado actual

Punto de partida de la propuesta, antes de implementar este plan, según
`src/pallet_state.h`, `src/pallet3d.cpp` y los registros de los tres planes
anteriores. Los cambios posteriores se documentan en el registro de ejecución:

| Situación | Presentación actual | Mecanismo |
| --- | --- | --- |
| Exterior, 38 mapas | 3D ortográfico o primera persona | `View::Overworld` |
| Interiores alcanzados por warp, 179 | 3D | `View::Overworld` con escena aislada |
| Cuadro de texto inferior en primera persona | 3D con texto compuesto | `bottom_dialogue` copia las filas 96–143 del framebuffer |
| Cuadro de texto inferior en cámara ortográfica | 2D completo | `dialogue_overlay` exige `first_person` |
| Menú Start, equipo, mochila, PC, Pokédex, tienda, nombres | 2D completo | `View::Dialogue` sin composición |
| Combate normal | 3D con retratos y marcadores | `View::Battle`, menús compuestos desde el framebuffer |
| Animaciones de combate | Imagen original completa sobre la arena | `animation_running` por dirección de retorno en la pila |
| Fundidos de warp, puerta, combate, carga | 2D completo | `View::Transition` cuando BGP no es E4 o el LCD está apagado |
| Cambio entre 3D y 2D | Corte inmediato | `pallet3d_covers_frame` decide por frame si se sube el framebuffer |
| Intro, título, menú principal, opciones, nombre del jugador | 2D completo | No hay mapa cargado; `View::Unsupported` |
| Menú Esc de la aplicación | ImGui del runtime | Paleta, savestates, reinicio; ajeno al juego |

Hechos verificados que condicionan el diseño:

- El compositor ya sabe recortar regiones del framebuffer original y dibujarlas
  con ImGui sobre el 3D. Solo reconoce un diseño: el cuadro de seis filas con
  borde completo, y solo en primera persona.
- Los bordes de ventana del juego usan los tiles 79–7E; las regiones de texto
  se distinguen del mapa porque sus tiles son 60 o superiores. Esa lógica ya
  está en `view()` y en `bottom_dialogue`.
- Cuando el 3D cubre el frame, el runtime no sube ni dibuja el framebuffer.
  Cualquier fundido cruzado entre 3D y 2D tiene que dibujar el framebuffer
  desde el propio módulo 3D, como ya hace el diálogo compuesto.
- El juego escribe BGP en el registro 47 aunque corra en modo color. En los
  trazas aparecen E4 en juego, 00 en fundidos a blanco y FF en fundidos a negro.
  Los valores intermedios no se han registrado porque la traza solo escribe al
  cambiar de estado o de casilla.
- Para la pantalla de título existen los símbolos `vTitleLogo` (VRAM 8800) y
  `vTitleLogo2` (VRAM 9310), además de `wYellowIntroAnimatedObjectStructPointer`
  (C636) para la intro de Pikachu y `wSaveFileStatus` (D087) para el menú
  principal. No hay flag de "estoy en el título"; la técnica de dirección de
  retorno viva en la pila, ya usada en `battle_state.h`, sirve para detectarlo.
- El runtime conserva `g_last_guest_framebuffer` y `g_lcd_off_framebuffer`, así
  que hay imagen válida incluso con el LCD apagado durante una carga de mapa.
- Al redactar la propuesta estaba en curso B2 de `PLAN_INTERIORES_COMBATES.md`.
  Ese plan quedó cerrado antes de iniciar este, con sus cuatro baterías de
  validación aprobadas; véase el registro al final de este documento.

## Objetivo y alcance

Que ninguna pantalla del juego rompa la presentación 3D con un corte a 2D
cuando la escena de fondo sigue siendo válida, y que las pantallas que no
tienen escena, la intro y el título, tengan una presentación propia coherente
con el prototipo. Todo el texto y todos los menús siguen siendo la imagen que
pinta el juego. No se redibuja ninguna tipografía ni se reimplementa ningún
menú. El motor recompilado sigue mandando y ningún módulo escribe en memoria.

Quedan fuera: traducir o restilizar el texto, sustituir la música, menús
nuevos de la aplicación, el Pokédex en 3D y cualquier cambio en el runtime
descargado o en el C generado.

## Decisiones de arquitectura

1. Un único compositor de framebuffer en `src/lcd_overlay.h` sustituye al
   recorte de diálogo actual. Recibe una lista de rectángulos en tiles, sube
   solo esas regiones a la textura y las dibuja con ImGui con la misma escala
   entera que hoy. `pallet3d.cpp` y `battle3d.h` lo usan; no se duplica código.
2. Un clasificador de disposición en `src/menu_layout.h` lee `wTileMap` y
   devuelve las regiones con contenido de interfaz: cuadro inferior, columna
   del menú Start, caja de sí/no, lista de tienda, o pantalla completa. Solo
   usa tiles de borde y umbrales de índice ya validados. Lo que no reconoce
   se declara pantalla completa.
3. Regla de composición: si la escena de fondo es válida, el 3D se mantiene y
   se superponen las regiones de interfaz. Si la interfaz ocupa toda la
   pantalla, el 3D se mantiene atenuado y desenfocado detrás y la imagen
   original se centra encima con marco. Nunca se vuelve a la imagen 2D a
   pantalla completa mientras exista escena válida.
4. Las transiciones se derivan del estado del juego, no de temporizadores
   propios: BGP para fundidos, `wIsInBattle` y `ready()` para la entrada en
   combate, LCD apagado y cambio de mapa para warps. El 3D reproduce la misma
   curva de brillo que la imagen original, así los tiempos coinciden con la
   música y los efectos de sonido.
5. El cambio entre 3D y 2D, cuando es inevitable, es un fundido cruzado corto
   dibujado por el módulo 3D. Mientras dura, `pallet3d_covers_frame` responde
   verdadero y el módulo dibuja el framebuffer con alfa.
6. La pantalla de título es una escena 3D propia en `src/title3d.h`: Pueblo
   Paleta ya cargado del catálogo, cámara en travelling lento, cielo y niebla
   de la primera persona, y el logo y los menús del juego compuestos desde el
   framebuffer con transparencia por color. La intro de Pikachu y las pantallas
   de copyright se conservan tal cual.
7. Cada estado nuevo tiene detección positiva y verificable. Si la detección
   falla, la conducta es la actual: imagen original completa.

## Bloque A: menús y texto sobre la escena

### Fase A1: compositor y cuadro inferior en ambas cámaras

Trabajo:

- Extraer el recorte actual de `pallet3d_draw` a `lcd_overlay.h`, con subida
  parcial por rectángulo y caché de la última región subida.
- Permitir `dialogue_overlay` en cámara ortográfica. La cámara mantiene foco y
  giro durante el texto; el HUD del prototipo se oculta mientras hay texto.
- Cubrir los cuadros de texto de interiores, carteles y NPC, que ya cumplen el
  patrón de borde completo.

Criterios de aceptación:

- [x] Hablar con un NPC en Paleta y en la casa del jugador mantiene el 3D en ortográfica y en primera persona.
- [x] El texto se lee con la misma nitidez y escala que en 2D.
- [x] Las regresiones `route`, `interior` y `journey` pasan con el compositor nuevo.
- [x] Cero errores OpenGL y WRAM, VRAM y framebuffer intactos por frame.

### Fase A2: menú Start, sí/no, tienda y disposiciones parciales

Trabajo:

- Clasificador de regiones: columna derecha del menú Start, cuadro inferior
  más caja de sí/no, cuadro inferior más lista de tienda, cuadro de texto de
  la tarjeta del Centro Pokémon. Cada caso se define por sus tiles de borde en
  posiciones fijas y se prueba con savestates locales.
- El resto de la pantalla, fuera de las regiones, deja ver el 3D. Las
  regiones se dibujan con un sombreado suave detrás para separar del fondo.
- Mientras hay menú, el HUD del prototipo desaparece y los controles relativos
  quedan neutros, como ya ocurre.

Criterios de aceptación:

- [x] Abrir Start, recorrer sus entradas y cerrarlo no abandona el 3D.
- [x] Guardar la partida con su pregunta de confirmación se hace sobre el 3D.
- [x] Comprar en la tienda de Ciudad Verde y curar en el centro mantienen la escena.
- [x] Toda disposición no reconocida sigue cayendo a la conducta de la fase A3, nunca a un frame en blanco.

### Fase A3: pantallas completas sobre fondo atenuado

Trabajo:

- Equipo, mochila, PC, Pokédex, tarjeta de entrenador, opciones y pantalla
  de nombres: la última escena 3D válida se mantiene, se atenúa y se desenfoca
  con un pase barato en el fragment shader. La imagen original se centra con
  escala entera y un marco fino.
- Conservar la escena detrás aunque el mapa vivo deje de ser válido durante
  la pantalla completa: se usa la última malla residente sin releer bloques.
- Volver a la escena desde una pantalla completa recupera cámara y HUD sin
  animar desde valores antiguos.

Criterios de aceptación:

- [x] Abrir el equipo, cambiar el orden, ver el resumen de un Pokémon y volver no produce ningún frame 2D a pantalla completa.
- [x] Usar el PC de Bill en el centro Pokémon, depositar y retirar, mantiene el fondo.
- [x] Las capturas de cada pantalla completa se revisan en ortográfica y primera persona.

## Bloque B: intro, título y menú principal

### Fase B1: detección y escena de título

Trabajo:

- Detectar el título con dirección de retorno viva de la rutina de título en
  la pila, verificando la instrucción de llamada en ROM, como hace
  `live_return`. Verificar como respaldo que `vTitleLogo` contiene el logo,
  comparando con los datos del logo en ROM una vez localizados.
- Detectar el menú principal, continuar y nueva partida, por el mismo método
  y por `wSaveFileStatus`.
- `title3d.h`: escena de Paleta cargada del catálogo sin actores, cámara con
  travelling lento en bucle, cielo y niebla de primera persona, hora fija de
  atardecer en la paleta.
- Composición del logo y de los textos del título: recorte del framebuffer con
  transparencia del color de fondo del título, escala entera y posición fija.
  El retrato de Pikachu del título se extrae de VRAM con el decodificador de
  retratos y se coloca como billboard en la escena.
- El menú principal se compone como pantalla completa de la fase A3 sobre la
  misma escena atenuada.

Criterios de aceptación:

- [ ] Arrancar sin partida y con partida llega al título en 3D con el logo y el texto originales.
- [ ] Pulsar Start, entrar en Continuar y aparecer en el mapa guardado encadena título, menú y mundo sin frames en blanco.
- [ ] Nueva partida y la pantalla de nombre se ven sobre el fondo atenuado y el nombre se escribe con normalidad.
- [ ] Dejar el título en reposo hasta que el juego vuelve a la intro no rompe la detección.

### Fase B2: intro y copyright

Trabajo:

- Conservar copyright, logo de Game Freak y la intro de Pikachu en 2D, pero
  con fundido de entrada al título 3D en lugar de corte.
- Documentar que la intro se mantiene original y por qué: su animación usa
  objetos propios y no aporta escena.

Criterios de aceptación:

- [ ] La secuencia completa desde el arranque hasta el mapa se graba y revisa sin cortes bruscos.
- [ ] `pallet_render_smoke` incorpora un modo `boot` que arranca desde ROM sin savestate y llega al menú principal.

## Bloque C: transiciones

### Fase C1: fundidos derivados de BGP

Trabajo:

- Medir en el motor las secuencias reales de BGP en fundido a negro y a
  blanco, con una traza por frame, y fijarlas como tabla documentada.
- Convertir el BGP vivo en brillo de la escena 3D: media ponderada de las
  cuatro sombras. Aplicar en el shader como multiplicador, hacia negro o hacia
  blanco según la dirección detectada.
- Mantener el 3D durante el fundido de salida de un warp y durante el fundido
  de entrada del mapa nuevo. Entre ambos, mientras el LCD está apagado o el
  mapa vivo aún no es válido, se dibuja negro o blanco según corresponda.
- Entrada por puerta: el jugador desaparece en la puerta como en el original;
  la cámara no se mueve hasta que el mapa nuevo es válido.

Criterios de aceptación:

- [x] Entrar y salir de la casa del jugador es un fundido continuo sin ningún frame 2D.
- [x] Bajar y subir escaleras del centro comercial y usar el ascensor conservan el fundido.
- [x] Cargar un savestate en otro mapa produce un fundido corto en lugar de un corte.
- [x] La duración medida del fundido coincide con la del juego original con margen de un frame.

### Fase C2: entrada y salida de combate

Trabajo:

- Desde que `wIsInBattle` se activa hasta que `ready()` acepta la escena, el
  juego ejecuta destellos y un barrido. Reproducir el destello con BGP y
  sustituir el barrido por un acercamiento de cámara hacia el jugador con
  desenfoque radial, de la misma duración medida.
- Salida del combate: fundido desde la arena hacia la escena de mapa con la
  cámara ya restaurada, sin frame intermedio 2D.
- Encuentro de entrenador: la presentación con el retrato del entrenador se
  compone sobre la arena en lugar de a pantalla completa, cuando B2 del plan
  de combates la haya cubierto.

Criterios de aceptación:

- [x] Un encuentro salvaje en Ruta 1 y un combate de entrenador en Ruta 22 se graban sin frames 2D entre mundo y arena.
- [x] La sincronía con la música de combate se conserva: el primer frame de arena coincide con el primer frame original de HUD.

### Fase C3: fundido cruzado 3D y 2D y casos especiales

Trabajo:

- Cuando la caída a 2D sea inevitable, dibujar el framebuffer completo con
  alfa creciente durante unos 200 ms sobre la última escena 3D, y lo inverso
  al volver. Incluye F2 manual, link, tutorial y Safari.
- Vuelo, teletransporte y excavar: el fundido es el de C1; la cámara se
  recoloca sin animar en el destino.
- Bicicleta y surf ya funcionan; comprobar que subir y bajar no provocan
  frames de transición.
- Pérdida de foco de la ventana y menú Esc no alteran el estado de transición.

Criterios de aceptación:

- [ ] F2 en cualquier estado produce un fundido cruzado y no un corte.
- [ ] Volar de Ciudad Verde a Paleta encadena fundido, carga y aparición sin frame 2D.
- [ ] Ninguna transición deja la máscara de controles relativos activa.

## Limitaciones asumidas

- El texto y los menús son la imagen original ampliada, con su tipografía y su
  ritmo. Las pantallas completas se enmarcan, no se rediseñan.
- La intro de Pikachu y las pantallas de copyright siguen siendo 2D.
- El barrido de entrada en combate no se reproduce tile a tile; se sustituye
  por un efecto 3D de la misma duración.
- Un menú con disposición no reconocida se trata como pantalla completa. Es
  la conducta segura, no un error.
- Las secuencias de BGP se miden en esta ROM; otra ROM no está soportada.

## Validación y entrega

- CTest completo y las tres baterías `world_qa.sh`, `firstperson_qa.sh` e
  `interiors_qa.sh` antes y después de cada fase; las 38 capturas exteriores
  siguen idénticas a la referencia salvo donde el HUD cambie.
- Nuevos modos de `pallet_render_smoke`: `menus` recorre Start, equipo,
  mochila, guardado y PC; `boot` arranca desde ROM; `transitions` graba
  entrada a casa, escaleras, ascensor, encuentro y vuelo. Cada modo comprueba
  por frame que nunca hay frame en blanco, que el modo presentado es el
  esperado y que la memoria sigue intacta.
- Pruebas unitarias del clasificador de disposición con savestates locales y
  de la tabla de BGP con las trazas medidas.
- Fixtures privadas bajo `build/qa/`: Paleta con Start abierto, centro Pokémon
  frente al PC, tienda en compra, título sin partida y con partida, Ruta 22
  frente a un entrenador. No se incorporan al repositorio.
- Entregar `build/pokeyellow3d`, `PALLET3D.md` actualizado con la tabla de
  situaciones y lo que sigue en 2D, y capturas y grabaciones en `build/qa/logs/`.
- Marcar las casillas solo con evidencia registrada.

## Orden recomendado

1. A1 y C1, porque comparten el compositor y eliminan los dos cortes más
   frecuentes: hablar y entrar en edificios.
2. A2 y A3, que completan los menús sobre la escena.
3. C2 y C3, que dependen del cierre de B2 del plan de combates.
4. B1 y B2, el título, que reutilizan todo lo anterior y no bloquean nada.

## Referencias técnicas

- [Fundidos de paleta](https://github.com/pret/pokeyellow/blob/master/home/fade.asm).
- [Transiciones de combate](https://github.com/pret/pokeyellow/blob/master/engine/battle/battle_transitions.asm).
- [Pantalla de título](https://github.com/pret/pokeyellow/blob/master/engine/movie/title.asm).
- [Intro de Amarillo](https://github.com/pret/pokeyellow/blob/master/engine/movie/intro_yellow.asm).
- [Menú principal](https://github.com/pret/pokeyellow/blob/master/engine/menus/main_menu.asm).
- [Menú Start](https://github.com/pret/pokeyellow/blob/master/home/start_menu.asm) y [disposición de su cuadro](https://github.com/pret/pokeyellow/blob/master/engine/menus/draw_start_menu.asm).
- [Carga de mapa y warps](https://github.com/pret/pokeyellow/blob/master/home/overworld.asm).
- `src/battle_state.h`, función `live_return`: técnica de detección por pila.
- `cmake/Pallet3D.cmake`: hook de `pallet3d_covers_frame` que omite la subida del framebuffer.

## Registro de ejecución

### Inicio y fase A1, 2026-09-20

- Base previa: CTest 7/7 y baterías completas del plan de interiores/combates:
  `build/qa/kanto-PW04nG/`, `firstperson-uorAnd/`, `interiors-VfIUU4/` y
  `battles-xtPavN/`. El ejecutable y las evidencias se verificaron al cerrar
  `PLAN_INTERIORES_COMBATES.md` antes de empezar este plan.
- `src/lcd_overlay.h` concentra la subida y composición de regiones en tiles.
  Usa textura propia con filtrado nearest y escala entera; conserva una caché
  de píxeles por región y solo vuelve a subir las regiones que cambian.
  `pallet3d.cpp` y `battle3d.h` usan este mismo compositor.
- Los cuadros inferiores reconocidos mantienen el mundo tanto en ortográfica
  como en primera persona. El HUD se oculta y se congela la cámara durante el
  texto. La detección valida la escena y los bloques vivos; también admite
  cargar directamente un savestate con diálogo abierto. No depende de haber
  visto previamente un frame del mapa ni de volver primero al mapa al salir
  de la lista de equipo (Corte y bicicleta cubren ambos casos).
- `dialogue` en `pallet_render_smoke` conversa con la NPC real de Paleta en
  ambas cámaras, siguiendo su posición viva. `interior` e `interior-fp`
  comprueban también la conversación con la madre.
- Las cuatro capturas están revisadas en
  `build/qa/ui-a1/logs/dialogue-review.png`. Por conversación se contrastan
  **los 7.680 píxeles del cuadro original, con cero diferencias**. Tres
  presentaciones de un estado fijo no cambian la cámara ni la imagen y no
  causan subidas de textura. Se verifican GL, controles neutros y las regiones
  completas WRAM/VRAM/framebuffer sin escrituras del renderer.
- Primer pase de interiores aprobado en `build/qa/interiors-NKNKjp/`.
  Las regresiones de Corte y bicicleta detectaron los casos de retorno desde
  menú y carga directa descritos arriba; ambos se corrigieron y sus pruebas
  aisladas pasan en `build/qa/ui-a1/logs/{cut-action,bike-use}.log`.
  La repetición final de las cuatro baterías queda aprobada:
  - Kanto: `build/qa/kanto-zkqJ2N/`, incluidos `route`, `journey`, Corte,
    bicicleta, surf y escenarios separados. Las 38 capturas siguen idénticas
    a `kanto-Jth3Ld`, según `build/qa/ui-a1/logs/exterior-comparison.txt`.
  - Primera persona: `build/qa/firstperson-flW7ij/`, controles, paridad del
    motor, ocho orientaciones y rendimiento. Se corrigió una prueba cuyo
    límite de 600 frames podía agotarse antes de que transcurrieran los
    aproximadamente 815 ms reales de interpolación. Ahora cede 2 ms entre
    frames hasta converger; no se alteró la interpolación de producción.
  - Combates: `build/qa/battles-X2ZVET/`, captura, cambios, entrenador y
    efectos. El nuevo compositor conserva la igualdad exacta de los 23.040
    píxeles del respaldo LCD de Vuelo.
  - Interiores: `build/qa/interiors-wt5ABt/`, incluidos los dos recorridos
    por la casa con comparación de píxeles del diálogo, `town`, ascensor,
    cueva y las 179 mallas.
- CTest 7/7 en las cuatro baterías. Comandos y resúmenes:
  `build/qa/logs/ui-a1-{world,firstperson,battles,interiors-final}.log`.
  `build/pokeyellow3d` compilado. Estas evidencias cierran solo A1;
  el objetivo completo sigue activo y no se acredita aún el resto del plan.

### Medición inicial de C1, 2026-09-20

- `UI_TRACE` permite al helper registrar por frame ciclos, mapa, posición,
  vista, BGP, LCDC, fuente, sprites, combate y modo presentado, sin modificar
  el motor. Traza de 2.007 frames:
  `build/qa/ui-a1/logs/house-bgp.csv`; resumen por tramos:
  `house-bgp-runs.csv` en la misma carpeta.
- En los seis cruces casa/escaleras/laboratorio se observa **E4 → F9 → FE →
  FF → E4**. Los escalones F9 y FE duran nueve frames cada uno en este runtime;
  FF dura 21. El ID de mapa de destino cambia ocho frames antes de F9,
  cuando sus demás datos todavía no forman una escena válida. Por tanto, el
  renderer deberá conservar explícitamente la escena saliente durante el
  fundido, sin deducirla del nuevo ID de mapa.
- `home/fade.asm` de pret confirma los valores y un `DelayFrames` de ocho por
  escalón; las llamadas y el muestreo de este runtime añaden el frame observado.
  No basta con programar ocho frames por temporizador. En este recorrido no
  se observa una rampa de entrada independiente: el motor restaura E4.
- Las rutinas fuente consultadas están en `build/qa/ui-a1/references/`.
  Falta medir también fundido a blanco y transiciones de combate, derivar y
  probar la tabla de brillo e integrar su aplicación. C1 sigue pendiente.

### Implementación de C1, 2026-09-20

- `src/fade_state.h` deriva el brillo de BGP con cuatro pesos iguales. La tabla
  completa y la fórmula están documentadas en `PALLET3D.md`. La detección de
  warp valida siete instrucciones CALL de la ROM y busca sus retornos en la
  pila viva, incluido el CALL condicional de `MapEntryAfterBattle`. Las
  variables de warp persistentes no bastan para activar el renderer.
- `WorldFrame` separa la preparación del mundo de su presentación. Conserva
  cámara, mallas residentes, vértices de actores y atlas durante el cambio de
  header, sin leer bloques o sprites del mapa a medio cargar. El HUD y el
  contorno a través del tejado se ocultan; la puerta puede ocultar al jugador.
- La primera prueba detectó dos frames entre el retorno de `LoadMapData` y la
  restauración de BGP. Se mantiene el extremo negro/blanco de la transición
  reconocida en ese intervalo, sin retener indefinidamente otras paletas de
  mapas oscuros. La escena nueva se prepara al volver a un mapa válido.
- La vuelta desde blanco mide **00 → 40 → 90 → E4**. La rutina espera nueve
  frames en cada uno de 40, 90 **y E4** antes de retornar. El comprobador de
  trazas se corrigió al medir ese último escalón, que inicialmente omitía.
  La entrada en combate también registra destellos de tres frames por paleta;
  su composición corresponde a C2, que continúa pendiente.
- La adaptación SDL llama a `pallet3d_state_loaded` solo después de una carga
  correcta. Un cambio de mapa hace dos mitades nominales de 90 ms, con cambio
  de malla y cámara en negro. La espera de texturas/mallas se añade al tiempo,
  y un salto del reloj de presentación no puede consumir todo el fundido.
  Las pruebas exigen varios frames intermedios visibles en ambas cámaras,
  la orientación original en primera persona y ausencia de escrituras del
  renderer. No se usa el retroceso del contador de ciclos para detectar cargas.
- Nuevos modos `transitions`, `transitions-fp`, `transitions-white`,
  `transition-reload` y `transition-reload-fp`. El observador verifica por frame
  la cobertura, memoria, GL, cámara y caché; compara el brillo de los píxeles
  y comprueba todos los píxeles de los extremos negro/blanco. `UI_VIDEO=1`
  graba los recorridos con `ffmpeg` y `UI_TRACE` conserva sus registros.
- Evidencia inicial revisada en `build/qa/ui-transitions-U2qSg6/`: 21
  transiciones y 977 frames compuestos; casa, centro comercial/ascensor en
  ambas cámaras y regreso desde blanco. `logs/fades-reviewed.png` contiene
  los escalones revisados. La traza de la casa reproduce exactamente los
  2.007 frames/ciclos/posiciones/BGP previos a C1, según
  `logs/original-comparison.txt`. El verificador final aprueba las cinco
  trazas en `logs/traces.txt`; sus primeras ejecuciones habían detectado el
  escalón E4 descrito arriba. Las cargas se volvieron a comprobar tras mejorar
  el avance con caché fría: 12 y 13 frames intermedios, respectivamente.
- Regresiones completas aprobadas:
  - Kanto: `build/qa/kanto-4e8Kfd/`. Sus 38 capturas son idénticas, byte a
    byte, a A1 (`kanto-zkqJ2N`), según `logs/exterior-comparison.txt` de la
    carpeta de transiciones.
  - Primera persona: `build/qa/firstperson-EXo8ha/`. Paridad del motor,
    controles, ocho vistas y cinco mapas residentes; medianas 3,94 ms en
    primera persona y 3,39 ms en ortográfica sobre Intel UHD 620.
  - Interiores: `build/qa/interiors-dcy8uU/`, incluidos casa, ciudad,
    escaleras, ascensor, cueva y las 179 mallas.
  - Combates: `build/qa/battles-eovzS8/`, incluidos captura, cambios,
    entrenador, efectos y cero diferencias en el respaldo LCD de Vuelo.
  - CTest final: 10/10, incluido `fade_state` y las dos pruebas sintéticas
    incorporadas al workspace durante esta fase. El último ajuste se limita
    al reloj del callback de carga, vuelto a probar en ambas cámaras.
- Comando reproducible de C1:

  ```sh
  tests/ui_transitions_qa.sh build/roms/pokeyellow.gbc \
    build/qa/interiors-fJDbZk/house.state \
    build/qa/interiors-wt5ABt/mart-start.state \
    build/qa/firstperson-9cB3FJ/route.state
  ```

- `build/pokeyellow3d` está compilado y el runtime descargado sigue sin cambios.
  La repetición final del script de C1 pasa íntegra en
  `build/qa/ui-transitions-jpxO1N/`, incluida la carga en ambas cámaras y el
  verificador de trazas corregido. Resumen:
  `build/qa/logs/ui-c1-transitions-final.log`. Esto cierra C1; A2/A3, B1/B2
  y C2/C3 aún no cumplen sus criterios y el objetivo completo sigue activo.

### Observaciones para las siguientes fases

- La tienda necesita validar bordes con solapamientos: Buy/Sell/Quit ocupa
  `(0,0,11,7)`, dinero `(11,0,9,3)` con título MONEY en el borde, stock
  `(4,2,16,11)` y cantidad `(7,9,13,3)`. La lista pisa el borde superior del
  diálogo inferior; al confirmar, el diálogo vuelve a pisar la lista. No
  basta con exigir que cada rectángulo conserve cuatro bordes completos.
  Las disposiciones privadas inspeccionadas están en
  `build/qa/ui-a1/logs/layout-mart-*.txt`.
- En la traza del encuentro de Ruta 1 de
  `build/qa/ui-c1/white/logs/bgp.csv`, el flag de combate aparece en el frame
  2669 y el detector actual `ready()` acepta la arena en el 3240. Esos 571
  frames incluyen presentación/texto, no solo el barrido. C2 deberá medir
  por separado el barrido y el primer HUD real; usar ese intervalo completo
  como duración del zoom sería incorrecto.

### Inicio de A2: clasificación aislada, 2026-09-20

- `src/menu_layout.h` reconoce bordes visibles de ventanas superpuestas en
  posiciones fijas y exige que no haya texto fuera de la unión de regiones.
  Una esquina rota o texto exterior obliga a tratar la imagen como pantalla
  completa. El clasificador aún no está conectado a la presentación.
- `tests/menu_layout_test.cpp` prueba ese rechazo y admite mapas de tiles
  privados exportados desde savestates. Se comprobaron compra, stock y
  confirmación reales de la tienda (3, 4 y 5 regiones, respectivamente), y
  equipo de combate como pantalla completa. Entradas y resultados locales:
  `build/qa/ui-a2/layouts/`.
- Pendiente en A2: validar Start/guardar/sí-no/centro con fixtures reales,
  integrar la composición con sombras y cámara congelada, y comprobar todos
  los criterios de la fase. Las disposiciones completas necesitan A3 para
  mantener el fondo atenuado y desenfocado.

### Integración de A2/A3, 2026-09-20

- El clasificador ya está conectado al renderer. Start y las ventanas parciales
  conservan la escena y añaden una sombra suave. Las pantallas completas usan
  las mallas residentes, un pase de desenfoque de nueve muestras y atenuación,
  y el framebuffer original centrado a escala entera con marco. Se reserva
  margen suficiente para que también se vean los interiores pequeños detrás.
- `menu_state.h` verifica las llamadas vivas de `DisplayTextID` desde el bucle
  del mundo y los eventos de texto predefinido. Así se conserva el fondo mientras
  equipo y PC reutilizan los bloques de WRAM. El renderer no los reconstruye
  durante esas pantallas; neutraliza el movimiento relativo y oculta su HUD.
- Las pruebas reales descubrieron una colisión del detector de C1: un registro
  guardado con valor `0200` en la segunda página del resumen parecía un retorno
  de `MapEntryAfterBattle`. Esa detección ahora excluye la vida de un menú de
  texto, incompatible con la llamada exterior de entrada al mapa. Se añadió una
  regresión junto a las pruebas de instrucciones ROM y retornos desapilados.
- Se verificaron Start, cambio de orden, ambas páginas de resumen, mochila,
  ficha, opciones, Pokédex y guardado en ambas cámaras, con cero diferencias
  en los píxeles originales compuestos: `build/qa/ui-a2/menus-{ortho,fp}/`.
  Curación y depósito/retirada real de Rattata pasan en ortográfica en
  `build/qa/ui-a2/center-ortho/`. Son evidencias de desarrollo; la batería final
  debe repetirlas después de los últimos ajustes de margen y clasificación.
- Las disposiciones reales añaden tarjeta de guardado `(4,0,16,10)`, sí/no de
  guardado `(0,7,6,5)` y Heal/Cancel del centro `(11,6,9,6)`. La cantidad de
  compra requiere conservar el orden en que la lista pisa el diálogo.
- `tests/ui_menus_qa.sh ROM TWO_POKEMON_WORLD_STATE` prepara copias privadas y
  recorre menús, centro/PC y compra en ambas cámaras. La fixture requiere
  Pokédex, encargo entregado, Pikachu y otro Pokémon capturado, y dinero para
  una Poké Ball. El guardado se exporta solo dentro de la carpeta QA.
- CTest pasa 14/14 en este punto, incluidas las pruebas sintéticas añadidas al
  workspace. A2/A3 aún no están cerradas: falta completar la batería final,
  las regresiones generales y la pantalla de nombres en ambas cámaras.

### Cierre de A2/A3, 2026-09-20

- Batería final **PASS** en `build/qa/ui-menus-P4sFEq/`; resumen en
  `build/qa/logs/ui-a2-menus-final.log`. Ocho recorridos: Start y pantallas
  completas, centro/PC, tienda e inspector de motes, cada uno en ambas cámaras.
  La escena permanece cubierta durante todos los frames observados. Se verifica
  la memoria completa WRAM/VRAM/framebuffer por frame, GL, controles neutros,
  cámara y mallas estables, y ausencia de nuevas subidas del LCD sin cambios.
- Las 60 capturas comparadas comprueban **1.238.016 píxeles originales con
  cero diferencias**, incluyendo 23.040 píxeles por pantalla completa. Registro
  desglosado: `logs/pixels.json`. `logs/layouts.txt` vuelve a clasificar todos
  los mapas de tiles privados con su resultado esperado.
- Las acciones se ejecutan con los controles originales: intercambio de
  Pikachu/Rattata, guardado en SRAM privada, compra de una Poké Ball, curación,
  depósito y retirada de Rattata. El inspector de motes abre la pantalla de
  nombres, se escribe `ABC` y se comprueba el nombre almacenado por el motor.
  No se cambian inventario, equipo o nombres para simular esos resultados.
- Capturas revisadas en `logs/full-{ortho,fp}.png`,
  `logs/partial-reviewed.png` y `logs/returns-reviewed.png`. La comprobación
  adicional de primera persona en `build/qa/ui-a2/return-fp/` conserva imágenes
  antes/después y el estado privado de regreso para repetir la inspección.
- La preparación de Paleta pasa primero por su casa y sale por la puerta real:
  los warps exteriores conservan coordenadas y los interiores con `LAST_MAP`
  dependen del mapa de procedencia. Esto evita empezar en una casilla inválida.
  Los hashes de ROM y estado de entrada permanecen intactos.
- Regresiones aprobadas, sobre el ejecutable compilado:
  - Mundo: `build/qa/kanto-cLFlTA/`. Las 38 capturas exteriores son idénticas
    a C1 (`kanto-4e8Kfd`); comparación en `logs/exterior-comparison.txt` de la
    batería de menús.
  - Primera persona: `build/qa/firstperson-v94zRL/`, incluidos controles SDL,
    paridad del motor, vistas y benchmark. Start mantiene 3D y máscara neutra.
  - Interiores: `build/qa/interiors-CyDCiu/`, recorridos reales y 179 mallas.
  - Combates: `build/qa/battles-LojUdN/`, captura, cambio, entrenador y efectos.
  - C1: `build/qa/ui-transitions-8eBpGD/`, puertas, escaleras, ascensor, vuelta
    desde blanco y carga de estado. Las cinco trazas conservan las duraciones
    originales y cobertura; la corrección de pila no altera estos fundidos.
  - CTest final 14/14. Runtime descargado sin modificaciones y `git diff --check`
    sin errores. `build/pokeyellow3d` actualizado.
- Esto cierra los menús del mundo. El título/nueva partida, entrada/salida de
  combate y los fundidos generales de 3D/2D siguen pendientes en B1/B2 y C2/C3.
  El objetivo completo permanece activo.

### Integración de C2, 2026-09-20

- `battle_transition_state.h` reconoce las llamadas originales de entrada,
  barrido, presentación, bucle y salida; verifica también destino y banco de
  las llamadas lejanas. El entrenador mantiene `wIsInBattle=0` durante su
  barrido: el flag por sí solo no puede identificarlo.
- Trazas originales en `build/qa/ui-c2/probe/`: Ruta 1 usa 108 frames de
  destellos y 30 de barrido; el rival de Ruta 22 usa 153 frames de espiral sin
  esos destellos. Los 571 frames hasta el antiguo `ready()` no son la duración
  del barrido. La escritura de E4 precede en un frame al primer cuadro visible.
- La cámara se acerca sobre las mallas retenidas, con desenfoque radial y
  progreso derivado de las escrituras de tiles del barrido original. La arena
  arranca al aparecer el cuadro original y permanece durante la presentación
  y los menús completos. Las pantallas de equipo y mochila se enmarcan sobre
  la arena atenuada. El retorno vivo de `EndOfBattle` cubre el frame en que el
  flag ya está borrado y todavía no ha comenzado el warp de regreso.
- Primer pase completo aprobado en `build/qa/ui-battles-ghqoTa/`, con encuentro
  salvaje y entrenador en ambas cámaras: primer HUD/arena en los frames 224
  y 250 respectivamente, sin huecos de cobertura ni escrituras de memoria.
  Capturas de desarrollo revisadas en `trainer-ortho/logs/review.png` y
  `build/qa/ui-c2/composed/wild/logs/review.png`.
- `tests/ui_battles_qa.sh ROM ROUTE1_10_28 ROUTE22_30_5` graba ambos encuentros
  en ambas cámaras. Comprueba memoria, GL, controles neutros y mallas/cámara
  residentes; `check_battle_timing.py` comprueba sincronía, duraciones y paridad
  exacta de las trazas del motor entre cámaras. La prueba `battle_transition`
  cubre las llamadas de los ocho barridos, pila, bancos y latencia del LCD.
- C2 aún no se da por cerrada: falta terminar la repetición con el último
  ajuste del desvanecimiento de los menús y las regresiones generales.

### Cierre de C2, 2026-09-20

- Batería final **PASS** en `build/qa/ui-battles-0luMpy/`; resumen en
  `build/qa/logs/ui-c2-battles-final.log`. Los cuatro recorridos suman **20.514
  frames cubiertos**, con GL, memoria y controles comprobados por frame.
  La cámara base y las mallas del mundo permanecen intactas durante el combate.
- `logs/timing.txt`: 30 frames de barrido y 108 de destellos en Ruta 1;
  153 de barrido y ninguno de esos destellos en Ruta 22. Primer cuadro/arena
  exactamente en 224 y 250, respectivamente, en ambas cámaras. Se conservan
  las trazas originales del motor, pila y tiles entre las dos cámaras y frente
  al renderer anterior (`logs/original-comparison.txt`). No se altera el reloj
  del juego ni sus acciones para obtener la presentación.
- Grabaciones `original-intro.mp4` y `composed.mp4` en los cuatro subdirectorios
  de la batería. Capturas de entrada, presentación, menú y regreso revisadas en
  `logs/{wild,trainer}-{ortho,fp}-review.png`, también copiadas a
  `build/qa/logs/ui-c2-{wild,trainer}-{ortho,fp}.png`.
- El cierre de los menús completos mantiene su marco durante todo el
  desvanecimiento. La información pública de la arena ya refleja también su
  presentación temprana, antes de que estén listos ambos combatientes; las
  comprobaciones exactas de retratos siguen esperando los HUD originales.
- Regresiones completas aprobadas:
  - Mundo: `build/qa/kanto-DYkIsY/`; las 38 capturas exteriores son idénticas
    a A2/A3, según `logs/exterior-comparison.txt` de la batería de C2.
  - Primera persona: `build/qa/firstperson-KiQzeG/`, incluidos controles,
    paridad, regreso al mundo, vistas y benchmark.
  - Interiores: `build/qa/interiors-OKLjRa/`, recorridos y 179 mallas.
  - Combates: `build/qa/battles-tPhG0a/`, captura, cambios, entrenador,
    debilitamiento, efectos y respaldo LCD original.
  - Menús: `build/qa/ui-menus-R28lDE/`, las ocho variantes y comparación
    exacta de los píxeles originales.
  - C1: `build/qa/ui-transitions-3a8Gs1/`, puertas, escaleras, ascensor,
    regreso desde blanco y cargas, con las cinco trazas de duración aprobadas.
  - CTest 15/15. Runtime descargado sin cambios; `git diff --check` limpio.
- `build/pokeyellow3d` y la documentación están actualizados. C2 queda cerrada.
  El goal completo sigue activo: faltan C3 (fundidos generales y viajes) y
  B1/B2 (título, menú principal, nueva partida e intro).

### Punto de pausa: avance de C3, 2026-09-20

- Goal pausado por petición del usuario; este registro permite retomar el
  trabajo sin dar por cerrada C3 ni el objetivo completo.
- Compositor de fundidos de 200 ms entre frames completos 3D/2D, después del
  dibujo de ImGui. Conserva la imagen mezclada al invertir F2 y congela el
  progreso al perder foco o abrir Esc, sin copiar el menú de ajustes al fondo.
  Los controles relativos permanecen neutros durante el fundido.
- Compilación y CTest **16/16 aprobados**. La batería
  `tests/ui_crossfade_qa.sh` termina con **PASS** en
  `build/qa/ui-crossfade-5bOG41/`; registro:
  `build/qa/logs/ui-c3-crossfade.log`. Cubre mundo, combate y puerta en ambas
  cámaras, menús, comparación de píxeles, inversión de F2 y pausa por foco/Esc.
  Los modos no soportados se comprueban mediante flags controlados en fixtures
  privadas: no equivalen a recorridos reales de link, tutorial o Safari.
- Pendiente al reanudar: revisar visualmente las capturas de esta batería;
  comprobar Vuelo Ciudad Verde→Paleta, Teletransporte y Excavar mediante el
  motor original; verificar bicicleta/surf y pausa durante carga de estado;
  ejecutar la regresión completa posterior a C3 y documentar sus resultados.
  Revisar también la validez del mapa retenido al entrar en combate tras
  desplazarse con F2 desactivado. Después quedan B1/B2 y la validación final.
- Las regresiones completas anotadas en el cierre de C2 preceden a estos
  cambios de C3; no se presentan como validación de la implementación parcial.
