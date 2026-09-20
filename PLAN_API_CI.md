# Plan: API de extensión del runtime y CI en GitHub Actions

Fecha: 2026-09-20. Estado: propuesta; ninguna fase iniciada. Desarrolla los
puntos 1 y 2 de `PLAN_MEJORAS.md`.

## Objetivo y alcance

Dos resultados encadenados. Primero, que la capa 3D se integre con
gb-recompiled a través de una interfaz declarada y versionada, en lugar de
reescribir el frontend SDL en tiempo de configuración. Segundo, que cada push
y cada pull request de `github.com/dobbygl/pokeyellow3d` compile en Linux y
Windows y ejecute una batería de pruebas que no necesita la ROM, con los
binarios publicados en cada etiqueta.

Quedan fuera: cambiar el comportamiento del juego o del renderer, incluir la
ROM o cualquier derivado suyo en el repositorio o en la CI, y las pruebas de
recorrido con savestates, que siguen siendo locales.

## Punto de partida verificado

- `cmake/Pallet3D.cmake` aplica 14 reemplazos textuales sobre
  `runtime/src/platform_sdl.cpp` del runtime descargado y compila la copia
  resultante en lugar del original. Cada reemplazo aborta la configuración si
  su ancla desaparece. Los puntos tocados son: inclusión de cabecera, tamaño
  del búfer de profundidad, dibujo tras `ImGui::NewFrame`, evento antes del
  mando, cierre antes de destruir GL, captura antes de `SDL_GL_SwapWindow`,
  omisión de la subida y el dibujo del framebuffer cuando el 3D cubre el frame
  en tres puntos, y un canal de d-pad propio con liberación de teclas WASD en
  cinco puntos, incluida la grabación de entradas.
- `CMakeLists.txt` obtiene gb-recompiled por `FetchContent` con la revisión
  fijada en `GBRT_REF`. El runtime enlaza SDL2, CURL, Threads y OpenGL o GLES2,
  y su CMake ya distingue Linux, Windows y macOS.
- Las fuentes C generadas del juego están en el repositorio, 13 archivos y
  unos 700 KB el mayor, así que la compilación no necesita rgbds ni pret.
- Pruebas CTest actuales: `firstperson` corre sin ROM. `pallet_state`,
  `kanto_rom`, `kanto_geometry`, `interior`, `interior_audit` y
  `battle_state` se registran solo si existe `build/roms/pokeyellow.gbc`.
  `pallet_render_smoke` está excluido de `all` y requiere ROM y savestates.
- `pallet_render_smoke` ya inicializa el runtime sin cabeza con
  `SDL_VIDEODRIVER=offscreen` y `SDL_AUDIODRIVER=dummy` sobre el backend
  OpenGL real, y `pallet3d_preview` dibuja un mapa sin actores ni partida.
- `.gitignore` excluye `build/`, las ROM y las partidas. No existe `.github/`.
- El lector `kanto::Rom` valida rangos y lanza excepciones con diagnóstico;
  las estructuras de cabeceras, tilesets, cornisas y pares de colisión están
  descritas en `src/kanto_rom.h` con sus offsets, lo que permite generar una
  imagen sintética que las satisfaga.

## Decisiones de arquitectura

1. La interfaz vive en el runtime, no en este repositorio. Se define en un
   nuevo `runtime/include/gb_presentation.h` con una estructura de callbacks
   y una constante `GB_PRESENTATION_API_VERSION`. Este repositorio solo
   registra su implementación.
2. Camino en tres pasos: fork `dobbygl/gb-recompiled` con la interfaz y una
   etiqueta; este repositorio apunta a esa etiqueta; pull request al proyecto
   original. Cuando se acepte, `GBRT_REF` pasa a la etiqueta original. El
   repositorio no depende de que el pull request se acepte para funcionar.
3. La interfaz cubre exactamente lo que hoy hacen los parches, sin ampliar:
   atributos GL antes de crear el contexto, dibujo por frame que devuelve si
   cubre el framebuffer, evento con posibilidad de consumirlo, cierre,
   captura antes del intercambio de búferes, y un canal de d-pad externo con
   una llamada para soltar las teclas de un conjunto de acciones. La
   grabación de entradas incluye el canal externo, como ahora.
4. Compatibilidad de versión explícita: `Pallet3D.cmake` comprueba la
   constante de la cabecera y falla con mensaje si no coincide. Desaparecen
   todas las anclas textuales.
5. Las pruebas sin ROM usan una imagen sintética construida en memoria por
   `tests/synthetic_rom.h`, con las tablas mínimas que el lector y los
   clasificadores esperan, y un `GBContext` de prueba con WRAM, VRAM, E/S y
   framebuffer propios. Nunca contiene datos copiados de la ROM real.
6. Las pruebas que exigen ROM conservan su registro condicional y se ejecutan
   en local o en un runner propio. La CI las lista como omitidas, no como
   fallidas.
7. Un solo flujo `ci.yml` con matriz de sistemas y un flujo `release.yml`
   por etiqueta. Sin secretos.

## Fase 1: interfaz de presentación en un fork del runtime

Trabajo:

- Crear el fork `dobbygl/gb-recompiled` desde la revisión fijada actual.
- Añadir `gb_presentation.h`:
  - `GBPresentationHooks` con `gl_attributes()`, `frame(ctx, w, h, menu_open) -> bool covers`,
    `event(const SDL_Event*, menu_open) -> bool consumed`, `before_swap(w, h)`,
    `shutdown()`, y `input_poll(ctx, menu_open)`.
  - `gb_platform_set_presentation(const GBPresentationHooks*)`.
  - `gb_platform_set_external_dpad(uint8_t mask)` y
    `gb_platform_release_keys(const SDL_Scancode*, size_t)`.
  - `GB_PRESENTATION_API_VERSION` entera.
- Implementar en `platform_sdl.cpp` los seis puntos de llamada y el canal de
  d-pad, replicando el comportamiento actual de omisión de subida del
  framebuffer y de grabación de entradas. Sin presentación registrada, el
  runtime se comporta exactamente como antes.
- Etiqueta `presentation-api-v1` en el fork.

Criterios de aceptación:

- [ ] El runtime del fork compila y ejecuta un juego sin presentación registrada con comportamiento idéntico.
- [ ] Una presentación de prueba en el propio fork recibe frame, evento, cierre y captura en el orden esperado.
- [ ] La constante de versión está documentada en el README del runtime.

## Fase 2: migración de la capa 3D a la interfaz

Trabajo:

- `GBRT_REF` apunta a `presentation-api-v1` del fork, con la URL del fork
  como variable de caché para poder volver al original.
- `src/pallet3d.cpp` expone una `GBPresentationHooks` estática y la registra
  desde el lanzador o desde `pokeyellow_main`. Los controles relativos usan
  `gb_platform_set_external_dpad` y `gb_platform_release_keys`.
- `cmake/Pallet3D.cmake` queda reducido a: comprobar la versión de la API,
  añadir `src/pallet3d.cpp` al ejecutable y fijar el tamaño del búfer de
  profundidad a través del callback de atributos.
- Eliminar el directorio `pallet-runtime` generado y toda referencia a él en
  documentación y scripts.

Criterios de aceptación:

- [ ] `cmake/Pallet3D.cmake` no contiene ninguna llamada de reemplazo textual.
- [ ] CTest completo y las tres baterías `world_qa.sh`, `firstperson_qa.sh` e `interiors_qa.sh` pasan en local.
- [ ] Las 38 capturas exteriores y las de interiores son idénticas a las de la versión anterior.
- [ ] La grabación y reproducción de entradas con controles relativos produce el mismo estado final que antes.

## Fase 3: ROM sintética y pruebas sin ROM

Trabajo:

- `tests/synthetic_rom.h`: constructor de una imagen de 1 MiB con cabecera de
  cartucho válida, `MapHeaderBanks`, `MapHeaderPointers`, cabeceras de
  tilesets, tabla de cornisas y pares de colisión en los offsets que espera
  `kanto_rom.h`. Genera un mundo pequeño: tres exteriores conectados con
  desplazamientos, un interior por warp, un tileset exterior y uno interior
  con gráficos procedurales, hierba, agua, una cornisa y un cartel.
- `tests/synthetic_context.h`: `GBContext` con WRAM, VRAM, E/S, HRAM y
  framebuffer propios, con ayudantes para colocar al jugador, escribir el
  mapa vivo, abrir un cuadro de texto por tiles de borde y montar un tilemap
  de combate con retratos.
- Pruebas nuevas, todas registradas siempre:
  - `rom_reader_synthetic`: catálogo, conexiones, orígenes, warps, rechazo de
    imágenes truncadas.
  - `terrain_synthetic`: clasificación exterior e interior sobre el mundo generado.
  - `view_synthetic`: `Unsupported`, `Transition`, `Dialogue`, `Overworld` y
    `Battle` a partir del contexto sintético.
  - `battle_state_synthetic`: rectángulos de retrato, nombres, paleta por
    defecto y descompresión de un retrato sintético si la fase A1 de
    `PLAN_POKEDEX_PC.md` ya existe.
  - `render_preview_synthetic`: inicializa SDL sin cabeza, construye las
    mallas del mundo sintético con `pallet3d_preview`, dibuja diez frames y
    comprueba GL sin errores, memoria intacta y una captura no vacía.
- Las pruebas con ROM real siguen registradas solo si la ROM existe. Añadir
  una etiqueta CTest `rom` para poder excluirlas con `-LE rom`.

Criterios de aceptación:

- [ ] `ctest -LE rom` pasa en un directorio de compilación sin `roms/`.
- [ ] Ninguna prueba sintética contiene bytes copiados de la ROM real; el generador es puramente procedural.
- [ ] `render_preview_synthetic` pasa con `SDL_VIDEODRIVER=offscreen` sobre Mesa por software.

## Fase 4: flujo de CI

Trabajo:

- `.github/workflows/ci.yml` con disparo en push y pull request:
  - Matriz: `ubuntu-24.04` con GCC y con Clang; `windows-2022` con MSVC y
    dependencias por vcpkg; `macos-14` marcado como no bloqueante hasta
    confirmar el backend GL del runtime.
  - Pasos: checkout, instalación de dependencias, caché de `_deps` de
    FetchContent y de ccache, configuración con `-DPOKEYELLOW_3D=ON`,
    compilación con Ninja, `ctest -LE rom --output-on-failure`.
  - En Linux, `render_preview_synthetic` bajo `xvfb-run` o con Mesa
    `llvmpipe` por EGL sin superficie; se fija el que funcione en el runner.
  - Compilación adicional con `-DPOKEYELLOW_3D=OFF` en Linux para no romper el
    ejecutable 2D.
  - Avisos como errores para `src/` y `tests/` con `-Wall -Wextra -Werror`;
    el runtime y el C generado quedan fuera de esa regla.
  - Comprobación de formato con `clang-format --dry-run` sobre `src/` y `tests/`.
- `.github/workflows/release.yml` con disparo por etiqueta `v*`: compila en
  Linux y Windows, empaqueta `pokeyellow3d` con el lanzador, `PALLET3D.md` y
  un `README` de ejecución, y publica los archivos en la release. Sin ROM.
- Insignias de estado en `README.md` y una sección "Contribuir" con los
  comandos locales y la nota de que las pruebas `rom` son locales.
- Protección de rama: `main` exige el flujo en verde para fusionar.

Criterios de aceptación:

- [ ] Un push a `main` y un pull request de prueba muestran el flujo en verde en Linux y Windows.
- [ ] Un pull request que rompa el lector sintético falla en la CI con la prueba señalada.
- [ ] Una etiqueta `v0.1.0` produce una release con binarios de Linux y Windows.
- [ ] El tiempo total del flujo con caché caliente es inferior a diez minutos.

## Fase 5: contribución al proyecto original

Trabajo:

- Pull request a `GB-Recomp/gb-recompiled` con `gb_presentation.h`, la
  implementación, la presentación de ejemplo y la documentación. Sin ninguna
  referencia a Pokémon en el código del runtime.
- Atender la revisión; si la interfaz cambia, actualizar la etiqueta del fork
  y este repositorio en el mismo cambio.
- Al fusionarse, `GBRT_REF` apunta a la etiqueta del proyecto original y el
  fork queda como espejo.

Criterios de aceptación:

- [ ] El pull request está abierto con la CI del proyecto original en verde.
- [ ] Este repositorio compila contra la etiqueta original tras la fusión, o contra el fork mientras tanto, sin parches textuales en ningún caso.

## Limitaciones asumidas

- Las pruebas de recorrido, combate real, interiores reales y comparaciones
  de capturas siguen necesitando la ROM y savestates privados. La CI no las
  sustituye; garantiza el lector, los clasificadores, la selección de vista y
  el renderer.
- macOS depende de que el runtime ofrezca un backend GL compatible; hasta
  confirmarlo no bloquea.
- La aceptación del pull request no depende de este proyecto; el fork cubre
  el intervalo.

## Validación y entrega

- Cada fase termina con CTest completo en local con ROM y con `ctest -LE rom`
  en un directorio sin ROM.
- Las capturas de regresión se comparan byte a byte tras la fase 2.
- Entregables: fork etiquetado, `Pallet3D.cmake` sin parches, generador
  sintético y sus pruebas, dos flujos de GitHub Actions, README con insignias
  y sección de contribución, y el pull request al proyecto original.
- Marcar las casillas solo con evidencia registrada, incluidos los enlaces a
  las ejecuciones de la CI.

## Orden recomendado

1. Fase 3 puede empezar ya, en paralelo con la fase 1, porque no toca la
   integración con el runtime.
2. Fase 1 y fase 2 en secuencia, con la regresión de capturas como puerta.
3. Fase 4 en cuanto la fase 3 tenga la primera prueba sintética; se amplía a
   medida que llegan las demás.
4. Fase 5 al cerrar la fase 2.

## Referencias técnicas

- `cmake/Pallet3D.cmake`: inventario de los 14 puntos de integración actuales.
- `build/_deps/gb_recompiled-src/runtime/src/platform_sdl.cpp`:
  `update_effective_joypad_state`, subida del framebuffer y bucle de eventos.
- `build/_deps/gb_recompiled-src/runtime/CMakeLists.txt`: dependencias y ramas por sistema.
- `tests/pallet_render_smoke.cpp`: inicialización sin cabeza que reutiliza la prueba sintética.
- [GB-Recomp/gb-recompiled](https://github.com/GB-Recomp/gb-recompiled).
- [GitHub Actions: matrices y caché](https://docs.github.com/actions).
