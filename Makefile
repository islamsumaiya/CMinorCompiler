CC      ?= gcc
CFLAGS  := -std=c11 -O2 -Wall -Wextra -Wpedantic -Iinclude
TARGET  := cminorcc

SRCS := \
	src/main.c \
	src/common.c \
	src/lexer/lexer.c \
	src/parser/parser.c \
	src/semantic/sema.c \
	src/ir/ir_gen.c \
	src/backend/llvm_emit.c \
	src/backend/qbe_emit.c \
	src/backend/runner.c

OBJS := $(SRCS:.c=.o)

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TARGET)
	bash tests/run_tests.sh

clean:
	rm -f $(OBJS) $(TARGET) *.s *.o *.ll *.ssa
