CC=clang++
SRC=src/engine
STD=c++23
OUT=build/engine

SRCS := $(wildcard $(SRC)/*.cpp)
OBJS := $(SRCS:$(SRC)/%.cpp=build/%.o)

BUILD ?= release

FLAGS = -Wall -Wextra -Wpedantic -march=native
ifeq ($(BUILD), debug)
	FLAGS += -g -O0 -march=native -DDEBUG
else
	FLAGS += -O3 -flto -DNDEBUG
endif

run: $(OUT)
	./$(OUT)

$(OUT): $(OBJS)
	$(CC) $(OBJS) -o $(OUT) -std=$(STD) $(FLAGS)

build/%.o: $(SRC)/%.cpp
	mkdir -p build
	$(CC) -c $< -o $@ -std=$(STD) $(FLAGS) -MMD -MP

clean:
	rm -f $(OUT) build/*.o build/*.d

-include $(OBJS:.o=.d)
