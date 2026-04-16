CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lcrypto

SRCS = object.c tree.c index.c commit.c pes.c
OBJS = $(SRCS:.c=.o)

all: pes test_objects test_tree

pes: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

object.o: object.c pes.h
	$(CC) $(CFLAGS) -c object.c -o object.o

tree.o: tree.c tree.h index.h pes.h
	$(CC) $(CFLAGS) -c tree.c -o tree.o

index.o: index.c index.h pes.h
	$(CC) $(CFLAGS) -c index.c -o index.o

commit.o: commit.c commit.h tree.h pes.h
	$(CC) $(CFLAGS) -c commit.c -o commit.o

pes.o: pes.c pes.h index.h commit.h
	$(CC) $(CFLAGS) -c pes.c -o pes.o

test_objects: test_objects.o object.o
	$(CC) -o $@ $^ $(LDFLAGS)

test_tree: test_tree.o object.o tree.o index.o
	$(CC) -o $@ $^ $(LDFLAGS)

test_objects.o: test_objects.c pes.h
	$(CC) $(CFLAGS) -c test_objects.c -o test_objects.o

test_tree.o: test_tree.c tree.h index.h pes.h
	$(CC) $(CFLAGS) -c test_tree.c -o test_tree.o

clean:
	rm -f pes test_objects test_tree *.o
	rm -rf .pes

test: test-unit test-integration

test-unit: test_objects test_tree
	@echo "=== Running Phase 1 tests ==="
	./test_objects
	@echo ""
	@echo "=== Running Phase 2 tests ==="
	./test_tree

test-integration: pes
	@echo "=== Running integration tests ==="
	bash test_sequence.sh
