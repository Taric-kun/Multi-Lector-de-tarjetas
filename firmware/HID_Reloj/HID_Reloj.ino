/*
  HID_Reloj.ino  (placa GV_CableCedula v2.0)
  ------------------------------------------------------------------
  Lector HID (Wiegand) -> reloj ZKTeco (W26).
    Lector: DATA0 (verde) -> J3 pin 5 (A4), DATA1 (blanco) -> J3 pin 6 (A5)
    Reloj:  D8 = DATA0, D9 = DATA1
  DIP switch (ON = pin a GND):
    Todos OFF      -> envia el numero impreso de la tarjeta (6136, 8958...)
    Switch 1 ON    -> envia la cantidad de bits de la tarjeta (26, 35...)
  Formatos:
    26 bits (H10301):            numero = bits 9..24
    35 bits (Corporate 1000):    numero = bits 14..33
    otros >= 26 bits:            24 bits bajos sin la paridad final
  ------------------------------------------------------------------
*/

#include <avr/interrupt.h>

const uint8_t PIN_RELOJ_D0 = 8;
const uint8_t PIN_RELOJ_D1 = 9;
const uint8_t SW1 = 4;

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

static void pulso(uint8_t pin) {
  digitalWrite(pin, LOW);
  delayMicroseconds(40);
  digitalWrite(pin, HIGH);
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
  pulso((unosAlto & 1) ? PIN_RELOJ_D1 : PIN_RELOJ_D0);
  for (int8_t i = 23; i >= 0; i--) pulso(((d >> i) & 1) ? PIN_RELOJ_D1 : PIN_RELOJ_D0);
  pulso(((unosBajo & 1) ^ 1) ? PIN_RELOJ_D1 : PIN_RELOJ_D0);
}

// ===================== PROGRAMA PRINCIPAL =========================

void setup() {
  pinMode(PIN_RELOJ_D0, OUTPUT);
  pinMode(PIN_RELOJ_D1, OUTPUT);
  digitalWrite(PIN_RELOJ_D0, HIGH);
  digitalWrite(PIN_RELOJ_D1, HIGH);
  pinMode(SW1, INPUT_PULLUP);

  pinMode(A4, INPUT_PULLUP);
  pinMode(A5, INPUT_PULLUP);
  estadoPrevio = PINC & MASK_IN;
  PCMSK1 |= (1 << PCINT12) | (1 << PCINT13);
  PCIFR  |= (1 << PCIF1);
  PCICR  |= (1 << PCIE1);
}

void loop() {
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

  if (digitalRead(SW1) == LOW) {       // switch 1 ON: cantidad de bits
    enviarW26(n);
    return;
  }

  if (n == 35)      enviarW26(tomarBits(raw, n, 14, 33));
  else if (n == 26) enviarW26(tomarBits(raw, n, 9, 24));
  else if (n > 26)  enviarW26(tomarBits(raw, n, n - 25, n - 2));
}
