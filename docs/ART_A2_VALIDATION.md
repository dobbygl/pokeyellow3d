# A2: clasificación de interiores

Base: `e089bd945a58d6fde9e552646ce554f906939749` (B2, PR #34).
Referencia histórica: `7955aaa0ddbba3d9e2fd78085902be167faccf23`.
Runtime: `00cc26dafb9a41ea9d935508e9fbe9e25b5f5a6e`.

## Alcance del cambio

`src/interior_art.h` documenta 143 reglas por tileset y gráficos. Cubren las
2.960 casillas que el clasificador original dejaba sin clasificar. Las reglas
se consultan después del clasificador original: sus warps, suelos transitables
y perímetro tienen prioridad. Se preservan las 869 paredes que coinciden con
alguna regla nueva. Un gráfico futuro sin regla sigue plano y sin clasificar;
el audit lo denuncia.

El ajuste «Pase artístico» gobierna tanto la geometría como los volúmenes
usados por la oclusión ambiental. OFF sigue llamando al clasificador original.
La clasificación lee los bloques vivos del motor, incluidos los cambios de
eventos. No modifica ROM, colisiones, movimiento ni estado del juego.

Decisiones visuales frente al borrador de Claude:

- `Machine`, `Window` y `Rock` identifican familias distintas. De momento usan
  cajas de mobiliario o pared; luces y muebles detallados corresponden a D1.
- Las plantas y arbustos nuevos tienen altura 0,65 para limitar la oclusión.
- Las cuatro casillas dudosas de la caseta de la azotea usan altura 0,65,
  consistente con sus piezas existentes.
- En FACILITY se corrigen también 100 mitades traseras de mesas ya
  clasificadas: de 1,45 a 0,65. La corrección es solo artística y no afecta
  al tileset CEMETERY, aunque ambos compartían una regla antigua.
- Las paredes invisibles del gimnasio de Fucsia continúan planas. Cuatro
  reglas usan además el tile superior derecho para distinguir gráficos con
  la misma pareja izquierda.

## Comprobaciones y límites

El audit registra 179 interiores, 21 tilesets interiores, cero casillas sin
clasificar y cero gráficos desconocidos. Todas las reglas se ejercitan con
la ROM. El CSV sin `--art` conserva exactamente el formato y contenido de B2.

El catálogo compara 3.906 imágenes y sus estados serializados:

- 868 pares OFF exactos frente a B2: 217 mapas, dos cámaras y dos estilos.
- 434 pares OFF exactos frente a `7955aaa`, en ortográfica. Su helper no
  ofrece la API de previsualización FP; no se atribuye esa cobertura.
- Estado del motor idéntico entre referencia, OFF y ON.
- Los 152 exteriores ON conservan exactamente la imagen de B2.
- Cambian 97 interiores en ortográfica y 88 en FP. Algunas casillas
  clasificadas son vacíos que deben seguir planos.

Revisión visual: atlas de los 25 tilesets, 14 hojas del borrador y 174 imágenes
comparadas de 29 mapas en ambas cámaras. Incluye los 21 tilesets interiores,
los cuatro exteriores y cuatro mapas dudosos adicionales. Se revisaron
suelo, alturas, mesas, plantas y caseta. No equivale a recorrer todas las
posiciones posibles del jugador.

`render_preview_synthetic` comprueba que A2 genera volumen visible en ambas
cámaras, reconstruye una sola vez al cambiar el ajuste y reutiliza la malla.
OFF y el fallo real de inicialización del FBO restauran todos los píxeles y
vértices de referencia. Un sprite procedural junto a una máquina permanece
visible desde cuatro ángulos de habitación. Se conservan los oráculos
independientes de memoria y OpenGL.

El nuevo runner de recorridos compara el renderer de B2 con el candidato,
usando el mismo arnés: casa, diálogo de la madre, laboratorio, escaleras,
ascensor, cueva y Ciudad Verde (curación, tienda y diálogos). La casa se
recorre en ambas cámaras; escaleras y cueva también prueban FP. Para cada
escenario y estilo compara OFF píxel a píxel y ON/OFF en estado serializado.
Los diálogos integrados activan el oráculo de glifos existente.

Se amplía únicamente el presupuesto de espera del arnés para el diálogo de
la madre: puede incluir curación y varias páginas. La aserción sigue exigiendo
que el motor vuelva al mundo. El helper del padre se recompila con el mismo
arnés y su renderer congelado; no se modifica su lógica de presentación.
Las primeras ejecuciones fallidas se conservan como evidencia: espera corta
de la madre y omisión de `QA_GLYPH_ORACLE` en el runner nuevo.

Esta validación no sustituye las regresiones históricas completas de menús,
combate, PC, Pokédex y título ni cierra el presupuesto de B2. La fusión de A2
queda pendiente de la revisión de B2 y de los gates generales del plan.

## Reproducir sin publicar datos del juego

Variables: `RUNTIME_FIJADO` apunta al checkout del runtime; `ROM` al cartucho
privado; `PALLET_STATE` a la fixture de Paleta (9,7), con equipo. El runner
prepara las demás fixtures mediante entradas del motor o su warp de QA.
`EVIDENCIA_PRIVADA` debe quedar fuera de Git. Los helpers congelados se
identifican mediante sus SHA y recetas de construcción.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DFETCHCONTENT_SOURCE_DIR_GB_RECOMPILED="$RUNTIME_FIJADO" \
  -DGBRT_ENABLE_GBCAM=OFF
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
build/interior_audit "$ROM" --art > build/qa/logs/interiors.csv
python3 tests/art_reference_build.py --build build --runtime "$RUNTIME_FIJADO" \
  --parent e089bd945a58d6fde9e552646ce554f906939749 \
  --parent-current-smoke --output "$EVIDENCIA_PRIVADA"
python3 tests/art_b2_catalog_qa.py --phase a2 --smoke build/pallet_render_smoke \
  --parent "$SMOKE_PADRE" --reference "$SMOKE_7955AAA" \
  --rom "$ROM" --state "$PALLET_STATE" --output "$EVIDENCIA_PRIVADA"
python3 tests/interior_art_qa.py --smoke build/pallet_render_smoke \
  --parent "$SMOKE_PADRE" --rom "$ROM" --state "$PALLET_STATE" \
  --output "$EVIDENCIA_PRIVADA"
```

Ejecutar las baterías gráficas en serie. Sin ROM, `ctest -LE rom` excluye las
fixtures privadas; `interior_art_rom` queda registrado y devuelve 77 si falta
el cartucho. Los catálogos y recorridos conservan binarios, entradas,
hashes, logs y capturas comprimidas sin pérdida, fuera del repositorio.

Evidencia local de esta implementación (24 de septiembre de 2026):
`/home/jgomez/Projects/jgomez/pokemon2/pokeyellow-art-a2-evidence-p7dv4d5_`.
El catálogo está en `a2-catalog-fii9_1by`; la revisión de clasificación y
contactos está en `a2-classification-catalogs.json` y `contacts/`.
