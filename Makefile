CC       := gcc
AS       := nasm
CFLAGS   := -Wall -Wextra -Wpedantic -g -nostdlib -fno-builtin -O2 -std=c17 -ffreestanding -fno-stack-protector
# Executables are fully static and non-PIE, so they need no dynamic loader.
LDFLAGS  := -nostdlib -static -no-pie
ASFLAGS  := -f elf64
INCLUDES := -Iintf

SRC_DIR  := src
TEST_DIR := test
BUILD_DIR := build
BUILD_TEST_DIR := build_test
TEST_BUILD_DIR := $(BUILD_DIR)/test
PRINTF_TEST_DIR := $(TEST_DIR)/printf
# The printf driver needs a case name argument, so it is kept out of
# $(TEST_BUILD_DIR), whose binaries are all run without arguments.
PRINTF_BUILD_DIR := $(BUILD_DIR)/printf_test

LIB_NAME := mylibc
STATIC   := $(BUILD_DIR)/lib$(LIB_NAME).a
LIB_NAME_TEST := $(LIB_NAME)_test
STATIC_TEST   := $(BUILD_TEST_DIR)/lib$(LIB_NAME_TEST).a

CFLAGS_TEST := $(CFLAGS) -D__LIBC_TEST -Wno-unused-function -Wno-unused-variable

#
# Collect sources recursively under src/ (POSIX).
#
SRCS     := $(shell find $(SRC_DIR) -type f -name '*.c')
ASM_SRCS := $(shell find $(SRC_DIR) -type f -name '*.asm')
OBJS     := $(patsubst $(SRC_DIR)/%.c,   $(BUILD_DIR)/%.o, $(SRCS)) \
            $(patsubst $(SRC_DIR)/%.asm, $(BUILD_DIR)/%.o, $(ASM_SRCS))
OBJS_TEST := $(patsubst $(SRC_DIR)/%.c,   $(BUILD_TEST_DIR)/%.o, $(SRCS)) \
             $(patsubst $(SRC_DIR)/%.asm, $(BUILD_TEST_DIR)/%.o, $(ASM_SRCS))

TEST_SRCS := $(wildcard $(TEST_DIR)/*.c)
TEST_BINS := $(patsubst $(TEST_DIR)/%.c, $(TEST_BUILD_DIR)/%, $(TEST_SRCS))
PRINTF_BINS := $(PRINTF_BUILD_DIR)/printf $(PRINTF_BUILD_DIR)/printf_mylibc

.PHONY: all static static_test tests clean

all: static static_test tests

# --- Library target ---

static: $(STATIC)

$(STATIC): $(OBJS) | $(BUILD_DIR)
	ar rcs $@ $^

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.asm | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

# --- Test-instrumented library target ---

static_test: $(STATIC_TEST)

$(STATIC_TEST): $(OBJS_TEST) | $(BUILD_TEST_DIR)
	ar rcs $@ $^

$(BUILD_TEST_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_TEST_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_TEST) $(INCLUDES) -c $< -o $@

$(BUILD_TEST_DIR)/%.o: $(SRC_DIR)/%.asm | $(BUILD_TEST_DIR)
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

# --- Test targets ---

tests: $(TEST_BINS) $(PRINTF_BINS)

$(TEST_BUILD_DIR)/%: $(TEST_DIR)/%.c $(STATIC_TEST) | $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS_TEST) $(INCLUDES) $< -L$(BUILD_TEST_DIR) -l$(LIB_NAME_TEST) $(LDFLAGS) -o $@

# The printf test lives in its own folder and pulls its case table in from a
# separate file.
$(PRINTF_BUILD_DIR)/printf: $(PRINTF_TEST_DIR)/printf.c $(PRINTF_TEST_DIR)/printf_cases.inc $(STATIC_TEST) | $(PRINTF_BUILD_DIR)
	$(CC) $(CFLAGS_TEST) $(INCLUDES) $< -L$(BUILD_TEST_DIR) -l$(LIB_NAME_TEST) $(LDFLAGS) -o $@

# test/printf/printf_test.sh compares stdout byte for byte, so it needs a build
# linked against the uninstrumented library (the test library logs to stdout).
$(PRINTF_BUILD_DIR)/printf_mylibc: $(PRINTF_TEST_DIR)/printf.c $(PRINTF_TEST_DIR)/printf_cases.inc $(STATIC) | $(PRINTF_BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(BUILD_DIR) -l$(LIB_NAME) $(LDFLAGS) -o $@

# --- Utility ---

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_TEST_DIR):
	mkdir -p $(BUILD_TEST_DIR)

$(TEST_BUILD_DIR):
	mkdir -p $(TEST_BUILD_DIR)

$(PRINTF_BUILD_DIR):
	mkdir -p $(PRINTF_BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR) $(BUILD_TEST_DIR)