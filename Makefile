CC=avr-gcc
LD=avr-gcc
MCU=atmega32
F_CPU=12000000L
OPTLEVEL=5
CFLAGS=-mmcu=$(MCU) -std=gnu99 -O$(OPTLEVEL) -DF_CPU=$(F_CPU) -Wall
LDFLAGS=-mmcu=$(MCU) 
OBJCOPY=avr-objcopy
OBJDUMP=avr-objdump
FORMAT=ihex
OBJS=main.o
PORT=/dev/ttyS4

all: pong lst

pong: pong.hex

lst: pong.lst

clean:
	rm -rf *.hex *.elf *.o *.lst

%.hex: %.elf
	$(OBJCOPY) -O $(FORMAT) -R .eeprom $< $@

%.lst: %.elf
	$(OBJDUMP) -h -S $< > $@

pong.elf: $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o pong.elf

.o: .c
	$(CC) $(CFLAGS) -c $<

burn: pong
	avrdude -c ponyser -p $(MCU) -P $(PORT) -U lfuse:w:0xff:m -U hfuse:w:0xdf:m -U flash:w:pong.hex 

