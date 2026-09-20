# Diagnóstico: el laboratorio de Oak «se ve en 2D»

Fecha: 2026-09-20. Estado: diagnóstico cerrado, plan propuesto, sin implementar.
Alcance: sólo análisis. No se ha modificado `src/`, `tests/`, `cmake/`,
`CMakeLists.txt` ni el contenido previo de `build/`. Todas las ejecuciones se
hicieron sobre copias privadas en `build/qa/oaklab/`, nunca sobre el ejecutable,
la ROM ni la partida originales del usuario.

Resumen en una línea: **el 3D sí se activa dentro del laboratorio; lo que falla
es la cámara.** Los interiores se dibujan con `room_yaw = 0`, y con giro cero la
proyección ortográfica de `firstperson::orthographic` colapsa a una elevación
frontal donde **todas las caras de normal X miden cero píxeles de ancho**. Sólo
quedan visibles las caras superiores —que llevan el arte original del tileset— y
el resultado es un aplastamiento vertical uniforme del tilemap 2D.

---

## 1. Hechos verificados

### 1.1 El binario corresponde al código actual

El diagnóstico se ejecutó contra **dos binarios consecutivos** y da el mismo
resultado en los dos. El segundo es el que hay ahora mismo en `build/`.

| Binario | Compilado | `sha256` | Fuente más reciente | Verificado |
| --- | --- | --- | --- | --- |
| Primero | 11:23:52 | `eb993e75…9375` | `src/pallet3d.cpp` 11:23:46, 790 líneas | sí |
| Segundo (actual) | 11:38:03 | `9c34a4ed…a408` | `src/pallet3d.cpp` 11:37:58, 803 líneas | sí |

En ambos momentos `find src cmake CMakeLists.txt -newer <binario>` devolvió
vacío: cada binario era posterior a todas sus fuentes. `git log -1` sigue siendo
`3654e37 fix: restore validated renderer before overlay refactor`; los cambios
de las 11:37 están sin commitear.

Los únicos archivos más nuevos que el binario eran `tests/check_ui_traces.py` y
`tests/ui_transitions_qa.sh`, que no se compilan dentro de `pokeyellow3d`. **No
hizo falta compilar una copia propia**: el binario probado es el del código
actual. Hashes de todas las entradas en
`build/qa/oaklab/logs/inputs.sha256`.

La segunda compilación la hizo el agente Codex a mitad del análisis. La
repetición contra ese binario está en `build/qa/oaklab/rebuild/` y da
exactamente el mismo resultado: `state=3` dentro del mapa 40, `vertices=5154` y
capturas con y sin cámara idénticas (`md5 a3bb4ba3…f707`). Los cambios de Codex
desplazaron números de línea pero no tocaron ninguna de las funciones
implicadas.

### 1.2 Dentro del laboratorio el estado SÍ es Overworld (3D activo)

Se cargó una copia de la partida real del usuario (`build/pokeyellow.state10` →
`build/qa/oaklab/user-state10.state`, Pueblo Paleta 8,15, equipo de 1, Pikachu
nivel 7, `font=0`) y se caminó hasta el laboratorio con el modo `play` de
`pallet_render_smoke`, sólo con entradas de mando. Ruta reconstruida paso a
paso, todos los tramos en `build/qa/oaklab/logs/walk-step*.log`:

```
(8,15) → (12,15) → (12,14) → (14,14) → (11,14) → (8,16) → (8,6)
       → (8,11) → (8,13) → (9,13) → (9,12) → (12,12) → puerta
```

Traza del cruce de la puerta, `build/qa/oaklab/logs/walk-enter-lab.log`
(`PALLET3D_TRACE=1`, entrada `10:U:40`, 260 frames):

```
[3D] state=3 map=0  xy=12,12 font=0 sprites=1 bgp=e4
[3D] state=0 map=40 xy=12,11 font=0 sprites=1 bgp=e4
[CROSSING] frame=29 0 -> 40 xy=12,11
[3D] state=1 map=40 xy=5,11 font=0 sprites=1 bgp=ff
[3D] state=3 map=40 xy=5,11 font=0 sprites=1 bgp=e4      <-- línea clave
[3D] mesh map=40 vertices=5154 bytes=185544 build=0.19ms resident=1
[PLAY] map=40 xy=5,11 battle=0 party=1 hp=21 font=0
```

Lectura: un frame en `Unsupported` (0) mientras el header ya es 40 pero las
coordenadas siguen siendo las del exterior, un tramo en `Transition` (1) con
`bgp=ff` (fundido del motor) y **estabilización en `Overworld` (3)**, tras la
cual el renderer construye la malla del interior. El modo `play` valida en cada
frame que `pallet3d_active()` coincide con `view()`; no hubo ningún retorno 23.

Por tanto **queda descartada la hipótesis de fallo de activación**. No falla
`ensure_scene` (la escena 40 existe y se usa), no falla `valid_live_map` (si
fallase el estado sería 1, `Transition`), no falla `load_catalog` (el log dice
«39 scenes» tras descubrir el interior), y `font=0` durante todo el recorrido:
en esta partida no hay ningún script de Oak activo que cargue la fuente.

El mismo estado se reproduce con la fixture `build/qa/interiors/logs/oaks-lab.state`
(`build/qa/oaklab/logs/probe-oaks-lab.log`: `[SMOKE] view=3 map=40 xy=5,11`).
Partida y fixture llegan al mismo sitio.

Estado guardado de la partida del usuario ya dentro del laboratorio:
`build/qa/oaklab/logs/user-inside-lab.state`.

### 1.3 La imagen resultante es geométricamente indistinguible del 2D

Capturas nuevas (PPM crudo en `logs/`, PNG convertido con Python sin dependencias
externas en la raíz de `build/qa/oaklab/`):

| Captura | Qué muestra |
| --- | --- |
| `build/qa/oaklab/user-oaklab.png` | Partida real del usuario dentro del laboratorio, `state=3` |
| `build/qa/oaklab/user-pallet.png` | La misma partida en Pueblo Paleta, para contraste |
| `build/qa/oaklab/ref-oaks-lab.png` | Fixture A1 del laboratorio (conversión de `build/qa/interiors/logs/oaks-lab.ppm`) |
| `build/qa/oaklab/ref-oaks-lab-fp.png` | El mismo laboratorio en primera persona |
| `build/qa/oaklab/ref-mart-2f.png` | Centro Comercial Azulona 2F con `room_yaw=0` |
| `build/qa/oaklab/ref-mart-2f-camera.png` | El mismo mapa tras diez pulsaciones de `E` |
| `build/qa/oaklab/ref-exterior-final-camera.png` | Exterior de referencia (`build/qa/logs/final-camera.ppm`) |

### 1.4 La cámara del laboratorio está bloqueada por completo

`pallet3d.cpp:693-702` sólo permite girar y hacer zoom en interiores cuando
`large = room->width>14 || room->height>14`. Auditoría de la ROM
(`build/qa/logs/interiors.csv`): **OAKS LAB mide 10 × 12 casillas**, por lo que
`large` es falso y `Q`, `E` y la rueda del ratón devuelven `true` sin hacer
nada.

Comprobación empírica con el modo `camera` del smoke, que pulsa `E` diez veces y
aplica una rueda `+4` antes de capturar:

```
md5  a3bb4ba358e05497b49d6fc6d276f707  logs/user-oaklab-camera.ppm
md5  a3bb4ba358e05497b49d6fc6d276f707  logs/user-oaklab-plain.ppm
```

**Las dos capturas son idénticas byte a byte.** El HUD sigue anunciando
«Q / E Girar     Rueda Zoom» (`pallet3d.cpp:596`), controles que en este mapa no
existen.

Alcance del bloqueo sobre el catálogo completo: de los **179 interiores, 107
(59,8 %) tienen la cámara congelada** en `room_yaw = 0`; sólo 72 pueden girarse.

### 1.5 Desglose exacto de la geometría del mapa 40

`build/qa/logs/interiors.csv`, fila del mapa 40:

```
map=40  OAKS LAB  interior=1  tileset=5  width=10  height=12  warps=2
floor=80  warp_cells=2  wall=8  furniture=30  counter=0  water=0
unclassified_flat_cells=0  unknown_graphics=0
```

Nota de corrección sobre el enunciado: el laboratorio usa **tileset 5**
(`case 5: case 7:` de `interior_scene.h:42`, «Lab / dojo / gym»), no el 20. El
tileset 20 («Research lab») lo usan otros 7 mapas. La rama de paleta es la misma
(`pallet3d.cpp:186` agrupa `id==5||id==7||id==20||id==22`).

La clasificación artística del laboratorio es **completa**: 0 celdas sin regla y
0 gráficos desconocidos. La falta de cobertura del clasificador no interviene en
este problema.

Presupuesto de vértices reconstruido desde `interior_map` y contrastado con la
traza `vertices=5154`:

| Origen | Cálculo | Vértices |
| --- | --- | --- |
| Suelo, `create_map:227-231` | 20 × 24 quads × 6 | 2.880 |
| Zócalo de la sala, `create_map:235` | 5 quads × 6 | 30 |
| 30 muebles | (5 quads caja + 4 quads tapa) × 6 | 1.620 |
| 8 paredes | (5 + 4 + 4 quads) × 6 | 624 |
| **Total** | | **5.154** ✓ |

El 56 % de los vértices es suelo plano y las 38 celdas con volumen aportan
2.244 vértices, un desglose que cuadra al vértice con la traza. Para dar escala,
Pueblo Paleta genera 15.162 vértices sobre 360 casillas frente a las 120 del
laboratorio: por casilla, 42 vértices en el exterior y 43 en el interior. **El
problema no es falta de geometría.** (El reparto interno del exterior entre
casas, árboles, cornisas y hierba no se ha desglosado; sólo se compara el
total.)

### 1.6 Diversidad cromática medida

Píxeles distintos del fondo, contados sobre las capturas de 800×720
(`build/qa/oaklab/stats.py`, sin dependencias externas; la conversión a PNG
usa `build/qa/oaklab/ppm2png.py`):

| Captura | Colores únicos | Píxeles fuera de la banda verde/teal |
| --- | ---: | ---: |
| `user-oaklab.ppm` (laboratorio) | 323 | 0,5 % |
| `mart-2f.ppm` (interior, `yaw=0`) | 206 | 20,1 % |
| `mart-2f-camera.ppm` (interior, `yaw≈0,4`) | 349 | 31,3 % |
| `user-pallet.ppm` (exterior) | 2.751 | 41,1 % |
| `kanto-pallet.ppm` (exterior) | 3.220 | 48,4 % |

El exterior tiene **8,5 veces más colores** que el laboratorio y casi la mitad de
su imagen fuera de la banda verde. El laboratorio es, a efectos prácticos,
monocromo.

---

## 2. Diagnóstico

> Las referencias de línea corresponden a `src/pallet3d.cpp` con 803 líneas
> (2026-09-20 11:37, binario `sha256 9c34a4ed…a408`). Codex sigue editando el
> archivo en paralelo, así que el anclaje fiable es la expresión citada, no el
> número. El diagnóstico se reverificó contra ese binario recién compilado:
> mismo `state=3`, mismos 5.154 vértices y misma captura idéntica byte a byte
> (`build/qa/oaklab/rebuild/`).

### 2.1 Causa raíz: con `room_yaw = 0` la proyección degenera

`pallet3d.cpp:460` elige el giro de cámara:

```cpp
float view_yaw = current.interior ? room_yaw : yaw;
```

Los valores iniciales están en `pallet3d.cpp:45-46`:

```cpp
float yaw = -.32f, zoom = 1.f;   // exterior
float room_yaw = 0, room_zoom = 1;   // interior
```

y `update_meshes` (`pallet3d.cpp:307`) restablece `room_yaw = 0` en cada cambio
de componente, es decir **cada vez que se entra en una habitación**.

`firstperson::orthographic` (`firstperson.h:55-61`) es, en columna mayor:

```
clip.x = sx·( c·X − s·Z ) + …
clip.y = sy·( up·Y − tilt·( s·X + c·Z ) ) + …      tilt = 0,78   up = 0,6257795
```

Con `yaw = 0` (`c = 1`, `s = 0`) queda:

```
clip.x = sx · (X − focus_x)
clip.y = sy · ( 0,6258·Y − 0,78·(Z − focus_z) )
```

`clip.x` **no depende de Z**. Consecuencia directa: las dos caras de normal X de
cada `box()` (`pallet3d.cpp:133-135`, quads de X constante) tienen sus cuatro
vértices con el mismo `clip.x` y **se proyectan con anchura cero**. Nunca se
dibujan. De las cinco caras que emite `box()` sólo sobreviven la superior y la
frontal; la trasera queda ocluida.

Peor aún, la cara superior de un mueble a altura *h* aparece en
`clip.y = sy·(0,6258·h − 0,78·Z)`: el **mismo factor 0,78 en profundidad que el
suelo**, sólo desplazado hacia arriba. La imagen completa del interior es por
tanto un aplastamiento vertical uniforme al 78 % del tilemap 2D, con los muebles
subidos unos píxeles. Como el arte original de Game Boy ya está dibujado en
alzado (una estantería «se ve de frente» en el mapa 2D), tumbarlo sobre la cara
superior reproduce literalmente la imagen 2D.

El exterior no sufre esto porque arranca en `yaw = -0.32`: ahí
`clip.x = sx·(c·X − s·Z)` con `s ≈ -0,3146`, las caras de X constante ganan
`0,3146·sx` de ancho por unidad de profundidad, y los laterales de casas, árboles
y rocas se ven.

### 2.2 Prueba A/B en el mismo mapa, misma malla y misma paleta

`ref-mart-2f.png` y `ref-mart-2f-camera.png` son Centro Comercial Azulona 2F
(mapa 123, 20 × 8, `large = true`) renderizado por el mismo binario con la misma
clasificación y la misma rampa de color. La única diferencia es `room_yaw`:

- `room_yaw = 0`: lectura plana, laterales de estanterías invisibles, se
  percibe como un tilemap.
- `room_yaw ≈ 0,4` tras diez `E`: volumen inequívoco, laterales, sombreado de
  caras y profundidad.

Esa pareja de capturas aísla la variable. Y el mapa 40 nunca puede llegar al
segundo caso porque `large` es falso (§1.4).

### 2.3 Factores agravantes (reales, pero secundarios)

Los cuatro contribuyen a la ilegibilidad; ninguno explica por sí solo el aspecto
2D, y todos quedarían parcialmente compensados por el giro de cámara.

1. **Una sola rampa monocroma por sala.** `create_atlas` (`pallet3d.cpp:185-194`)
   define cuatro rampas de interior para los **21 tilesets** de interior:
   verde para `{5,7,20,22}` (41 mapas, incluido el laboratorio), azul para
   `{2,6}` (21 mapas), gris para `{11,17}` (21 mapas) y crema por defecto (96
   mapas). Además, para interiores las paletas 0 y 1 del atlas se rellenan con
   la **misma** `room`, y `floor_palette` (`pallet3d.cpp:86`) devuelve siempre 0
   salvo el agua de cueva: dentro de una habitación sólo existe **una** rampa de
   cuatro tonos. Los exteriores usan tres rampas distintas (`ground`, `facade`,
   `water`).

   Matiz importante para no atribuir mal el efecto: **parte del déficit de color
   es consecuencia de la cámara, no de la paleta.** En la pareja A/B del mismo
   mapa (§1.6), pasar de `yaw=0` a `yaw≈0,4` sube los colores únicos de 206 a
   349 (+69 %) sin tocar ni una línea del atlas, porque los tintes
   `shade(c,.75f)`, `.85f` y `.67f` que `box()` aplica a tres de sus cinco caras
   (`pallet3d.cpp:133-135`) sólo llegan a pintarse cuando esas caras tienen
   anchura. El déficit residual, una vez girada la cámara, es el que corresponde
   de verdad a la paleta.

2. **Laterales de mueble en color plano.** `interior_map` (`pallet3d.cpp:207-211`)
   calcula `side` como el **promedio de los 64 píxeles** del tile superior
   izquierdo de la celda y llama a `box(...)` **sin UV de detalle**, por lo que
   `detailed_quad` toma la salida temprana `if(detail.w==0)` y emite un quad sin
   textura. Los laterales y el frontal de cada mueble son un color sólido. Se ve
   con claridad en `ref-oaks-lab-fp.png`, donde las mesas tienen tapa
   texturizada y costados lisos.

3. **El arte se duplica en tapa y frontal, y sólo en paredes.** Sólo
   `Kind::Wall` y `Kind::Counter` reciben cara frontal texturizada
   (`pallet3d.cpp:217-220`), y esa cara reutiliza **el mismo tile `t`** que ya se
   pintó en la tapa, apilado por filas: el gráfico aparece dos veces. Los 30
   muebles del laboratorio (`Kind::Furniture`, entre ellos las estanterías
   `top==0x0d`) **no tienen cara frontal texturizada en absoluto**.

4. **No hay sombras de contacto del mobiliario.** `shadow()` se invoca en
   `pallet3d.cpp:152` (casas), `:245` y `:265` (árboles) y `:470` (actores).
   `interior_map` no la llama nunca. Nada de lo construido dentro de una
   habitación proyecta sombra, y sin sombra no hay señal de altura cuando la
   silueta lateral es invisible.

**Corrección a una de las hipótesis del encargo:** *sí* hay sombras de sprites en
interiores. `pallet3d.cpp:470` ejecuta `shadow(vertices,a.x,a.z,.32f,.20f)` para
todos los actores sin distinguir interior de exterior, y las elipses se ven bajo
los NPC y bajo el jugador en `user-oaklab.png`. Lo que falta es sombra de
**escenografía**.

### 2.4 Qué NO está roto

Para evitar trabajo innecesario, se descartan explícitamente:

- `pallet::view()`, `ensure_scene`, `valid_live_map` y `load_catalog` en el mapa
  40 (§1.2). No hay que tocar `pallet_state.h`.
- El flag de fuente por scripts de Oak: `font=0` en todo el recorrido de la
  partida real.
- Los bloques vivos de WRAM: el estado sería `Transition`, no `Overworld`.
- La cobertura del clasificador: 0 celdas sin regla en el mapa 40 (§1.5).
- El volumen de geometría: 38 celdas con volumen sobre 120, proporción
  equivalente a la del exterior (§1.5).
- La primera persona: `ref-oaks-lab-fp.png` demuestra que la misma malla, vista
  con perspectiva, se lee como 3D sin ambigüedad.

### 2.5 Comparación con el exterior (pregunta 4)

`user-pallet.png` y `user-oaklab.png` salen de la **misma partida** con minutos
de diferencia. Diferencias que explican la brecha de legibilidad:

| Señal 3D | Exterior (Paleta) | Interior (laboratorio) |
| --- | --- | --- |
| Giro de cámara | `yaw = -0.32`, ajustable `±0.75` | `room_yaw = 0`, bloqueado |
| Caras laterales visibles | sí, todas | ninguna (ancho cero) |
| Rampas de color simultáneas | 3 (`ground`, `facade`, `water`) | 1 |
| Colores únicos en pantalla | 2.751 | 323 |
| Volúmenes con silueta (tejados a dos aguas, copas escalonadas) | sí | no, prismas rectos |
| Sombras proyectadas | casas, árboles, actores | sólo actores |
| Zoom | `0.7 … 2.8` | bloqueado |

El exterior acumula seis señales de profundidad; el interior conserva una sola
(las elipses de sombra de los sprites). Ésa es toda la diferencia.

---

## 3. Plan de solución

Cuatro fases independientes salvo donde se indica. La Fase 1 resuelve la causa
raíz; las demás suben el listón de legibilidad. Todo el trabajo cae en
`src/pallet3d.cpp` y `src/interior_scene.h`, con pruebas en
`tests/interior_integration.h` y `tests/interiors_qa.sh`.

### Fase 1 — Giro de cámara en interiores (causa raíz)

Trabajo concreto:

1. `src/pallet3d.cpp:46` — dar a `room_yaw` un valor inicial distinto de cero.
   Punto de partida propuesto: `-0.32f`, el mismo del exterior, para que entrar
   y salir de un edificio no cambie la orientación mental del jugador. Aviso al
   implementador: ese valor es una **extrapolación** desde el exterior, no una
   medida. El único giro de interior comprobado empíricamente que se lee como 3D
   es `≈0,4` (`ref-mart-2f-camera.png`, tope del `clamp` actual). Decidir el
   valor final comparando capturas del mapa 40 a `-0.32`, `0.32` y `0.4`.
2. `src/pallet3d.cpp:307` — en `update_meshes`, al cambiar de componente
   restablecer `room_yaw` al valor por defecto, no a `0`.
3. `src/pallet3d.cpp:695` — eliminar la condición `large`. La razón original
   (que una sala pequeña no cabría girada) ya está cubierta: el cálculo de
   `unit` en `:526-527` incluye `|cs|` y `|sn|` y reencuadra la sala completa
   para cualquier giro. Mantener el recorte `clamp(room_yaw, -.4f, .4f)`.
4. `src/pallet3d.cpp:696` — habilitar también la rueda (`room_zoom`) en salas
   pequeñas, o dejarla deshabilitada de forma consciente y quitar «Rueda Zoom»
   del HUD (`:596`) cuando no aplique.
5. `src/pallet3d.cpp:700` — `R` debe restablecer al giro por defecto, no a `0`.

Criterios de aceptación:

- Con el estado `build/qa/oaklab/logs/user-inside-lab.state`, el modo `camera`
  del smoke produce una captura **distinta** de la del modo por defecto
  (hoy son idénticas: md5 `a3bb4ba3…f707` en ambas).
- En una captura del mapa 40 se distinguen caras laterales de mueble: para una
  celda de mueble de 1×1, la anchura en pantalla de la cara de X constante pasa
  de 0 px a ≥ 8 px.
- El contador `px_fuera_banda_verde` de `user-oaklab.ppm` no baja; los colores
  únicos suben (el sombreado `shade(c,.75f)/.85f/.67f` de `box()` empieza a
  aportar tonos).
- `tests/interiors_qa.sh` sigue en verde, con las 179 capturas de catálogo
  regeneradas y revisadas.
- Los 107 interiores hoy bloqueados responden a `Q`/`E`.

Riesgo controlado: revisar las salas más estrechas del catálogo (columna
`width`/`height` de `build/qa/logs/interiors.csv`) para confirmar que ninguna se
sale del encuadre a `room_yaw = ±0.4`.

### Fase 2 — Paleta por familia de tileset

Trabajo concreto, en `create_atlas` (`src/pallet3d.cpp:182-198`):

1. Sustituir el bloque de cuatro `if` por una tabla de rampas indexada por
   tileset, con una entrada por cada una de las 21 familias de interior. Las
   agrupaciones actuales mezclan laboratorio, dojo, gimnasio e instalación en
   una sola rampa verde (41 mapas).
2. Usar las tres ranuras de paleta del atlas, que hoy se desperdician: en
   interiores las ranuras 0 y 1 reciben la **misma** `room`. Asignar ranura 0 a
   suelo, ranura 1 a paredes y mobiliario, ranura 2 a los materiales especiales
   (agua, cristal, metal).
3. `floor_palette` (`src/pallet3d.cpp:85-95`) debe devolver la ranura correcta
   para interiores en vez de `0` fijo, siguiendo la misma lógica que ya aplica a
   `scene.tileset==0/14/23` en exteriores.
4. `interior_map` (`:206`) debe pedir `tile_uv(scene, tile, ranura)` coherente
   con la clase de celda devuelta por `interior::classify`.

Criterios de aceptación:

- El criterio de color se mide **contra una captura posterior a la Fase 1**, no
  contra la actual: girar la cámara ya sube el recuento por sí solo (§2.3.1), y
  atribuir esa ganancia a la paleta daría por buena la Fase 2 sin trabajo. Pasos:
  (a) tras la Fase 1, regenerar `user-oaklab.ppm` y anotar su nueva línea base
  con `build/qa/oaklab/stats.py`; (b) exigir que la Fase 2 **duplique** ese
  recuento de colores únicos y lleve los píxeles fuera de la banda verde/teal
  por encima del 15 % (hoy 0,5 %). Referencia superior: el exterior, 2.751
  colores y 41 %.
- Las 179 capturas de catálogo muestran al menos cuatro familias cromáticas
  distinguibles a simple vista; el informe de A1
  (`build/qa/interiors/logs/interior-families-*.png`) se regenera.
- Ninguna regresión en cuevas: `interior::cave` sigue usando su rampa gris y el
  agua su ranura propia.

### Fase 3 — Laterales texturizados y sombras de mobiliario

Trabajo concreto, en `interior_map` (`src/pallet3d.cpp:201-223`):

1. Pasar una `UV` de detalle real a `box(...)` en `:212`, en lugar del `Solid`
   implícito, para que `detailed_quad` recorra la rama texturizada. El exterior
   ya lo hace con `foliage` y `stone` (`:267`, `:271`). Conservar `side` como
   tinte multiplicativo, no como sustituto de la textura.
2. Extender la cara frontal texturizada de `:217-220` a `Kind::Furniture`, y
   dejar de reutilizar el mismo tile en tapa y frontal: la fila superior del
   bloque 2×2 pertenece al alzado (cara frontal) y la inferior a la planta (cara
   superior). Eso elimina la duplicación descrita en §2.3.3.
3. Llamar a `shadow(scenery, …)` por cada celda con `cell.height > 0`,
   dimensionada con la altura de la celda, igual que `make_house:152`.
4. Revisar las reglas de `interior_scene.h` del `case 5: case 7:` con el
   laboratorio delante: hoy `top==0x0d` da estantería de 1,45 y `top==0x3b` mesa
   de 0,65 sin distinguir mesa de trabajo de encimera.

Criterios de aceptación:

- En `ref-oaks-lab-fp.png` regenerada, los costados de las mesas dejan de ser
  color plano.
- Cada celda de mueble o pared del mapa 40 proyecta una sombra de contacto
  visible en la captura ortográfica.
- Conteo de vértices del mapa 40 dentro de un presupuesto acordado (hoy 5.154;
  las sombras añaden 60 vértices por celda, 38 celdas → +2.280; evaluar si se
  usa un quad único en lugar del abanico de 20 triángulos de `shadow()`).
- La medición de presentación del catálogo de interiores no empeora más de un
  15 % respecto a los tiempos registrados en `build/qa/interiors/logs/`.

### Fase 4 — Evidencia y documentación

1. Sustituir `docs/screenshots/oaks-lab.png` (referenciada en `README.md:44`)
   por una captura posterior a las fases 1-3. La actual documenta exactamente el
   defecto del que se queja el usuario.
2. Añadir a `tests/interior_integration.h` una aserción de cámara: tras entrar
   en un interior, `room_yaw` debe ser distinto de cero, y una secuencia de
   `Q`/`E` debe cambiar la superficie renderizada. Hoy `interior_journey` valida
   mapa, mallas residentes, componente y preferencia de cámara, pero no que la
   cámara esté en una orientación útil.
3. Actualizar `PALLET3D.md` («Las salas pequeñas se encuadran completas; las
   grandes permiten giro limitado y zoom»), que describe el bloqueo como una
   decisión de diseño sin decir que afecta al 59,8 % de los interiores.
4. Registrar el resultado en `PLAN_INTERIORES_COMBATES.md`, sección «Registro de
   ejecución».

Criterio de aceptación: un lector del README que no conozca el proyecto
identifica el interior como 3D sin necesidad de leer el texto.

### Orden recomendado

Fase 1 primero y sola: es un cambio de pocas líneas, resuelve la queja y permite
volver a juzgar las fases 2 y 3 con la cámara ya correcta. Puede ocurrir que con
el giro puesto, la Fase 3 baje de prioridad. La Fase 2 es independiente y puede
ir en paralelo. La Fase 4 va al final.

---

## 4. Riesgos

| Riesgo | Mitigación |
| --- | --- |
| Girar salas pequeñas puede dejar la pared norte tapando al jugador, o sacar esquinas del encuadre | El cálculo de `unit` en `:526-527` ya contempla el giro; validar las salas de menor superficie del CSV antes de fijar el valor por defecto. La silueta dorada de rayos X (`draw_world_frame:575-579`) ya cubre la oclusión |
| `room_yaw` inicial distinto de cero cambia las 179 capturas de catálogo y las de recorrido | Regenerar la batería completa con `tests/interiors_qa.sh` y revisar el contacto por familias, como ya se hizo en A2 |
| Texturizar laterales multiplica los vértices y el coste de presentación | `detailed_quad` no añade geometría (reutiliza el canal alfa como índice de celda del atlas); el coste real está en las sombras. Medir con `CATALOG_BENCH_FRAMES` antes y después |
| Tocar `create_atlas` afecta también a exteriores, que comparten la función | Las ramas de exterior e interior ya están separadas por `current.interior`; mantener la separación y cubrir con `pallet_state_test` |
| Otro agente (Codex) está editando `src/` y `tests/` en paralelo | Este diagnóstico se hizo sobre el binario `sha256 eb993e75…9375`, congelado en `build/qa/oaklab/`. Reverificar la vigencia del análisis antes de implementar |
| Las coordenadas del recorrido dependen de la partida concreta del usuario | El estado intermedio `build/qa/oaklab/logs/user-inside-lab.state` reproduce el caso sin repetir la caminata |
| Cambiar la rampa de color puede romper la lectura de cuevas y del muelle | `interior::cave` y `floor_palette` ya tratan esos casos aparte; incluir cueva y ascensor en la revisión visual (`interior-transitions`) |

---

## 5. Índice de evidencias

Todo bajo `build/qa/oaklab/`:

```
logs/inputs.sha256              hashes de ejecutable, ROM y estados usados
logs/probe-user-state10.log     partida real: map=0 xy=8,15 view=3 font=0
logs/probe-user-state1.log      partida real anterior: map=0 xy=9,8 view=3
logs/probe-oaks-lab.log         fixture A1: map=40 xy=5,11 view=3
logs/walk-step1..11.log         reconstrucción de la ruta hasta la puerta
logs/pallet-at-lab-door.state   partida del usuario en Paleta (12,12)
logs/walk-enter-lab.log         traza del cruce: state 3 -> 0 -> 1 -> 3
logs/user-inside-lab.state      partida del usuario dentro del mapa 40
logs/user-oaklab.ppm            captura dentro del laboratorio
logs/user-oaklab-plain.ppm      captura sin tocar la cámara
logs/user-oaklab-camera.ppm     captura tras E x10 + rueda (idéntica)
logs/user-pallet.ppm            captura del exterior, misma partida
user-oaklab.png                 PNG de la anterior
user-pallet.png                 PNG del exterior
ref-oaks-lab.png                fixture A1 convertida
ref-oaks-lab-fp.png             primera persona en el laboratorio
ref-lab-return.png              salida del laboratorio
ref-mart-2f.png                 interior grande con room_yaw = 0
ref-mart-2f-camera.png          el mismo con room_yaw ~ 0,4
ref-exterior-final-camera.png   exterior de referencia
rebuild/                        reverificación contra el binario de las 11:38
ppm2png.py  stats.py            utilidades de conversión y medida, sin dependencias
```

Datos de ROM reutilizados: `build/qa/logs/interiors.csv` (auditoría de los 179
interiores, generada previamente por `build/interior_audit`).
