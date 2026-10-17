# Pet Design: especificación de producto

Estado: **lista para arrancar** (decisiones en `DECISIONS.md`). Sustituye al stub anterior y a `SPEC-pet.md`. Los números son **valores iniciales propuestos**, a validar con el simulador headless en la fase 1 (se marcan como *inicial*). Lo técnico está en `PET_TECH.md`, el arte en `STYLEGUIDE.md`, las decisiones pendientes en `DECISIONS.md`, y el porqué de cada mecánica en `RESEARCH.md`.

## 1. Objetivo

Una mascota virtual para el reloj que **se alimenta de las señales WiFi de su entorno**, evoluciona según su dieta y los cuidados, y **nunca muere**. En casa tiene una dieta estable pero repetitiva; al salir con el reloj descubre señales nuevas, y cómo es esa dieta a lo largo de su vida decide en qué se convierte.

Principios (decididos):
1. **Indulgente.** El descuido solo ralentiza el crecimiento, empeora el ánimo y orienta hacia formas torpes o raras, igual de simpáticas. Nada es irreversible.
2. **Crecimiento impulsado por el entorno.** La dieta de señales es el eje principal.
3. **Uso mixto** muñeca y mesa, con modos que el dispositivo detecta solo.
4. **Arte 100 % por código** (ver `STYLEGUIDE.md`).
5. **Evolución visible:** la criatura cambia de forma entre etapas y según la rama.

## 2. Alcance

### MVP (primera versión jugable)
- Una especie: huevo → cría → juvenil → adulto, con **dos adultos** según el eje de novedad.
- **Solo WiFi pasivo.** Despensa con bocados doméstico, exótico y urbano.
- Alimentación manual, caricia, ciclo de sueño con RTC, detección de modo mesa/muñeca, cara expresiva, partida persistente y versionada.
- Dos formas de instalarla: standalone o como app del Launcher (ver `PET_TECH.md`, sección 2).

### Fuera del MVP
BLE y eje WiFi/BLE completo, Señalpedia, herencia generacional, minijuegos, voz, componente social entre dispositivos, integración como esfera del Launcher, despertar al levantar la muñeca (ver 8).

## 3. Bucle principal

```
escaneo WiFi → bocados → DESPENSA → (jugador elige / picoteo autónomo) → mascota
                                         │
                      saciedad · ánimo · energía · puntos de crecimiento
                                         ↓
                       etapa (edad + puntos) y rama (dieta + cuidado)
```

## 4. Modelo de estadísticas

Tres capas, definidas por **magnitud de cambio** y no por temporizadores separados (Catode32). Valores 0–100 salvo que se diga otra cosa.

| Capa | Estadística | Comportamiento (*inicial*) |
| --- | --- | --- |
| Rápida | **Saciedad** | Baja ~8/h con la mascota despierta (de 100 a 0 en ~12 h). De noche, ×0,25. Sube al comer. |
| Rápida | **Energía** | Baja ~5/h despierta; sube ~12/h dormida. Con < 20 la mascota busca dormir. |
| Media | **Ánimo** | Baja ~2/h. Sube con caricias y comida (ver 6). Con saciedad < 25 baja el doble. |
| Media | **Despensa** | Capacidad 20 bocados; caducidad por tipo (ver 5). |
| Lenta | **Puntos de crecimiento (PC)** | Acumulados por comida y por días de visita (ver 7). |
| Lenta | **Eje de dieta** | `exóticos comidos / (exóticos + domésticos comidos)` durante cría y juvenil. |
| Lenta | **Errores de cuidado** | Contador por etapa; se reinicia en cada evolución (ver 7). |
| Lenta | **Fracción manual** | Bocados dados por el jugador / total comidos, en la etapa. |
| Personalidad | **Rasgos** (curiosidad, calma, glotonería, 0–9) | Sembrados al nacer; sesgan decisiones y la apariencia. |

**Amortiguación** (Catode32): toda ganancia se multiplica por `((100−v)/100)^0,7` y toda pérdida por `(v/100)^0,7`; por debajo de 25, las ganancias valen hasta ×3, para que recuperarse sea fácil.

## 5. Despensa y bocados

Cada escaneo no alimenta directamente: **llena la despensa**.

| Bocado | Origen | Sabor | Caducidad (*inicial*) |
| --- | --- | --- | --- |
| Doméstico | AP familiar visible | básico | 24 h |
| Exótico | AP nuevo | novedad | 48 h |
| Urbano | ≥ 15 redes o ≥ 3 abiertas en un escaneo (enfriamiento 30 min) | "ciudad" | 36 h |
| Social (BLE) | fase posterior | "social" | — |

**Familiaridad por BSSID** (Catode32): tabla de 32 familiares + 32 recientes, indexada por hash con sal del dispositivo (nunca BSSID ni SSID en claro). Por cada escaneo: +1 si visto, −0,25 si no, se elimina a ≤ 0, pasa a familiar con ≥ 5. Un AP es **nuevo** si no está en ninguna tabla.

**Bocados por escaneo** (*inicial*): domésticos `min(2, familiares visibles)`; exóticos `min(3, nuevos)`; urbano 0 o 1. **Tope diario de novedad:** 12 exóticos por día (evita el "farmeo" paseando junto a un bloque de pisos).

Si la despensa está llena se descarta primero el bocado más antiguo de tipo doméstico.

**Alimentar:** el jugador elige el tipo desde la despensa (decisión principal). Efecto de comer (*inicial*, antes de amortiguación):

| Bocado | Saciedad | Ánimo | PC |
| --- | --- | --- | --- |
| Doméstico | +12 | +1 | 0 |
| Exótico | +8 | +8 | +4 |
| Urbano | +10 | +4 | +3 |

**Picoteo autónomo:** si la saciedad < 30 y el jugador no actúa en 5 min, la mascota come el bocado más antiguo con **60 % de aprovechamiento** (saciedad, ánimo y PC × 0,6) y sin contar como bocado manual.

## 6. Ánimo, expresión y reacciones

**Escalera de ánimo** (TamaFi; gana la primera que cumpla): dormida → hambrienta (saciedad < 25) → cansada (energía < 20) → triste (ánimo < 40) → aburrida (sin señal nueva en 2 h estando despierta) → curiosa (señal nueva en el último escaneo) → feliz (ánimo ≥ 70) → calma.

**Reacciones** (con la pantalla encendida; ideas de Lumo/RoboEyes, código propio):

| Disparador | Reacción | Efecto |
| --- | --- | --- |
| Toque | feliz 2,5 s | ánimo +2 (máx. +10/h con amortiguación) |
| ≥ 4 toques en 1,5 s | mareo/enfado 3 s | sin ganancia mientras dure |
| Sacudida | sorpresa 2 s | sin ganancia; > 3 sacudidas por minuto: ánimo −1 |
| Levantar el reloj (IMU, pantalla ya encendida) | saludo | — |
| USB enchufado / desenchufado | ojos de corazón / puchero | solo estético |
| Batería ≤ 20 % | cara de sueño por defecto | solo estético |

**Motor de decisión** (cada 8–15 s en la escena; TamaFi + Catode32): cada comportamiento pasa una puerta, calcula un deseo (necesidad + rasgo/2 + bono de ánimo + azar), se resta la penalización por recencia `50 − 10·posición` (últimos 5), y se elige al azar dentro del mejor bin de 10. Comportamientos del MVP: reposar, curiosear, jugar/botar, pedir atención, mirar alrededor, picotear, dormir.

## 7. Evolución

### Etapas (ciclo objetivo ≈ 7 días)

| Etapa | Entra cuando (*inicial*) |
| --- | --- |
| Huevo | al instalar; eclosiona tras 10 min de uso activo |
| Cría | tras eclosionar |
| Juvenil | edad ≥ 36 h **y** PC ≥ 40 |
| Adulto | edad ≥ 6 días **y** PC ≥ 200 (desde juvenil) |

**Plazo máximo:** a los 14 días de edad evoluciona igualmente, para que nada se quede atascado.

### Puntos de crecimiento
- Comida: +PC según la tabla de 5, con **tope de 50 PC/día** por comida.
- **Visita diaria:** +10 PC por cada día con al menos una interacción o bocado.
- Máximo ≈ 60 PC/día: un jugador activo llega a adulto en ~6–7 días; uno casual, en ~10; quien solo hace una visita al día en casa, en ~14 (por el plazo máximo).

### Rama del adulto (MVP: dos adultos)
- **Adulto A «Hogareño»:** eje de dieta < 0,30.
- **Adulto B «Explorador»:** eje de dieta ≥ 0,30.
- La **calidad de cuidado** no cambia la rama, cambia la **variante** (mismo cuerpo, parámetros distintos):
  - **Pulida:** ≤ 2 errores en juvenil y ánimo medio ≥ 60.
  - **Torpe:** ≥ 4 errores en juvenil.
  - **Rara:** ánimo medio < 40.
  - Normal: el resto.
- Ejes posteriores (WiFi/BLE) añadirán ramas sin tocar estas.

### Error de cuidado
Una necesidad (saciedad o ánimo ≤ 5) que **sigue así 15 minutos de tiempo activo** sin respuesta del jugador cuenta 1 error (ventana de atención del Tamagotchi P1). Con la app cerrada o el reloj apagado (ausencia): **máximo 1 error por cada 12 h de ausencia y 2 por ausencia.** Los errores se reinician en cada evolución (Digimon).

## 8. Tiempo, sueño y ausencia

- **Noche** (RTC, hora local, sin zona horaria): 23:00–08:00 por defecto, ajustable. De noche no se escanea ni se come; si se enciende la pantalla (con botón) la mascota se muestra adormilada 10 s, sin penalización.
- **Tiempo ausente:** al arrancar se ejecuta **un único paso** que simula el hueco `dt = ahora − última marca` (tope 7 días; si el RTC perdió la hora, `dt = 0` y se avisa). Reglas (*inicial*):
  1. La despensa envejece y caduca.
  2. La mascota picotea de la despensa (aprovechamiento 50 %) por tramos de 1 h.
  3. La saciedad baja a ×0,35 con **suelo en 15**; el ánimo baja 1/h con **suelo en 35**; la energía se repone de noche.
  4. Errores de cuidado según la regla de ausencia.
  5. No hay PC de visita diaria sin interacción.
  - Resultado: una mascota hambrienta, pidiendo comida y algo triste; nunca peor que eso.
- **Despertar al levantar la muñeca:** depende de la interrupción del QMI8658 (GPIO21, sin validar en core) y de despertar la pantalla por táctil/IMU, que el Launcher evita. En el MVP se despierta con **botones**; la reacción de saludo funciona solo con la pantalla ya encendida.

## 9. Modos de uso

| | Mesa | Muñeca |
| --- | --- | --- |
| Detección | USB conectado (`watch_battery_t.usb`) **y** reloj quieto ≥ 60 s por IMU, con histéresis | todo lo demás |
| Escaneo WiFi | cada ~60 s *(a confirmar)* | cada 10–15 min o tras movimiento sostenido *(a fijar tras medir)* |
| Pantalla | escena completa con animaciones ricas; opción de modo atenuado con desplazamiento de píxeles | apagada por defecto; se enciende por botón; vuelve a apagarse por inactividad |
| Contra el quemado | desplazamiento de píxeles, sin elementos estáticos más de ~60 s | n/a |

**Al encender la pantalla en muñeca** (por botón) se ven la hora y la mascota, más **avisos de despensa**: bocados a punto de caducar, despensa llena o señal exótica nueva sin comer. Los avisos son visuales (icono sobre el HUD), sin sonido de noche.

La **duración de batería en muñeca es requisito de primer nivel**: las frecuencias de escaneo no se fijan hasta medir (fase 2, prueba inicial).

## 10. Interfaz

Pantalla vertical 410×502 con esquinas redondeadas (~100 px de radio, según pocket-tank): nada importante en las esquinas.

```
┌────────────────────────┐  y=0
│   [batería]  HH:MM  [despensa N]   │  HUD ~70 px
│                        │
│         ESCENA         │  criatura centrada
│   (criatura, comida)   │  (escala entera, ver PET_TECH)
│                        │
├────────────────────────┤
│  [Alimentar][Estado][⚙] │  bandeja ~120 px
└────────────────────────┘  y=502
```

- **Toque en la criatura:** caricia. **Deslizar arriba:** despensa. **Deslizar abajo:** estado (barras de saciedad, ánimo, energía; etapa y edad).
- **Despensa:** lista de bocados por tipo con cantidad y tiempo hasta caducar; tocar uno lo da a la mascota.
- **BOOT** = aceptar/acción principal (abre la despensa; confirma). **PWR corto** = atrás/menú; en la raíz y en modo app, vuelve al Launcher (convención de core, `ARCHITECTURE.md`, "Convencion De Botones").
- **Ajustes:** noche (inicio y fin), sonido, brillo, reiniciar mascota (con confirmación).
- Widgets con LVGL y escena de la criatura como imagen generada por `pet_render` (`DECISIONS.md`, T5).

## 11. Sonido
Pitidos y melodías cortas de onda cuadrada o triangular generadas en C (sin WAV), para: señal nueva descubierta, caricia, llamada de hambre. **Silenciable** y apagado de noche. Es lo último de la fase 2 y se recorta si complica el consumo (`DECISIONS.md`, R3).

## 12. Privacidad y datos
- Solo escaneo **pasivo**: nunca conectarse ni interactuar con redes ajenas.
- Solo **hashes con sal** del BSSID; nunca SSID ni MAC en claro en almacenamiento persistente, ni en logs.
- Partida versionada, con CRC y resistente a cortes (ver `PET_TECH.md`).

## 13. Criterios de aceptación (verificables en el simulador headless, fase 1)

| # | Criterio |
| --- | --- |
| A1 | Con dieta ≥ 60 % exótica durante juvenil, el adulto es B «Explorador» en el 100 % de 1.000 semillas; con dieta 0 % exótica, A «Hogareño» en el 100 %. |
| A2 | Un jugador activo (tres visitas al día, alimentando) llega a adulto entre los días 6 y 8; uno que solo hace una visita al día, no más tarde del día 14. |
| A3 | En una ausencia de 7 días, saciedad ≥ 15, ánimo ≥ 35 y errores añadidos ≤ 2 (suelos del apartado 8). |
| A4 | Los errores de cuidado nunca bajan de 0 ni se acumulan entre etapas; se reinician al evolucionar. |
| A5 | Ningún valor de la partida puede dejar a la mascota «muerta»: no existe ese estado. |
| A6 | No más de 12 bocados exóticos por día, aunque haya 100 AP nuevos. |
| A7 | El mismo escenario y la misma semilla producen exactamente el mismo resultado (determinismo). |
| A8 | Cargar una partida de la versión N−1 produce una partida válida (migración). Un guardado truncado se rechaza y se usa la copia anterior. |
| A9 | Ningún BSSID ni SSID en claro aparece en la partida serializada. |
| A10 | La rama no cambia con las variantes de cuidado: cambia solo la variante. |
