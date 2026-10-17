# Research: referencias para la mascota

Análisis de las referencias del brief y de otras fuentes, hecho el 2026-10-17 leyendo código y READMEs directamente (API de GitHub y ficheros raw). Todo lo que sigue es **material de diseño**: no se copia código GPL ni arte de terceros. Los números salen del código fuente salvo que se indique lo contrario.

Estado de las conclusiones: de aquí salen `PET_DESIGN.md` (mecánicas), `PET_TECH.md` (arquitectura) y `STYLEGUIDE.md` (arte).

## 0. Correcciones al brief original

| Punto del brief | Realidad |
| --- | --- |
| RoboEyes "idea, sin código" | Correcto en la práctica, pero es **GPL-3.0** (cabecera y `LICENSE.txt`). Solo ideas. |
| pocket-tank como referencia de "mascota" | Es una **pecera**: un LLM de 14,3 M parámetros elige 1 de 8 objetivos por pez y una capa de reflejos lo mueve. **No hay comida por WiFi** (WiFi solo para OTA y reloj). |
| pocket-tank "ya corre en esta placa" con BSP | Corre en esta placa, pero con **drivers propios** (`esp_lcd_sh8601`, FT3168 por I2C directo, sin BSP), y **sin LVGL en el dispositivo** (LVGL solo da la ventana SDL en el simulador). |
| TamaFi "evolución por etapas" | Las etapas ADULT y ELDER son **inalcanzables** por un bug (ver 1). Las cuatro etapas usan el mismo sprite. |
| TamaFi, código MIT | El código es MIT. **El arte no se puede redistribuir** (ver 1). |
| Catode32, arte "no se usa" | Confirmado: CC BY-NC-ND 4.0 (ND impide incluso escalarlo). |

## 1. TamaFi (cifertech/TamaFi)

Licencia: código MIT. Arte de terceros: Mecha-Stone Golem (itch.io) **prohíbe revender y redistribuir**; MyPixelWorld CC BY 4.0; un pack GUI de licencia dudosa; VolosR/SpritesTuT sin licencia. 464 estrellas, sketch Arduino con TFT_eSPI (~1.170 líneas en `TamaFi.ino` + `ui.cpp`). El propio README admite partes simplificadas.

**Estadísticas.** `hunger` (en realidad saciedad), `happiness`, `health`, 0–100, inicio 70. Rasgos al primer arranque: curiosity 40–89, activity 30–89 (no se usa), stress 20–79.

**Decaimiento** (solo con el dispositivo encendido, sin recuperación offline):

| Stat | Ritmo | De 100 a 0 |
| --- | --- | --- |
| hunger | −2 cada 5 s | ~4 min |
| happiness | −1 cada 7 s (−3 si el último escaneo dio 0 redes y hace > 30 s) | ~12 min |
| health | −1 cada 10 s (−2 si hunger o happiness < 20) | ~17 min |

Muere cuando las tres llegan a 0.

**Ánimo** (escalera de prioridades, gana la primera coincidencia): SICK (health < 25, o 0 redes hace > 60 s) → HUNGRY (hunger < 25) → EXCITED (happiness > 80 y > 8 redes) → HAPPY (happiness > 60 y ≥ 1 red) → BORED (0 redes hace > 30 s) → CURIOUS (hay red oculta o abierta) → CALM. El ánimo solo cambia la velocidad de la animación idle (120/200/280 ms por frame).

**Motor de decisión** (cada 8–15 s aleatorios, en idle). Cada opción puntúa su "deseo" y gana el máximo:
- Hunt = (100 − hunger) + curiosity/2; ×0,5 si 0 redes; +20 si HUNGRY.
- Discover = curiosity + 10·ocultas + 6·abiertas + 2·redes + rand(0–20); ×0,5 si 0 redes; +15 CURIOUS, +10 BORED/EXCITED.
- Rest = (100 − health) + stress/2; −10 si hunger < 20; +20 SICK; −10 HUNGRY.
- Idle = 10.

**De escaneo a comida.** Cada escaneo da `WifiStats`: redes, fuertes (RSSI > −60), ocultas, abiertas, WPA, RSSI medio.
- Hunt: `hunger += min(35, 2·redes + 3·fuertes)`; `happiness += min(30, 3·(2·ocultas + abiertas) + (rssiMedio+100)/3)`; `health += 5` si rssiMedio > −75, +5 si > −65, +3 si fuertes > 5. Con 0 redes: hunger −15, happiness −10, health −5.
- Discover: `happiness += min(35, (4·ocultas + 3·abiertas + redes)/2)`, `hunger −5`.
- No existe un "score de calidad del entorno": son los contadores del último escaneo.

**Evolución.** BABY → TEEN (≥ 20 min, media > 35) → ADULT (≥ 60 min, media > 45) → ELDER (≥ 180 min, media > 40). **Bug:** `ageMinutes` se resta 60 al llegar a 60 y la evolución solo mira `ageMinutes`, así que nunca pasa de 59: solo TEEN es alcanzable.

**Persistencia.** `Preferences`, cada 30 s (15/30/60 seleccionable).

**Se adopta:** fórmulas escaneo → comida (reescaladas), motor de deseo, ánimo como escalera, rasgos que sesgan decisiones. **Se descarta:** el arte, los ritmos (vida de minutos), la frecuencia de escaneo (cada 8–15 s es inviable en batería), la muerte.

## 2. pocket-tank (mediacutlet/pocket-tank)

Licencia: código MIT; los 44 iconos PNG caen bajo esa MIT. Los sonidos están generados con ElevenLabs y el modelo se entrenó con salida de Gemma: revisar términos antes de reutilizar sonidos o modelo. 221 estrellas, creado 2026-08-24, última actividad 2026-10-16, v0.3.5 alpha, con CI de autotests y notas de desarrollo muy detalladas (`docs/`).

**Arquitectura.** `common/` (núcleo C compartido: `tank.c`, `render.c` de ~3.800 líneas, `progression.c`, audio, iconos), `sim/` (simulador SDL2; LVGL 9.2.2 solo para la ventana con un canvas RGB565) y `firmware/` (ESP-IDF 5.4). Cada pieza de hardware es un fichero pequeño sustituible con versión stub: `display_port_sh8601.c`, `touch_port_ft3168.c`, etc. Detecta la placa por I2C (el RTC en 0x51 identifica el reloj).

**Datos de la placa** (`docs/board-amoled-2.06-watch.md`): mundo vertical 410×502 con esquinas redondeadas (radio ~100 px), RST LCD GPIO 8, RST táctil GPIO 9, panel alimentado por ALDO2 (hay que encenderlo antes del panel), 30–32 fps (render 6–7 ms, envío al panel ~13 ms), FT3168 con NACK periódico en lectura idle (~cada 3,7 s) que se ignora, calibración táctil observada en muñeca (x = 0,963·x + 14,8; y = 0,957·y + 13,5). Los drivers de core ya resuelven lo equivalente en este workspace; esto es solo contraste.

**Arte.** Todo dibujado por código a un búfer RGB565: gradientes, suelo negro puro (AMOLED), viñeta; peces como polígono + cola animada + marcas ganadas. Máscara de píxeles sucios por región. Paleta nocturna = multiplicador de atenuación. Iconos 16/24 px de tres tonos, convertidos por `tools/gen_icons.py`.

**Tiempo y sueño.** Filosofía: "shaped by attention, never ruined by absence". Etapa 1 (20 min): estado guardado, panel y táctil apagados, light sleep en cortes de 1 s (~4,7 mA), un toque reanuda. Etapa 2: apagado suave por el PMIC (< 40 µA, RTC vivo). Al arrancar: carga el guardado y ejecuta **un único `tank_tick_sleep`** con el tiempo real transcurrido, **con tope de 7 días**; el hambre sube 0,8/h dormido y dormir cuenta 25 % para crecer. Sin RTC, el tiempo parte del mayor entre la hora de compilación y la del guardado (nunca retrocede).

**Persistencia.** Un blob NVS escrito por eventos (≤ cada 30 s) más latido de 10 min. El formato **solo crece por el final**, así versiones nuevas y viejas leen el mismo guardado.

**Números útiles.** Hambre 0–10, +0,010/s (de saciado a hambriento ~17 min); autoalimentación solo si el hambre > 7,5; etapas fry → juvenile 20 min, adult 90 min, elder 8 h de tiempo cuidado; rasgos 0–9 que derivan ~1 unidad por 6 h; economía con "sand dollars".

**Se adopta:** núcleo compartido + simulador, un único tick de "tiempo ausente" con tope, blob que solo crece, guardado por eventos + latido, filosofía sin culpa, el patrón de render por código con máscara de suciedad. **No se adopta:** deep sleep tal cual (no funciona bajo el Launcher, ver `PET_TECH.md`), LLM, sus drivers (usamos `watch_board`).

## 3. Catode32 (moonbench/catode32)

Licencia: código MIT; arte CC BY-NC-ND 4.0 + cláusula de marca. **No se usa arte.** 121 estrellas, 578 commits, Rust (`esp-hal`) con emulador de escritorio. OLED 1-bit 128×64; gato compuesto por partes con puntos de anclaje.

**Capas de estadísticas** (17 almacenadas, 0–100, más `health` derivada). Las capas no son temporizadores distintos: cada comportamiento aplica un conjunto de cambios al terminar y **el tamaño del cambio fija la velocidad**.

| Capa | Stats | Escala |
| --- | --- | --- |
| Rápida | fullness, energy, comfort, playfulness, focus | ~diaria |
| Media | fulfillment, cleanliness, intelligence, maturity, affection | ~semanal |
| Lenta | fitness, serenity | ~mensual |
| Personalidad | courage, loyalty, mischievousness, curiosity, sociability | 50 + offset de la semilla |

Ejemplo (dormir): energy +35 (+12 si fullness > 60), focus +9, comfort +10, playfulness +15, fullness −5. Idle: comfort −0,4, cleanliness −0,06, serenity +0,0075.

**Amortiguación:** ganancias × `((100−v)/100)^0,7`, pérdidas × `(v/100)^0,7`; por debajo de 25 las ganancias llegan a ×3 (recuperación). Enfermedad (0–10, se acumula por minuto despierto con clima malo o necesidades bajas) reduce ganancias ×0,8/0,6/0,4; un sueño resta 1,0.

**Selección de comportamiento:** (1) refugio si hay mal tiempo; (2) vagar con p = 0,2 (baja con la enfermedad); (3) salida de escena ponderada; (4) quedarse quieto con p = (serenity−25)/150; (5) si no, cada comportamiento pasa su `can_trigger` y puntúa por `priority` (rango aleatorio según stats, **menor = más urgente**); penalización por recencia `50 − 10·posición` en los últimos 5; puntuaciones redondeadas a bins de 10 y elección aleatoria dentro del mejor bin.

**Comodidad por WiFi (`wifi_tracker.rs`), lo más relevante:** escaneo cada hora, hasta 20 AP por BSSID; dos listas (familiar 16, reciente 8); por escaneo +1 si visto, −0,25 si no, se elimina a ≤ 0, se promueve a familiar con cuenta ≥ 5; `in_familiar_location` si algún AP familiar está visible; la radio se enciende solo durante el escaneo.

**Tiempo:** 1 minuto de juego = 15 s reales (día = 6 h); nada se simula apagado.

**Se adopta:** capas por magnitud de cambio, curva de amortiguación y recuperación, bucle de selección, tracker de BSSID familiares, enfermedad por descuido (aquí solo como ánimo/aspecto, nunca muerte).

## 4. Tamagotchi original (P1) y Digimon V-Pet

Fuentes: wiki de Tamagotchi (P1 y lista de personajes), wiki de Care, Wikimon (Ver.1), humulos.com (Color). Las reimplementaciones abiertas (tamalib, TamaTool, anabolyc/Tamagotchi) son GPL-2 y **requieren un ROM de Bandai**: no se usan, solo se lee la documentación.

**P1:** 4 corazones de hambre y 4 de felicidad; disciplina 0–100 % (+25 % por regaño correcto); peso (comida +1, snack +2 y +1 corazón); edad +1 por día. **Error de cuidado:** el icono de atención se enciende por medidor vacío o luz encendida al dormir; si no se atiende en **15 min** cuenta como error. Error de disciplina: llamada falsa sin atender. Ambos se acumulan en cría y juvenil.

| Etapa | Cuándo | Resultado |
| --- | --- | --- |
| Huevo | 5 min | Babytchi |
| Babytchi | 65 min | Marutchi |
| Marutchi | 48 h | Tamatchi con 0–2 errores; Kuchitamatchi con 3+ |

Adultos: mejor cuidado → adulto más longevo (Mametchi 15–16 días con 0–2 errores y 0 de disciplina; Kuchipatchi 5–6 días con 3+ errores; Tarakotchi 3–4 días con muchos). Las fuentes discrepan en algunos umbrales.

**Digimon Ver.1:** error de cuidado a los 20 min de medidor vacío o luz encendida (si ambos vacíos a la vez cuenta 1); contadores (entrenamientos, sobrealimentación, interrupciones de sueño) **que se reinician en cada evolución**; forma de "fallback" (Numemon) si nada coincide. Color: temporizadores 10 min / 12 h / 24 h / 36 h / 48 h; llamadas caducan a los 10 min (corazones) o 1 h (luz).

**Se adopta:** errores de cuidado como eje de rama, ventana de atención antes de contar el error, contadores por etapa, forma de fallback simpática. **No se adopta:** muerte, longevidad ligada a la rama (aquí nada es irreversible), batallas.

## 5. Ojos y caras

| Proyecto | Licencia | Uso |
| --- | --- | --- |
| espp `expressive_eyes` (esp-cpp/espp, componente ESP-IDF) | **MIT** | 9 expresiones, `look_at`, parpadeo automático, transiciones mezcladas; **dibuja mediante callbacks**, se puede enchufar a un render propio. Candidata a referencia directa de la lógica de cara. |
| FluxGarage RoboEyes | **GPL-3.0** | Solo ideas: valores que se mueven a mitad de camino hacia el objetivo cada frame (`cur = (cur+next)/2`, 50 fps); párpados como triángulos del color del fondo (TIRED baja las esquinas exteriores, ANGRY las interiores, HAPPY tapa la mitad inferior); parpadeo cada 1 s + 0–4 s, sacadas cada 1 s + 0–3 s. |
| Lumo (Aymo/Lumo) | GPL-3.0 | Solo ideas de reacción (ver abajo). Mismo hardware familia (AMOLED 1.75). |
| esp32-eyes (Playful Technology) | GPL-3.0 | Solo referencia. |

**Reacciones de Lumo (como ideas):** toque → feliz 2,5 s; doble toque → sorpresa 2 s; spam de toques (4 ponderados en 1,5 s) → enfado 3 s e ignora el táctil; levantarlo (desviación ≥ 0,20 g sobre media lenta α 0,03, muestreo 50 Hz, enfriamiento 1,5 s) → susto 3,5 s; USB enchufado → amor 2 s; desenchufado → enfado; batería ≤ 20 % → cara de sueño por defecto; 5 min inactivo → duerme.

**Pwnagotchi** (GPL-3, ataca redes: solo ideas de caras y ánimos): ánimo por épocas (ciclos de escaneo): excited tras 10 activas, bored tras 15 inactivas, sad tras 25, angry al doble, lonely tras 5 sin interacción. Idea aprovechable: contar nutrición por ciclo de escaneo.

## 6. Otros proyectos con comida por señales

| Proyecto | Licencia | Notas |
| --- | --- | --- |
| Macagotchi (SpaceMonkeyAlfa) | MPL-2.0 | Pasivo; felicidad por **SSID únicos del día**, racha diaria, escaneo cada 30 s. Idea: novedad diaria y racha. |
| Dragotchi (Flipper Zero) | GPL-3 | Forrajeo por RSSI sub-GHz solo recepción; ciclo de cuidado completo; adulto por puntuación de cuidado (≥ 90 = inmortal); simulación offline. Buena referencia de diseño. |
| Palnagotchi (M5) | MIT | Solo se anuncia a la red pwngrid y cuenta amigos. |
| Minigotchi-ESP32, Sablina, Tamafi-CYD | GPL / sin licencia | Envían deauth o auditan seguridad: **activos**, descartados. |
| StackChan-Gotchi | sin claridad | ESP-IDF + LVGL; posible lectura de arquitectura. |
| tamago-ai | README MIT, sin LICENSE | Etapas huevo→adulto dibujadas con CSS box-shadow; concepto interesante, sin madurez. |
| SamFirn/hatch | todos los derechos | 211 criaturas definidas por código (siluetas ASCII + ojos/boca): demuestra que **silueta por código + capa de cara** sostiene un árbol evolutivo. No reutilizable. |

## 7. Assets y arte

### 7.1 Hallazgo principal
**No existe ningún pack con licencia libre que tenga la misma criatura en varias etapas con animaciones de cuidado.** Toda opción obliga a hacer la cara y la animación.

### 7.2 Packs revisados

| Pack | Licencia | Contenido | Veredicto |
| --- | --- | --- | --- |
| Openmon Set 1 / Set 3 (Screen Smith, itch) | CC0 | 64×64 estáticos; Set 1 con una línea inicial que parece 3×3 etapas (a verificar al descargar); Set 3 con una línea de acero de 6 | **Mejor CC0 con etapas.** Estilo Pokémon, sin animación ni caras. Referencia o placeholder. Evitar Set 5 y la "Wolf Line" (marcados como IA). |
| Shibu Front Sprites | CC0 | 19 sprites 64×64: base + una evolución por tipo | Encaja "una cría, varios adultos"; no hay juvenil. |
| Tiny Creatures (Clint Bellanger) | CC0 | 180 sprites 16×16 (arte real ~12×14), contorno de 2 px | Coherente, sin etapas ni caras. Para criaturas menores o iconos. |
| Tuxemon | código GPL-3; arte **por ítem** (CC-BY-SA 4.0 mayoría, CC0, PD, CC-BY; **13 NC**) | 412 hojas 64×64 con YAML de evolución | Único gran set con datos de evolución, pero estilo irregular y obliga a filtrar licencia y mantener atribuciones. |
| DawnLike 1.81 | CC-BY 4.0 | cientos de monstruos 16×16 con 2 frames, paleta DB16 | Coherente, pero de roguelike. |
| 0x72 DungeonTileset II, Chare, Blob, Hexany, Ninja Adventure | CC0 | varios | Bajo encaje (mazmorra, sin etapas); Blob (32×32) vale como referencia de "cría". |
| Kenney (Tiny Dungeon, 1-Bit, Monster Builder) | CC0 | tiles 16×16; Monster Builder: 178 partes vectoriales | Monster Builder es buena referencia de **sistema de partes combinables**. |
| megupets (OpenGameArt) | CC0 (uno con CC-BY dual) | criaturas con expresiones | Probablemente ilustración grande, no pixel art (sin verificar). |

**Descartados o con bandera:** Pipoya y Oryx (sin redistribución), akoro "Evolving Pixel Mons" (53 criaturas × 5 etapas, **de pago** y licencia ambigua; solo con leer su fichero de licencia), Worthypie Fakemon (asistido por IA, términos propios), packs de itch sin licencia (Mono Pets 16, Monster Raising misc, Tamacutechi: solo con confirmación escrita del autor), Tamaguino/ArduinoGotchi (GPL, o requieren ROM), oneko (dominio público, evitar los bitmaps de fan art).

### 7.3 Generadores procedurales

| Proyecto | Licencia | Idea |
| --- | --- | --- |
| zfedoran/pixel-sprite-generator (método de Dave Bollinger) | MIT | Plantilla 2D con celdas vacío/cuerpo/cuerpo-o-borde/borde siempre; se randomiza, se espeja, se contorna y se sombrea. ~200 líneas, **portable a C**. |
| ZzSprite (Frank Force) | MIT | Sprites simétricos por semilla; modo **mutate** (muta forma o color): encaja con "la criatura evoluciona". |
| Sprator (yurkth) | MIT | Ruido 4×8 + autómata celular + espejo + contorno. Útil para patrones de huevo y manchas. |
| Deep-Fold SpriteGenerator | MIT (código) | Generación de paletas y formas por autómata celular (GDScript). |
| Procedural Fakemon Engine, TiMoGen, IBOL | sin licencia o cerrados | Solo concepto (SDF + caras paramétricas; partes). |

### 7.4 Paletas (AMOLED)
Las paletas de color no tienen copyright efectivo. **Sweetie-16** (GrafxKid): su color más oscuro es un azul marino (no negro), por lo que sirve de contorno visible sobre fondo negro. PICO-8 incluye `#000000`, que desaparece sobre AMOLED. Regla: contorno de color oscuro o borde claro, nunca negro puro.

### 7.5 Herramientas y estado legal de la IA
Editores: Pixelorama (MIT), LibreSprite (GPL-2), Aseprite (EULA; ~20 $ o compilar). Todos exportan PNG + JSON convertibles a arrays C. Sobre arte generado por IA: la guía de la Oficina de Copyright de EE. UU. (29-01-2025) dice que los prompts solos no son autoría y el Supremo rechazó Thaler v. Perlmutter el 2026-03-02; en la práctica, el arte IA en bruto probablemente no es protegible. Si se usa IA, redibujar a mano y declararlo en el README. Si se encarga arte, exigir CC0 o CC-BY en contrato.

## 8. Qué se adopta (síntesis)

1. **Comida desde escaneos:** TamaFi reescalado (`comida = min(35, 2·redes + 3·fuertes)`, etc.) + novedad por BSSID (Catode32/Macagotchi) + tope diario. Escaneo cada 5–15 min en muñeca, no cada 8–15 s.
2. **Decisiones:** deseo (TamaFi) + penalización por recencia y azar en el mejor bin (Catode32).
3. **Velocidades de estadísticas:** por magnitud de cambio + amortiguación `^0,7` + recuperación por debajo de 25; salud como mezcla ponderada.
4. **Evolución:** errores de cuidado con ventana de atención de 15 min (P1), contadores reiniciados por etapa (Digimon), forma de fallback simpática; sin muerte.
5. **Tiempo ausente:** un único tick de simulación del hueco, con tope de 7 días, guardado por eventos + latido, blob que solo crece (pocket-tank).
6. **Cara y reacciones:** lógica propia estilo RoboEyes/espp (mitad de camino hacia el objetivo, párpados como máscaras), reacciones de Lumo, ánimos por época de Pwnagotchi.
7. **Arte:** máscaras a mano + semilla que muta por etapa + cara y animaciones por código, paleta Sweetie-16 adaptada. Packs CC0 solo como referencia/placeholder y para iconos.
