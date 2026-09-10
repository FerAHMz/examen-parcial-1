# Makefile del examen parcial 1 - Consultoria HPC con OpenMP
#
#   make            compila las cuatro versiones en bin/
#   make run-sec    corre el programa secuencial (red de 10 usuarios)
#   make run-demo   corre la demo paralela (misma red, para comparar salidas)
#   make bench      corre el benchmark de BFS incluido en nodos-paralelo
#   make resultados INTEGRANTE="Nombre" ejecuta todas las pruebas y graficas
#   make clean
#
# En macOS el 'gcc' del sistema es clang y no trae OpenMP: se usa libomp de
# Homebrew (brew install libomp). En Linux basta con gcc -fopenmp.

CC      ?= cc
CFLAGS  ?= -O2 -Wall
BIN      = bin

UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
  LIBOMP   := $(shell brew --prefix libomp 2>/dev/null)
  OMPFLAGS := -Xpreprocessor -fopenmp -I$(LIBOMP)/include
  OMPLIBS  := -L$(LIBOMP)/lib -lomp
else
  OMPFLAGS := -fopenmp
  OMPLIBS  :=
endif

all: $(BIN)/nodos-secuencia $(BIN)/nodos-paralelo \
	$(BIN)/histograma-secuencial $(BIN)/histograma-paralelo

$(BIN):
	mkdir -p $(BIN)

$(BIN)/nodos-secuencia: secuencial/nodos-secuencia.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN)/nodos-paralelo: paralelo/nodos-paralelo.c | $(BIN)
	$(CC) $(CFLAGS) $(OMPFLAGS) -o $@ $< $(OMPLIBS) -lm

$(BIN)/histograma-secuencial: secuencial/histograma_secuencial.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN)/histograma-paralelo: paralelo/histograma_paralelo.c | $(BIN)
	$(CC) $(CFLAGS) $(OMPFLAGS) -o $@ $< $(OMPLIBS)

run-sec: $(BIN)/nodos-secuencia
	./$(BIN)/nodos-secuencia

run-demo: $(BIN)/nodos-paralelo
	./$(BIN)/nodos-paralelo --demo

# Tamano por defecto: 8,000,000 usuarios con 16 amistades promedio.
# Para una maquina con menos RAM: make bench USUARIOS=2000000 GRADO=12
USUARIOS ?= 8000000
GRADO ?= 16
REPS  ?= 3
bench: $(BIN)/nodos-paralelo
	./$(BIN)/nodos-paralelo $(USUARIOS) $(GRADO) $(REPS)

INTEGRANTE ?= Sin identificar
resultados:
	python3 pruebas_rendimiento.py --integrante "$(INTEGRANTE)"

clean:
	rm -rf $(BIN)

.PHONY: all run-sec run-demo bench resultados clean
