CC       := gcc
AS       := nasm
CFLAGS   := -Wall -Wextra -Wpedantic -g -nostdlib
ASFLAGS  := -f elf64
INCLUDES := -Iintf

SRC_DIR  := src
TEST_DIR := test
BUILD_DIR := build

LIB_NAME := mylibc
STATIC   := $(BUILD_DIR)/lib$(LIB_NAME).a

SRCS     := $(wildcard $(SRC_DIR)/*.c)
ASM_SRCS := $(wildcard $(SRC_DIR)/*.asm)
OBJS     := $(patsubst $(SRC_DIR)/%.c,   $(BUILD_DIR)/%.o, $(SRCS)) \
            $(patsubst $(SRC_DIR)/%.asm, $(BUILD_DIR)/%.o, $(ASM_SRCS))

TEST_SRCS := $(wildcard $(TEST_DIR)/*.c)
TEST_BINS := $(patsubst $(TEST_DIR)/%.c, $(BUILD_DIR)/test_%, $(TEST_SRCS))

.PHONY: all static tests clean

all: static tests

# --- Library target ---

static: $(STATIC)

$(STATIC): $(OBJS) | $(BUILD_DIR)
	ar rcs $@ $^

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.asm | $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

# --- Test targets ---

tests: $(TEST_BINS)

$(BUILD_DIR)/test_%: $(TEST_DIR)/%.c $(STATIC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(BUILD_DIR) -l$(LIB_NAME) -nostdlib -o $@
	./$@

# --- Utility ---

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)