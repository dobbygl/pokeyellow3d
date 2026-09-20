# Plan: interiores y combates en 3D

Fecha: 2026-09-19. Estado: A1 y A2 completadas; B1 en desarrollo; B2 pendiente.

## Objetivo y alcance

Extender `build/pokeyellow3d` a los dos ámbitos que hoy siempre vuelven a 2D:
los interiores a los que se entra por warp y los combates. El motor recompilado
sigue siendo la única autoridad sobre estado, reglas, texto, menús y guardado.
Ninguna parte de este plan escribe en la memoria de la máquina; la vista se
deriva de ROM, WRAM, VRAM y del framebuffer que el juego ya ha pintado.

El plan tiene dos bloques independientes. Los interiores amplían el catálogo de
escenas y las reglas de geometría. Los combates añaden una escena nueva que no
comparte mundo con los mapas. Cualquiera de los dos puede entregarse sin el
otro. Ambos dependen de que `PLAN_KANTO_3D.md` haya cerrado su fase 2, porque
tocan `src/pallet3d.cpp` y `src/pallet_state.h`. La primera persona de
`PLAN_PRIMERA_PERSONA.md` es ortogonal: si existe, los interiores la heredan;
los combates no la usan.

Quedan fuera: combates por cable link, la lucha tutorial del anciano, el Safari
como escena distinta, modelos 3D de Pokémon, texto o menús redibujados con otra
tipografía, y cualquier cambio en el runtime descargado o en el C generado.

## Punto de partida verificado

- `src/kanto_rom.h` ya lee los 25 tilesets y, para cada mapa, sus warps con
  destino y entrada. El catálogo actual solo instancia los 36 mapas conectados
  más Bosque Verde y muelle; los destinos de warp no se cargan.
- `pallet::view()` exige que el mapa esté en el catálogo, que su tileset
  coincida con el esperado y que `wIsInBattle` (0xD056) sea cero. Por eso
  interiores y combates presentan hoy la vista 2D.
- Cada tileset expone su lista de tiles transitables (`collisions`), sus
  gráficos y su blockset. Con eso se distingue suelo de pared o mueble sin
  inventar una segunda simulación.
- `wCurMapTileset` (0xD366) identifica el tileset vivo. Los tilesets de
  interior en Amarillo son: casa de Rojo 1 y 2, tienda, dojo, centro Pokémon,
  gimnasio, casa, puerta de bosque, museo, subterráneo, puerta, barco,
  cementerio, interior, caverna, vestíbulo, mansión, laboratorio, club,
  instalación y casa de la playa. Caverna y subterráneo son cuevas, no
  edificios; se tratan aparte.
- El bucle `route` de `pallet_render_smoke` ya entra en la casa del jugador y
  comprueba el retorno a 2D. Es la primera fixture de interiores.
- Símbolos de combate disponibles en `pokeyellow_internal.h`: `wIsInBattle`,
  `wBattleType` (0xD059), `wCurOpponent` (0xD058), `wTrainerClass`,
  `wEnemyMonSpecies` (0xCFE4), `wEnemyMonHP` (0xCFE5), `wEnemyMonLevel`,
  `wEnemyMonStatus`, `wEnemyMonNick`, `wBattleMonSpecies` (0xD013),
  `wBattleMonHP` (0xD014), `wBattleMonLevel`, `wBattleMonStatus`, `wBattleMonNick`,
  `wPlayerMoveNum`, `wEnemyMoveNum`, `wMoveMenuType`, `wCurrentMenuItem`,
  `wEnemyHPBarColor`, `wSubAnimTransform` y `wLinkState`.
- Los retratos de combate no van por OAM: el juego descomprime la imagen frontal
  del rival y la trasera del jugador en la zona de tiles de fondo de VRAM y las
  pinta en `wTileMap` (0xC3A0, 20 por 18). El decodificador de `sprite_image`
  ya lee tiles de VRAM por índice; el mismo mecanismo sirve aquí.
- `gb_get_framebuffer(ctx)` devuelve la imagen LCD del frame actual. Las
  pruebas ya lo usan. Permite recortar las filas del cuadro de texto y los
  menús para componerlos sobre el 3D sin redibujarlos.
- El identificador de animación de movimiento (`wAnimationID` en pret) no está
  exportado en el header interno. Hay que verificar su dirección contra
  `wram.asm` antes de depender de él.

## Decisiones de arquitectura

1. Los interiores son escenas del mismo catálogo `kanto::World`, con
   `component` propio por mapa y origen cero. No comparten mundo con el
   exterior; el cambio de warp reajusta la cámara de forma explícita.
2. La clasificación de interior usa las colisiones del tileset como base:
   tile transitable es suelo; tile no transitable en el borde superior o en el
   perímetro es pared; tile no transitable rodeado de suelo es mueble. Los
   warps del header marcan puertas, escaleras y alfombras, que siempre son
   planos. Todo tile sin clasificar se dibuja plano con su textura original y
   se registra, igual que en el plan de Kanto.
3. La escena de combate es un módulo aparte, `src/battle3d.h`, con su propia
   cámara y geometría. `pallet3d.cpp` le cede el frame cuando `view()` devuelve
   un estado nuevo `Battle`. No se mezcla con las mallas de mapa.
4. Los retratos de combate se extraen de VRAM siguiendo `wTileMap`, no de la
   ROM comprimida. Así la imagen coincide siempre con lo que el motor decidió
   mostrar, incluidas sustituciones, cambios de Pokémon y transformaciones.
5. Texto, menús de lucha, cajas de movimientos y listas de equipo se componen
   desde el framebuffer original como textura sobre la vista 3D. No se
   reimplementa ningún menú.
6. Los marcadores de nombre, nivel, barra de vida y estado se dibujan con ImGui
   leyendo WRAM, con la misma información que muestra el juego. La barra de
   vida interpola entre lecturas para que el descenso sea continuo.
7. Ante cualquier estado no cubierto se vuelve a la vista 2D completa. Esa es
   la conducta correcta, no un fallo: combates link, tutorial, Safari,
   transformaciones no reconocidas o tilemap sin retrato.
8. Ningún módulo nuevo escribe en WRAM, VRAM ni framebuffer. Las pruebas lo
   siguen comprobando en cada frame.

## Bloque A: interiores

### Fase A1: catálogo de interiores y primer edificio

Trabajo:

- Ampliar `kanto::World` para instanciar bajo demanda los destinos de warp de
  los mapas ya catalogados, con `component` igual al propio id y origen cero.
  Resolver el destino especial "último mapa" sin cargar nada.
- Leer título de cada interior a partir del nombre de la constante de mapa;
  hasta entonces conservar "MAPA n".
- Añadir a `pallet_state.h` la selección de escena para interiores: mapa en
  catálogo, tileset coincidente, dimensiones coincidentes y estado de LCD y
  sprites igual al de exteriores.
- Regla de geometría de interior en `create_map`: pared con textura del tile
  original en las dos filas superiores y en el perímetro, altura fija de dos
  unidades; muebles como cajas con la textura del tile en la cara superior y
  color derivado en los laterales; suelo plano con su tile. Puertas, escaleras
  y alfombras planas según los warps.
- Cámara ortográfica encuadrando la sala completa, sin giro libre en salas
  pequeñas, y reajuste explícito al entrar o salir.
- Paleta de interior distinta a la de exterior en el atlas, elegida por tileset.
- Primer hito: la casa del jugador, planta baja y planta alta, y el laboratorio
  de Oak.

Criterios de aceptación:

- [x] Entrar en la casa del jugador desde Paleta muestra la sala en 3D; subir la escalera cambia de escena; salir devuelve el exterior con su cámara.
- [x] Los NPC de la sala se sitúan sobre el suelo y las conversaciones funcionan con la caída a 2D actual.
- [x] El PC, la televisión, la mesa y la cama tienen volumen; ningún mueble oculta permanentemente al jugador.
- [x] `route` sigue pasando y añade la comprobación de que el interior se presenta en 3D.
- [x] Cero errores OpenGL y WRAM intacta en cada frame.

### Fase A2: cobertura de los edificios de los 36 mapas

Trabajo:

- Recorrer todos los warps de los 36 mapas exteriores y de los interiores
  alcanzados desde ellos, con límite de profundidad, y auditar: dimensiones,
  tileset, tiles sin clasificar, muebles inferidos y paredes. Informe CSV bajo
  `build/qa/logs/` como el de Kanto.
- Reglas por tileset para tienda, centro Pokémon, gimnasio, casa, laboratorio,
  puerta, mansión, vestíbulo, club, instalación, museo, barco y cementerio.
  Mostradores y estanterías son muebles altos; mesas de curación y puertas de
  gimnasio, bajos.
- Interiores de varias plantas y edificios grandes: Torre Pokémon, Silph,
  centro comercial de Azulona y el barco. Cámara con giro limitado y zoom en
  salas anchas.
- Cuevas y subterráneo: techo oscuro con niebla, paredes de roca por colisión,
  agua interior. Se aceptan como escenas de este bloque pero no bloquean la
  entrega del resto.
- Reutilizar las sustituciones de bloques vivos de WRAM para puertas que se
  abren, ascensores y muros que se mueven por script.

Criterios de aceptación:

- [x] La auditoría lista todos los interiores alcanzados y ninguno falla al decodificar.
- [x] Tienda, centro Pokémon y gimnasio de Ciudad Verde revisados con capturas.
- [x] Un recorrido curar en el centro Pokémon, comprar en la tienda y volver a Ruta 1 pasa en `pallet_render_smoke`.
- [x] Cambiar de planta, usar un ascensor y entrar en una cueva no dejan frames con escena incorrecta.
- [x] La caché de mallas sigue acotada al mapa actual y sus vecinos.

## Bloque B: combates

### Fase B1: escena estática con retratos y marcadores

Trabajo:

- Añadir `View::Battle` a `pallet_state.h`: `wIsInBattle` distinto de cero,
  `wLinkState` cero, `wBattleType` distinto del tutorial, LCD activa y retrato
  del rival presente en `wTileMap`. Hasta que el retrato aparece, se conserva
  la transición 2D original.
- `src/battle3d.h`: arena con suelo derivado del terreno donde empezó el
  combate, leído del tile de la casilla del jugador en el mapa anterior:
  hierba, tierra, agua, cueva o interior de gimnasio. Dos plataformas, una
  lejana para el rival y otra cercana para el jugador.
- Decodificar el retrato frontal del rival y el trasero del jugador desde VRAM
  siguiendo los rectángulos de `wTileMap` que pinta el motor. Verificar esos
  rectángulos en `engine/battle/core.asm` y documentarlos como constantes.
  Colorear con la paleta por especie de la ROM cuando esté disponible;
  escala de grises del juego en caso contrario.
- Billboards de ambos retratos orientados a la cámara; cámara detrás y por
  encima del jugador mirando al rival, con un balanceo lento en reposo.
- Marcadores con ImGui: mote, nivel, barra de vida con el color que decide el
  juego, estado, barra de experiencia y bolas de equipo en combates de
  entrenador, todo leído de WRAM.
- Componer desde el framebuffer las seis filas inferiores del LCD como textura
  al pie de la pantalla. Cuando el juego abre la lista de equipo, la mochila o
  un menú que ocupa toda la pantalla, componer la pantalla entera.
- Cambio de Pokémon y derrota: al cambiar la especie o llegar la vida a cero,
  el billboard se desvanece y el nuevo aparece; la silueta no se inventa.

Criterios de aceptación:

- [x] Un encuentro salvaje en Ruta 1 se presenta en 3D con ambos retratos correctos y sus marcadores coincidentes con los valores de WRAM.
- [x] Menú de lucha, elección de movimiento, mochila, lista de equipo y huida son operables y legibles.
- [ ] El cambio de Pokémon, la derrota de un rival y el fin del combate no dejan frames con retrato equivocado.
- [x] El recorrido `journey` gana su combate con la escena 3D activa y vuelve a Ruta 1 en 3D.
- [x] Cero errores OpenGL y WRAM, VRAM y framebuffer intactos en cada frame.

### Fase B2: animaciones y feedback

Trabajo:

- Verificar la dirección de `wAnimationID` y de los contadores de subanimación
  en `wram.asm`, y detectar el inicio y el fin de cada animación de movimiento.
- Primera entrega: durante una animación se compone la pantalla original
  completa sobre el 3D, con un fundido de entrada y salida. Es fiel, cubre los
  165 movimientos y las capturas de Poké Ball, y no requiere clasificar nada.
- Segunda entrega: efectos 3D por categoría, derivados del tipo del movimiento
  leído de la tabla `Moves` de la ROM y de si el objetivo es el rival o uno
  mismo: golpe físico con embestida del billboard, proyectil con estela,
  estado con destello sobre el objetivo, movimiento propio con brillo. Los
  movimientos no clasificados conservan la primera entrega.
- Feedback de daño: sacudida del billboard y parpadeo cuando la vida baja;
  animación de la barra al ritmo del juego.
- Captura de Pokémon salvajes: trayectoria de la bola y sacudidas siguiendo el
  texto y el estado del motor, con la primera entrega como respaldo.
- Combates de entrenador: retrato del entrenador desde VRAM durante la
  presentación, con la clase leída de `wTrainerClass`.

Criterios de aceptación:

- [ ] Toda animación de movimiento se ve completa, en 2D compuesto o en 3D, sin frames negros ni retratos duplicados.
- [ ] Al menos las cuatro categorías básicas tienen efecto 3D y las capturas revisadas muestran cada una.
- [ ] Una captura de Pokémon salvaje y un combate de entrenador pasan en `pallet_render_smoke` con la escena activa.
- [ ] Ningún efecto depende de escribir el estado de la máquina.

## Limitaciones asumidas

- Los Pokémon son retratos planos del juego orientados a la cámara, no modelos.
  Es coherente con los actores del prototipo y con la resolución original.
- Los interiores tienen alturas y muebles interpretados; los tiles sin regla se
  ven planos hasta que se clasifiquen.
- Texto y menús son la imagen original ampliada. Mantienen su tipografía y su
  ritmo de escritura.
- Combates link, tutorial del anciano y Safari permanecen en 2D.
- Las cuevas se aceptan como escenas de interior con reglas mínimas; su
  fidelidad artística no bloquea la entrega.

## Validación y entrega

- Ejecutar CTest y los modos actuales de `pallet_render_smoke` antes y después
  de cada fase. Las capturas de exterior deben seguir siendo equivalentes.
- Nuevos modos de `pallet_render_smoke`: `interior` para casa y laboratorio,
  `town` para el recorrido por Ciudad Verde, `battle3d` para encuentro,
  captura y entrenador. Cada modo comprueba modo presentado, GL y memoria
  intacta por frame y deja capturas en `build/qa/logs/`.
- Pruebas unitarias del clasificador de interior, de la selección `Battle` y de
  la lectura de rectángulos de retrato con savestates locales.
- Fixtures privadas bajo `build/qa/`: savestate en la casa del jugador, en el
  centro Pokémon de Ciudad Verde, al inicio de un encuentro y frente a un
  entrenador de Ruta 22. No se incorporan al repositorio.
- Entregar `build/pokeyellow3d`, `PALLET3D.md` actualizado con el alcance de
  interiores y combates, el informe CSV de interiores y las capturas.
- Marcar las casillas solo con evidencia registrada.

## Orden recomendado

1. Fase A1, porque reutiliza el lector y las mallas existentes y tiene fixture.
2. Fase B1, porque desbloquea la parte más visible con riesgo acotado.
3. Fase A2 y fase B2 en paralelo si hay dos ventanas de trabajo; si no, A2
   antes, porque su auditoría también sirve al plan de Kanto.

## Referencias técnicas

- [Headers de tilesets y colisiones](https://github.com/pret/pokeyellow/blob/master/data/tilesets/tileset_headers.asm).
- [Constantes de tilesets](https://github.com/pret/pokeyellow/blob/master/constants/tileset_constants.asm).
- [Warps y objetos de mapa](https://github.com/pret/pokeyellow/blob/master/macros/scripts/maps.asm).
- [Núcleo de combate y dibujo de retratos](https://github.com/pret/pokeyellow/blob/master/engine/battle/core.asm).
- [Animaciones de movimiento](https://github.com/pret/pokeyellow/blob/master/engine/battle/animations.asm).
- [Paletas por especie](https://github.com/pret/pokeyellow/blob/master/data/pokemon/palettes.asm).
- [Memoria del juego](https://github.com/pret/pokeyellow/blob/master/ram/wram.asm).

## Registro de ejecución

### A1: casa del jugador y laboratorio, 2026-09-20

- Catálogo bajo demanda, títulos de constantes, escenas aisladas y atlas por
  tileset. `LAST_MAP` y el destino dinámico del ascensor Silph no se instancian.
- `interior_test`: clasificador, puertas/escaleras planas, muebles de ambas
  plantas, selección de vista y recursos disponibles en la ROM del runtime.
  Como preparación de A2, el recorrido recursivo encuentra 221 mapas y 25
  tilesets; falta su auditoría artística y la integración completa de A2.
- `interior` e `interior-fp`: recorrido por ambas plantas, conversación con la
  madre, regreso a Paleta, laboratorio de Oak y salida. Movimiento real del
  motor, sin modificar la partida; GL y WRAM/VRAM/framebuffer comprobados en
  cada frame. Evidencias en `build/qa/interiors/logs/interior-a1*.log` y
  capturas `reds-house-*.ppm`, `mother-dialogue*.ppm`, `oaks-lab*.ppm`.
- CTest 5/5: `build/qa/interiors/logs/a1-ctest.log`.
- Regresión completa Kanto: `build/qa/kanto-02mrNg/`; primera persona:
  `build/qa/firstperson-VRYzJq/`. Ambas PASS. Las 38 capturas de exterior son
  idénticas byte a byte a la referencia anterior a A1 `kanto-Jth3Ld`;
  comprobación en `build/qa/interiors/logs/a1-exterior-comparison.txt`.
- Ejecutable `build/pokeyellow3d` compilado. Estos resultados cierran A1;
  no acreditan los criterios de A2, B1 ni B2.

### B1: primer encuentro y menús, 2026-09-20 (fase incompleta)

- `src/battle_state.h` verifica el combate normal, el HUD y los rectángulos
  de retrato. `init_battle.asm` confirma columnas de siete tiles, rival en
  (12,0), jugador en (1,5). La ventana LCD usa normalmente 9C00 con WX=7/WY=0;
  comprobar solo el mapa de fondo 9800 impediría validar los retratos.
- `src/battle3d.h` presenta arena, plataformas, cámara propia, retratos de VRAM
  con paletas CGB de la ROM, nombres, nivel, estado, HP y experiencia. Conserva
  la imagen original durante menús extensos y animaciones. Se corrigió la
  transparencia del torso blanco en retratos traseros recortados por abajo.
- `battle_state_test` verifica selección, rechazo de link/tutorial/Safari,
  lectura de marcadores, orden de tiles, transparencia, paletas, experiencia,
  disponibilidad en el manifiesto y que `wAnimationID` por sí solo no indica
  animación. Se verifica el retorno de `PlayMoveAnimation` en la pila activa;
  los contadores y el ID tienen alias y persisten fuera de la animación.
- `build/qa/interiors/logs/battle-probe-3d.log`: encuentro real completo,
  913 frames con escena de combate, 529 con arena visible; GL y todas las
  regiones WRAM/VRAM/framebuffer intactas en cada frame. Captura revisada:
  `build/qa/interiors/logs/battle-arena.ppm`.
- `build/qa/interiors/logs/b1-menus.log`: lucha, movimientos, mochila, equipo,
  huida y regreso a Ruta 1, mediante botones originales. Capturas y estados
  privados `battle-{fight,moves,bag,party,escaped}.*` en la misma carpeta.
- `build/qa/interiors/logs/b1-journey.log`: victoria real, 446 frames con escena
  de combate activa y regreso al exterior, sin cambios del renderer en memoria.
- **Pendiente para cerrar B1:** validar cambios de Pokémon, derrota del jugador
  y reemplazo de rival sin retratos antiguos. B2 sigue pendiente: el respaldo original
  no equivale a tener los cuatro efectos 3D, captura ni introducción de entrenador.
- Regresiones de esta versión: CTest 6/6 y Kanto completo en
  `build/qa/kanto-N2q3I6/`; primera persona completa en
  `build/qa/firstperson-OIbyVs/`, incluyendo paridad exacta del estado final
  entre ambas cámaras y controles SDL. Las 38 capturas de exterior siguen
  idénticas a la referencia anterior a A1; resultado en
  `build/qa/interiors/logs/b1-exterior-comparison.txt`.
- Se corrigió una expectativa del helper: un diálogo superpuesto mantiene
  viva la cámara en primera persona, mientras que la escena de combate usa
  otra cámara. La comprobación de orientación al regresar distingue ambos casos.

### A2: catálogo, Ciudad Verde y transiciones, 2026-09-20

- `tests/interiors_qa.sh` reproduce A1 y A2 con copias privadas de ROM y
  estados. Ejecución completa PASS en `build/qa/interiors-fJDbZk/`, registro
  `build/qa/interiors/logs/a2-suite.log`; CTest 7/7. Los hashes de las entradas
  se verifican al terminar y no se invoca el guardado de batería.
- `interior_audit`: 221 mapas alcanzados, 179 interiores y 25 tilesets, con
  límite de profundidad 32. CSV en `build/qa/logs/interiors.csv` y en la carpeta
  de la batería. Cero gráficos fuera de rango o warps elevados. Se registran
  **2.960 casillas sin clasificación artística en 104 interiores**: conservan
  su textura plana. No se confunde decodificación completa con arte completo.
- Reglas por familia de tileset para estanterías, mostradores, mesas, plantas,
  equipo de curación, puertas de gimnasio, paredes y roca. Los muebles del
  perímetro se clasifican antes de aplicar la pared genérica. Los casos no
  reconocidos ya no reciben automáticamente una caja de mueble.
- `town`: cura de 18 a 21 HP, primera visita a la tienda, entrega del paquete
  y obtención de la Pokédex, compra de diez Poké Balls, segunda visita al
  centro y vuelta a Ruta 1. Todo mediante movimiento y botones originales;
  termina en `logs/town-route1.state` con diez bolas. El diálogo de la Pokédex
  se espera hasta que el script devuelve el control, con límite de seguridad.
- `interior-transitions`: escaleras 1F–2F–1F del centro comercial, giro/zoom de
  sala grande, entrada al ascensor, selección real de 2F y salida a la planta
  elegida. Cueva Diglett: entrada, cambio a la cueva principal y regreso.
  La preparación de estas fixtures solicita un warp al motor; los cruces
  evaluados y el menú de ascensor usan controles normales.
- Revisadas las capturas de centro, tienda y gimnasio de Ciudad Verde, las
  plantas y el menú del ascensor. Contactos en
  `build/qa/interiors/logs/a2-{viridian,elevator}-review.png`. Captura adicional
  mirando hacia el pasillo de la cueva:
  `build/qa/interiors/logs/diglett-cave-fp.png`, con techo oscuro y niebla.
- `interior-catalog` construye las 179 mallas con el mismo renderer, comprueba
  expulsión de caché hasta una malla residente, GL y todas las regiones de
  WRAM, VRAM y framebuffer intactas. Los recorridos comprueban los mismos
  invariantes por frame, incluidos diálogos y transiciones.
- Los interiores reutilizan la lectura e invalidación de bloques vivos de la
  escena actual. La regresión de Corte/carga sigue verificando ese mecanismo
  común; no se afirma haber accionado cada puerta de Silph o cada script.
- Regresión completa de Kanto PASS en `build/qa/kanto-0dH5RK/`; primera persona
  PASS en `build/qa/firstperson-9cB3FJ/`, incluida paridad del estado del motor.
  Las 38 capturas exteriores son idénticas byte a byte a `kanto-Jth3Ld`, según
  `build/qa/interiors/logs/a2-exterior-comparison.txt`.
- `build/pokeyellow3d` compilado. A2 queda cerrada con las limitaciones artísticas
  anteriores. El objetivo completo sigue pendiente de B1 y B2.

### Preparación de B1/B2: captura real, 2026-09-20

- Nuevo modo de prueba `battle-capture`, a partir de `town-route1.state`.
  Encuentro salvaje por movimiento normal, apertura de mochila y lanzamiento
  de las Poké Balls compradas. Se captura un Pidgey de nivel 4 tras cuatro
  intentos y el equipo pasa de uno a dos miembros, sin conceder Pokémon ni
  modificar inventario o RNG desde el helper.
- Evidencia PASS: `build/qa/interiors/logs/b2-capture-probe.log`, con 1.573 frames
  de escena de combate y memoria/GL comprobados por frame. Revisión visual en
  `build/qa/interiors/logs/b2-capture-review.png`. Estado privado del equipo
  resultante: `build/qa/interiors/logs/capture-complete.state`.
- La animación de captura sigue siendo la original en 2D. Esta prueba prepara
  la validación de cambios de equipo y no acredita aún los efectos 3D ni el
  combate de entrenador exigidos por B2.
