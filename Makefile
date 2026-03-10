CC       := gcc
CFLAGS   := -Wall -Wextra -Wpedantic -g -nostdlib
INCLUDES := -Iintf

SRC_DIR  := src
TEST_DIR := test
BUILD_DIR := build

LIB_NAME := mylibc
SHARED   := $(BUILD_DIR)/lib$(LIB_NAME).so
STATIC   := $(BUILD_DIR)/lib$(LIB_NAME).a

SRCS     := $(wildcard $(SRC_DIR)/*.c)
OBJS     := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

TEST_SRCS := $(wildcard $(TEST_DIR)/*.c)
TEST_BINS := $(patsubst $(TEST_DIR)/%.c, $(BUILD_DIR)/test_%, $(TEST_SRCS))

.PHONY: all shared static tests clean

all: shared tests

# --- Library targets ---

shared: $(SHARED)

static: $(STATIC)

$(SHARED): $(OBJS) | $(BUILD_DIR)
	$(CC) -shared -o $@ $^

$(STATIC): $(OBJS) | $(BUILD_DIR)
	ar rcs $@ $^

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -fPIC -c $< -o $@

# --- Test targets ---

tests: $(TEST_BINS)

$(BUILD_DIR)/test_%: $(TEST_DIR)/%.c $(SHARED) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(BUILD_DIR) -l$(LIB_NAME) -Wl,-rpath,$(BUILD_DIR) -nostdlib -o $@
	./$@

# --- Utility ---

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)