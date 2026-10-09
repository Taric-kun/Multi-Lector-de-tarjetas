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
