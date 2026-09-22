# Plan: combate integrado sin pantalla original

Fecha: 2026-09-22. Estado: propuesto; ninguna fase iniciada. Complementa
`PLAN_INTERIORES_COMBATES.md` (bloque B) y `PLAN_PANTALLAS_COMPLETAS.md`,
que dejaron fuera el tramo final del combate.

## Análisis del estado actual

Auditoría hecha el 2026-09-22 sobre `main` en `808fb91` (código idéntico a
`7955aaa`, base v0.4.1), jugando con `pallet_render_smoke ... play` sobre
fixtures reales: Pidgey salvaje, rival de la Ruta 22 con Spearow y Eevee,
captura de Rattata y cambio de Pokémon, en estilo integrado y clásico.
Capturas privadas en el scratchpad de la sesión de auditoría
(`battle-audit/cap/`), reproducibles con su `seq.sh`; deben regenerarse
bajo `build/qa/` al empezar la primera fase.

| Momento del combate | Estilo integrado hoy | Causa |
| --- | --- | --- |
| Transición de entrada | Integrada (mundo 3D con zoom y desenfoque) | `pallet3d.cpp`, C2 de `PLAN_INTERIORES_COMBATES.md` |
| Intro del entrenador y primer envío | Integrada | `battle3d.h`, `battle::trainer_intro` |
| Escenario, retratos, PS y experiencia | Integrados | `battle3d.h`, `panel` |
| Mensajes, LUCHA/PKMN/OBJ/HUIR, movimientos | Integrados | `battle_menu::classify` (Message, Fight, Moves) |
| Texto durante un ataque con efecto 3D | Mixto: efecto 3D, texto con el recorte original | La rama integrada exige `!animation` y cae en `lcd_overlay::draw(Bottom)` |
| Lanzamiento y sacudidas de Poké Ball | Mixto: bola 3D, "ASH used POKé BALL!" original | Misma condición `!animation` |
| "Go! RATTATA!" y "Come back!" | LCD original completo | Salida/retirada cuenta como animación sin efecto, o falta el retrato del jugador |
| Lista de equipo tras "Will ASH change POKéMON?" o con "is already out!" | LCD original enmarcado | Contexto no reconocido por `pokemon_menu::context` |
| KO del rival, experiencia, subida de nivel, aprendizaje de movimiento | LCD original enmarcado | Sin retrato rival, `!battle::rectangle(Enemy)` activa `full_overlay` |
| Siguiente Pokémon del rival y sí/no de cambio | LCD original enmarcado | Misma causa |
| Derrota del entrenador, diálogo y dinero | LCD original enmarcado | Misma causa; `trainer_intro` solo cubre la entrada |
| Captura lograda, ficha de Pokédex y pregunta de mote | LCD original enmarcado | Misma causa; `view()` evalúa el combate antes que el Pokédex 3D |
| Pantalla de mote | Integrada | `full_menu::draw_naming` |
| Evolución tras el combate | Probablemente LCD enmarcado (no reproducido) | Disposición desconocida, `Kind::Full` |
| Fundido de salida del combate | LCD original enmarcado aclarándose | `full_overlay` todavía activo |
| Movimientos con presentación `Effect::Original` | LCD completo sin marco | Tabla de `battle::move` en `battle_state.h` |
| Carga en frío a mitad de animación | LCD completo durante los primeros frames | Retratos aún sin caché |

Además, en la lista de movimientos integrada las filas se solapan con el
cuadro TIPO/PP (visible con GROWL y TAIL WHIP).

Hechos que condicionan el diseño:

- En `battle3d.h` el disparador del LCD completo es
  `full_overlay = !opening && !intro && !effect && !moves && (animation ||
  !rectangle(Enemy) || !rectangle(Player))`. Usar "falta un retrato" como
  señal de pantalla desconocida es la causa de casi todo el tramo final.
  El retrato desaparece de forma legítima tras un KO, una retirada o una
  captura, y el resto del HUD sigue siendo verificable.
- `battle_menu::classify` solo conoce tres disposiciones: `Message`,
  `Fight` y `Moves`. El sí/no, la caja de estadísticas de subida de nivel,
  la lista de movimientos a olvidar y las bolas del equipo rival no tienen
  clasificador y caen al fallback.
- Toda la lectura del estado es de solo lectura (`battle_state.h`) y se
  apoya en `live_return` para saber qué rutina del motor está activa. El
  mismo mecanismo sirve para identificar las rutinas del tramo final sin
  escribir memoria.
- El estilo Clásico dibuja siempre las seis filas originales por diseño.
  Este plan no lo cambia: todo lo nuevo va detrás de `styled`.
- Quedan fuera por decisión previa y documentada: combate de Pikachu con
  Oak, tutorial del viejo, Safari y cable link (`battle::normal`), y los
  nombres con glifos que la fuente ROM no soporta.
- La auditoría concluye que la cámara de combate es propia
  (`eye.reset()`), así que ortográfica y primera persona comparten la
  misma presentación salvo en la transición de entrada. Se verificó por
  código; la primera fase lo confirma con capturas en ambas cámaras.

## Objetivo y alcance

Que en estilo integrado ningún momento de un combate normal muestre la
pantalla original de Game Boy: desde la transición de entrada hasta la
vuelta al mundo, incluidos KO, experiencia, subida de nivel, aprendizaje
de movimiento, cambios de Pokémon, derrota del entrenador, dinero,
captura, Pokédex y evolución. El motor sigue siendo la única autoridad;
nada de este plan escribe memoria, altera tiempos del motor ni cambia la
lógica del combate.

Quedan fuera los combates especiales ya excluidos, el estilo Clásico, los
modelos 3D de Pokémon y cualquier cambio en el runtime o el C generado.

## Decisiones de arquitectura

1. Sin ajuste nuevo. Todo el trabajo se activa con el estilo de menú
   integrado; con Clásico la imagen es la de `7955aaa` byte a byte.
2. Un estado de combate explícito en `battle_state.h`
   (`battle::Phase`): entrada, intro de entrenador, turno, KO, experiencia,
   subida de nivel, aprendizaje, cambio, outro de entrenador, captura,
   evolución y salida. Cada fase se identifica por `live_return` contra la
   rutina correspondiente de `engine/battle/core.asm`,
   `engine/battle/experience.asm`, `engine/pokemon/learn_move.asm`,
   `engine/items/item_effects.asm` o `engine/movie/evolution.asm`, con las
   direcciones verificadas contra el C generado y la ROM canónica.
3. El LCD completo pasa a ser el fallback de una fase no reconocida, no de
   un retrato ausente. Un retrato ausente con fase conocida se traduce en
   ocultar su billboard y conservar escenario, paneles y texto integrados.
4. Las disposiciones nuevas se añaden a `battle_menu::Kind` con la misma
   técnica de rectángulos de `menu_layout`: `YesNo`, `LevelStats`,
   `ForgetMove` y `PartyBalls`. Una disposición no reconocida mantiene el
   fallback actual completo, sin mezcla parcial.
5. Los mensajes durante animaciones se dibujan integrados cuando la
   animación tiene efecto 3D o es la de captura. Con `Effect::Original` se
   conserva el LCD completo hasta la fase D.
6. Las pantallas completas que aparecen dentro del combate (ficha de
   Pokédex tras la captura, evolución) se encaminan a `dex3d` o a
   `full_menu` aunque `wIsInBattle` siga activo, con el mismo patrón de
   detección y fallback atómico de `PLAN_PANTALLAS_COMPLETAS.md`.
7. Se preservan el guard `menu_open` y las rutas de v0.4.1: nada nuevo en
   el foreground de ImGui con Esc abierto; resumen durante PlayCry, carga
   en frío y mochila de combate intactos.
8. Cualquier discrepancia entre la fase detectada y la VRAM visible
   (rectángulos, nombres, paleta) invalida la fase y cae al fallback
   actual. Nunca se muestra un HUD integrado con datos que el LCD no
   muestra en ese frame.

## Bloque A: correcciones

### Fase A1: texto durante efectos, cambio forzado y lista de movimientos

Trabajo:

- Permitir la rama integrada de mensajes durante animaciones con efecto 3D
  y durante la captura (`effect == true`), manteniendo `!animation` para
  `Effect::Original`.
- Ampliar `pokemon_menu::context` para la lista de equipo del cambio
  forzado, del sí/no "Will ASH change POKéMON?" y del mensaje "is already
  out!".
- Corregir el espaciado de la lista de movimientos integrada para que no
  se solape con el cuadro TIPO/PP con cuatro movimientos.
- Regenerar la auditoría bajo `build/qa/battle-audit-baseline/` con los
  cuatro fixtures, ambos estilos y ambas cámaras, como referencia de
  partida del plan.

Criterios de aceptación:

- [ ] En los recorridos salvaje, de entrenador y de captura, ningún mensaje acompañado de efecto 3D o de Poké Ball se dibuja con el recorte original.
- [ ] La lista de equipo del cambio forzado y la de "is already out!" se muestran integradas y responden igual que las originales.
- [ ] Clásico idéntico byte a byte a `7955aaa` en ambas cámaras; `ui_battles_qa.sh` y `battles_qa.sh` en PASS en ambos estilos.

## Bloque B: HUD persistente

### Fase B1: fase de combate y retratos ausentes

Trabajo:

- Introducir `battle::Phase` con sus detectores y tests sintéticos por
  fase, y un test con ROM que recorre los fixtures y registra la secuencia
  de fases.
- Reescribir la condición de `full_overlay` sobre la fase: KO,
  experiencia, subida de nivel y cambio conservan escenario y paneles, con
  el billboard ausente desvanecido.
- Paneles de PS y experiencia animados durante la ganancia de experiencia,
  leyendo los valores del motor en cada frame.

Criterios de aceptación:

- [ ] Tras el KO de un salvaje y de cada Pokémon del rival, "fainted", la experiencia y la subida de nivel se ven integradas en ambas cámaras.
- [ ] Una fase no reconocida o incoherente con la VRAM cae al LCD enmarcado completo, comprobado con controles negativos.
- [ ] `check_battle_timing.py` sin cambios en los tiempos del motor y memoria intacta por frame.

### Fase B2: disposiciones nuevas

Trabajo:

- Añadir `YesNo`, `LevelStats`, `ForgetMove` y `PartyBalls` a
  `battle_menu::classify` con tests de clasificación sobre tilemaps
  reales y controles negativos.
- Presentarlas con `menu_text::regions` y los tokens de `ui_theme`,
  conservando textos, números y cursor originales.
- Recorrido de aprendizaje de movimiento con cuatro movimientos: aceptar,
  rechazar y olvidar cada una de las cuatro posiciones.

Criterios de aceptación:

- [ ] Sí/no, caja de estadísticas, lista de olvidar y bolas del equipo rival integradas en los recorridos de entrenador y de subida de nivel.
- [ ] El aprendizaje de movimiento produce el mismo estado final que el original en los cinco caminos.
- [ ] Baterías de combate y de menús en PASS en ambos estilos y cámaras.

## Bloque C: secuencias

### Fase C1: salida y retirada de Pokémon

Trabajo:

- Presentar "Go!" y "Come back!" en 3D: billboard que aparece o se retira
  sobre su plataforma con la Poké Ball, sincronizado con el contador de la
  animación original.
- Incluir el envío del siguiente Pokémon del rival.

Criterios de aceptación:

- [ ] Cambios voluntarios, forzados y del rival sin LCD original en ambas cámaras.
- [ ] Tiempos del motor sin cambios y memoria intacta por frame.

### Fase C2: outro del entrenador y salida del combate

Trabajo:

- Fase de outro: retrato del entrenador como billboard, diálogo y
  recompensa de dinero en el panel integrado; ampliar
  `battle::trainer_intro` con su simétrico.
- Fundido de salida desde el escenario 3D hacia el mundo, sin pasar por
  el LCD enmarcado.
- Derrota del jugador y huida con la misma salida.

Criterios de aceptación:

- [ ] Victoria contra el rival de la Ruta 22 y contra un entrenador de ruta, derrota del jugador y huida, sin LCD original.
- [ ] El dinero mostrado coincide con el que el motor escribe.

### Fase C3: captura, Pokédex y evolución

Trabajo:

- Tras "All right! … caught", encaminar la ficha de Pokédex a `dex3d`
  aunque `wIsInBattle` siga activo; la pregunta de mote como `YesNo`.
- Evolución tras el combate como pantalla completa integrada: billboard
  que alterna especie con el ritmo original y cancelación con B.
- Fixture privado de evolución por nivel generado desde una partida real.

Criterios de aceptación:

- [ ] Captura de un Pokémon nuevo y de uno ya registrado, con y sin mote, sin LCD original.
- [ ] Evolución completa y cancelada sin LCD original y con el mismo estado final.

## Bloque D: animaciones originales

### Fase D1: inventario y familias restantes

Trabajo:

- Inventariar los movimientos con `Effect::Original` (la auditoría cuenta
  50 de 165; confirmar) y agruparlos por protocolo: varios turnos,
  transformación, atrapar, KO directo y especiales.
- Implementar en 3D las familias que no necesiten modelos, y documentar
  las que se mantienen originales con su razón.
- Carga en frío a mitad de animación: decodificar retratos desde la ROM
  para no mostrar el LCD en los primeros frames.

Criterios de aceptación:

- [ ] Cada movimiento tiene presentación integrada o una excepción documentada en `PALLET3D.md`.
- [ ] Recorrido con un movimiento de cada familia en ambas cámaras sin cambios en el estado del motor.

## Limitaciones asumidas

- Los Pokémon siguen siendo billboards con los sprites de la ROM.
- El estilo Clásico conserva el LCD original por diseño.
- Los combates especiales ya excluidos no cambian.
- Algunas animaciones de la fase D pueden quedar originales si su
  protocolo no se puede presentar sin escribir memoria.

## Validación y entrega

- Una rama y un PR por fase, en el orden A1, B1, B2, C1, C2, C3, D1.
  Ninguna fase posterior empieza antes de fusionar la anterior con CI
  Linux y Windows/MSVC verde. El plan se incorpora primero mediante PR.
- CTest completo, `ctest -LE rom` sin ROM, `clang-format`, y las baterías
  `ui_battles_qa.sh`, `battles_qa.sh` y de menús antes y después de cada
  fase. Clásico idéntico a `7955aaa` en ambas cámaras.
- Recorrido de combate integrado ampliado por fase: salvaje, entrenador,
  captura, cambio, aprendizaje, derrota y evolución, en ambas cámaras, con
  un detector automático de frames que muestren el LCD original completo
  o enmarcado y su recuento por fase.
- Oráculos independientes y controles negativos para cada detector de
  fase y disposición. Memoria intacta y cero errores GL por frame.
- Fixtures privadas bajo `build/qa/`; ninguna ROM, partida, savestate ni
  captura del juego en git. Tests con ROM con etiqueta `rom` y
  `SKIP_RETURN_CODE 77`.
- No publicar ni enviar un tag `v*` sin confirmación explícita del
  usuario.

## Orden recomendado

1. A1: arreglos pequeños visibles en todos los turnos.
2. B1 y B2: quitan el LCD del tramo final, que es lo que más se ve.
3. C1, C2 y C3: secuencias completas.
4. D1 al final, porque es la de mayor coste y menor frecuencia.

Relación con `PLAN_PASE_ARTISTICO.md`: este plan toca sobre todo
`battle3d.h`, `battle_state.h`, `battle_menu_layout.h` y
`pokemon_menu_state.h`. El pase artístico comparte el programa principal
de `pallet3d.cpp`, así que se ejecuta después, salvo A1, que puede
adelantarse si no coincide con una fase del pase que toque el shader.

## Referencias técnicas

- `src/battle3d.h`: `full_overlay`, `framed_overlay`, paneles y rama de
  mensajes integrados.
- `src/battle_state.h`: `normal`, `ready`, `rectangle`, `trainer_intro`,
  `animation_running`, `capture_running`, `live_return`, `move`.
- `src/battle_menu_layout.h`: `Kind` y `classify`.
- `src/pokemon_menu_state.h`: `context` y `party_return`.
- `src/pallet_state.h`: orden de evaluación de `view()`.
- `tests/ui_battles_qa.sh`, `tests/battles_qa.sh`,
  `tests/check_battle_timing.py`.
- [pret/pokeyellow](https://github.com/pret/pokeyellow): `engine/battle/core.asm`,
  `engine/battle/experience.asm`, `engine/pokemon/learn_move.asm`,
  `engine/items/item_effects.asm`, `engine/movie/evolution.asm`.

## Registro de ejecución

### Preparación del plan — 2026-09-22

- Auditoría de combate sobre `808fb91` con el binario
  `build/pallet_render_smoke` posterior a `7955aaa`, sin compilar ni
  modificar código. Tabla de estado actual derivada de esa auditoría.
- Sin criterios marcados.
