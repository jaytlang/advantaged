SRC = $(shell find . -type f -name "*.c")
HDR = $(shell find . -type f -name "*.h")
OBJ = $(SRC:.c=.o)
DEP = $(OBJ:.o=.d)

PROG = advd
FMT = .clang-format

CFLAGS = -O2 -g -Wall -Wextra -Werror -pedantic -MD 

.PHONY: all
all: $(PROG)

$(PROG): $(OBJ)
	@echo "	LD	$(PROG)"
	@$(CC) -o $@ $^ $(LDFLAGS)

-include $(DEP)

%.o: %.c
	@echo "	CC	$<"
	@$(CC) -c -o $@ $< $(CFLAGS)

.PHONY: clean format
clean:
	@echo "	CLEAN	$(PROG)"
	@rm -f $(PROG) $(OBJ) $(DEP)

format:
	@echo "	FMT	$(PROG)"
	@echo $(SRC) $(HDR) | xargs clang-format -i
	@echo $(SRC) $(HDR) | xargs sed -i 's/\ {/{/;s/}\ /}/'

