/*
  Multi_Lector.ino  (placa GV_CableCedula v2.0, ATmega328P)
  ------------------------------------------------------------------
  Se graba en la seccion de arranque (0x7000) junto al firmware de
  cedulas (firmwarev02, en 0x0000). Al encender se lee el switch 1:
    Switch 1 OFF -> salta al firmware de cedulas (sin cambios).
    Switch 1 ON  -> lector de tarjetas HID (este programa):
       SW2 OFF, SW3 OFF -> envia el numero de la tarjeta
       SW2 ON,  SW3 OFF -> envia la cantidad de bits
       SW2 OFF, SW3 ON  -> diagnostico: bits crudos en 3 numeros
                           (1xxxxx, 2xxxxx, 3xxxxx; ver README)
       SW2 ON,  SW3 ON  -> prueba de enlace: envia 123456 cada 5 s
  El cambio de cedula <-> tarjetas se aplica al reiniciar la placa;
  SW2 y SW3 se leen en cada lectura.

  Conexiones:
    Lector DATA0 (verde) -> J3 pin 5 (A4), DATA1 (blanco) -> J3 pin 6 (A5)
    Reloj: D8 = DATA0, D9 = DATA1
    Switch 1 = D4, switch 2 = D3, switch 3 = D2 (ON = GND)
  Formatos:
    26 bits (H10301):          numero = bits 9..24
    35 bits (Corporate 1000):  numero = bits 14..33
    36 bits (BCD, 9 digitos):  numero = digitos 4..8 (bits 12..31),
                               ej. 0x79700799A -> 00799
    otros:                     24 bits bajos sin la paridad final
  ------------------------------------------------------------------
*/

#include <avr/interrupt.h>
#include <avr/io.h>

// ===================== SELECTOR DE ARRANQUE =======================
// Corre antes que todo (init1): si el switch 1 esta OFF salta a 0x0000
// (firmware de cedulas). Si esta ON, mueve los vectores de interrupcion
// a la seccion de arranque y sigue con este programa.
extern "C" void selectorArranque(void) __attribute__((naked, used, section(".init1")));
extern "C" void selectorArranque(void) {
  asm volatile(
    "sbi %[portd], 4      \n"   // pull-up en D4 (switch 1)
    "ldi r24, 200         \n"
    "1: dec r24           \n"   // ~40 us para que suba la linea
    "nop                  \n"
    "brne 1b              \n"
    "sbis %[pind], 4      \n"   // D4 en alto = switch OFF
    "rjmp 2f              \n"
    "cbi %[portd], 4      \n"   // dejar el pin como estaba
    "jmp 0                \n"   // -> firmware de cedulas
    "2: ldi r24, 1        \n"
    "out %[mcucr], r24    \n"   // IVCE
    "ldi r24, 2           \n"
    "out %[mcucr], r24    \n"   // IVSEL: vectores en 0x7000
    :
    : [portd] "I" (_SFR_IO_ADDR(PORTD)),
      [pind]  "I" (_SFR_IO_ADDR(PIND)),
      [mcucr] "I" (_SFR_IO_ADDR(MCUCR))
    : "r24");
}

// ===================== CONFIGURACION ==============================

// Acceso directo a puertos (el programa debe caber en 4 KB)
const uint8_t RELOJ_D0 = (1 << 0);     // D8 = PB0
const uint8_t RELOJ_D1 = (1 << 1);     // D9 = PB1
const uint8_t SW2 = (1 << 3);          // D3 = PD3
const uint8_t SW3 = (1 << 2);          // D2 = PD2

const uint8_t MASK_IN_D0 = (1 << 4);   // A4
const uint8_t MASK_IN_D1 = (1 << 5);   // A5
const uint8_t MASK_IN = MASK_IN_D0 | MASK_IN_D1;

const uint16_t MIN_LOW_US = 20;
const uint16_t MAX_LOW_US = 250;
const uint32_t FIN_TRAMA_US = 120000UL;

// ===================== LECTURA ====================================

volatile uint64_t bitsLeidos = 0;
volatile uint8_t  cantBits   = 0;
volatile uint32_t ultimoPulsoUs = 0;
volatile uint32_t bajada[2] = {0, 0};
volatile uint8_t  estadoPrevio = MASK_IN;

ISR(PCINT1_vect) {
  uint8_t ahora8 = PINC & MASK_IN;
  uint8_t cambio = ahora8 ^ estadoPrevio;
  estadoPrevio = ahora8;
  uint32_t t = micros();

  for (uint8_t linea = 0; linea < 2; linea++) {
    uint8_t mask = linea ? MASK_IN_D1 : MASK_IN_D0;
    if (!(cambio & mask)) continue;
    if (!(ahora8 & mask)) {
      bajada[linea] = t;
    } else {
      uint32_t w = t - bajada[linea];
      if (w >= MIN_LOW_US && w <= MAX_LOW_US && cantBits < 64) {
        bitsLeidos = (bitsLeidos << 1) | linea;
        cantBits++;
        ultimoPulsoUs = t;
      }
    }
  }
}

// Bits [desde..hasta], contando desde el primero recibido (indice 0)
static uint32_t tomarBits(uint64_t raw, uint8_t n, uint8_t desde, uint8_t hasta) {
  uint8_t ancho = hasta - desde + 1;
  return (uint32_t)((raw >> (n - 1 - hasta)) & ((1ULL << ancho) - 1));
}

// ===================== SALIDA =====================================

static void pulso(uint8_t mask) {
  PORTB &= ~mask;
  delayMicroseconds(40);
  PORTB |= mask;
  delayMicroseconds(2100);
}

// Envia un numero de 24 bits como Wiegand 26
static void enviarW26(uint32_t numero) {
  uint32_t d = numero & 0xFFFFFF;
  uint8_t unosAlto = 0, unosBajo = 0;
  for (uint8_t i = 0; i < 12; i++) {
    unosBajo += (d >> i) & 1;
    unosAlto += (d >> (i + 12)) & 1;
  }
  pulso((unosAlto & 1) ? RELOJ_D1 : RELOJ_D0);
  for (int8_t i = 23; i >= 0; i--) pulso(((d >> i) & 1) ? RELOJ_D1 : RELOJ_D0);
  pulso(((unosBajo & 1) ^ 1) ? RELOJ_D1 : RELOJ_D0);
}

// ===================== PROGRAMA PRINCIPAL =========================

void setup() {
  PORTB |= RELOJ_D0 | RELOJ_D1;         // reposo en alto
  DDRB  |= RELOJ_D0 | RELOJ_D1;
  DDRD  &= ~(SW2 | SW3);
  PORTD |= SW2 | SW3;                   // pull-up switches
  DDRC  &= ~MASK_IN;
  PORTC |= MASK_IN;                     // pull-up A4/A5
  estadoPrevio = PINC & MASK_IN;
  PCMSK1 |= (1 << PCINT12) | (1 << PCINT13);
  PCIFR  |= (1 << PCIF1);
  PCICR  |= (1 << PCIE1);
}

void loop() {
  bool sw2 = !(PIND & SW2);
  bool sw3 = !(PIND & SW3);

  if (sw2 && sw3) {                    // prueba de enlace
    enviarW26(123456);
    delay(5000);
    return;
  }

  noInterrupts();
  uint8_t n = cantBits;
  uint32_t ultimo = ultimoPulsoUs;
  interrupts();

  if (n == 0 || (micros() - ultimo) <= FIN_TRAMA_US) return;

  noInterrupts();
  uint64_t raw = bitsLeidos;
  n = cantBits;
  cantBits = 0; bitsLeidos = 0;
  interrupts();

  if (sw2) {                           // cantidad de bits
    enviarW26(n);
    return;
  }

  if (sw3) {                           // diagnostico: bits crudos
    // primer bit recibido = bit mas alto del primer numero
    uint64_t a = raw << (64 - n);
    enviarW26(0x10000UL | (uint16_t)(a >> 48));
    delay(1500);
    enviarW26(0x20000UL | (uint16_t)(a >> 32));
    delay(1500);
    enviarW26(0x30000UL | (((a >> 16) & 0xFF00) | n));
    return;
  }

  if (n == 36) {                       // BCD: 5 digitos en bits 12..31
    uint32_t bcd = tomarBits(raw, n, 12, 31), num = 0;
    bool ok = true;
    for (int8_t i = 4; i >= 0; i--) {
      uint8_t dig = (bcd >> (4 * i)) & 0xF;
      if (dig > 9) ok = false;
      num = num * 10 + dig;
    }
    enviarW26(ok ? num : tomarBits(raw, n, n - 25, n - 2));
  }
  else if (n == 35) enviarW26(tomarBits(raw, n, 14, 33));
  else if (n == 26) enviarW26(tomarBits(raw, n, 9, 24));
  else if (n > 26)  enviarW26(tomarBits(raw, n, n - 25, n - 2));
}
