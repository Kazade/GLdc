TARGET = offset.elf
OBJS = main.o

all: rm-elf $(TARGET)

include $(KOS_BASE)/Makefile.rules

clean:
	-rm -f $(TARGET) $(OBJS)

rm-elf:
	-rm -f $(TARGET)

$(TARGET): $(OBJS)
	$(KOS_CC) $(KOS_CFLAGS) $(KOS_LDFLAGS) -o $(TARGET) $(KOS_START) \
		$(OBJS) $(OBJEXTRA) -lm -lkosutils $(KOS_LIBS)

run: $(TARGET)
	$(KOS_LOADER) $(TARGET)

dist: $(TARGET)
	$(KOS_CC_BASE)/sh-elf/bin/objcopy -R .stack -O binary $(TARGET) $(basename $(TARGET))
	$(KOS_BASE)/utils/scramble/scramble $(basename $(TARGET)) 1ST_READ.BIN
