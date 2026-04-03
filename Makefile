CC       := gcc
AS       := nasm
CFLAGS   := -Wall -Wextra -Wpedantic -g -nostdlib -fno-builtin
ASFLAGS  := -f elf64
INCLUDES := -Iintf

SRC_DIR  := src
TEST_DIR := test
BUILD_DIR := build
BUILD_TEST_DIR := build_test
TEST_BUILD_DIR := $(BUILD_DIR)/test

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

tests: $(TEST_BINS)

$(TEST_BUILD_DIR)/%: $(TEST_DIR)/%.c $(STATIC_TEST) | $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS_TEST) $(INCLUDES) $< -L$(BUILD_TEST_DIR) -l$(LIB_NAME_TEST) -nostdlib -o $@

# --- Utility ---

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_TEST_DIR):
	mkdir -p $(BUILD_TEST_DIR)

$(TEST_BUILD_DIR):
	mkdir -p $(TEST_BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR) $(BUILD_TEST_DIR)