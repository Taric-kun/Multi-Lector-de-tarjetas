.\avr\bin\avrdude -C .\avr\etc\avrdude.conf -v -p m328p -c usbtiny -U lfuse:w:0xff:m -U hfuse:w:0xda:m -U efuse:w:0xfd:m -U flash:w:.\HID_Reloj.hex:i
pause
