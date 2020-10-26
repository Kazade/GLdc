TARGET = tests.elf
OBJS = test_runner.o

TESTS = $(shell find . -name "*.cpp")

CXXFLAGS := $(CXXFLAGS) -std=c++11

all: rm-elf $(TARGET)

include $(KOS_BASE)/Makefile.rules

clean:
	-rm -f $(TARGET) $(OBJS) romdisk.*

rm-elf:
	-rm -f $(TARGET) romdisk.*

test_runner.cpp: $(shell python3 test_generator.py --output=test_runner.cpp $(TESTS))

$(TARGET): $(OBJS) romdisk.o
	kos-c++ -o $(TARGET) $(OBJS) romdisk.o

romdisk.img:
	$(KOS_GENROMFS) -f romdisk.img -d romdisk -v

romdisk.o: romdisk.img
	$(KOS_BASE)/utils/bin2o/bin2o romdisk.img romdisk romdisk.o

run: $(TARGET)
	$(KOS_LOADER) $(TARGET)

dist:
	rm -f $(OBJS) romdisk.o romdisk.img
	$(KOS_STRIP) $(TARGET)
