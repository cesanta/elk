CFLAGS ?= -W -Wall -Werror -Wno-deprecated -I.. -g -fsanitize=address,undefined -coverage -lm $(EXTRA)
CWD ?= $(realpath $(CURDIR))
ROOT ?= $(realpath $(CURDIR)/..)
DOCKER = docker run $(DA) --rm -it -e WINEDEBUG=-all -v $(ROOT):$(ROOT) -w $(CWD)

define clean
  rm -rf  *.o *.dSYM ut* cli fuzzer* *.gcov *.gcno *.gcda *.obj *.exe *.ilk *.pdb slow-unit* _CL_* infer-out data.txt crash-* a.out tmp
endef

# %(call build,ENVIRONMENT,COMPILE,FLAGS,OUTPUT,RUN)
define build
  $(call clean)
  $1 $2 unit_test.c $3
  $1 $4
endef

all: test test++ vc2017 linux mingw cli

test:
	$(call build,,$(CC),$(CFLAGS) -o ut,$(RUN) ./ut)

test++:
	$(call build,,$(CXX),$(CFLAGS) -Wno-deprecated -o ut,./ut)

vc2017:
	$(call build,$(DOCKER) mdashnet/vc2017 wine64,cl,/nologo /W3 /O2 /I. $(EXTRA) /Feut.exe,ut.exe)

linux:
	$(call build,$(DOCKER) mdashnet/cc2,$(CC), $(CFLAGS) -o ut,./ut)

mingw:
	$(call build,$(DOCKER) mdashnet/mingw,i686-w64-mingw32-gcc,-W -Wall $(EXTRA) -o ut,wine ut)

arduino:
	curl -s http://downloads.arduino.cc/arduino-1.8.13-linux64.tar.xz -o /tmp/a.tgz
	tar -xf /tmp/a.tgz
	mv arduino-* $@

uno: arduino
	rm -rf tmp; mkdir tmp; cp ../examples/BlinkyJS/BlinkyJS.ino tmp/tmp.ino
	cp ../elk.c ../elk.h tmp/
	$(DOCKER) mdashnet/cc2 ./arduino/arduino --verify --board arduino:avr:nano tmp/tmp.ino

cli:
	$(CC) ../elk.c ../examples/cmdline/main.c $(CFLAGS) -lm -o $@


coverage: test
	gcov -l -n *.gcno | sed '/^$$/d' | sed 'N;s/\n/ /'
	@gcov test.gcno >/dev/null

upload-coverage: coverage
	curl -s https://codecov.io/bash | /bin/bash

clean:
	$(call clean)
