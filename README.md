# ESP32 2.4 GHz Jammer v1 (dual NRF24 + OLED)

> **AVISO LEGAL** — Emitir ruido en 2.4 GHz interfiere con WiFi/Bluetooth y es ilegal
> en prácticamente todos los países (UE: Directiva RED; ES: Ley 9/2014; EE.UU.: FCC §333).
> Este firmware se publica para uso educativo y pruebas en **entorno RF controlado**
> (jaula de Faraday, cámara anecoica o laboratorio con licencia).

Basado en el proyecto `smoochiee/Ble-jammer` (GPL-3.0), adaptado para el cableado
indicado y ampliado con OLED de estado y LED de heartbeat.

---

## 1. En qué consiste el dispositivo

Es un **jammer (interferidor) de banda 2,4 GHz** construido alrededor de un ESP32 y
**dos módulos de radio NRF24L01+PA+LNA**. Su único trabajo es inundar de ruido la
banda ISM de 2,4 GHz para degradar o tumbar los enlaces que viven ahí: **WiFi
2,4 GHz, Bluetooth clásico y Bluetooth Low Energy (BLE)**.

La idea de fondo es sencilla: las dos radios emiten una **portadora continua a máxima
potencia** y **saltan de canal miles de veces por segundo** de forma aleatoria sobre
toda la banda. El receptor de la víctima ve su relación señal/ruido (SNR) caer por
debajo del umbral de decodificación, falla el CRC, no manda ACK, el transmisor
reintenta, vuelve a chocar con el ruido y la conexión muere por timeout. No se rompe
ningún protocolo: es **fuerza bruta de RF**.

Una pantalla OLED muestra el estado en tiempo real (canal de cada radio, modo, número
de saltos y uptime) y un LED parpadea como heartbeat para confirmar que el firmware
sigue vivo.

> Para la teoría completa (cómo coexisten WiFi/BT/BLE, qué son FHSS/AFH/CRC y por qué
> el jamming los vence), ver el documento de apoyo: [TEORIA_RF.md](TEORIA_RF.md).

---

## 2. Componentes

| Componente                  | Función                                                    | Cantidad |
|-----------------------------|------------------------------------------------------------|----------|
| ESP32 DevKit (`esp32dev`)   | Microcontrolador a 240 MHz; controla las radios y la OLED | 1        |
| NRF24L01+**PA+LNA**         | Transceptor 2,4 GHz; emite la portadora de jamming         | 2        |
| Condensador 10 µF           | Desacoplo VCC-GND de cada NRF24 (estabiliza el rail en TX) | 2        |
| OLED 0.96" I2C (SH1106/SSD1306) | Pantalla de estado                                     | 1        |
| LED (azul) + resistencia 4k7| Heartbeat visual (~4 Hz)                                    | 1 + 1    |
| Módulo TP4056               | Cargador Li-Ion + protección                               | 1        |
| Batería Li-Ion + conector JST | Alimentación portátil                                    | 1        |
| Slide switch                | Interruptor de encendido a la salida del TP4056            | 1        |

**Notas de componentes:**

- Es imprescindible la variante **PA+LNA** del NRF24 (+20 dBm / 100 mW), no la versión
  pelada de corto alcance.
- La mayoría de OLED I2C baratas montan el controlador **SH1106** (no SSD1306). El
  firmware usa por defecto la clase `U8G2_SH1106_128X64_NONAME_F_HW_I2C`; si tu pantalla
  sale desplazada ~2 px o en blanco, cambia a la variante `SSD1306` (ver [src/main.cpp](src/main.cpp#L13)).
- El condensador de 10 µF va **directamente entre VCC y GND** de cada NRF24, lo más
  cerca posible del módulo y **respetando la polaridad**.

---

## 3. Conexiones pin a pin

El ESP32 usa sus **dos buses SPI hardware** (HSPI y VSPI), uno por cada NRF24, para que
ambas radios emitan **simultáneamente** sin compartir líneas. El mapeo vive en un solo
sitio: [include/pins.h](include/pins.h).

### NRF24L01 #1 — bus HSPI

| Señal NRF24 | Pin ESP32 | `pins.h`     |
|-------------|-----------|--------------|
| SCK         | GPIO 14   | `HSPI_SCK`   |
| MISO        | GPIO 12   | `HSPI_MISO`  |
| MOSI        | GPIO 13   | `HSPI_MOSI`  |
| CSN         | GPIO 15   | `HSPI_CSN`   |
| CE          | GPIO 16   | `HSPI_CE`    |
| VCC         | 3V3       | —            |
| GND         | GND       | —            |

### NRF24L01 #2 — bus VSPI

| Señal NRF24 | Pin ESP32 | `pins.h`     |
|-------------|-----------|--------------|
| SCK         | GPIO 18   | `VSPI_SCK`   |
| MISO        | GPIO 19   | `VSPI_MISO`  |
| MOSI        | GPIO 23   | `VSPI_MOSI`  |
| CSN         | GPIO 21   | `VSPI_CSN`   |
| CE          | GPIO 22   | `VSPI_CE`    |
| VCC         | 3V3       | —            |
| GND         | GND       | —            |

### OLED 0.96" — bus I2C

| Señal OLED | Pin ESP32 | `pins.h`           |
|------------|-----------|--------------------|
| SDA        | GPIO 4    | `OLED_SDA`         |
| SCL        | GPIO 5    | `OLED_SCL`         |
| VCC        | 3V3       | —                  |
| GND        | GND       | —                  |
|            | dir. I2C  | `OLED_ADDR = 0x3C` |

### LED de estado y alimentación

| Elemento     | Conexión                                              | `pins.h`  |
|--------------|-------------------------------------------------------|-----------|
| LED (ánodo)  | GPIO 27 → resistencia 4k7 → LED+ ; LED- → GND         | `LED_PIN` |
| Alimentación | TP4056 OUT+ → slide switch → 3V3 ; OUT- → GND         | —         |

Cada NRF24 lleva además su condensador de 10 µF entre **VCC y GND** (ver §2).

> **Importante sobre alimentación:** dos NRF24L01+PA+LNA pueden tirar ~230 mA en TX.
> El desacoplo cercano (los 10 µF) es lo que evita el *sag* de tensión que cuelga la
> OLED o resetea el ESP32. Pasar de 2 radios exige fuente externa dedicada (ver
> [TEORIA_RF.md §7](TEORIA_RF.md)).

---

## 4. Teoría: la banda de 2,4 GHz y cómo interfiere el jammer

### 4.1. La banda ISM de 2,4 GHz

La banda **ISM (Industrial, Scientific, Medical)** de 2,4 GHz va de **2 400 MHz a
2 483,5 MHz** (83,5 MHz de ancho) y es de uso libre sin licencia en casi todo el mundo.
Por eso conviven en ella WiFi, Bluetooth, BLE, Zigbee, mandos de drones y hasta los
hornos microondas. Para coexistir, la franja se divide en **canales** y cada protocolo
trocea la banda a su manera:

- **Bluetooth clásico** — 79 canales de 1 MHz.
- **Bluetooth Low Energy (BLE)** — 40 canales de 2 MHz (3 de ellos de *advertising*).
- **WiFi 2,4 GHz** — 14 canales muy anchos (20-22 MHz), solapados.

### 4.2. Los canales del NRF24

El NRF24L01 numera sus canales del **0 al 125**, separados 1 MHz:

```
frecuencia (MHz) = 2 400 + N      →   ch 0 = 2 400 MHz ,  ch 79 = 2 479 MHz
```

El firmware limita el salto a `CH_MAX = 80` → canales **0-79** → la franja
2 400-2 479 MHz, que es justo donde están las víctimas (WiFi 1-14, BT clásico y BLE).
Un canal NRF24 mapea casi 1:1 con un canal de Bluetooth clásico, y como los canales
BLE y WiFi son más anchos, basta con pisar **un** canal NRF24 cercano para meter ruido.

### 4.3. Por qué funciona el jamming

Los protocolos no son ingenuos —están diseñados para una banda ruidosa— pero el jammer
ataca sus tres mecanismos de defensa a la vez:

1. **Frente a FHSS** (Bluetooth salta 1 600 veces/s): el jammer **también salta**, pero
   muchísimo más rápido —decenas de miles de saltos por segundo por radio—, así que en
   cada slot del BT ya ha barrido 20-30 canales aleatorios.
2. **Frente a AFH** (los protocolos sacan de rotación los canales malos): se **distribuye
   el ruido uniforme y aleatoriamente** sobre los 80 canales con `random()`, de modo que
   ningún canal queda libre el tiempo suficiente para que AFH lo "ahorre".
3. **Frente a CRC / ACK / reintentos**: se emite **portadora continua a máxima potencia**
   (`startConstCarrier` con `RF24_PA_MAX`). El receptor ve la SNR por debajo de su umbral,
   el CRC falla, no manda ACK, el transmisor reintenta, vuelve a chocar → la conexión cae
   por timeout.

Es fuerza bruta: no se rompe el protocolo, simplemente se mete más ruido del que el
receptor tolera. Por eso hace falta **proximidad** (~10-15 m con NRF24 PA+LNA estándar):
tu señal tiene que dominar en la antena de la víctima.

> El desarrollo completo (matemática del salto, AFH, comparativa con jammers multibanda
> profesionales y cómo ampliar a otras bandas) está en [TEORIA_RF.md](TEORIA_RF.md).

---

## 5. Qué se puede hacer con el dispositivo actualmente

El firmware **v1** que hay en este repo hace lo siguiente:

- **Arranca siempre activo.** Al encender (slide switch del TP4056) empieza a jamear de
  inmediato; no hay botón de start/stop ni modos seleccionables.
- **Jamming continuo de 2,4 GHz** sobre los canales 0-79 (2 400-2 479 MHz), cubriendo
  WiFi 1-14, Bluetooth clásico y BLE simultáneamente con las dos radios.
- **Salto de canal aleatorio** en ambas radios en cada iteración del `loop()`, con un
  *jitter* de `delayMicroseconds(random(60))` para que el patrón no sea periódico
  (>40 000 saltos/s por radio).
- **Apaga el WiFi y el Bluetooth internos del ESP32** al arrancar (`killEspRadios()`)
  para que no compitan por la banda.
- **OLED de estado** que refresca cada 200 ms: título, canal actual de cada radio (o
  `FAIL` si esa radio no inicializó), modo (`rand 0-79`), contador de saltos (`Hops`) y
  uptime.
- **LED heartbeat** parpadeando a ~4 Hz como señal de vida.
- **Log por serie** a 115200 baudios con el estado de arranque de cada radio.

### Ajustes rápidos disponibles (editando el código)

- **Limitar a banda BLE/BT estrecha:** cambiar `CH_MAX` en [src/main.cpp](src/main.cpp#L20)
  (p. ej. `40`).
- **Cambiar el controlador de la OLED:** alternar entre las clases `SH1106` y `SSD1306`
  en [src/main.cpp](src/main.cpp#L13).
- **Probar otra dirección I2C:** algunos clones usan `0x3D` en vez de `0x3C`
  ([include/pins.h](include/pins.h#L22)).

### Lo que **no** hace (todavía)

- No tiene interfaz de control (web/botones) ni selección de modos en caliente.
- No escucha el espectro (no es un jammer *cognitivo* / reactivo): emite a ciegas.
- Es **monobanda**: solo 2,4 GHz. No toca sub-GHz, GPS, celular ni 5,8 GHz (ver
  [TEORIA_RF.md §8-10](TEORIA_RF.md) para cómo se ampliaría).

---

## 6. Build / Flash (PlatformIO)

El proyecto se compila con **PlatformIO** sobre el framework Arduino-ESP32. La
configuración está en [platformio.ini](platformio.ini).

```bash
# instalar PlatformIO Core (una sola vez)
pip install platformio

# compilar el firmware del jammer
pio run

# subir al ESP32 (mantén pulsado BOOT si no entra en modo flash solo)
pio run -t upload

# abrir el monitor serie (115200 baudios)
pio device monitor
```

### Entornos definidos

- **`esp32dev`** (por defecto) — el firmware del jammer. Excluye `i2c_scan.cpp`.
- **`scan`** — utilidad para escanear el bus I2C y descubrir la dirección de la OLED:

  ```bash
  pio run -e scan -t upload && pio device monitor
  ```

### Dependencias

Se descargan automáticamente la primera vez que ejecutas `pio run` (declaradas en
`lib_deps` de [platformio.ini](platformio.ini)):

- `nrf24/RF24` — control de los módulos NRF24L01.
- `olikraus/U8g2` — driver de la pantalla OLED.

---

## 7. Ficheros

- [platformio.ini](platformio.ini) — configuración de entornos y dependencias.
- [include/pins.h](include/pins.h) — mapeo de pines en un solo sitio.
- [src/main.cpp](src/main.cpp) — bucle principal: inicializa HSPI+VSPI+I2C, apaga las
  radios internas del ESP32 y hace hop aleatorio + refresco de OLED.
- [src/i2c_scan.cpp](src/i2c_scan.cpp) — sketch auxiliar (entorno `scan`) para detectar
  la dirección I2C de la OLED.
- [TEORIA_RF.md](TEORIA_RF.md) — documento técnico de apoyo: teoría de RF, FHSS/AFH/CRC,
  matemática del jamming y comparativa con sistemas multibanda profesionales.

---

## 8. Notas y resolución de problemas

- **OLED no enciende:** prueba la dirección `0x3D` o usa el entorno `scan` para
  detectarla. Si sale desplazada, cambia la clase del controlador (SH1106 ↔ SSD1306).
- **Solo arranca una radio** (`HSPI ch: FAIL` o `VSPI ch: FAIL` en pantalla): casi
  siempre es alimentación/desacoplo. Revisa los condensadores de 10 µF y el cableado
  3V3/GND del módulo afectado.
- **El ESP32 se resetea o la OLED parpadea:** el rail de 3,3 V no aguanta las dos radios
  en TX; acerca/refuerza el desacoplo o usa una fuente externa.
- **Para limitar la banda:** baja `CH_MAX` en [src/main.cpp](src/main.cpp#L20).
