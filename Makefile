# AVR Makefile

PROG=avr-thermo
CPU=atmega8

CFLAGS= -g -O3 -Wall -Wstrict-prototypes -Wa,-ahlms=$(PROG).lst -mmcu=$(CPU) 

ASFLAGS        = -Wa,-adhlns=$(<:.S=.lst),-gstabs 
ALL_ASFLAGS    = -mmcu=$(CPU) -I. -x assembler-with-cpp $(ASFLAGS)

LFLAGS= -Wl,-Map=$(PROG).map,--cref -mmcu=$(CPU) -lm
#LFLAGS= -Wl,-u,vfprintf,-Map=$(PROG).map,--cref -mmcu=$(CPU) -lprintf_min -lm
INCL =  3310_routines.h
SRC = main.c 
OBJ = main.o 3310_routines.o

# default target when "make" is run w/o arguments
all: $(PROG).rom

# compile serialecho.c into serialecho.o
%.o: %.c
	avr-gcc -c $(CFLAGS) -I. $*.c

%.o : %.S
	avr-gcc -c $(ALL_ASFLAGS) $< -o $@
	
# link up sample.o and timer.o into sample.elf
$(PROG).elf: $(OBJ)
	avr-gcc $(OBJ) $(LFLAGS) -o $(PROG).elf

$(OBJ): $(INCL)

# copy ROM (FLASH) object out of sample.elf into sample.rom
$(PROG).rom: $(PROG).elf
	avr-objcopy -O srec $(PROG).elf $(PROG).rom

# command to program chip (optional) (invoked by running "make install")
install:
	uisp -dprog=stk200 -dlpt=/dev/parport0 --erase --upload --verify if=$(PROG).rom

# command to clean up junk (no source files) (invoked by "make clean")
clean:
	rm -f *.o *.elf *.map *~ *.lst

