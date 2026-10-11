CFLAGS = -g -I..

TESTS = t1 t_depth

all: $(TESTS)

t1: t1.c ../toml.c ../toml.h
	$(CC) $(CFLAGS) -o $@ t1.c ../toml.c

t_depth: t_depth.c ../toml.c ../toml.h
	$(CC) $(CFLAGS) -o $@ t_depth.c ../toml.c

check: all
	./t1
	./t_depth

clean:
	rm -f $(TESTS)

.PHONY: all check clean
