TEST_SOURCES=color_test.c port_test.c stop.c tacho_test.c touch_test.c ultrasound_test.c
TEST_OBJECTS=color_test.o port_test.o stop.o tacho_test.o touch_test.o ultrasound_test.o
TEST_TARGETS=color_test port_test stop tacho_test touch_test ultrasound_test

all: tests

cordless: $(TEST_TARGETS)
	cp $^ /home/robot/cordless/

clean:
	rm -f $(TEST_OBJECTS) $(TEST_TARGETS)

tests: $(TEST_TARGETS)

%.o: %.c
	gcc $< -c -o $@ -I/usr/local/include

%: %.o
	gcc $< -o $@ -L/usr/local/lib -lzlog -lpthread -lev3dev-c

.SUFFIXES:

.PHONY: all cordless tests clean
