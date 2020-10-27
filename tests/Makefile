TARGET = tests.elf
OBJS = test_runner.o
TESTS := $(shell find . -name \*.h)

CXXFLAGS := $(CXXFLAGS) -std=c++11

all: rm-elf $(TARGET)

include $(KOS_BASE)/Makefile.rules

clean:
	-rm -f $(TARGET) $(OBJS) romdisk.* test_runner.o test_runner.cpp

rm-elf:
	-rm -f $(TARGET) romdisk.*

test_runner.cpp:
	$(shell python3 test_generator.py --output=test_runner.cpp $(TESTS))

test_runner.o: test_runner.cpp

$(TARGET): $(OBJS) romdisk.o
	kos-c++ -o $(TARGET) $(OBJS) ../libGLdc.a romdisk.o

romdisk.img:
	$(KOS_GENROMFS) -f romdisk.img -d romdisk -v

romdisk.o: romdisk.img
	$(KOS_BASE)/utils/bin2o/bin2o romdisk.img romdisk romdisk.o

run: $(TARGET)
	$(KOS_LOADER) $(TARGET)

dist:
	rm -f $(OBJS) romdisk.o romdisk.img
	$(KOS_STRIP) $(TARGET)

