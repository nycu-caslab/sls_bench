CC = clang++-14
CFLAGS = -Wall -Wextra -Wshadow -Wpedantic -O1 -std=c++20 -fsanitize=address -g

.PHONY: all clean

all: sls_bench run_each run_ratio

sls_bench: bench.o sls.o 
	$(CC) $(CFLAGS) -o bench bench.o sls.o

run_each: each.o sls.o
	$(CC) $(CFLAGS) -o each each.o sls.o

run_ratio: ratio.o sls.o
	$(CC) $(CFLAGS) -o ratio ratio.o sls.o

bench.o: bench.cpp
	$(CC) $(CFLAGS) -c $< -o $@

each.o: each.cpp
	$(CC) $(CFLAGS) -c $< -o $@

ratio.o: ratio.cpp
	$(CC) $(CFLAGS) -c $< -o $@

sls.o: src/sls.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f bench each ratio *.o
