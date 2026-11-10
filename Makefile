CC      ?= gcc
CFLAGS  := -O2 -std=c11 -Wall -Wextra
LDLIBS  := -lm

all: cv_seq cv_par

cv_seq: coef_variacao.c
	$(CC) $(CFLAGS) -Wno-unknown-pragmas -o $@ $< $(LDLIBS)

cv_par: coef_variacao.c
	$(CC) $(CFLAGS) -fopenmp -o $@ $< $(LDLIBS)

dados.txt:
	python3 gerar_dados.py 1000 dados.txt

run: all dados.txt
	./cv_seq dados.txt
	./cv_par dados.txt

bench: all
	./benchmark.sh

test: all
	./testes/rodar_testes.sh

clean:
	rm -f cv_seq cv_par

.PHONY: all run bench test clean
