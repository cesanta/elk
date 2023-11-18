ASAN ?= -fsanitize=address,undefined -fno-sanitize-recover=all
CFLAGS ?= -W -Wall -Wextra -Werror -Wno-deprecated -I.. \
          -Wundef -Wshadow -Wdouble-promotion -fno-common -Wconversion \
          -g3 -O3 -ffunction-sections -fdata-sections \
          $(ASAN) -coverage $(ARCH_FLAGS) $(EXTRA_CFLAGS)
CWD ?= $(realpath $(CURDIR))
ROOT ?= $(realpath $(CURDIR)/..)
DOCKER = docker run $(DA) --rm -e WINEDEBUG=-all -v $(ROOT):$(ROOT) -w $(CWD)
ARCH_FLAGS ?= #-Wformat-truncation
DESTDIR ?= .

define clean
  rm -rf  *.o *.dSYM ut* elk fuzzer* *.gcov *.gcno *.gcda *.obj *.exe *.ilk *.pdb slow-unit* _CL_* infer-out data.txt crash-* a.out tmp
endef

# %(call build,ENVIRONMENT,COMPILE,FLAGS,OUTPUT,RUN)
define build
  $(call clean)
  $1 $2 unit_test.c $3
  $1 $4
endef

all: test test++ vc2017 vc22 linux mingw elk uno

valgrind: ASAN =
valgrind: RUN = valgrind --tool=memcheck --gen-suppressions=all --leak-check=full --show-leak-kinds=all --leak-resolution=high --track-origins=yes --error-exitcode=1 --exit-on-first-error=yes
valgrind:
	$(call build,,$(CC),$(CFLAGS) -o ut,$(RUN) ./ut)

test:
	$(call build,,$(CC),$(CFLAGS) -o ut,$(RUN) ./ut)

test++:
	$(call build,,$(CXX),$(CFLAGS) -Wno-deprecated -o ut,./ut)

vc2017:
	$(call build,$(DOCKER) mdashnet/vc2017 wine64,cl,/nologo /W3 /O2 /I. $(EXTRA) /Feut.exe,ut.exe)

vc22:
	$(call build,$(DOCKER) mdashnet/vc22 wine64,cl,/nologo /W3 /O2 /MD /I. $(EXTRA) /Feut.exe,ut.exe)

linux:
	$(call build,$(DOCKER) mdashnet/cc2,$(CC), $(CFLAGS) -o ut,./ut)

mingw:
	$(call build,$(DOCKER) mdashnet/mingw,x86_64-w64-mingw32-gcc,-W -Wall $(EXTRA) -o ut,wine ut)

arduino:
	curl -s http://downloads.arduino.cc/arduino-1.8.13-linux64.tar.xz -o /tmp/a.tgz
	tar -xf /tmp/a.tgz
	mv arduino-* $@

uno: arduino
	rm -rf tmp; mkdir tmp; cp ../examples/BlinkyJS/BlinkyJS.ino tmp/tmp.ino
	cp ../elk.c ../elk.h tmp/
	$(DOCKER) mdashnet/cc2 ./arduino/arduino --verify --board arduino:avr:nano tmp/tmp.ino

CMD ?= --verify
esp32: arduino
	rm -rf tmp; mkdir tmp; cp -r ../examples/Esp32JS/*.ino tmp/tmp.ino
	cp -r ../examples/Esp32JS/*.[ch] ../elk.[ch] tmp/
	./arduino/arduino $(CMD) --board esp32:esp32:esp32 tmp/tmp.ino
#	$(DOCKER) mdashnet/cc2 ./arduino/arduino --verify --board esp32:esp32:esp32 tmp/tmp.ino

elk: ../elk.c ../examples/cmdline/main.c
	$(CC) $^ -I.. $(CFLAGS) -DJS_DUMP -o $(DESTDIR)/$@

coverage: test
	gcov -l -n *.gcno | sed '/^$$/d' | sed 'N;s/\n/ /'
	@gcov test.gcno >/dev/null

upload-coverage: coverage
	curl -s https://codecov.io/bash | /bin/bash

clean:
	$(call clean)
