TARGET = kernel.elf
BUILD  = build

CC = x86_64-elf-gcc
AS = x86_64-elf-as
LD = x86_64-elf-ld

CFLAGS  = -m64 -ffreestanding -O2 -Wall -Wextra -g
LDFLAGS = -T linker.ld -nostdlib

all: $(BUILD)/$(TARGET)

$(BUILD)/$(TARGET): $(BUILD)/kernel.o
	$(LD) $(LDFLAGS) -o $(BUILD)/$(TARGET) $(BUILD)/kernel.o

$(BUILD)/kernel.o: kernel.S | $(BUILD)
	$(AS) --gdwarf-2 kernel.S -o $(BUILD)/kernel.o

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
