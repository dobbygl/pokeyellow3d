# Plan: modo primera persona sobre el prototipo 3D

Fecha: 2026-09-19. Estado: fases 1–3 completadas y verificadas con la ROM local.

## Objetivo y alcance

Añadir una cámara en primera persona al ejecutable `build/pokeyellow3d` como
modo de presentación alternativo a la cámara ortográfica actual. El motor
recompilado sigue siendo la única autoridad sobre movimiento, colisiones,
giros, saltos de cornisa, encuentros, diálogos, combates y guardado. El modo
no escribe en la memoria de la máquina: la única vía de influencia sobre el
juego es el mando virtual del runtime.

Quedan fuera de este plan: movimiento libre entre casillas, interiores en 3D,
combates en 3D, cambios en la lógica del juego y cualquier edición del runtime
descargado o del C generado. Se conserva la coexistencia con
`PLAN_KANTO_3D.md`: este plan no depende de que sus fases 3 y 4 terminen, pero
sí de que la fase 2 esté cerrada para no editar `src/pallet3d.cpp` en paralelo.

## Punto de partida verificado

- `src/pallet3d.cpp` dibuja mallas estáticas por mapa con test de profundidad
  y sprites de actores orientados a la cámara. El vertex shader es ortográfico:
  rotación por `camera`, foco por `focus`, profundidad `-toward/256`, sin división
  perspectiva.
- `src/pallet_state.h` interpola la posición del jugador con los ocho avances
  de `wWalkCounter` (0xCFC4) y expone `view()` para decidir entre 3D y 2D.
- La orientación del jugador está en `wSpritePlayerStateData1FacingDirection`
  (0xC109): 0 abajo, 4 arriba, 8 izquierda, 12 derecha. Las mismas posiciones
  existen para cada NPC en su bloque de 16 bytes.
- El frontend SDL combina `g_manual_joypad_*` con `g_script_joypad_*` en
  `update_effective_joypad_state()` y oculta todo el mando mientras el menú Esc
  está abierto. `gb_platform_set_input_script` ya inyecta pulsaciones por frame.
- `cmake/Pallet3D.cmake` genera una copia adaptada de `platform_sdl.cpp` con
  puntos de inserción textuales que fallan si el runtime cambia.
- `pallet3d_event` recibe cada `SDL_Event` antes que el mando y puede
  consumirlo devolviendo `true`. Solo actúa cuando el 3D está presentado.
- Escala del mundo: una casilla es una unidad; sprite unos 1,9 de alto; copa de
  árbol hasta 1,68; alero de casa entre 1,5 y 1,8.
- En Pokémon Amarillo, una pulsación corta de dirección gira al jugador sin
  moverse; mantenerla inicia el paso. Es el mecanismo que usa este plan para
  girar la cámara sin simular movimiento propio.
- Las pruebas `pallet_render_smoke` comprueban en cada frame errores OpenGL, el
  modo realmente presentado y que dibujar no modifica WRAM. `tests/world_journey.h`
  conduce el motor con entradas normales.

## Decisiones de arquitectura

1. Una sola pasada de dibujo y un solo shader para ambas cámaras. Sustituir la
   rotación manual del vertex shader por una matriz vista-proyección `mat4`.
   La cámara ortográfica se expresa con la misma matriz; así el cambio en
   `pallet3d.cpp` es pequeño y no bifurca la geometría.
2. El código nuevo vive en `src/firstperson.h`: cálculo de la matriz de
   cámara, interpolación de giro y mapeador de controles. `pallet3d.cpp` solo
   consulta el modo activo y pide la matriz.
3. La orientación de la cámara es la orientación del motor, nunca al revés. La
   cámara interpola hacia el valor leído en 0xC109; no se guarda un yaw propio
   que pueda divergir de a quién se habla con el botón A.
4. Controles relativos al jugador, tipo tanque. W mantiene la dirección hacia
   la que mira. A y D inyectan un toque corto de la dirección perpendicular
   para girar sin avanzar. S inyecta el toque opuesto para dar la vuelta. Las
   flechas conservan su función original absoluta en todo momento.
5. La inyección entra por una máscara propia combinada con `&` en
   `update_effective_joypad_state()`, mediante un nuevo `pallet_hook`. No se
   reutiliza `g_script_joypad_*` para no interferir con los scripts de prueba ni
   con la grabación de entradas.
6. Fuera del estado `View::Overworld` el mapeador no inyecta nada y no consume
   teclas. Menús, diálogos, interiores y combate se comportan exactamente como hoy.
7. El ratón no gira al jugador. Como máximo ofrece una mirada cosmética de
   pocos grados que vuelve al centro al soltarlo, sin alterar la orientación
   de interacción.
8. El sprite del jugador y su silueta dorada no se dibujan en primera persona.
   Pikachu y los demás actores sí, como hasta ahora.

## Fase 1: cámara en perspectiva

Trabajo:

- Añadir el uniform `mat4 view_projection` al shader y construir con él la
  cámara ortográfica actual. Verificar con las capturas existentes que la vista
  no cambia.
- Añadir en `src/firstperson.h` la cámara en perspectiva: ojo en la posición
  interpolada del jugador más una altura de 1,1, campo visual de unos 70
  grados, plano cercano 0,1 y lejano acorde a los mapas residentes.
- Interpolar el yaw hacia la orientación del motor con una constante de unos
  150 ms; giros de 180 grados por el camino corto.
- Ocultar el actor 0 y la pasada x-ray cuando el modo está activo.
- Reorientar los billboards de actores a la posición real de la cámara, con
  base en el suelo, en lugar de la inclinación fija de la cámara ortográfica.
- Cielo con gradiente y niebla por distancia en el fragment shader; ambos
  desactivados en la cámara ortográfica.
- Tecla F3 alterna primera persona. F2 sigue alternando 3D y 2D. El HUD indica
  el modo y las teclas.

Criterios de aceptación:

- [x] La cámara ortográfica produce capturas equivalentes a las previas al cambio de shader.
- [x] En primera persona la cámara avanza con el paso del jugador sin saltos ni retrocesos.
- [x] Un giro del jugador en 2D se refleja como giro suave de la cámara.
- [x] El jugador no aparece en la imagen; Pikachu y los NPC sí, de pie sobre el suelo.
- [x] Cero errores OpenGL y WRAM intacta en cada frame, medido por `pallet_render_smoke`.

## Fase 2: controles relativos al jugador

Trabajo:

- Nuevo `pallet_hook` que inserta una máscara de d-pad propia en
  `update_effective_joypad_state()` y un setter accesible desde `firstperson.h`.
  La inserción debe fallar de forma explícita si cambia el punto de integración.
- Mapeador: W mantiene pulsada la dirección de 0xC109. A y D calculan la
  perpendicular y emiten un toque de la duración mínima que gira sin iniciar
  paso; medir esa duración con el motor y fijarla como constante documentada.
  S emite el toque opuesto. Mientras hay un toque en curso se ignoran nuevas
  órdenes de giro.
- Consumir W, A, S y D en `pallet3d_event` solo cuando el modo está activo y
  `view()` devuelve `Overworld`. En cualquier otro estado las teclas siguen su
  camino habitual y la máscara vuelve a 0xFF.
- Respetar `g_show_menu`: con el menú Esc abierto no se inyecta nada.
- Tecla R recentra la mirada cosmética si se implementa.

Criterios de aceptación:

- [x] Con W el jugador camina hacia donde mira; al soltar se detiene en la casilla siguiente.
- [x] A y D giran 90 grados sin desplazar al jugador, incluso en repetición rápida.
- [x] S da la vuelta sin desplazar al jugador.
- [x] Z habla con el NPC o cartel que ocupa el centro de la vista.
- [x] Al abrirse un diálogo, menú o combate la máscara es 0xFF y WASD vuelven al mando original.
- [x] Un script de `gb_platform_set_input_script` produce el mismo recorrido con y sin primera persona activa.

## Fase 3: recorrido, transiciones y pulido

Trabajo:

- Recorrido de prueba en primera persona sobre `tests/world_journey.h`: salir de
  Paleta, entrar en Ruta 1, cruzar la frontera de mapa, saltar una cornisa,
  provocar un encuentro, combatir en 2D y volver al 3D en primera persona.
- Al volver de un combate o de un interior, restaurar la cámara con la
  orientación del motor sin animar desde el valor antiguo.
- Superponer el cuadro de texto del juego sobre la vista 3D en `View::Dialogue`
  cuando el mapa siga siendo el mismo: copiar las filas inferiores del
  framebuffer original como textura. Si el cuadro no se detecta con fiabilidad,
  conservar la caída completa a 2D y registrar el caso.
- Añadir caras laterales con tiles a las casas y tapar los huecos que se ven
  desde dentro del mapa. Revisar árboles y rocas de cerca; conservar el estilo
  de píxel visible pero evitar caras sin textura.
- Pequeño balanceo vertical de cámara en el salto de cornisa, derivado del
  contador de paso, sin tocar la altura del suelo.
- Medir la frecuencia de frames con los cinco mapas residentes visibles desde
  el suelo y añadir descarte de mallas por distancia si hace falta.

Criterios de aceptación:

- [x] El recorrido completo pasa en `pallet_render_smoke` con un argumento `firstperson`.
- [x] Los cambios de mapa, la entrada a interiores y el retorno del combate no producen frames con cámara incorrecta.
- [x] Los diálogos se leen sin abandonar la vista 3D, o el caso está documentado como caída a 2D.
- [x] Capturas revisadas de Paleta y Ruta 1 desde el suelo, en las cuatro orientaciones.
- [x] Ninguna medida de rendimiento empeora la cámara ortográfica.

## Limitaciones asumidas

- El movimiento es por casillas y la cámara solo puede mirar en cuatro
  direcciones de interacción. No es un juego de movimiento libre.
- Los NPC fuera de la pantalla original del Game Boy se dibujan en su casilla
  con orientación pero sin animación. Desde el suelo se ve más lejos que en la
  vista original, así que se notarán actores quietos a distancia.
- Las cornisas siguen sin altura real; el salto se representa con el balanceo
  de cámara, no con un cambio de nivel.
- Interiores y combates permanecen en 2D. Entrar por una puerta abandona la
  vista en primera persona hasta volver al exterior.
- Los laterales de los edificios y las alturas son interpretaciones visuales,
  como en el resto del prototipo.

## Validación y entrega

- Ejecutar CTest completo y `pallet_render_smoke` en sus modos actuales antes y
  después de cada fase; las capturas ortográficas deben seguir siendo equivalentes.
- Añadir pruebas unitarias del mapeador: orientación a d-pad, perpendiculares,
  duración del toque y máscara 0xFF fuera del overworld.
- Usar copias aisladas de ROM y partida bajo `build/qa/`. No incorporar ROM,
  gráficos ni savestates al repositorio.
- Entregar `build/pokeyellow3d` con F3 operativo, `PALLET3D.md` actualizado con
  la tabla de teclas y las limitaciones, y capturas en `build/qa/logs/`.
- Marcar las casillas de este plan solo con evidencia registrada.

## Coordinación con el plan de Kanto

Las cuatro fases de `PLAN_KANTO_3D.md` quedaron cerradas antes de implementar
este plan. La primera persona conserva las 38 escenas, los 140 edificios y la
caché de cinco mapas. No se realizaron ediciones concurrentes del renderer.

## Referencias técnicas

- [Bucle del overworld y giro sin paso](https://github.com/pret/pokeyellow/blob/master/home/overworld.asm).
- [Avance del sprite del jugador](https://github.com/pret/pokeyellow/blob/master/engine/overworld/advance_player_sprite.asm).
- [Datos de sprites en WRAM](https://github.com/pret/pokeyellow/blob/master/ram/wram.asm).
- [Lógica de cornisas](https://github.com/pret/pokeyellow/blob/master/engine/overworld/ledges.asm).
- `build/_deps/gb_recompiled-src/runtime/src/platform_sdl.cpp`, función
  `update_effective_joypad_state`, punto de inserción de la máscara.

## Registro de implementación y verificación

### Cámara, controles y recorrido

- Implementados `src/firstperson.h`, la matriz compartida en `pallet3d.cpp`, F3,
  sprites verticales sin jugador, cielo y niebla. Laterales de casas, hojas y
  rocas reutilizan tiles del atlas sin aumentar geometría ni llamadas de dibujo.
- Se midió el toque de giro con el motor (`build/qa/firstperson/logs/turn-probe-free.log`).
  Dos frames efectivos, 140.448 ciclos, giran sin iniciar paso; el script requiere
  tres frames porque su primer frame es de activación. El canal relativo usa
  ciclos, confirma la orientación y espera reposo antes de emitir otro toque.
- Direcciones contrastadas con `pokeyellow_internal.h`: orientación C109,
  contador de paso CFC4, movimiento D527, comprobación de giro CC4B, índice de
  salto D713 y flags de movimiento D735. El renderer solo lee estos datos.
- `tests/firstperson_test.cpp`: proyecciones, equivalencia de la matriz anterior,
  suavizado y camino corto, perpendiculares, duración, confirmación del motor,
  W y máscara neutra fuera del exterior y tras retroceder ciclos.
- La adaptación SDL genera `build/pallet-runtime/platform_sdl.cpp`; no cambia
  las fuentes descargadas ni el C del juego. Los hooks comprueban su ancla.
  La máscara relativa es independiente de los scripts y se incorpora a la
  grabación manual como las direcciones reales del mando.
- Suite final de primera persona: **PASS**, `build/qa/firstperson-CWqk6R/`, ejecutada
  mediante `tests/firstperson_qa.sh`. Sus cuatro pruebas CTest pasan y los hashes
  de ROM y fixtures privadas coinciden al terminar.
- `logs/journey-fp.log` y `logs/journey-ortho.log`: 3.197 frames en cada modo,
  1.927 exteriores, 1.231 de combate, victoria real y estado WRAM final idéntico
  `fe0b1e2988cb990d`. Primera persona comprueba la posición interpolada en cada
  frame, 30 frames de balanceo de salto, ausencia del jugador y orientación
  correcta en el primer frame de retorno del combate.
- `logs/controls.log`: teclado SDL real, A/D/S, 16 giros con fases de muestreo
  distintas, repetición rápida, W y suelta, flechas, Esc, menú Start, F2 y Z
  frente al cartel. `logs/relative-input.txt` contiene las cuatro direcciones
  convertidas del control relativo, con sus duraciones en ciclos.
- `logs/house.log`: entrada a la casa en 2D, máscara neutra y restauración de
  primera persona con orientación comprobada desde el primer frame exterior.
- `logs/pallet-views.log` y `logs/route-views.log`: las cuatro orientaciones sin
  modificar la posición. PNG revisados en `build/qa/logs/firstperson/`, incluido
  `eight-views.png`. Pikachu y los NPC permanecen visibles y apoyados en el suelo.
- `sign-dialogue.png`: texto original de Paleta superpuesto a 3D. El detector
  exige borde inferior completo, mismo mapa y parte superior sin texto. Start,
  pantallas completas y otros cuadros no reconocidos conservan 2D; se documenta
  esta limitación en `PALLET3D.md`.
- Cinco mapas residentes en Azafrán: 204.360 vértices, 7.356.960 bytes de mallas.
  Siete rondas de 100 presentaciones sincronizadas con `glFinish` en Intel UHD
  620, 800×720: mediana 3,639 ms FP y 3,377 ms ortográfica en la versión final
  del renderer (`logs/benchmark-*.log`). Es tiempo de presentación, no FPS del motor.
  No hace falta añadir descarte por distancia para ese presupuesto de dibujo.

### Regresión gráfica y rendimiento

- Baseline previo conservado en `build/qa/firstperson-baseline/`. La primera
  comparación de las 38 imágenes, excluyendo el HUD actualizado, dio un máximo
  de 0,021 % de píxeles con diferencia RGB superior a 8; las diferencias mínimas
  son compatibles con el redondeo de la matriz. Informe inicial:
  `build/qa/firstperson/logs/ortho-images.json`.
- Regresión completa de Kanto antes del último ajuste del shader: **PASS**,
  `build/qa/kanto-GdyDKx/`, mediante `tests/world_qa.sh`: recorridos, menús,
  interiores, Corte y recarga, surf, bicicleta, bosque, muelle y 38 mallas.
- La primera comparación de rendimiento detectó sobrecoste ortográfico. Se
  trasladó la resolución de materiales planos al vertex shader y se separó
  la ruta uniforme ortográfica de los efectos FP, conservando un solo programa.
  El cierre siguiente registra la comparación y regresión de la versión final.

### Cierre: versión final, regresión y rendimiento

- **PASS** de CTest completo y de `tests/firstperson_qa.sh` en
  `build/qa/firstperson-CWqk6R/`. **PASS** de `tests/world_qa.sh` en
  `build/qa/kanto-e34e4y/`, con todos los modos de regresión anteriores. Resúmenes:
  `build/qa/firstperson/logs/firstperson-release.log` y `kanto-release.log`.
- Optimizaciones finales: solo transferir sprites cuando cambian sus píxeles y
  omitir la subida/composición 2D cuando el renderer 3D inicializado va a cubrir
  completamente el exterior. El framebuffer del motor y sus contadores siguen
  produciéndose; F2, inicialización, menús, transiciones e interiores conservan
  la ruta original. Se revisaron `f2-original.png` y `house-2d.png` además de las
  ocho vistas cardinales y el diálogo, en `build/qa/logs/firstperson/`.
- Las 38 capturas ortográficas finales se comparan con el ejecutable anterior,
  excluyendo únicamente las franjas del HUD actualizado. Máximo de **0,02099 %**
  de píxeles con diferencia RGB mayor de 8; la matriz también pasa la prueba
  algebraica de equivalencia. Informe: `build/qa/firstperson/logs/ortho-images-final.json`.
- Rendimiento ortográfico: cuatro pares alternados anterior/actual, 200 frames
  sincronizados por cada una de las 38 escenas, **60.800 presentaciones** en
  total. Se conservan todas las muestras de esta tanda; vértices y bytes de
  geometría idénticos en todos los mapas y rondas. Media global **2,104 → 1,423 ms**
  (**32,35 % menos**). Las medianas de los 38 mapas mejoran entre **17,74 % y
  44,89 %**. Los intervalos del 95 % de las diferencias pareadas por mapa son
  negativos; el límite superior menos favorable es −0,213 ms.
- Logs y comparación: `build/qa/firstperson/logs/performance-release/` y su
  `comparison.json`. Las tandas anteriores, afectadas por carga externa o de
  versiones intermedias, se conservan como historial y no se mezclan con esta
  medición final. Hardware Intel UHD 620, superficie 800×720, SDL `offscreen`.
- Para reproducir las mediciones, desde `build/qa/firstperson`, ejecutar
  `CATALOG_BENCH_FRAMES=200 SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy
  ./pallet_render_smoke ../roms/pokeyellow.gbc ../progress.state10 catalog`;
  sustituir el helper por `../firstperson-baseline/pallet_render_smoke` para la
  referencia anterior y alternar el orden de cada pareja. Esa referencia es un
  artefacto local conservado antes del cambio; no se distribuye con el código.
- Entregado `build/pokeyellow3d`, con F3 operativo; `PALLET3D.md` y `README.md`
  actualizados. Huellas de los ejecutables en
  `build/qa/firstperson/logs/artifacts.sha256`. Las pruebas usan copias privadas,
  comprueban sus hashes y no invocan el guardado de batería. No se modificaron
  las fuentes descargadas del runtime ni el C generado del juego.

Límites del entregable: controles por casillas y cuatro orientaciones; interiores
y combates 2D; cuadros de texto no reconocidos también 2D; alturas, laterales y
salto son interpretaciones visuales. La cámara no introduce una segunda física
ni permite atravesar las colisiones del juego.
