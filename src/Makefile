CC = g++
SFLAGS = -Wall -Wextra -std=c++20
OFLAGS = -O3 -march=native -flto -DNDEBUG -DNOMINMAX
LFLAGS = -static
SRC = $(wildcard *.cpp)
OBJ = $(SRC:.cpp=.o)
BIN = fire_mingw

$(BIN): $(OBJ)
	$(CC) $(OBJ) $(LFLAGS) -o $@

$(OBJ): %.o: %.cpp
	$(CC) $(SFLAGS) $(OFLAGS) -c $<

clean:
	@rm -f $(OBJ)
