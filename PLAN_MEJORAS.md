# Plan: diez mejoras tras completar el diseño 3D

Fecha: 2026-09-20. Estado: propuesta. Ordenado por prioridad; cada punto es
independiente salvo donde se indica una dependencia.

## Estado de partida

Los planes `PLAN_KANTO_3D.md`, `PLAN_PRIMERA_PERSONA.md`,
`PLAN_INTERIORES_COMBATES.md`, `PLAN_MENUS_TITULO_TRANSICIONES.md` y
`PLAN_POKEDEX_PC.md` definen la presentación 3D completa. Este documento
recoge lo que viene después. Hechos verificados el 2026-09-20:

- Repositorio `github.com/dobbygl/pokeyellow3d`, 12 commits, sin `.github/`.
- La integración con el runtime son 14 parches textuales en
  `cmake/Pallet3D.cmake` sobre `platform_sdl.cpp`, con gb-recompiled fijado a
  una revisión concreta por `GBRT_REF`.
- De las pruebas CTest solo `firstperson` corre sin ROM; las otras seis y todos
  los recorridos de `pallet_render_smoke` exigen la ROM y savestates privados.
- El mundo se dibuja con cajas de color y tiles de 8 píxeles; 2.960 casillas
  de 104 interiores no tienen clasificación artística.
- El lector de tilesets ya extrae el flag de animación, pero agua y flores se
  dibujan estáticas. Los NPC fuera de la pantalla original no se animan.
- El runtime trae SDL2, OpenGL ES 2, ImGui, soporte de mando por
  `SDL_GameController`, transferencia serie con callbacks y un lanzador con
  tabla de juegos verificada por SHA-256.
- El juego expone la pista de música actual en `wMapMusicSoundID`,
  `wNewSoundID` y `wLastMusicSoundID`.

## Resumen

| # | Mejora | Ganancia | Esfuerzo | Depende de |
| --- | --- | --- | --- | --- |
| 1 | API de extensión estable en gb-recompiled | Elimina el riesgo estructural | Medio | — |
| 2 | CI en GitHub Actions y pruebas sin ROM | Regresiones y contribuciones | Medio | 1 |
| 3 | Pase artístico dirigido por datos e iluminación | El mayor salto visual | Alto | 2 |
| 4 | Ciclo día y noche presentacional | Atmósfera con coste bajo | Bajo | 3 |
| 5 | Mundo animado | Elimina la sensación de maqueta | Medio | 2 |
| 6 | Build web y mando | Alcance masivo | Medio | 1, 2 |
| 7 | Rojo, Azul y Amarillo en español | Triplica la audiencia | Alto | 2 |
| 8 | Audio mejorado opcional | Inmersión | Medio | 1 |
| 9 | Modo foto y cámaras de evento | Compartibilidad | Bajo | 3 |
| 10 | Cable link entre instancias | Intercambios y combates | Medio | 1 |

Los puntos 1 y 2 se detallan en `PLAN_API_CI.md`.

## 1. API de extensión estable en gb-recompiled

Qué: sustituir los 14 parches textuales por una interfaz de presentación del
runtime, con callbacks de frame, evento, cierre, captura, cobertura del
framebuffer y máscara de mando. Primero en un fork propio, después como
contribución a `GB-Recomp/gb-recompiled`.

Por qué: cualquier cambio del frontend SDL rompe la compilación; la revisión
fijada impide adoptar mejoras del runtime; otros juegos recompilados no pueden
reutilizar la capa 3D.

Criterios de aceptación:

- [ ] `cmake/Pallet3D.cmake` no contiene ningún reemplazo textual del runtime.
- [ ] La capa 3D compila contra una versión etiquetada del runtime que declara la versión de la API.
- [ ] Las baterías de regresión existentes pasan sin cambios de comportamiento.

## 2. CI en GitHub Actions y pruebas sin ROM

Qué: flujo de integración continua que compila en Linux y Windows, ejecuta las
pruebas que no requieren ROM y publica binarios en cada etiqueta. Una ROM
sintética generada en código alimenta el lector, los clasificadores, la
selección de vista y un render sin cabeza.

Por qué: hoy no existe red de seguridad automática y nadie externo puede
verificar una contribución.

Criterios de aceptación:

- [ ] Cada push y cada pull request compila y pasa CTest en Linux y Windows.
- [ ] Al menos el lector de ROM, los clasificadores de terreno e interior, `view()` y el renderer en modo previsualización se prueban sin ROM.
- [ ] Las pruebas que exigen ROM quedan separadas y documentadas para ejecución local.

## 3. Pase artístico dirigido por datos e iluminación

Qué: pipeline por familia de tile que admita modelos y texturas opcionales
con caída al tile original; luz direccional, sombras por mapa de sombras,
oclusión ambiental y materiales por familia. Completar la clasificación de
las 2.960 casillas pendientes.

Por qué: es la mejora que cualquiera percibe en la primera captura. La
arquitectura ya separa datos de ROM y retoques artísticos; falta el arte.

Criterios de aceptación:

- [ ] Ninguna casilla de los 38 exteriores ni de los 179 interiores queda sin clasificar.
- [ ] Sombras e iluminación activas en ambas cámaras sin superar el doble del tiempo de presentación actual.
- [ ] Un recurso artístico ausente nunca produce un hueco: siempre cae al tile original.

## 4. Ciclo día y noche presentacional

Qué: hora del sistema convertida en posición del sol, color del cielo, niebla
y luz de ventanas por la noche. Solo presentación; el juego no cambia.

Por qué: la primera generación no tiene reloj y el ambiente estático se
percibe como plano. Con la iluminación del punto 3 el coste es bajo.

Criterios de aceptación:

- [ ] Amanecer, mediodía, atardecer y noche se revisan en Paleta, Ruta 1 y Ciudad Verde.
- [ ] Un ajuste de la aplicación permite fijar la hora o desactivar el ciclo.
- [ ] Los encuentros, scripts y RNG no cambian con la hora.

## 5. Mundo animado

Qué: animaciones de tileset de agua y flores según el flag de la ROM; hierba
con viento; NPC fuera de la pantalla original animados a partir de su estado
de movimiento y de las hojas de sprites de la ROM; partículas al andar en
hierba y al surfear.

Por qué: hoy el mundo es una maqueta con actores congelados a distancia.

Criterios de aceptación:

- [ ] Agua y flores se animan con la cadencia del juego original.
- [ ] Un NPC caminando fuera de la pantalla original muestra sus frames de andar.
- [ ] Los recorridos de regresión siguen pasando con memoria intacta.

## 6. Build web y mando

Qué: compilación con Emscripten, con la ROM aportada por el usuario en el
navegador y verificada por hash; mapeo de mando para movimiento relativo,
cámara y atajos.

Por qué: SDL2, OpenGL ES 2 e ImGui son la combinación natural para WebAssembly.
Jugar sin instalar multiplica el alcance.

Criterios de aceptación:

- [ ] La versión web arranca, carga una ROM local y llega al exterior en 3D.
- [ ] Un mando recorre Paleta, habla con un NPC y combate sin teclado.
- [ ] La CI publica la versión web en cada etiqueta.

## 7. Rojo, Azul y Amarillo en español

Qué: tabla de direcciones por variante de ROM, verificada contra los símbolos
de cada proyecto pret, y selección automática por hash en el lanzador.

Por qué: la capa 3D solo entiende la ROM inglesa de Amarillo. Las otras tres
variantes comparten estructura y multiplican la audiencia.

Criterios de aceptación:

- [ ] Cada variante pasa la auditoría de ROM y el recorrido de Paleta a Ciudad Verde.
- [ ] Ninguna dirección se comparte entre variantes sin verificación registrada.
- [ ] El lanzador identifica la variante por hash y rechaza ROM desconocidas.

## 8. Audio mejorado opcional

Qué: detección de la pista actual por WRAM, pack de música aportado por el
usuario con fundido cruzado, y efectos posicionales en 3D. Audio original por
defecto y ningún recurso incluido en el repositorio.

Por qué: la inmersión del 3D contrasta con el chip original, y la pista
actual ya es legible.

Criterios de aceptación:

- [ ] Cambiar de mapa o entrar en combate cambia la pista del pack con fundido.
- [ ] Sin pack, el audio es idéntico al actual.
- [ ] Los efectos de pasos y puertas se posicionan respecto a la cámara.

## 9. Modo foto y cámaras de evento

Qué: cámara libre con el juego en pausa, cámaras guiadas en los eventos de
Oak y del Team Rocket detectadas por script, captura PNG y GIF desde la
aplicación.

Por qué: es barato, no escribe en memoria y las capturas compartidas son el
mejor crecimiento posible para el proyecto.

Criterios de aceptación:

- [ ] El modo foto no avanza el juego ni altera WRAM.
- [ ] La entrada de Oak en Ruta 1 tiene cámara guiada y vuelve a la cámara normal al terminar.
- [ ] Las capturas se guardan junto a la partida con nombre por mapa y fecha.

## 10. Cable link entre instancias

Qué: transporte por socket local o de red para la transferencia serie del
runtime, con las pantallas del Club Cable compuestas sobre el 3D.

Por qué: intercambios y combates originales son el mayor valor nostálgico
que queda, y la escena 3D de combate ya existe.

Criterios de aceptación:

- [ ] Dos instancias intercambian un Pokémon y ambas partidas quedan coherentes.
- [ ] Un combate link completo se presenta en 2D, como define el plan de combates, sin desincronía.
- [ ] La desconexión se gestiona con el mensaje original del juego.
