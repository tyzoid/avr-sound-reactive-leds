DEVICE     = attiny85
CLOCK      = 8000000
PROGRAMMER = avrisp2
PORT	   = usb
BAUD       = 19200
FILENAME   = lights
COMPILE    = avr-gcc -Wall -Os -DF_CPU=$(CLOCK) -mmcu=$(DEVICE)

all: usb clean build upload

usb:
	ls /dev/cu.*

build:
	$(COMPILE) -c $(FILENAME).c -o $(FILENAME).o
	$(COMPILE) -o $(FILENAME).elf $(FILENAME).o
	avr-objcopy -j .text -j .data -O ihex $(FILENAME).elf $(FILENAME).hex
	avr-size --format=avr --mcu=$(DEVICE) $(FILENAME).elf

upload:
	avrdude -vv -p $(DEVICE) -c $(PROGRAMMER) -P $(PORT) -b $(BAUD) -U flash:w:$(FILENAME).hex:i 

fuse:
	avrdude -vv -p $(DEVICE) -c $(PROGRAMMER) -P $(PORT) -b $(BAUD) -U lfuse:w:0xe2:m

clean:
	rm $(FILENAME).o
	rm $(FILENAME).elf
	rm $(FILENAME).hex
