# Plan: Pokédex y PC en 3D

Fecha: 2026-09-20. Estado: propuesta; ninguna fase iniciada.

## Objetivo y alcance

Dar presentación 3D a las dos pantallas de datos más usadas del juego: el
Pokédex, con su lista, su ficha, su grito y su zona, y el PC, con el
almacenamiento de Pokémon de Bill, el almacén de objetos del jugador, la
valoración de Oak y el Salón de la Fama. Hoy todas pasan a la imagen original
a pantalla completa.

Se mantienen los principios de los planes anteriores. El motor recompilado es
la única autoridad sobre estado, listas, cursores, texto, sonido y guardado.
Los menús y el texto siguen siendo la imagen que pinta el juego, compuesta
sobre el 3D. El módulo nuevo solo lee ROM, WRAM, VRAM, RAM de cartucho y el
framebuffer, y nunca escribe en ellos. Toda pantalla no reconocida vuelve a la
conducta actual.

Este plan depende de la fase A3 de `PLAN_MENUS_TITULO_TRANSICIONES.md`, que
establece el compositor de regiones y el fondo atenuado para pantallas
completas. Reutiliza el decodificador de retratos de `src/battle_state.h` y el
catálogo de mapas de `src/kanto_rom.h`. No toca combates ni primera persona.

Quedan fuera: modelos 3D de Pokémon, cambiar el orden o el contenido de las
listas, redibujar la tipografía, el intercambio por cable y cualquier edición
del runtime descargado o del C generado.

## Punto de partida verificado

- WRAM disponible en `pokeyellow_internal.h`: `wPokedexOwned` (D2F6) y
  `wPokedexSeen` (D309), mapas de bits de 19 bytes; `wCurrentMenuItem` (CC26),
  `wListScrollOffset` (CC36), `wMaxMenuItem` (CC28), `wListMenuID` (CF93) y
  `wListPointer` (CF8A) para todas las listas; `wWhichPokemon` (CF91) y
  `wCurPartySpecies` (CF90) para el Pokémon seleccionado; `wPartyMons` (D16A);
  `wBoxMons` (DA95) y `wCurrentBoxNum` (DA9F) para la caja activa; `wBoxItems`
  (D53A) y `wNumBoxItems` (D539) para el almacén de objetos; `wNumHoFTeams`
  (DAA1) y `wHallOfFameCurScript` (D64A).
- La especie mostrada en la ficha del Pokédex está en `wd11e` según pret. No
  está exportada en el header interno; hay que verificarla contra `wram.asm` y
  fijarla como constante documentada, como se hizo con la orientación.
- RAM de cartucho accesible como `ctx->eram`. Las cajas 1 a 12 y el Salón de
  la Fama tienen símbolos `sBox1` a `sBox12` y `sHallOfFame`. Solo la caja
  activa está en WRAM; las demás requieren leer `eram` con el banco correcto,
  que también es información de solo lectura.
- ROM: `BaseStats`, `MonPartyData`, `WildDataPointers`, `InternalMapEntries`,
  `ExternalMapEntries`, `DisplayTownMap` y los gráficos del logo tienen símbolo
  con dirección relativa al banco. Las lecturas absolutas ya usadas en el
  proyecto, como la tabla de orden del Pokédex en 0x410B1, muestran cómo
  resolver el banco y verificarlo contra la ROM.
- La ficha del Pokédex dibuja el retrato frontal con el mismo mecanismo que el
  combate: siete columnas de siete tiles consecutivos en `wTileMap`, desde
  VRAM. `battle::portrait` y `battle::palette` sirven sin cambios; solo cambia
  el origen del rectángulo, que hay que verificar en `engine/pokedex/pokedex.asm`.
- La lista del Pokédex y las listas del PC no pintan retratos. Para mostrar
  imágenes de especies que no están en VRAM hace falta el descompresor de
  imágenes de la primera generación, portado en C++ de solo lectura.
- Los interiores ya dan volumen al PC: `src/interior_scene.h` clasifica el
  tile de ordenador y escritorio como mueble. La cámara de interior puede
  acercarse a él con las mismas matrices de `src/firstperson.h`.
- El mundo exterior de 36 mapas comparte un origen común. Los datos de nidos
  del juego salen de las tablas de encuentros por mapa, las mismas que usa
  `FindNest` para la opción AREA. Con el catálogo se puede colocar cada nido
  en coordenadas del mundo 3D.
- La técnica `live_return` de `battle_state.h` detecta rutinas activas por su
  dirección de retorno en la pila, verificando la llamada en ROM. Es la vía
  para saber si la ficha, el mapa de zona, el PC de Bill o el almacén están
  en pantalla, sin flags que el juego no tiene.

## Decisiones de arquitectura

1. Un módulo `src/dex3d.h` para el Pokédex y otro `src/pc3d.h` para el PC, con
   detección positiva por `live_return` y respaldo por disposición de tiles.
   `pallet3d.cpp` solo delega el frame cuando `view()` devuelve los estados
   nuevos `Pokedex` y `Computer`.
2. Descompresor de imágenes en `src/mon_pic.h`, portado de la rutina original,
   con caché por especie y orientación. Se valida contra VRAM: cuando el juego
   ha cargado un retrato, la salida del descompresor debe coincidir byte a byte
   con los tiles de VRAM. Esa prueba es la garantía del port.
3. El Pokédex se presenta como un dispositivo: un cuerpo rojo en perspectiva,
   la pantalla izquierda con la lista compuesta desde el framebuffer y la
   pantalla derecha con el retrato de la especie bajo el cursor. Los textos de
   la ficha siguen siendo los originales, compuestos.
4. La opción AREA muestra el mundo 3D real desde gran altura con la cámara
   ortográfica, centrado en Kanto, con marcadores de nido derivados de las
   tablas de encuentros y parpadeo a la misma cadencia que el mapa original.
   Los mapas no catalogados como exteriores conectados, como las cuevas, se
   marcan sobre la entrada correspondiente.
5. El PC se presenta desde el interior en el que está: la cámara se acerca al
   monitor, el menú principal del PC se compone en su pantalla y las listas se
   componen a pantalla completa sobre el interior atenuado. Las cajas se
   muestran además como una estantería 3D con los retratos descomprimidos de
   sus Pokémon, con la caja activa en primer plano.
6. El Salón de la Fama se presenta como galería: un pedestal por Pokémon con
   su retrato, leyendo los equipos de `sHallOfFame`. El texto original se
   compone debajo.
7. Ningún dato de listas se interpreta dos veces: el cursor, el desplazamiento
   y la selección se leen del juego; el 3D solo los refleja.
8. Cualquier detección fallida, cualquier banco de RAM no verificado o
   cualquier retrato cuya descompresión no coincida con VRAM devuelve la
   conducta actual: imagen original sobre fondo atenuado.

## Bloque A: Pokédex

### Fase A1: descompresor verificado y ficha 3D

Trabajo:

- Portar la descompresión de imágenes de la primera generación a `mon_pic.h`:
  lectura de punteros por especie desde `BaseStats` y las excepciones de Mew,
  tamaño de imagen, dos planos, codificación por pares y modos de mezcla.
  Salida en tiles de 8 por 8 con el mismo orden que VRAM.
- Prueba de equivalencia: con savestates de combate y de ficha ya disponibles,
  comparar la salida con `vFrontPic` en VRAM para al menos veinte especies de
  tamaños distintos, incluidas las de 5 por 5 y 6 por 6 tiles.
- Detectar la ficha del Pokédex por `live_return` de la rutina de datos y
  verificar su rectángulo de retrato en `wTileMap`.
- Escena de ficha: dispositivo en perspectiva, retrato en la pantalla derecha
  como billboard con la paleta de la ROM, número, nombre, categoría, altura y
  peso compuestos desde el framebuffer. El botón de grito no cambia: el sonido
  es el original.

Criterios de aceptación:

- [ ] Las veinte comparaciones con VRAM son idénticas byte a byte y la prueba forma parte de CTest.
- [ ] Abrir la ficha de Pikachu desde el menú Start muestra el dispositivo con el retrato correcto y el texto original.
- [ ] Recorrer varias fichas con izquierda y derecha cambia el retrato sin frames con el anterior.
- [ ] Cero errores OpenGL y WRAM, VRAM, RAM de cartucho y framebuffer intactos por frame.

### Fase A2: lista con retrato bajo el cursor

Trabajo:

- Detectar la lista del Pokédex y leer cursor y desplazamiento de WRAM para
  saber qué número está seleccionado; convertir el número a especie interna
  con la tabla de orden ya usada por `battle::dex`.
- Pantalla izquierda con la lista compuesta; pantalla derecha con el retrato
  descomprimido si la especie está registrada como capturada, silueta oscura
  si solo está vista, y vacía si no aparece. Este criterio sigue exactamente
  los mapas de bits del juego.
- Contador de vistos y capturados en el cuerpo del dispositivo, leído de los
  mismos mapas de bits.
- Precarga y caché de retratos por especie con límite de memoria y descarte
  de los más antiguos.

Criterios de aceptación:

- [ ] Desplazar la lista actualiza el retrato al mismo ritmo que el cursor original.
- [ ] Vistos, capturados y ausentes se distinguen según los mapas de bits y coinciden con los contadores del juego.
- [ ] La caché no crece al recorrer los 151 números de un extremo a otro varias veces.

### Fase A3: zona sobre el mundo 3D

Trabajo:

- Leer las tablas de encuentros de hierba y agua de cada mapa y construir los
  nidos de una especie: mapa y tipo de encuentro. Comparar con la salida del
  mapa original para tres especies con nidos en varios mapas.
- Detectar la pantalla AREA por `live_return` de `DisplayTownMap` con el modo
  de nidos activo.
- Cámara ortográfica cenital sobre el mundo de 36 mapas con niebla suave y
  etiquetas de ciudad; marcadores en el centro de cada mapa con nido, con
  parpadeo sincronizado con el contador del juego. Los mapas no conectados se
  marcan en su entrada del mundo exterior.
- El cuadro de texto original con el nombre de la especie se compone abajo.

Criterios de aceptación:

- [ ] Los nidos de Pidgey, Zubat y Magikarp coinciden con los que muestra el mapa original.
- [ ] La vista se abre y se cierra sin frames 2D completos y respeta las mallas residentes al volver al mapa.
- [ ] Una especie sin nido muestra el mensaje original y el mundo sin marcadores.

## Bloque B: PC

### Fase B1: acceso al PC desde el interior

Trabajo:

- Detectar el menú principal del PC y cada submenú por `live_return`: PC de
  Bill, PC del jugador, PC de Oak y Salón de la Fama, tanto en el centro
  Pokémon como en la habitación del jugador.
- Localizar el ordenador en la escena: el mueble clasificado como ordenador
  adyacente al jugador en la dirección hacia la que mira.
- Movimiento de cámara hacia el monitor con la matriz de primera persona,
  desde la cámara actual, en unos 400 ms; retorno al soltar el PC.
- Menú principal compuesto en la pantalla del monitor con perspectiva; el
  texto de encendido y apagado, igual.

Criterios de aceptación:

- [ ] Encender el PC del centro Pokémon de Ciudad Verde acerca la cámara y muestra el menú en el monitor.
- [ ] El PC de la habitación del jugador funciona igual desde ambas cámaras.
- [ ] Apagar el PC devuelve la cámara y el HUD al estado previo sin saltos.

### Fase B2: cajas de Bill con estantería 3D

Trabajo:

- Leer la caja activa de WRAM y las restantes de `eram` con el banco de cada
  caja verificado por sus sumas de comprobación, sin depender de la selección
  de banco actual del cartucho.
- Estantería 3D con doce cajas; la activa en primer plano con hasta veinte
  retratos descomprimidos en cuadrícula, con nombre y nivel; las demás como
  cajas cerradas con su contador.
- Depositar, retirar, liberar y cambiar de caja: las listas originales se
  componen a pantalla completa sobre la estantería atenuada; el cursor de la
  lista resalta el retrato correspondiente en la cuadrícula.
- La cuadrícula se reconstruye solo cuando cambian los datos de caja o de
  equipo, comparando una huella de los bytes leídos.

Criterios de aceptación:

- [ ] Depositar un Pokémon del equipo y retirarlo actualiza la cuadrícula sin frames con datos antiguos.
- [ ] Cambiar de caja muestra el contenido correcto de la caja elegida, incluidas cajas leídas de RAM de cartucho.
- [ ] Liberar un Pokémon lo elimina de la cuadrícula tras la confirmación original.
- [ ] La prueba de la fixture de captura de Pidgey deposita y retira ese Pidgey con éxito.

### Fase B3: almacén de objetos, valoración de Oak y Salón de la Fama

Trabajo:

- PC del jugador: las listas de retirar, depositar y tirar se componen a
  pantalla completa; el monitor muestra el contador de objetos guardados
  leído de WRAM.
- PC de Oak: el texto de valoración se compone; el monitor muestra vistos y
  capturados.
- Salón de la Fama: leer los equipos de `sHallOfFame`, un pedestal por
  Pokémon con retrato descomprimido, nombre y nivel; la cámara recorre la
  fila al ritmo del texto original.

Criterios de aceptación:

- [ ] Guardar y retirar una Poké Ball en el almacén mantiene la escena y el contador correcto.
- [ ] La valoración de Oak se lee sobre el monitor con los contadores coincidentes.
- [ ] Con una fixture privada de campeón, el Salón de la Fama muestra el equipo real leído de RAM de cartucho.

## Limitaciones asumidas

- Los Pokémon son retratos originales, con la paleta de la ROM, no modelos.
- El texto y las listas siguen siendo la imagen original ampliada.
- Las especies cuyo retrato no pueda verificarse contra VRAM en las pruebas se
  muestran con silueta hasta añadir una fixture que las cargue.
- El mapa de zona no dibuja rutas de agua ni cuevas como escenas; marca la
  entrada en el mundo exterior.
- El Salón de la Fama requiere una partida con campeón para probarse; sin ella
  la fase se valida con la lectura de datos y capturas manuales.

## Validación y entrega

- CTest y las baterías `world_qa.sh`, `firstperson_qa.sh` e `interiors_qa.sh`
  antes y después de cada fase; las capturas de exterior siguen idénticas.
- Nuevas pruebas unitarias: descompresor contra VRAM, nidos contra el mapa
  original, lectura de cajas con sumas de comprobación y detección de cada
  pantalla con savestates locales.
- Nuevos modos de `pallet_render_smoke`: `dex` recorre lista, ficha, grito y
  zona; `pc` enciende el PC, deposita, retira, cambia de caja y guarda un
  objeto. Ambos comprueban por frame el modo presentado, GL y memoria intacta.
- Fixtures privadas bajo `build/qa/`: Paleta con Pokédex, Ciudad Verde frente
  al PC con dos Pokémon en el equipo, partida con varias cajas ocupadas y, si
  existe, partida de campeón. No se incorporan al repositorio.
- Entregar `build/pokeyellow3d`, `PALLET3D.md` actualizado con el alcance del
  Pokédex y el PC, y capturas en `build/qa/logs/`.
- Marcar las casillas solo con evidencia registrada.

## Orden recomendado

1. A1, porque el descompresor verificado desbloquea todo lo demás.
2. B1 y A2, que reutilizan el compositor y no dependen entre sí.
3. B2, la parte más visible del PC.
4. A3 y B3, que completan zona, almacén, Oak y Salón de la Fama.

## Referencias técnicas

- [Pokédex: lista, ficha y zona](https://github.com/pret/pokeyellow/blob/master/engine/pokedex/pokedex.asm).
- [Descompresión de imágenes](https://github.com/pret/pokeyellow/blob/master/home/uncompress.asm).
- [Carga de retratos frontales](https://github.com/pret/pokeyellow/blob/master/engine/gfx/sprites.asm).
- [Mapa de ciudad y nidos](https://github.com/pret/pokeyellow/blob/master/engine/menus/town_map.asm).
- [Tablas de encuentros](https://github.com/pret/pokeyellow/blob/master/data/wild/grass_water.asm).
- [PC de Bill](https://github.com/pret/pokeyellow/blob/master/engine/pokemon/bills_pc.asm).
- [PC del jugador](https://github.com/pret/pokeyellow/blob/master/engine/menus/players_pc.asm).
- [Salón de la Fama](https://github.com/pret/pokeyellow/blob/master/engine/events/hall_of_fame.asm).
- [Memoria del juego y RAM de cartucho](https://github.com/pret/pokeyellow/blob/master/ram/wram.asm).
- `src/battle_state.h`: `portrait`, `palette`, `dex` y `live_return`.
