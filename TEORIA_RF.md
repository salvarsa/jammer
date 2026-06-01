# Teoría de RF aplicada al jammer

Documento técnico de apoyo al firmware. Explica qué es la banda de 2,4 GHz,
cómo conviven WiFi/Bluetooth/BLE en ella, qué mecanismos usan estos
protocolos para sobrevivir a las interferencias y por qué un jammer
basado en dos NRF24L01 consigue tumbarlos. Termina con la diferencia entre
nuestro montaje y un sistema multibanda profesional.

> Recordatorio legal — emitir energía en bandas reguladas interfiere con
> servicios de terceros. Este documento es educativo. Cualquier prueba
> práctica del firmware tiene que hacerse en jaula de Faraday o entorno
> RF aislado.

---

## 1. La banda ISM de 2,4 GHz

Una **banda** es un rango de frecuencias del espectro electromagnético.
La banda **ISM (Industrial, Scientific, Medical)** de 2,4 GHz va de
**2 400 MHz a 2 483,5 MHz** — 83,5 MHz de ancho — y está disponible para
uso sin licencia en casi todo el mundo, por eso conviven ahí WiFi,
Bluetooth, BLE, Zigbee, mandos de drones, microondas, etc.

```
2400 MHz  ─────────────────────────────────────────  2483,5 MHz
       ←───────────── banda ISM 2,4 GHz ─────────────→
```

Para que tantos sistemas coexistan, esa franja se divide en **canales**:
sub-frecuencias estrechas. Cada protocolo define su propio troceado.

---

## 2. Canales del NRF24L01

El NRF24L01 numera los canales del **0 al 125** y cada canal está separado
**1 MHz** del siguiente:

```
frecuencia (MHz) = 2 400 + N
```

- Canal 0 → 2 400 MHz
- Canal 45 → 2 445 MHz
- Canal 79 → 2 479 MHz
- Canal 125 → 2 525 MHz (ya fuera de la ISM oficial)

Cuando el firmware llama `radio.setChannel(45)` está escribiendo el valor
`45` al registro `RF_CH` del chip y el sintetizador interno mueve la
frecuencia portadora a 2 445 MHz.

En nuestro código usamos `CH_MAX = 80` → solo los canales 0-79 → la franja
2 400-2 479 MHz, que es donde están las víctimas interesantes.

---

## 3. Mapeo con WiFi, Bluetooth clásico y BLE

Cada protocolo trocea la misma banda a su manera:

### Bluetooth clásico — 79 canales × 1 MHz

```
canal BT  0  = 2 402 MHz   →  NRF24 ch 2
canal BT  1  = 2 403 MHz   →  NRF24 ch 3
...
canal BT 78  = 2 480 MHz   →  NRF24 ch 80
```

Mapeo casi 1:1 con el NRF24. Por eso los canales 0-79 del NRF24 cubren
toda la rejilla de Bluetooth clásico.

### Bluetooth Low Energy — 40 canales × 2 MHz

```
canal BLE  0 = 2 402 MHz  →  NRF24 ch 2
canal BLE  1 = 2 404 MHz  →  NRF24 ch 4
canal BLE 39 = 2 480 MHz  →  NRF24 ch 80
```

De esos 40 canales, **3 son de "advertising"** (37, 38, 39 — fijos en
2 402, 2 426 y 2 480 MHz), donde el dispositivo emite balizas para que lo
descubran. Los otros 37 son "data channels" usados durante una conexión.
Como los canales BLE son más anchos, **basta con pisar un canal NRF24
cercano** para inyectar ruido en el receptor BLE.

### WiFi — 14 canales × 20-22 MHz

```
WiFi ch  1  = 2 412 MHz  →  ocupa 2 401-2 423 MHz  → NRF24 ch  1-23
WiFi ch  6  = 2 437 MHz  →  ocupa 2 426-2 448 MHz  → NRF24 ch 26-48
WiFi ch 11  = 2 462 MHz  →  ocupa 2 451-2 473 MHz  → NRF24 ch 51-73
```

Cada canal WiFi es muy ancho (20 MHz). Tu NRF24 emite una rayita de 1 MHz
dentro de él, pero como WiFi necesita el ancho de banda completo para
decodificar, una sola rayita interfiriendo basta para tirarle paquetes.

### Visual conjunto

```
 2400      2412      2426      2437      2451      2462    2480 MHz
   │         │         │         │         │         │       │
   │ ┌───────┴────────┐│┌────────┴────────┐│┌────────┴──────┐│
   │ │   WiFi ch 1    │││   WiFi ch 6     │││   WiFi ch 11  ││
   │ └────────────────┘│└─────────────────┘│└───────────────┘│
   │                                                          │
   │ ▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏  │
   │   ↑ 79 canales BT clásico (1 MHz cada uno)              │
   │                                                          │
   │ ▏ ▏ ▏ ▏ ▏ ▏ ▏ ▏ ▏ ▏ ▏ ▏ ▏ ▏ ▏ ▏ ▏ ▏ ▏ ▏ ▏ ▏ ▏ ▏ ▏ ▏ ▏  │
   │   ↑ 40 canales BLE (2 MHz cada uno)                     │
   │                                                          │
   │ ▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏▏  │
   │   ↑ NRF24 ch 0-125 (1 MHz cada uno) ← tu jammer salta aquí
```

---

## 4. Mecanismos de robustez de los protocolos

WiFi, BT y BLE no son ingenuos: están diseñados sabiendo que la banda
ISM es ruidosa. Tres mecanismos clave que tu jammer tiene que vencer:

### 4.1. FHSS — Frequency Hopping Spread Spectrum

Bluetooth clásico **nunca se queda quieto**. Cambia de canal cada
**625 µs** → **1 600 saltos por segundo** sobre los 79 canales.

```
Tiempo:    0 µs    625 µs   1250 µs  1875 µs  2500 µs  3125 µs
Canal BT:   23  →    71  →    8   →    45  →    12  →    66
```

La secuencia parece aleatoria desde fuera pero **es determinista** para
los dos dispositivos involucrados — la calculan a partir de la
dirección MAC del maestro y su reloj interno. Como ambos extremos la
calculan igual, aterrizan en el mismo canal a la vez. Para un atacante
externo (sin esas claves) la secuencia parece pseudoaleatoria.

**Consecuencia para el jammer**: pisar un canal fijo es inútil — solo
estás en él 1/79 del tiempo (≈ 1,3 %). El jammer **también tiene que
saltar**, y más rápido que la víctima.

### 4.2. AFH — Adaptive Frequency Hopping

BLE (y BT 5+ en clásico) añade una capa más: si detecta que un canal
está sistemáticamente con interferencias, lo **saca de la rotación**.
Mantiene un mapa de "canales malos" y solo salta entre los buenos.

**Consecuencia**: si tu jammer pisa siempre el mismo subconjunto de
canales, AFH detecta cuáles son y los excluye → la víctima recupera
audio/datos en los canales restantes. La defensa es **distribuir el
ruido uniforme y aleatoriamente sobre toda la banda** para que AFH no
pueda "ahorrar" ningún canal.

### 4.3. Reintentos, CRC y ACK

Cada paquete RF lleva:

- **CRC** (Cyclic Redundancy Check): un checksum al final del paquete.
  El receptor lo recalcula sobre los bytes recibidos y si no coincide,
  descarta el paquete entero. El CRC detecta el ruido pero no lo corrige.
- **ACK** (Acknowledgement): el receptor manda un mini-paquete confirmando
  recepción correcta. Si el transmisor no recibe ACK en X milisegundos,
  asume pérdida.
- **Reintentos**: ante falta de ACK, el transmisor reenvía el paquete,
  hasta un máximo configurado (típico 3-5 intentos). Si supera ese
  máximo, sube el error a la capa superior.

**Consecuencia**: el jammer no necesita corromper el 100 % de paquetes.
Si corrompe el 60-70 %, los reintentos saturan el enlace, la latencia
se dispara y la conexión se cae por timeout aunque algunos paquetes
sí llegasen limpios.

---

## 5. Por qué funciona el jamming

Atacas el modelo de robustez de forma simultánea:

1. **Frente a FHSS**: saltas tú también, pero **decenas de miles de veces
   por segundo** vs. los 1 600 saltos/s del BT. En un slot de 625 µs del
   BT tu jammer ha barrido 20-30 canales aleatorios → probabilidad alta
   de pisar el que está usando.
2. **Frente a AFH**: distribuyes el ruido uniformemente sobre 0-79 con
   `random()` → ningún canal queda libre el tiempo suficiente para que
   AFH lo blanquee.
3. **Frente a CRC/ACK/retries**: emites **portadora continua** a máxima
   potencia (`startConstCarrier` con `RF24_PA_MAX`) → el receptor ve la
   relación señal/ruido (SNR) por debajo de su umbral de decodificación
   en el canal afectado → CRC falla → no manda ACK → el transmisor
   reintenta → vuelves a pisar → la conexión muere por timeout.

Es **brute-force**: no rompes el protocolo, simplemente metes más ruido
del que el receptor puede tolerar. Por eso necesitas proximidad (~10 m
con NRF24 PA+LNA estándar) — la víctima recibe el legítimo y a ti a la
vez, y tu señal tiene que dominar en su antena.

### La matemática del salto

Estática vs. dinámica:

| Estrategia jammer | Cobertura instantánea | Pérdida de paquetes BT |
|---|---|---|
| 1 NRF24 en canal 50 fijo | 1/79 ≈ 1,3 % | ~1 paquete de cada 79 (audio perfecto) |
| 1 NRF24 saltando 0-79 | 1/79 instantáneo, pero ×50 000 hops/s | ~70-80 % pérdidas |
| 2 NRF24 saltando 0-79 | 2/79 ≈ 2,5 % instantáneo | ~90 %+ pérdidas |
| 4 NRF24 saltando 0-79 | 4/79 ≈ 5 % instantáneo | ~95 %+ pérdidas |

Las dos primeras filas explican por qué **moverse rápido es más
importante que tener más radios**.

---

## 6. Cómo funciona nuestro jammer monobanda

Conceptualmente, el firmware hace cuatro cosas:

```
1. Apaga el WiFi y Bluetooth internos del ESP32 (compiten por la banda).
2. Inicializa los dos NRF24 (HSPI + VSPI) en modo "constant carrier" — 
   portadora continua, sin protocolo, a +20 dBm, en el canal 45.
3. En loop() infinito:
     - Cambia la radio HSPI a un canal aleatorio 0-79.
     - Cambia la radio VSPI a otro canal aleatorio 0-79.
     - Suma 1 al contador de hops.
     - delayMicroseconds(random(60)) → jitter para que el patrón no sea
       periódico.
4. En paralelo (sin bloquear el hop):
     - LED parpadea a 4 Hz como heartbeat visual.
     - OLED refresca cada 200 ms con canal, modo, hops y uptime.
```

El bucle es tan corto que el ESP32 a 240 MHz produce **>40 000 cambios
de canal por segundo en cada radio**. Sumando las dos, ~80 000 hops/s
sobre la banda completa.

La elección de **dos buses SPI hardware** (HSPI y VSPI) es lo que
permite que ambos NRF24 emitan **simultáneamente** sin compartir
SCK/MOSI/MISO. Si compartieran bus, solo uno podría estar en
configuración a la vez.

---

## 7. ¿Más radios = más ruido?

La respuesta corta: **sí, pero con rendimientos rápidamente decrecientes**.

### Lo que mejora con más radios

| Radios | Prob. de coincidir en un instante con un canal BT (de 79) |
|--------|------------------------------------------------------------|
| 1      | ≈ 1,3 %                                                    |
| 2      | ≈ 2,5 %                                                    |
| 4      | ≈ 5 %                                                      |
| 8      | ≈ 10 %                                                     |

Cuando multiplicas por la velocidad de salto (decenas de miles por
segundo), 2 NRF24 ya cubren la banda casi al 100 % en el slot de 625 µs
del BT. Pasar de 2 → 4 sube tasa de pérdidas del 90 % al 95 %. Pasar de
4 → 8 prácticamente no se nota.

### Lo que NO cambia con más radios

- **Potencia por canal**: cada NRF24 PA+LNA emite +20 dBm (100 mW).
  Cinco juntos no emiten más fuerte **en ningún canal** que uno solo —
  emiten en **más canales a la vez**. La SNR en la antena de la víctima
  para cada canal individual sigue siendo la misma.
- **Alcance**: como la potencia por canal no sube, la distancia útil
  tampoco.

### Los límites duros que vas a encontrar

**Buses SPI** — el ESP32 solo tiene HSPI y VSPI hardware. A partir del
tercer NRF24 hay que **compartir un bus** alternando los CSN, o usar
SPI bit-banged en GPIO sueltos (lento).

**Alimentación 3,3 V** — cada NRF24L01+PA+LNA tira ~115 mA en TX:

```
2 × PA+LNA = 230 mA   ← lo actual, OK con LDO del ESP32
4 × PA+LNA = 460 mA   ← borderline, sag de voltaje probable
6 × PA+LNA = 690 mA   ← seguro hunde el rail → I2C falla, OLED parpadea, ESP32 resetea
```

Pasar de 2 PA+LNA exige fuente externa (buck step-down a 3,3 V / 1 A
dedicado para las radios).

**Acoplo de antenas** — la longitud de onda a 2,4 GHz es ~12 cm, λ/4
≈ 3 cm. Si pegas 4 antenas SMA a menos de eso entre ellas, se acoplan
y distorsionan el patrón de radiación. Para 4+ radios hay que
separarlas físicamente.

### Lo que sí escala el ataque

| Objetivo | Solución correcta |
|---|---|
| Más canales cubiertos a la vez | Más radios (con alimentación y SPI resueltos) |
| Pegar más lejos (10 m → 30 m) | Amplificador 2,4 GHz externo + mejor antena |
| Pegar más fuerte en una dirección | Antena direccional (Yagi, patch, sector) |
| Más sucio en cada canal | Bajar `delayMicroseconds` a 0, subir velocidad SPI |

Doblar de 2 a 4 NRF24 quizá sube pérdidas del 90 % al 95 % — apenas
notable. Cambiar la antena SMA stock por una Yagi de 12 dBi duplica el
alcance de 10 m a 20 m — ese sí es un salto cualitativo.

---

## 8. Jammers multibanda: lo que hace un sistema profesional

Lo que tú haces con 2 NRF24 (pisar la banda 2,4 GHz) es **monobanda**.
Un jammer profesional o militar tiene que atacar simultáneamente
**muchas bandas distintas** porque los sistemas modernos usan varias
para diferentes funciones.

### Las bandas que cubre un equipo profesional

| Banda                  | Frecuencia        | Qué hay ahí                                  |
|------------------------|-------------------|----------------------------------------------|
| VHF                    | 30-300 MHz        | Walkies militares, FM, radios tácticas       |
| UHF                    | 300-1 000 MHz     | TETRA, P25, walkies, drones 433/915 MHz      |
| GPS L1 / L2 / L5       | 1,2-1,6 GHz       | Navegación por satélite                      |
| Celular                | 700 MHz - 2,7 GHz | GSM / 4G / 5G                                |
| WiFi/BT/BLE/Zigbee     | 2,4 GHz           | Lo único que cubre tu jammer                 |
| WiFi 5 / drones FPV    | 5-5,8 GHz         | Vídeo FPV, WiFi 5 GHz                        |
| Satcom / control drone | 8-12 GHz (X-band) | Enlaces satelitales                          |

Ejemplo concreto, un jammer anti-dron típico tiene que cubrir a la vez:

- 2,4 GHz → enlace de control del dron
- 5,8 GHz → vídeo FPV
- 1,5 GHz → GPS (para que el dron pierda navegación)
- 915 MHz / 433 MHz → telemetría

Si solo atacas 2,4 GHz, el dron mantiene GPS y vídeo y sigue siendo
útil.

---

## 9. Por qué los jammers militares usan varias antenas

Aquí está la diferencia clave con nuestro montaje: **no son más
"NRF24 en la misma banda" — son una antena distinta por banda, por
función o por sector**. Las razones, en orden:

### 9.1. Una antena por banda (físico inevitable)

Las antenas son **band-specific**. La longitud de onda manda el tamaño:

```
30 MHz   → λ = 10 m    → antena 2,5 m (un mástil)
433 MHz  → λ = 69 cm   → antena 17 cm
1,5 GHz  → λ = 20 cm   → antena 5 cm
2,4 GHz  → λ = 12,5 cm → antena 3 cm (tu SMA)
5,8 GHz  → λ = 5,2 cm  → antena 1,3 cm
```

Una antena de 17 cm pensada para 433 MHz alimentada con una señal de
2,4 GHz refleja la mayor parte de la potencia al transmisor en vez de
radiarla → eficiencia ridícula. Por eso un equipo serio tiene **un amp
+ una antena por banda**, no una "antena universal".

### 9.2. Diversidad de polarización

Las antenas emiten con polarización **vertical**, **horizontal** o
**circular**. Si tu víctima usa vertical y tú horizontal, pierdes
~20 dB de acoplo (factor 100×). Equipos serios emiten en **ambas** →
2 antenas por banda.

### 9.3. Sectores de cobertura (360° / direccional)

Una antena **omnidireccional** esparce energía en todas direcciones por
igual → bajo alcance lejano. Una **directiva** (Yagi, panel, sector)
concentra toda la potencia en un cono → mucho más alcance, pero solo
en esa dirección.

Solución típica: 6-8 antenas direccionales formando un anillo, cada
una cubriendo 60°-45° → cobertura 360° con la potencia concentrada.
Cada antena, su amplificador.

```
       N
   ↗   │   ↖
 ─┼────●────┼─ E      6 paneles direccionales = cobertura 360°
   ↘   │   ↙          a 30 m en lugar de una omni a 10 m
       S
```

### 9.4. MIMO / Beamforming

Múltiples antenas alimentadas con desfases controlados crean un **haz**
virtual que se puede apuntar electrónicamente sin mover nada físico.
Permite **concentrar energía sobre un objetivo concreto** (un dron, un
vehículo) sin afectar lo de alrededor. Requiere 4-16 antenas por banda
y mucho procesamiento DSP.

### 9.5. TX + RX simultáneo (cognitive jammers)

Jammers "inteligentes" escuchan primero para ver qué canales se están
usando y jamean solo esos. Necesitan antenas separadas de TX y RX
porque emitir y recibir a la vez en la misma antena requiere
duplexores carísimos.

### 9.6. Redundancia y modularidad

Equipos militares se diseñan para que si una antena falla por impacto,
las demás sigan operativas.

### Comparativa final

| | Nuestro jammer | Jammer militar IED (ej. Rohde & Schwarz) |
|---|---|---|
| Bandas atacadas | 1 (2,4 GHz) | 8-12 (de VHF a X-band) |
| Antenas | 2 | 12-24 |
| Potencia por antena | +20 dBm (100 mW) | +47 a +57 dBm (50-500 W) |
| Amplificador por antena | el PA integrado del NRF24 | PA dedicado de cientos de W |
| Alcance | ~10-15 m | 200-1 000 m |
| Coste | ~30 € | 100 000 - 500 000 € |

Las múltiples antenas en jammers militares no son "más radios en la
misma banda" — son una antena por banda de frecuencia, una por sector
geográfico, una por polarización y, en algunos casos, antenas separadas
para TX y RX. Imitar eso desde casa requiere amplificadores y módulos
SDR (HackRF, BladeRF) que ya entran en territorio de regulación
estricta y precios que escalan rápido.

---

## 10. Módulos para cubrir cada banda y conexión al ESP32

Si quisieras ampliar el proyecto a más bandas, esta es la cartografía
de qué módulo físico existe para cada una y si el ESP32 puede manejarlo
directamente.

### 10.1. Tabla resumen por banda

| Banda                   | Frecuencia        | Módulo hobbyista                      | Interfaz al MCU | ¿Conectable al ESP32?        | Potencia salida    |
|-------------------------|-------------------|---------------------------------------|-----------------|------------------------------|--------------------|
| HF                      | 3-30 MHz          | **Si5351** (generador de reloj)       | I2C             | Sí (sin modulación útil)     | ~5 dBm             |
| VHF                     | 30-300 MHz        | **Si4463**, RF4463PRO                 | SPI             | Sí                           | +10 a +20 dBm      |
| UHF baja                | 400-470 MHz       | **CC1101**, SX1278 (LoRa)             | SPI             | Sí                           | +10 a +12 dBm      |
| ISM 868/915 MHz         | 868/915 MHz       | **CC1101**, **SX1276** (LoRa)         | SPI             | Sí                           | +10 a +20 dBm      |
| GPS L1                  | 1 575 MHz         | (no hay módulo TX hobbyista)          | —               | No (requiere SDR + PC)       | —                  |
| Celular GSM/4G/5G       | 700-2 700 MHz     | (no hay módulo de jamming hobbyista)  | —               | No (requiere SDR + PC)       | —                  |
| **2,4 GHz ISM**         | 2 400-2 483 MHz   | **NRF24L01+PA+LNA** ← ya lo tienes    | SPI             | **Sí (HSPI + VSPI ya en uso)** | +20 dBm (100 mW) |
| 5,8 GHz ISM (FPV)       | 5 725-5 875 MHz   | **TS5828**, **TX5258** (módulos FPV)  | UART (config)   | Sí (TX continua, no jam fácil) | +20 a +27 dBm    |
| WiFi 5 GHz banda IEEE   | 5 150-5 850 MHz   | (no hay módulo barato)                | —               | No (requiere SDR)            | —                  |
| X-band                  | 8-12 GHz          | (material profesional)                | —               | No                           | —                  |

### 10.2. Módulos detallados que sí se conectan al ESP32

**CC1101 (Texas Instruments)** — el "todoterreno sub-GHz"

- Cubre 300-348 MHz, 387-464 MHz y 779-928 MHz vía registros configurables.
- Interfaz: **SPI** (4 líneas + CE) — idéntica al NRF24.
- Modulaciones: ASK/OOK, 2-FSK, GFSK, 4-FSK, MSK.
- Tiene modo "TX continuous unmodulated" análogo al `startConstCarrier`
  del NRF24 → sirve para jamming sub-GHz.
- Precio: ~3 € por módulo en AliExpress.
- Cómo lo conectarías: **compartir bus SPI con un NRF24** alternando los
  CSN. Pines típicos: SCK/MOSI/MISO comunes, CSN dedicado, GDO0 a GPIO
  para interrupción.

**SX1276 / SX1278 (Semtech, "LoRa")**

- SX1278 → 137-525 MHz. SX1276 → 137-1020 MHz (la versión "global").
- Interfaz: **SPI** + 6 líneas DIO (interrupts).
- Modulaciones: LoRa, FSK, OOK. Modo CW (continuous wave) disponible.
- Útil si quieres jamming + capacidad LoRa decente (mensajes legítimos).
- Precio: ~6 € por módulo.

**Si4463 / Si4432 (Silicon Labs)**

- Cubre 119-1 050 MHz.
- Interfaz: **SPI** + GPIO de interrupción.
- Menos común que el CC1101 en proyectos open-source, pero más
  flexible en modulaciones.

**Si5351 (Silicon Labs)** — generador de reloj de hasta 200 MHz

- En realidad **no es un transceiver** — es un PLL que genera ondas
  cuadradas en frecuencias arbitrarias.
- Interfaz: **I2C**.
- Sus armónicos llegan hasta ~800 MHz y se han usado en proyectos como
  "jammer ultra-cutre HF/VHF" con un filtro LPF a la salida.
- Limitaciones: salida muy débil (~5 dBm), señal cuadrada muy sucia
  (todos los armónicos impares). Solo aprendizaje, no operativo.

**TS5828 / TX5258 (módulos FPV de 5,8 GHz)**

- Pensados para transmitir vídeo analógico a gafas FPV.
- Interfaz: **UART** para configurar canal y potencia; potencia a +20
  o +27 dBm.
- Emiten una portadora modulada en FM; para "jamming puro" lo más
  cercano es activarlos en TX continuo sobre un canal y desplazarlo
  desde el ESP32.
- **Útil pero limitado**: no tienen modo "constant carrier sin
  modulación" como el NRF24 → la calidad del jam es peor.

### 10.3. Lo que NO se conecta al ESP32: los SDR

Para GPS, celular, 5 GHz WiFi y X-band no hay módulos hobbyistas
"plug-and-play". La solución profesional es un **SDR (Software Defined
Radio)**: un mezclador + DAC/ADC de banda ancha controlado por software.

| SDR                  | Cobertura TX         | Potencia salida | Interfaz  | ¿ESP32?  |
|----------------------|----------------------|------------------|-----------|----------|
| **HackRF One**       | 10 MHz - 6 GHz       | 10-15 dBm        | USB 2.0   | No       |
| **LimeSDR Mini 2.0** | 10 MHz - 3,5 GHz     | 10 dBm           | USB 3.0   | No       |
| **BladeRF xA4**      | 47 MHz - 6 GHz       | 8 dBm            | USB 3.0   | No       |
| **PlutoSDR**         | 325 MHz - 3,8 GHz    | 7 dBm            | USB 2.0   | No       |
| **USRP B210**        | 70 MHz - 6 GHz       | 10 dBm           | USB 3.0   | No       |

**¿Por qué no se conectan al ESP32?**

- Necesitan **USB host** (no device): el ESP32-S2/S3 sí tienen USB OTG,
  pero los SDR exigen drivers Linux complejos, GNU Radio o libuhd,
  varios MB/s de ancho de banda USB sostenido y procesamiento DSP en
  el host (FFT, modulación). El ESP32 no tiene CPU ni memoria para eso.
- El ESP32 clásico solo tiene UART-USB (CH340 / CP2102) — ni siquiera
  es USB de verdad.
- Conclusión: **un SDR exige un PC con Linux** (Raspberry Pi para algo
  portátil). El ESP32 no entra en este tier.

### 10.4. ¿Qué pasa si encadeno varios SPI al ESP32?

Aquí está el detalle técnico que define hasta dónde puedes llegar
"solo con módulos SPI":

**Buses SPI hardware del ESP32**

- **HSPI** (host SPI) — la usamos para el NRF24 #1.
- **VSPI** (vendor SPI) — la usamos para el NRF24 #2.
- Cada bus puede tener **múltiples dispositivos** compartiendo
  SCK/MOSI/MISO, distinguidos por CSN (chip-select). El ESP32 maneja
  hasta 3 CSN por bus por hardware, más por software.

**Por lo tanto** podrías encadenar, por ejemplo:

```
HSPI  ── NRF24 (2.4 GHz)
     └── CC1101 (433 MHz)
     └── SX1276 (915 MHz)

VSPI  ── NRF24 (2.4 GHz)
     └── Si4463 (VHF)
```

Es decir, **un ESP32 podría manejar 6-8 transceivers sub-GHz + 2,4 GHz
simultáneamente** con buenos `chip select`. Limitaciones:

1. **Alimentación 3,3 V** — cada PA+LNA tira 100 mA. Pasa rápido del
   medio amperio → necesitas buck step-down externo de 1-2 A.
2. **Concurrencia** — solo un dispositivo SPI puede estar
   recibiendo/transmitiendo comandos en un instante por bus. Pero los
   transceivers en "constant carrier" emiten **sin necesidad de SPI**,
   así que mientras unos emiten otros se reconfiguran. Es viable.
3. **Software** — librerías RF24, CC1101 (ELECHOUSE_CC1101) y LoRa son
   independientes y compatibles. La complejidad del firmware sube,
   pero no de forma explosiva.
4. **Antenas** — una por banda y módulo. Si pegas un CC1101 de 433 MHz
   a la antena de 2,4 GHz del NRF24, no radia nada útil.

### 10.5. Un proyecto "multibanda casero" realista

Si quisieras construir un equivalente "multibanda" controlado solo por
ESP32, sin SDR, dentro de lo viable:

```
ESP32 ─┬─ HSPI ─┬─ NRF24L01+PA+LNA  → antena 2,4 GHz omni
       │        └─ CC1101 #1         → antena 433 MHz monopolo
       │
       ├─ VSPI ─┬─ NRF24L01+PA+LNA  → antena 2,4 GHz omni
       │        └─ SX1276            → antena 868 MHz dipolo
       │
       ├─ I2C  ── OLED (estado)
       │
       └─ UART ── TX5258 5,8 GHz FPV → antena 5,8 GHz patch
```

Cubrirías:
- 2,4 GHz (BT/WiFi/BLE) — los dos NRF24.
- 433 MHz (drones telemetría, mandos) — el CC1101.
- 868 MHz (LoRa, sub-GHz IoT) — el SX1276.
- 5,8 GHz (FPV vídeo) — el TX5258.

Quedarían fuera GPS (1,5 GHz) y celular (700-2 700 MHz fragmentado) —
para esos solo hay SDR.

Coste aproximado del paquete: ~50-70 € en módulos. Más una fuente
externa de 3,3 V / 2 A. **Y mucha más complejidad legal**: con un solo
NRF24 ya estás en banda ISM regulada; con 4 bandas distintas estás
emitiendo en frecuencias asignadas a servicios concretos (aviación,
drones civiles, IoT comercial) y cualquier prueba al aire libre se
detecta y multa rápido.

### 10.6. Resumen ESP32-céntrico

| Lo que quieres jamear           | Cómo                                                 | ¿ESP32 solo? |
|---------------------------------|------------------------------------------------------|--------------|
| WiFi 2,4 / BT / BLE             | 2× NRF24 vía HSPI+VSPI                               | **Sí (ya lo haces)** |
| Mandos garage, sensores 433     | + CC1101 en SPI                                     | Sí           |
| LoRa, IoT 868/915 MHz           | + SX1276/SX1278 en SPI                              | Sí           |
| Walkies VHF marítimo/aviación   | + Si4463 en SPI (ilegal en esas bandas)             | Sí, pero NO  |
| FPV vídeo 5,8 GHz               | + módulo FPV TX por UART                            | Sí (parcial) |
| GPS L1                          | Requiere HackRF + PC                                | **No**       |
| Celular GSM/4G/5G               | Requiere HackRF/LimeSDR + PC                        | **No**       |
| Satcom / X-band                 | Material profesional                                | **No**       |

El ESP32 te lleva hasta el tier 2 (módulos transceiver SPI cubriendo
sub-GHz + 2,4 GHz + algo de 5,8 GHz). El tier 3 (bandas reguladas,
GPS, celular) exige SDR sobre Linux y queda fuera de cualquier
microcontrolador.

---

## Referencias

- [Bluetooth Core Specification 5.4](https://www.bluetooth.com/specifications/specs/core-specification-5-4/) — secciones de FHSS, canales y AFH.
- [IEEE 802.11 — WiFi PHY layer](https://standards.ieee.org/standard/802_11-2020.html) — distribución de canales y anchura.
- [Nordic NRF24L01+ datasheet](https://www.nordicsemi.com/Products/NRF24L01P) — registros, modos y `startConstCarrier`.
- [FCC Part 15 / ETSI EN 300 328](https://www.etsi.org/) — regulación de la banda ISM 2,4 GHz.
