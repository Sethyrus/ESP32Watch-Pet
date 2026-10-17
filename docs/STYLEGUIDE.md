# Style Guide: arte inicial

Estado: **aprobada en C0–C4 (2026-10-17).** Referencia visual en `docs/art/approved/`. Es la guía contra la que el agente revisa las hojas de contacto. Se ajustará con lo que se vea en pantalla. Antecedentes y fuentes en `RESEARCH.md` (sección 7); mecánicas en `PET_DESIGN.md`.

## 1. Enfoque

Arte **100 % generado por código**, sin assets de terceros:
- **Siluetas pixel art dibujadas a mano como máscaras en C**, una por etapa/rama (ver 3). Controlan la forma con precisión; es la parte más artesanal y la que más importa.
- **Una semilla por mascota** decide familia de color, patrón, estilo de orejas/antena y rasgos; se conserva en toda la vida, de modo que cada etapa se reconoce como *la misma criatura*.
- **Cara y animaciones por código**: ojos, boca y movimiento son transformaciones, no frames dibujados.
- **Iconos pequeños** (comida, interfaz): bitmaps de paleta escritos a mano y convertidos a arrays de C.
- Packs CC0 (Openmon, Tiny Creatures, Kenney) solo como **referencia visual** o placeholder; no se redistribuyen en el repo salvo decisión expresa (`DECISIONS.md`).

## 2. Paleta (AMOLED)

Base **Sweetie-16** (GrafxKid, vía Lospec; las paletas de color no tienen copyright efectivo). El fondo es **negro puro `#000000`** (índice 0, transparente): apaga los píxeles y ahorra consumo.

| Índice | Color | Uso |
| --- | --- | --- |
| 0 | `#000000` | fondo (no se usa para contornos) |
| 1 | `#1a1c2c` | ojos, sombra profunda |
| 2 | `#5d275d` | contorno coral |
| 3 | `#b13e53` | cuerpo oscuro coral |
| 4 | `#ef7d57` | cuerpo coral |
| 5 | `#ffcd75` | claro cálido |
| 6 | `#a7f070` | claro menta |
| 7 | `#38b764` | cuerpo menta |
| 8 | `#257179` | contorno/sombra menta |
| 9 | `#29366f` | contorno cielo |
| 10 | `#3b5dc9` | cuerpo cielo oscuro |
| 11 | `#41a6f6` | cuerpo cielo |
| 12 | `#73eff7` | claro cielo |
| 13 | `#f4f4f4` | brillos, blanco |
| 14 | `#94b0c2` | gris azulado claro |
| 15 | `#566c86` | gris azulado |
| 16 | `#333c57` | contorno gris |

(Son 16 colores del original + el negro de fondo; la indexación de implementación se fija en la fase 0.)

**Reglas AMOLED:**
- **Nunca contorno negro**: sobre fondo negro desaparece. El contorno es un tono oscuro *coloreado* de la rampa de la criatura (comprobado automáticamente: sombra y base ≥ 3:1 contra negro; el contorno oscuro de Sweetie solo llega a ~1,9:1 (coral, cielo, niebla), así que se exige ≥ 1,8:1 y la silueta la sostienen sombra y base — hallazgo de la fase 0).
- Fondos negros predominantes. Nada estático durante mucho tiempo (ver 8).
- El color de la rama **no** cambia el tono: el tono lo da la semilla. La rama cambia forma y partes.

**Rampas por familia** (4 tonos: contorno, sombra, base, claro), elegida por la semilla:

| Familia | Contorno | Sombra | Base | Claro |
| --- | --- | --- | --- | --- |
| Coral | `#5d275d` | `#b13e53` | `#ef7d57` | `#ffcd75` |
| Menta | `#257179` | `#38b764` | `#a7f070` | `#f4f4f4` |
| Cielo | `#29366f` | `#3b5dc9` | `#41a6f6` | `#73eff7` |
| Niebla | `#333c57` | `#566c86` | `#94b0c2` | `#f4f4f4` |

## 3. Rejilla, tamaños y máscaras

- **Lienzo de la criatura: 48×48 lógicos**, escala entera ×6 (288×288 px en pantalla). Un único tamaño de píxel en toda la escena; la criatura crece ocupando más rejilla.
- **Tamaño de máscara propuesto por etapa** (px lógicos / px en pantalla):

| Etapa | Máscara | En pantalla |
| --- | --- | --- |
| Huevo | 20×24 | 120×144 |
| Cría | 24×24 | 144×144 |
| Juvenil (pera) | 32×32 | 192×192 |
| Adulto (A y B) | 40×40 | 240×240 |

- **Formato de máscara:** cadenas ASCII en C, **medias máscaras espejadas** (la mitad del trabajo y simetría gratis), con códigos al estilo Bollinger:

| Código | Significado |
| --- | --- |
| `.` | vacío |
| `B` | cuerpo siempre |
| `b` | cuerpo o vacío, según la semilla |
| `O` | borde siempre |
| `E` | ancla de los ojos |
| `M` | ancla de la boca |
| `F` | ancla de una parte (orejas, antena, cola) |

- **Una pieza por estilo de parte** (orejas ×3, patrón ×3, cola ×2…) y etapa, no una criatura entera por semilla. Estimación: **~20–30 piezas pequeñas**, no cientos de frames.
- **Mutación entre etapas:** la semilla fija *ids de rasgos* (familia, patrón, estilo de oreja/antena); cada etapa dibuja esos rasgos a su escala. Eso mantiene la identidad (la idea de «mutar» de ZzSprite, sin depender de celdas que cambian de tamaño).
- **Pase de contorno** automático de 1 px alrededor del cuerpo (los píxeles `O` forzados lo completan); sombreado por rampa (arriba claro, abajo sombra) y un brillo opcional.

## 4. Ramas y variantes (MVP)

- **Adulto A «Hogareño»:** forma redondeada, baja, asentada (orejas caídas o pequeñas; rasgos de «casa»: acogedor).
- **Adulto B «Explorador»:** forma más alta y erguida, con parte distintiva (antena/alas pequeñas; rasgos de «señal»).
- Las dos comparten semilla, tono y rasgos heredados del juvenil: se reconocen como el mismo linaje.
- **Variantes de cuidado** (mismo cuerpo, parámetros distintos; ver `PET_DESIGN.md` sección 7). Todas deben seguir siendo simpáticas:

| Variante | Cómo se ve |
| --- | --- |
| Pulida | brillo extra en el cuerpo, destello ocasional |
| Normal | sin cambios |
| Torpe | una oreja/parte caída (media máscara de la otra mitad sustituida), ojos de distinto tamaño o desplazados 1 px, postura inclinada, tropiezos ocasionales |
| Rara | desplazamiento de tono dentro de la familia, manchas extra, mirada soñadora |

## 5. Cara

- **Ojos y boca son una capa aparte** colocada en las anclas `E` y `M` de la máscara; por eso las expresiones son coherentes en todas las formas.
- Ojos de 3–5 px de ancho en lógicos. Ejemplos (X = `#1a1c2c`, H = `#f4f4f4`):

```
abierto      medio       cerrado    feliz ^^    triste
.XX.         ....        ....       .XX.        XXXX
XXXX         ....        ....       X..X        .XX.
XHXX         XXXX        XXXX       ....        ....
XXXX         XXXX        ....       ....        ....
.XX.         ....        ....       ....        ....
```

- **Expresiones del MVP** (combinación de ojos + boca + pequeño adorno): neutral, feliz, triste, sueño, hambre, sorpresa, enfado, amor. Cada expresión debe diferir de las demás en **≥ N píxeles** (umbral a fijar en fase 0; comprobación automática).
- La lógica de suavizado (cada valor se mueve a mitad de camino hacia su objetivo en cada paso) sigue la idea de RoboEyes y de `expressive_eyes` (MIT), con código propio.

## 6. Animación (transformaciones, no frames)

Cuantizadas a ~80 ms (12,5 pasos/s) para conservar el aire pixel art:

| Animación | Definición (*inicial*) |
| --- | --- |
| Respirar | squash vertical de 1 px en el tercio superior, periodo 2,4 s |
| Parpadear | cada 1–5 s aleatorio, 120 ms |
| Botar | desplazamiento en y parabólico de 6 px lógicos, 0,6 s; squash de 1 px al caer |
| Comer | boca abre/cierra 3 veces en 1,2 s; el bocado se encoge |
| Dormir | ojos cerrados, respiración de 3,6 s, «Z» que sube cada 2 s |
| Triste | ojos caídos, cuerpo 1–2 px más bajo, píxel de lágrima |
| Hambre | temblor de 1 px en el vientre, mirada al icono de comida |
| Mirar | desplazamiento de los ojos de 1 px hacia un objetivo (sacadas cada 1–4 s) |

## 7. Iconos y texto
- Iconos de comida (doméstico, exótico, urbano), huevo, despensa, batería: **bitmaps de paleta 16×16 / 24×24 de 3 tonos**, escritos a mano y convertidos a arrays de C (script en `tools/`).
- Texto de interfaz con fuentes LVGL (la UI es LVGL, `DECISIONS.md` T5). Para coherencia pixel art se puede convertir una fuente pixel a formato LVGL con `lv_font_conv`; se decide en C0.

## 8. Contra el quemado
- Desplazamiento de toda la escena de 1–3 px de pantalla cada ~60 s.
- Ningún elemento de HUD fijo más de ~60 s en modo mesa; atenuar tras 10 s.
- Brillo máximo reducido en modo mesa; fondo negro.

## 9. Bucle de iteración visual (fase 0)

1. El código de dibujo está en `pet_render` y compila igual para PC y firmware.
2. Un único comando genera las **hojas de contacto** y la tira de frames:
   - **Hoja de estados:** filas = etapa × rama × variante; columnas = expresiones. Escala real de pantalla, fondo negro, rejilla visible opcional, etiquetas.
   - **Tira de animación:** una animación en N pasos consecutivos.
   - Salida en `sheets/` (no versionada); el nombre incluye la semilla y el commit.
3. El agente modifica, regenera, revisa **la hoja contra esta guía** y repite, con **un máximo de 5 iteraciones por ronda**.
4. Tras cada ronda se presenta la hoja al usuario en un **punto de control**; las hojas aprobadas se guardan en `docs/art/approved/` como referencia de estilo para comparar en iteraciones futuras. Si tras 3 rondas un punto de control no se aprueba, el agente pide una referencia visual o dirección concreta en lugar de seguir a ciegas.

### Revisión automática (antes de mirar la hoja)
- Sin píxeles huérfanos (salvo brillos marcados), contorno continuo de 1 px.
- Contraste contra negro: contorno ≥ 1,8:1, sombra y base ≥ 3:1.
- ≤ 16 colores por criatura; negro solo como fondo.
- Anclas de cara dentro del cuerpo; la máscara completa cabe en su tamaño de etapa.
- Distinción mínima entre expresiones.

### Revisión visual (agente y usuario)
1. La silueta se lee de un vistazo al 25 % de tamaño.
2. Es reconociblemente la misma criatura en todas las etapas de un mismo linaje.
3. Las dos ramas adultas son distinguibles sin la cara.
4. Las variantes «torpe» y «rara» siguen siendo simpáticas.
5. Las expresiones se leen sin etiqueta.
6. El movimiento se lee en la tira (respirar, botar, comer, dormir).

### Puntos de control
| | Entregable |
| --- | --- |
| C0 | Esta guía + hoja de moodboard (siluetas candidatas de huevo y cría) + paleta aprobada |
| C1 | Cría y juvenil de una especie, con la cara completa |
| C2 | Los dos adultos y sus variantes |
| C3 | Animaciones básicas en tira |
| C4 | Hoja completa de referencia: **primer entregable de arte** = guía + criatura en sus 4 etapas, ≥ 2 ramas adultas y animaciones básicas |

## 10. Cómo regenerar las hojas (fase 0)
`cd tools/sim && make check` (comprobaciones automáticas) y `make sheets` (escribe `sheets/c0_board.png`, `sheets/c0_faces.png` y `sheets/c0_legend.txt`, no versionados). El código de dibujo es `components/pet_render` (C99, mismo fuente para PC y firmware).
