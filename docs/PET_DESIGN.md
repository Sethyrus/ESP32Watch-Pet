# Pet Design v2: especificación de producto

Estado: **v2 en desarrollo** (2026-10-18). Sustituye al diseño v1 (en el historial de git), que resultó simple, poco claro y dependiente de llevar el reloj encima: el WiFi era la única fuente de comida y nada de lo que hacía el jugador quitaba el aburrimiento. Las decisiones están en `DECISIONS.md` (R16 en adelante); lo técnico, en `PET_TECH.md`; el arte, en `STYLEGUIDE.md`. Los números son iniciales y se calibran con los bots de `tests/` (sección 13).

## 1. Pilares

1. **Cada visita tiene algo que hacer** (de 30 s a 2 min) y una recompensa visible: monedas, experiencia, una reacción.
2. **Se juega con la criatura**, no se la mira: minijuegos protagonizados por ella y gestos directos (acariciar, limpiar, darle de comer).
3. **Cada necesidad tiene una acción clara que la arregla** y un icono que dice qué pide.
4. **Progresión visible:** nivel, monedas, evolución con pistas, álbum de formas, accesorios.
5. **Se puede jugar entera en casa.** El WiFi añade extras y colección, nunca es obligatorio.
6. **Indulgente:** nunca muere; el descuido solo ralentiza y cambia la variante adulta.

## 2. Bucle

```text
visita ─► mira la burbuja (qué pide) ─► cuida: comer · asear · dormir · caricias
   │                                          │
   └► minijuego (diversión + monedas + entrenamiento) ◄┘
                │
     monedas ─► comida, medicina, accesorios
     experiencia ─► nivel ─► evolución (edad + nivel)
     entrenamiento ─► rama del adulto      cuidado ─► variante del adulto
     WiFi (de fondo) ─► señales: monedas, chuches, accesorios, álbum
```

Largo plazo: adulto ─► **nueva generación** (huevo nuevo; se conservan monedas, accesorios, señales y álbum) ─► completar el álbum de formas.

## 3. Necesidades

Cuatro barras de 0 a 100, siempre visibles con su icono bajo la hora.

| Necesidad | Baja (despierta) | Se arregla con |
| --- | --- | --- |
| **Comida** | 9/h; de noche ×0,25 | Comer (sección 4) |
| **Diversión** | 8/h; de noche, nada | Jugar (minijuegos), caricias (poco), pastel, deseos |
| **Higiene** | 3/h + 6/h por cada caca en el suelo; de noche 1/h | Asear (quita las cacas y baña) o tocar cada caca |
| **Energía** | 4/h; −8 por minijuego | Dormir: de noche siempre; de día, siesta de 30 min |

- **Cacas:** la comida llena la tripa; con la tripa ≥ 60 y ≥ 90 min desde la última, sale una caca (máximo 4). Más o menos una caca cada dos comidas. Tocar una la limpia: +1 moneda, +2 de higiene, +2 XP.
- **Malito:** con higiene ≤ 10 o ≥ 3 cacas durante 60 min activos se pone malito. Malito: las ganancias de comida y diversión valen la mitad, la energía baja ×1,5 y no puede jugar. Se cura con **Medicina** o solo a las 12 h. Cuenta un error de cuidado.
- **Error de cuidado:** comida, diversión o higiene ≤ 5 durante 30 min activos (o malito). Ausencias: como v1, máximo 1 error por cada 12 h y 2 por ausencia.
- **Ánimo** (la cara) se deriva de las necesidades: su media, con suelo en la peor. Ya no es una barra propia.

**Burbuja:** sobre la criatura aparece el icono de lo que más necesita, por este orden: malito (medicina) → comida < 25 → higiene < 25 o ≥ 2 cacas (jabón) → energía < 20 (luna) → diversión < 25 (pelota) → deseo activo (estrella con su icono). Tocar la burbuja abre lo que arregla eso.

## 4. Monedas y comida

Monedas (●). Se empieza con 20.

| Gana | Cuánto |
| --- | --- |
| Minijuego | 2–15 según la puntuación |
| Deseo cumplido | 5 + nivel (máx. 20) |
| Primera visita del día | 5 + 2 por día de racha (máx. +10) |
| Limpiar una caca | 1 |
| Subir de nivel | 10 |
| Señales WiFi nuevas | 1 por señal (máx. 15/día) |

**Comer** muestra la carta: tocar un plato lo compra y se lo da en un solo gesto. Las chuches de señal (WiFi) y la medicina comprada se guardan en el inventario.

| Plato | Precio | Comida | Diversión | XP | Tripa |
| --- | --- | --- | --- | --- | --- |
| Pan | 3 | +25 | 0 | 2 | 30 |
| Fruta | 5 | +15 | +5 | 3 | 15 |
| Guiso | 8 | +45 | +5 | 4 | 45 |
| Pastel | 10 | +10 | +25 (la mitad si ya comió pastel hoy) | 4 | 25 |
| Chuche de señal | solo WiFi | +20 | +15 | 15 | 20 |
| Medicina | 20 | cura el malito | — | 2 | 0 |

- **Plato favorito** (uno de los cuatro, fijado por la semilla): +10 de diversión y +2 XP extra, con cara de amor. Se descubre al probarlo y queda en el álbum.
- Con comida ≥ 95 no quiere comer («Está lleno»). De noche duerme y no come.

## 5. Minijuegos

Duran de 20 a 40 s, se juegan con el dedo y la criatura es la protagonista. Cuestan 8 de energía y no se puede jugar con menos de 10 de energía, malito o de noche. Rendimiento 0–100 por partida:

- Diversión: +15 + 25 × rendimiento/100.
- XP: 10 + rendimiento/10.
- Monedas: 2 + 13 × rendimiento/100.
- Entrenamiento de su atributo: 2 + rendimiento/10.

| Juego | Atributo | Desde | Mecánica |
| --- | --- | --- | --- |
| **Atrapa** | Agilidad | nivel 1 | 30 s. La criatura sigue al dedo por abajo; caen comida (+1), estrellas (+3) y rayos (−2 y aturde 0,6 s). La velocidad sube con el tiempo. |
| **Memoria** | Ingenio | nivel 1 | Cuatro botones de colores. La criatura enseña una secuencia que crece desde 3 y hay que repetirla. Termina al fallar o al llegar a 10. |
| **Pesas** | Fuerza | nivel 2 | Tocar rápido para levantar la pesa antes de que se caiga; 5 pesas cada vez más pesadas, 6 s cada una. |

Al terminar, una tarjeta con la puntuación, el récord y lo ganado, y los botones «Otra vez» y «Salir».

## 6. Deseos

Cada 60–150 min despierta, la criatura pide algo concreto durante 2 h: un plato, un juego, caricias (3), que la asees (solo si hay cacas o higiene < 60) o ver una señal nueva (solo si ha visto alguna en los últimos 3 días). Cumplirlo da monedas, 15 XP y +10 de diversión. Si caduca no pasa nada. El primer deseo llega 10 min después de nacer.

## 7. Progresión

### Nivel

XP de todo lo que se hace (tablas de arriba, más caricias +1 hasta 5/h, asear +3, visita diaria +10, señal nueva +1). Para pasar del nivel n al n+1 hacen falta 40 × n XP. El nivel desbloquea juegos y accesorios de la tienda.

### Evolución

| Etapa | Entra cuando |
| --- | --- |
| Huevo | al empezar; eclosiona tras 2 min con la pantalla encendida (se agrieta antes) |
| Cría | al eclosionar |
| Juvenil | nivel ≥ 3 y edad ≥ 12 h (forzado a los 3 días) |
| Adulto | nivel ≥ 6 y edad ≥ 3 días (forzado a los 10 días) |

**Rama del adulto = lo que más ha entrenado** de cría y juvenil:

- Agilidad → **Saltarín** (el antiguo «Explorador»).
- Ingenio → **Sabio** (cabezón, con antena).
- Fuerza → **Forzudo** (ancho, con brazos).
- Si apenas ha jugado (entrenamiento total < 25) → **Hogareño**.

**Variante = cómo se le ha cuidado de juvenil:** Pulido (≤ 1 error y ánimo medio ≥ 52), Torpe (≥ 4 errores), Raro (ánimo medio < 30), Normal (el resto). Con los bots: el jugador activo saca Pulido, el casual Normal y el abandono Raro. Cuatro ramas por cuatro variantes dan **16 formas** para el álbum.

### Generaciones y álbum

Con el adulto de ≥ 3 días, «Nueva generación»: el adulto se va de viaje (nunca muere) y deja un huevo con otra semilla. Se conservan monedas, accesorios, señales y álbum. El **álbum** guarda las formas conseguidas (siluetas para las que faltan), los platos probados y los récords.

### Accesorios

Se compran en la Tienda (Más → Tienda) o se ganan con señales, y se ponen o quitan en el Armario; solo uno a la vez. Se dibujan por código sobre la cabeza.

| Accesorio | Precio | Desde |
| --- | --- | --- |
| Lazo | 60 | nivel 2 |
| Flor | 80 | nivel 3 |
| Gorro | 110 | nivel 4 |
| Gafas | 150 | nivel 5 |
| Chistera | 220 | nivel 7 |
| Corona | 400 | nivel 10 |
| Radar | — | 30 señales |
| Salacot | — | 100 señales |

## 8. WiFi: el olfato

El WiFi pasa de ser la comida a ser **exploración opcional**. Mientras la app está activa (también con la pantalla apagada), la criatura «olfatea» las redes de alrededor; v1 seguía igual en pasivo, con hashes con sal y sin SSID.

- Cada **señal nueva** (red nunca vista, con la tabla de familiares de v1) suma al contador de señales de por vida y da +1 moneda y +1 XP (máximo 15 al día). Cada una tiene 1/6 de dejar una **chuche de señal**.
- Un escaneo con muchas redes (≥ 15, o ≥ 3 abiertas) da una chuche extra (enfriamiento 30 min).
- **Hitos de señales:**
  - 10 → 25 monedas.
  - 30 → Radar.
  - 60 → 3 chuches.
  - 100 → Salacot.
  - 200 → 100 monedas.
- En casa las redes son siempre las mismas: apenas da nada, y no hace falta.

## 9. Interacción en la escena

- La criatura **pasea**, salta, mira y se sienta sola; si necesita algo se para mirando al jugador con la burbuja.
- **Tocarla** = caricia: corazones, +3 de diversión (hasta 15/h). Cuatro toques seguidos la marean.
- **Tocar una caca** la limpia con un destello.
- **Tocar la burbuja** = atajo a lo que pide.
- Reacciones a la carga USB (amor) y a la batería baja, como v1.
- **Siesta:** la criatura duerme 30 min, la energía sube +40 y se puede despertar tocándola.

## 10. Interfaz

```text
┌──────────────────────────────┐
│            HH:MM             │  hora
│  ⚡80%      Nv 3      ● 125   │  batería, nivel, monedas
│  🍖▓▓▓  🎾▓▓░  🫧▓▓▓  🌙▓░░   │  4 necesidades
│                              │
│   ESCENA 400×280 (criatura   │
│   paseando, cacas, burbuja)  │
│                              │
│ [Comer][Jugar][Asear][Dormir][Más] │  bandeja con iconos
│        ▓▓▓▓▓▓░░░ XP          │  barra de experiencia
└──────────────────────────────┘
```

- **Más:** Estado (necesidades con números, edad, atributos, pistas de evolución), Tienda, Armario, Álbum y Ajustes (noche, velocidad del tiempo de prueba, reiniciar).
- **Gestos:** deslizar hacia arriba abre Comer y hacia abajo, Estado.
- **Botones:** BOOT abre Comer (o «Otra vez» en un juego). PWR vuelve atrás: desde la raíz, al Launcher o, en standalone, apaga la pantalla.
- Avisos y recompensas en un rótulo sobre la escena («+8 ●», «¡Nivel 4!», «Ha evolucionado»).

## 11. Tiempo, sueño y ausencia

- **Noche** (23–08 por defecto, ajustable): duerme; no come ni juega; no hay escaneos.
- **Pantalla apagada = ausencia.** El reloj pasa casi todo el día con la pantalla apagada y el motor solo avanza con ella encendida; al encenderla se simula el hueco de una vez. Por eso los ritmos «despierta» de la sección 3 solo valen mientras se mira, y el ritmo real del día lo marca la ausencia: comida al 60 %, diversión e higiene al 50 %.
- **Ausencia** (un paso al volver, con tope de 7 días):
  - Suelos de comida 15, diversión 20 e higiene 10.
  - Como mucho 3 cacas.
  - La energía se repone de noche.
  - No enferma estando ausente.
  - Errores de cuidado como en v1.
  - Nunca peor que «con hambre, aburrido y sucio».

## 12. Privacidad

Igual que v1: escaneo pasivo y solo hashes con sal; nunca SSID ni MAC.

## 13. Cómo se prueba (R17)

El desarrollo no depende de llevar el reloj durante días:

1. **Bots de jugador** en `tests/`: simulan semanas en segundos con perfiles (activo, casual, solo en casa, abandono) y comprueban los criterios de abajo.
2. **Minijuegos deterministas** con bots que juegan bien, regular o al azar: puntuaciones en rango y dificultad creciente.
3. **Simulador de PC** (`tools/uisim`): la interfaz real con LVGL. Por guion saca capturas PNG que el agente revisa; también tiene un modo interactivo con ratón.
4. **Velocidad del tiempo** en Ajustes (×1, ×10, ×60) para ver un ciclo de vida en el reloj en horas, no en días.
5. **En el reloj, pruebas cortas** (5–10 min) solo de lo que no se puede simular: tacto, fluidez y sensación. El consumo se lee del diario horario (R14) sin pruebas dedicadas.

## 14. Criterios de aceptación

| # | Criterio |
| --- | --- |
| B1 | Jugador activo (3 visitas/día con juegos): juvenil el día 1–2, adulto el día 4–5. Casual (1 visita/día): adulto el día 7–10. Abandono (1 visita cada 3 días): adulto el día 10 (forzado). |
| B2 | Solo en casa (sin señales nuevas), un jugador activo llega igual de rápido que uno que sale: el WiFi no decide el ritmo. |
| B3 | Jugar sobre todo a un juego da su rama en ≥ 95 % de las semillas; sin jugar, Hogareño. Las 4 ramas son alcanzables. |
| B4 | Economía: un jugador activo nunca se queda sin poder comer y al tercer día tiene para ≥ 1 accesorio (≥ 60 monedas). En 14 días gana más o menos lo que cuestan todos los accesorios. |
| B5 | Ausencia de 7 días: comida ≥ 15, diversión ≥ 20, higiene ≥ 10, cacas ≤ 3, no enferma, errores añadidos ≤ 2. |
| B6 | No existe estado de muerte; toda necesidad tiene una acción que la sube. |
| B7 | Determinismo: misma semilla y mismo guion dan el mismo resultado. |
| B8 | Migración: una partida v2 (la del reloj) carga como v3 válida, conserva edad, etapa y rama, y convierte la despensa en inventario. Un guardado truncado se rechaza. |
| B9 | Ningún BSSID ni SSID en claro en la partida. |
| B10 | Minijuegos: un bot perfecto saca rendimiento medio ≥ 90 (mínimo ≥ 60), uno al azar ≤ 30 de media, y ninguna partida pasa de 45 s. |

### Estado (`make -C tests test`, 2026-10-18)

Todo en verde (4.964 comprobaciones del motor y 1.807 de los juegos, también con ASan/UBSan). Bots, 200 semillas por perfil:

| Perfil | Juvenil | Adulto | Variante | Monedas día 3 |
| --- | --- | --- | --- | --- |
| Activo (3 visitas, 2 partidas) | día 1 | día 4–5 | Pulido | ≥ 186 |
| Activo, siempre en casa | día 1 | día 4–5 | Pulido | ≥ 186 |
| Casual (1 visita, 1 partida) | día 2–4 | día 8–11 | Normal | ≥ 48 |
| Abandono (1 visita cada 3 días) | día 4 | día 11 (forzado) | Raro | ≥ 38 |

Jugar sobre todo a un juego da su rama el 100 % de las veces; sin jugar, Hogareño el 100 %.
