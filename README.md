# ESP32 2.4 GHz Jammer (dual NRF24 + OLED)

> **AVISO LEGAL** — Emitir ruido en 2.4 GHz interfiere con WiFi/Bluetooth y es ilegal
> en prácticamente todos los países (UE: Directiva RED; ES: Ley 9/2014; EE.UU.: FCC §333).
> Este firmware se publica para uso educativo y pruebas en **entorno RF controlado**
> (jaula de Faraday, cámara anecoica o laboratorio con licencia).

Basado en el proyecto `smoochiee/Ble-jammer` (GPL-3.0), adaptado para el cableado
indicado y ampliado con OLED de estado y LED.

## Hardware

| Componente              | Bus       | Pines ESP32                              |
|-------------------------|-----------|------------------------------------------|
| NRF24L01 #1 + 10µF      | HSPI      | CE=16, CSN=15, SCK=14, MOSI=13, MISO=12  |
| NRF24L01 #2 + 10µF      | VSPI      | CE=22, CSN=21, SCK=18, MOSI=23, MISO=19  |
| OLED 0.96" SSD1306      | I2C       | SDA=4, SCL=5, 3V3, GND                   |
| LED estado (azul) + 4k7 | GPIO      | GPIO27 → R → LED+ ; LED- → GND           |
| TP4056 + Li-Ion + JST   | Alim.     | OUT+ → switch → 3V3 ; OUT- → GND         |

El condensador de 10 µF va **directamente entre VCC y GND** de cada NRF24 (cuidado
con la polaridad).

## Funcionamiento

- Al encender (slide switch del TP4056) el jammer arranca **siempre activo**.
- Los dos NRF24 emiten una portadora constante (`startConstCarrier`) y saltan de
  canal aleatoriamente entre 0 y 79 (cubre BT clásico, BLE y WiFi 1-14).
- El LED parpadea a ~4 Hz como heartbeat.
- La OLED muestra: canal actual de cada radio, modo, número de saltos y tiempo
  encendido.

## Build / Flash (PlatformIO)

```bash
# instalar PlatformIO Core (una vez)
pip install platformio

# compilar
pio run

# subir al ESP32 (mantén pulsado BOOT si no entra solo)
pio run -t upload

# monitor serie
pio device monitor
```

Las librerías (`RF24`, `Adafruit SSD1306`, `Adafruit GFX`) se descargan
automáticamente la primera vez que ejecutas `pio run`.

## Ficheros

- [platformio.ini](platformio.ini) — configuración del entorno y dependencias.
- [include/pins.h](include/pins.h) — mapeo de pines en un solo sitio.
- [src/main.cpp](src/main.cpp) — bucle principal: inicializa HSPI+VSPI+I2C,
  apaga las radios internas del ESP32 y hace hop aleatorio + refresco de OLED.

## Notas

- Si la OLED no se enciende, prueba la dirección `0x3D` (algunos clones la usan).
- Si solo arranca una radio (`HSPI FAIL` o `VSPI FAIL` en la pantalla), revisa
  los condensadores: la fuente 3.3 V del ESP32 casi nunca aguanta dos NRF24 sin
  desacoplo cerca del módulo.
- Para limitar a banda BLE, cambia `CH_MAX` en `src/main.cpp` a `40`.
