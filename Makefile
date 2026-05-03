#Slopped together AI :(

BUILD := build
AFL_CC := afl-clang-fast
CLANG  := clang-21

AFL_ENV_OPTIONS := AFL_MAP_SIZE=1048576 AFL_LLVM_INSTRUMENT=CLASSIC AFL_LLVM_CTX_K=3

SRC     := main.c elk.c
CLI_SRC := cli.c elk.c
CLI_OPTIONS := -flto -ggdb3 -O0 -fno-omit-frame-pointer

.PHONY: all afl cli clean \
        normal san1 san2 san3 laf cmplog \
        cli-normal cli-san1 cli-san2 cli-san3

# ---- helpers ----

# Ensure build dir exists
$(BUILD):
	mkdir -p $(BUILD)

# Generic AFL build macro
define AFL_BUILD
$1: | $(BUILD)
	$2 $(AFL_ENV_OPTIONS) $(AFL_CC) $(SRC) -o $(BUILD)/$1
endef

# Generic CLI build macro
define CLI_BUILD
$1: | $(BUILD)
	$(CLANG) $2 $(CLI_SRC) $(CLI_OPTIONS) -o $(BUILD)/$1
endef

# ---- targets ----

all: afl cli
afl: normal san1 san2 san3 laf cmplog
cli: cli-normal cli-san1 cli-san2 cli-san3

# ---- AFL variants ----

$(eval $(call AFL_BUILD,normal,))

$(eval $(call AFL_BUILD,san1, \
	AFL_LLVM_ONLY_FSRV=1 \
	AFL_USE_ASAN=1 \
	AFL_USE_UBSAN=1 \
	AFL_USE_LSAN=1))

$(eval $(call AFL_BUILD,san2, \
	AFL_LLVM_ONLY_FSRV=1 \
	AFL_HARDEN=1 \
	AFL_USE_TSAN=1))

$(eval $(call AFL_BUILD,san3, \
	AFL_LLVM_ONLY_FSRV=1 \
	AFL_USE_MSAN=1 \
	AFL_USE_CFISAN=1))

$(eval $(call AFL_BUILD,laf, \
	AFL_LLVM_LAF_SPLIT_SWITCHES=1 \
	AFL_LLVM_LAF_TRANSFORM_COMPARES=1 \
	AFL_LLVM_LAF_SPLIT_COMPARES=1))

$(eval $(call AFL_BUILD,cmplog, \
	AFL_LLVM_CMPLOG=1))

# ---- CLI variants ----

$(eval $(call CLI_BUILD,cli-normal, -g))

$(eval $(call CLI_BUILD,cli-san1, \
	-fsanitize=address,leak,undefined))

$(eval $(call CLI_BUILD,cli-san2, \
	-fsanitize=thread))

$(eval $(call CLI_BUILD,cli-san3, \
	-fsanitize=memory,cfi -fvisibility=hidden))

# ---- clean ----

clean:
	rm -rf $(BUILD)