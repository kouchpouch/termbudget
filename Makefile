CC          := gcc
LIBS        := -lncurses

SRC_DIR     := ./src
OBJ_DIR     := ./obj
BINARY      := termbudget

SRCS        := $(wildcard $(SRC_DIR)/*.c)
OBJS        := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

DEPS        := $(OBJS:.o=.d)

DEP_FLAGS   := -MP -MD

CFLAGS      := -g -Wall -Wextra -Wpedantic -Werror $(DEP_FLAGS)

.PHONY: all clean

all: $(BINARY)

$(BINARY): $(OBJS)
	@$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(BINARY)

-include $(DEPS)
