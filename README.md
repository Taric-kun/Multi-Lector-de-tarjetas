# Multi-Lector-de-tarjetas
Configuraciones lector de tarjetas para "placa CI"

## HID_Reloj

Firmware para la placa GV_CableCedula v2.0 (ATmega328P). Lee un lector HID por Wiegand y envía el número de la tarjeta a un reloj control ZKTeco como Wiegand 26.

### Conexiones

| Señal | Pin de la placa |
|---|---|
| Lector DATA0 (verde) | J3 pin 5 (A4) |
| Lector DATA1 (blanco) | J3 pin 6 (A5) |
| Lector GND / +V | GND / +12 V del reloj |
| Reloj DATA0 / DATA1 | J1, D8 / D9 |

### DIP switch

* Todos en OFF: envía el número impreso de la tarjeta (ej. 6136, 8958).
* Switch 1 en ON: envía la cantidad de bits de la tarjeta (26, 35...).

Formatos: 26 bits (H10301, número = bits 9..24) y 35 bits (HID Corporate 1000, número = bits 14..33). Otros largos envían sus 24 bits bajos.

### Cargar el firmware

1. Desconectar el lector y el reloj de la placa.
2. Conectar el programador USBtiny al ICSP, directo a un puerto USB 2.0.
3. Ejecutar `Cargar HID_Reloj.bat`.

`HID_Reloj.hex` está rellenado con ceros hasta completar la última página de 128 bytes; sin ese relleno el USBtiny cortaba la grabación al 96 %.

### Compilar desde el código

`firmware/HID_Reloj/HID_Reloj.ino` se compila con el Arduino IDE (placa Arduino Nano / ATmega328P) o con avr-gcc. Después hay que rellenar el .hex a múltiplo de 128 bytes antes de grabarlo.

## Multi_Lector (cédulas + tarjetas en un solo archivo)

Un único `Multi_Lector.hex` que contiene el firmware original de cédulas (`firmwarev02`) y el lector de tarjetas HID. El switch 1 elige el modo al encender la placa.

### DIP switch

| Switch 1 | Switch 2 | Switch 3 | Función |
|---|---|---|---|
| OFF | - | - | Lector de cédulas (firmware original) |
| ON | OFF | OFF | Envía el número impreso de la tarjeta |
| ON | ON | OFF | Envía la cantidad de bits de la tarjeta |
| ON | OFF | ON | Diagnóstico: envía los bits crudos en 3 números (1xxxxx = bits 0-15, 2xxxxx = bits 16-31, 3xxxxx = bits 32-39 y cantidad de bits) |
| ON | ON | ON | Prueba de enlace: envía 123456 cada 5 s |

El cambio entre cédulas y tarjetas se aplica al reiniciar la placa; los switch 2 y 3 se leen en cada lectura.

Formatos de tarjeta: 26 bits (número = bits 9..24), 35 bits Corporate 1000 (bits 14..33) y 36 bits BCD (dígitos en bits 12..31, ej. 0x79700799A → 00799). Otros largos envían sus 24 bits bajos.

Las conexiones del lector y del reloj son las mismas de HID_Reloj.



### Compilar desde el código

`firmware/Multi_Lector/Multi_Lector.ino` se enlaza en la dirección 0x7000 (`-Wl,--section-start=.text=0x7000`) y debe ocupar menos de 4096 bytes. Luego `combinar.py` lo junta con `firmwarev02.hex` y rellena cada parte hasta completar páginas de 128 bytes:

```
python3 firmware/Multi_Lector/combinar.py firmwarev02.hex Multi_Lector_0x7000.hex Multi_Lector.hex
```
