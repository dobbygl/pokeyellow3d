# Plan: menús, pantalla de título y transiciones

Fecha: 2026-09-20. Estado: propuesta; ninguna fase iniciada.

## Análisis del estado actual

Lo que el ejecutable `build/pokeyellow3d` presenta hoy en 3D y lo que devuelve
a la imagen original, según `src/pallet_state.h`, `src/pallet3d.cpp` y los
registros de los tres planes anteriores:

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
- Codex está ejecutando B2 de `PLAN_INTERIORES_COMBATES.md` y edita
  `src/battle_state.h`, `src/battle3d.h` y `src/pallet3d.cpp`. Este plan debe
  empezar cuando B2 cierre o en una ventana acordada.

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

- [ ] Hablar con un NPC en Paleta y en la casa del jugador mantiene el 3D en ortográfica y en primera persona.
- [ ] El texto se lee con la misma nitidez y escala que en 2D.
- [ ] Las regresiones `route`, `interior` y `journey` pasan con el compositor nuevo.
- [ ] Cero errores OpenGL y WRAM, VRAM y framebuffer intactos por frame.

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

- [ ] Abrir Start, recorrer sus entradas y cerrarlo no abandona el 3D.
- [ ] Guardar la partida con su pregunta de confirmación se hace sobre el 3D.
- [ ] Comprar en la tienda de Ciudad Verde y curar en el centro mantienen la escena.
- [ ] Toda disposición no reconocida sigue cayendo a la conducta de la fase A3, nunca a un frame en blanco.

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

- [ ] Abrir el equipo, cambiar el orden, ver el resumen de un Pokémon y volver no produce ningún frame 2D a pantalla completa.
- [ ] Usar el PC de Bill en el centro Pokémon, depositar y retirar, mantiene el fondo.
- [ ] Las capturas de cada pantalla completa se revisan en ortográfica y primera persona.

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

- [ ] Entrar y salir de la casa del jugador es un fundido continuo sin ningún frame 2D.
- [ ] Bajar y subir escaleras del centro comercial y usar el ascensor conservan el fundido.
- [ ] Cargar un savestate en otro mapa produce un fundido corto en lugar de un corte.
- [ ] La duración medida del fundido coincide con la del juego original con margen de un frame.

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

- [ ] Un encuentro salvaje en Ruta 1 y un combate de entrenador en Ruta 22 se graban sin frames 2D entre mundo y arena.
- [ ] La sincronía con la música de combate se conserva: el primer frame de arena coincide con el primer frame original de HUD.

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

- [Fundidos de paleta](https://github.com/pret/pokeyellow/blob/master/engine/gfx/palettes.asm).
- [Transiciones de combate](https://github.com/pret/pokeyellow/blob/master/engine/battle/battle_transitions.asm).
- [Pantalla de título](https://github.com/pret/pokeyellow/blob/master/engine/movie/title.asm).
- [Intro de Amarillo](https://github.com/pret/pokeyellow/blob/master/engine/movie/intro_yellow.asm).
- [Menú principal](https://github.com/pret/pokeyellow/blob/master/engine/menus/main_menu.asm).
- [Menú Start y cuadros de texto](https://github.com/pret/pokeyellow/blob/master/engine/menus/start_menu.asm).
- [Carga de mapa y warps](https://github.com/pret/pokeyellow/blob/master/home/overworld.asm).
- `src/battle_state.h`, función `live_return`: técnica de detección por pila.
- `cmake/Pallet3D.cmake`: hook de `pallet3d_covers_frame` que omite la subida del framebuffer.
