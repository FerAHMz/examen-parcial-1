/*
* nodos-paralelo.c
* Solucion paralela para encontrar la menor cantidad de conexiones entre
* dos personas de una red social. Mantiene el recorrido BFS por niveles del
* algoritmo secuencial y reparte con OpenMP la expansion de cada nivel.
*
* Grupo: Joel Jaquez - 23369, Fernando Hernandez - 23645, Carlos Alburez - 23311
* Compilacion (macOS): clang -O2 -Wall -Xpreprocessor -fopenmp \
*   -I$(brew --prefix libomp)/include -L$(brew --prefix libomp)/lib -lomp \
*   -o nodos-paralelo nodos-paralelo.c -lm
* Compilacion (Linux): gcc -O2 -Wall -fopenmp -o nodos-paralelo nodos-paralelo.c -lm
* Ejecucion: ./nodos-paralelo [numUsuarios] [gradoMedio] [repeticiones]
*            ./nodos-paralelo --demo
*
* El binario genera la red, corre ambas versiones sobre ella, compara los
* resultados y reporta speedup y eficiencia.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <omp.h>

/* Representacion del grafo mediante una lista de amigos para cada usuario. */
typedef struct {
    int   numNodos;    /* Numero total de usuarios almacenados. */
    long  numAristas;  /* Cantidad de amistades distintas registradas. */
    int  *numAmigos;   /* Cantidad de amistades de cada usuario. */
    int **amigos;      /* Identificadores de los vecinos conectados a cada usuario. */
    int  *bloque;      /* Almacen contiguo donde viven todas las listas de amigos. */
} Grafo;

/* Libera el bloque compartido y despues la estructura completa. */
void liberarGrafo(Grafo *g) {
    free(g->bloque);
    free(g->amigos);
    free(g->numAmigos);
    free(g);
}

/*
 * Generador xorshift de 64 bits. Se usa en lugar de rand() porque la red se
 * construye en dos recorridos, uno para contar amigos y otro para escribirlos:
 * con la misma semilla ambos producen las mismas amistades y no hace falta
 * guardar la lista completa en memoria.
 */
typedef struct { uint64_t s; } RNG;

static inline uint32_t siguienteAleatorio(RNG *r) {
    r->s ^= r->s << 13;
    r->s ^= r->s >> 7;
    r->s ^= r->s << 17;
    return (uint32_t)(r->s >> 32);
}

/* Entero repartido de forma pareja dentro del rango [0, n). */
static inline int uniforme(RNG *r, int n) {
    return (int)(siguienteAleatorio(r) % (uint32_t)n);
}

/* Entero cargado hacia los identificadores bajos, para crear usuarios muy populares. */
static inline int sesgada(RNG *r, int n) {
    double u = (siguienteAleatorio(r) + 1.0) / 4294967297.0;
    int v = (int)(u * u * n);
    return (v >= n) ? n - 1 : v;
}

/*
 * Construye la red de prueba. Cada usuario i se enlaza con alguno anterior
 * elegido al azar, lo que deja a todos conectados en una red poco profunda;
 * las amistades adicionales usan un extremo sesgado y generan los usuarios con
 * miles de amigos. El ultimo usuario queda aislado a proposito.
 */
Grafo *generarRedSocial(int n, int gradoMedio, uint64_t semilla) {
    Grafo *g = (Grafo *)malloc(sizeof(Grafo));
    g->numNodos  = n;
    g->numAmigos = (int *)calloc(n, sizeof(int));

    long amistadesExtra = ((long)n * gradoMedio) / 2 - (n - 2);
    if (amistadesExtra < 0) amistadesExtra = 0;
    g->numAristas = (n - 2) + amistadesExtra;

    /* Primer recorrido: cuenta cuantos amigos tendra cada usuario. */
    RNG r = { semilla };
    for (int i = 1; i < n - 1; i++) {
        int j = uniforme(&r, i);
        g->numAmigos[i]++; g->numAmigos[j]++;
    }
    for (long e = 0; e < amistadesExtra; e++) {
        int u = sesgada(&r, n - 1);
        int v = uniforme(&r, n - 1);
        if (u == v) continue;              /* Descarta la amistad de alguien consigo mismo. */
        g->numAmigos[u]++; g->numAmigos[v]++;
    }

    /*
     * Todas las listas comparten un solo bloque y amigos[i] apunta dentro de el.
     * Con millones de usuarios eso evita millones de realloc y deja los vecinos
     * contiguos, que es lo que mas pesa en el tiempo del recorrido.
     */
    long total = 0;
    for (int i = 0; i < n; i++) total += g->numAmigos[i];
    g->bloque = (int  *)malloc((size_t)total * sizeof(int));
    g->amigos = (int **)malloc((size_t)n * sizeof(int *));

    long off = 0;
    for (int i = 0; i < n; i++) { g->amigos[i] = g->bloque + off; off += g->numAmigos[i]; }

    /* Segundo recorrido: repite las mismas amistades y ahora si las escribe. */
    int *cursor = (int *)calloc(n, sizeof(int));   /* Posicion libre en la lista de cada usuario. */
    r.s = semilla;
    for (int i = 1; i < n - 1; i++) {
        int j = uniforme(&r, i);
        g->amigos[i][cursor[i]++] = j;
        g->amigos[j][cursor[j]++] = i;
    }
    for (long e = 0; e < amistadesExtra; e++) {
        int u = sesgada(&r, n - 1);
        int v = uniforme(&r, n - 1);
        if (u == v) continue;
        g->amigos[u][cursor[u]++] = v;
        g->amigos[v][cursor[v]++] = u;
    }
    free(cursor);
    return g;
}

/*
 * BFS secuencial del programa original. No imprime: solo llena distancia e
 * invitadoPor, porque sirve de referencia para medir y verificar el paralelo.
 */
bool bfsSecuencial(const Grafo *g, int X, int Y, int *distancia, int *invitadoPor) {
    int n = g->numNodos;
    bool *visitado = (bool *)calloc(n, sizeof(bool)); /* Indica si el nodo ya fue descubierto. */
    int  *cola     = (int  *)malloc((size_t)n * sizeof(int));
    int   frente = 0;   /* Posicion del siguiente usuario que se atendera. */
    int   final  = 0;   /* Posicion donde se insertara el proximo usuario. */

    for (int i = 0; i < n; i++) {
        invitadoPor[i] = -1;   /* El valor -1 significa que aun no tiene predecesor. */
        distancia[i]   = -1;
    }

    cola[final++] = X;         /* X es el primer elemento pendiente. */
    visitado[X]   = true;      /* Evita que el origen vuelva a agregarse. */
    distancia[X]  = 0;         /* Se necesitan cero saltos para llegar al origen. */
    bool encontrado = false;   /* Permanece falso hasta localizar el destino. */

    while (frente < final) {
        int u = cola[frente++];           /* Extrae el usuario situado al frente. */
        if (u == Y) {                     /* Finaliza en cuanto BFS alcanza el destino. */
            encontrado = true;
            break;
        }
        for (int k = 0; k < g->numAmigos[u]; k++) {
            int v = g->amigos[u][k];               /* Obtiene el siguiente vecino de u. */
            if (!visitado[v]) {                    /* Solo procesa usuarios no descubiertos. */
                visitado[v]    = true;             /* Lo marca al encontrarlo para no repetirlo. */
                invitadoPor[v] = u;                /* Guarda desde cual usuario se llego hasta v. */
                distancia[v]   = distancia[u] + 1; /* Suma una conexion a la distancia de u. */
                cola[final++]  = v;                /* Deja al nuevo usuario pendiente. */
            }
        }
    }
    free(visitado);
    free(cola);
    return encontrado;
}

/*
 * Version paralela del mismo recorrido. La cola unica se reemplaza por dos
 * arreglos, la frontera del nivel actual y la del siguiente. Los usuarios de un
 * nivel estan a la misma distancia del origen, asi que expandirlos son tareas
 * independientes que se reparten entre los hilos. La espera al cerrar cada
 * nivel es la que conserva la garantia de ruta minima.
 *
 * Directivas empleadas:
 * - for schedule(dynamic, 64): un usuario puede tener dos amigos o miles, asi
 *   que el reparto fijo dejaria a un hilo con todo el trabajo. El bloque de 64
 *   evita que pedir trabajo cueste mas que hacerlo.
 * - nowait: cada hilo escribe en una zona propia, no necesita esperar ahi.
 * - atomic capture sobre visitado[v]: consultar la marca y ponerla en una sola
 *   operacion indivisible. Sin ella dos hilos darian por nuevo al mismo usuario
 *   y lo agregarian dos veces. Quien recibe la marca anterior en cero es su
 *   unico dueno, y por eso su predecesor y su distancia no necesitan proteccion.
 * - atomic read previo: casi todos los vecinos de un usuario popular ya fueron
 *   descubiertos; se consulta primero sin modificar y solo se paga la operacion
 *   completa cuando el usuario parece nuevo.
 * - arreglo privado por hilo: agregar cada hallazgo directo a la frontera
 *   siguiente volveria a serializar el encolado, asi que cada hilo acumula
 *   aparte y reserva su espacio de una sola vez.
 */
bool bfsParalelo(const Grafo *g, int X, int Y, int *distancia, int *invitadoPor,
                 int numHilos) {
    int n = g->numNodos;

    int *visitado  = (int *)calloc(n, sizeof(int));            /* Marca de descubierto, en int para tratarla de forma indivisible. */
    int *frontera  = (int *)malloc((size_t)n * sizeof(int));   /* Usuarios del nivel actual. */
    int *siguiente = (int *)malloc((size_t)n * sizeof(int));   /* Usuarios del nivel siguiente. */

    #pragma omp parallel for schedule(static) num_threads(numHilos)
    for (int i = 0; i < n; i++) {
        invitadoPor[i] = -1;   /* El valor -1 significa que aun no tiene predecesor. */
        distancia[i]   = -1;
    }

    frontera[0]  = X;          /* El nivel cero lo forma unicamente el origen. */
    visitado[X]  = 1;          /* Evita que el origen vuelva a agregarse. */
    distancia[X] = 0;          /* Se necesitan cero saltos para llegar al origen. */
    int tamFrontera  = 1;
    int tamSiguiente = 0;
    int encontrado = (X == Y) ? 1 : 0;   /* Permanece en cero hasta localizar el destino. */
    int nivel = 0;             /* Distancia en saltos del nivel que se procesa. */

    while (tamFrontera > 0 && !encontrado) {
        tamSiguiente = 0;
        nivel++;

        #pragma omp parallel num_threads(numHilos)
        {
            /* Cada hilo acumula aqui los usuarios que se adjudico. */
            int  capLocal = 1024;
            int *local    = (int *)malloc((size_t)capLocal * sizeof(int));
            int  nLocal   = 0;

            #pragma omp for schedule(dynamic, 64) nowait
            for (int i = 0; i < tamFrontera; i++) {
                int u = frontera[i];

                for (int k = 0; k < g->numAmigos[u]; k++) {
                    int v = g->amigos[u][k];       /* Obtiene el siguiente vecino de u. */

                    int marca;                     /* Consulta previa, sin modificar la marca. */
                    #pragma omp atomic read
                    marca = visitado[v];
                    if (marca) continue;

                    int previo;                    /* Reclamo: solo un hilo recibe cero. */
                    #pragma omp atomic capture
                    { previo = visitado[v]; visitado[v] = 1; }
                    if (previo) continue;          /* Otro hilo se lo adjudico primero. */

                    invitadoPor[v] = u;            /* Guarda desde cual usuario se llego hasta v. */
                    distancia[v]   = nivel;        /* Todo el nivel esta a la misma cantidad de saltos. */

                    if (v == Y) {                  /* Aviso de destino alcanzado para los demas hilos. */
                        #pragma omp atomic write
                        encontrado = 1;
                    }

                    if (nLocal == capLocal) {      /* Amplia el arreglo privado si se lleno. */
                        capLocal *= 2;
                        local = (int *)realloc(local, (size_t)capLocal * sizeof(int));
                    }
                    local[nLocal++] = v;           /* Deja al nuevo usuario para el nivel siguiente. */
                }
            }

            /* Traslado con una sola reserva indivisible por hilo y por nivel. */
            if (nLocal > 0) {
                int base;
                #pragma omp atomic capture
                { base = tamSiguiente; tamSiguiente += nLocal; }
                memcpy(siguiente + base, local, (size_t)nLocal * sizeof(int));
            }
            free(local);
        }   /* El cierre de la region paralela espera a todos y cierra el nivel. */

        int *tmp = frontera; frontera = siguiente; siguiente = tmp;
        tamFrontera = tamSiguiente;
    }

    free(visitado);
    free(frontera);
    free(siguiente);
    return encontrado != 0;
}

/*
 * Recorre los predecesores desde Y e imprime la secuencia al reves. Si no hay
 * arreglo de nombres, como en la red grande, imprime los identificadores.
 */
void imprimirCamino(int X, int Y, const int *invitadoPor, const int *distancia,
                    const char *nombres[], int n) {
    if (distancia[Y] < 0) {
        if (nombres) printf("%s y %s no estan conectados.\n", nombres[X], nombres[Y]);
        else         printf("Los usuarios %d y %d no estan conectados.\n", X, Y);
        return;
    }
    int *ruta = (int *)malloc((size_t)n * sizeof(int));
    int largo = 0;
    for (int nodo = Y; nodo != -1; nodo = invitadoPor[nodo])
        ruta[largo++] = nodo;

    if (nombres) {
        printf("Camino mas corto entre %s y %s (%d saltos):\n",
               nombres[X], nombres[Y], distancia[Y]);
        for (int i = largo - 1; i >= 0; i--) {
            printf("%s", nombres[ruta[i]]);
            if (i > 0) printf(" -> ");
        }
    } else {
        printf("Camino mas corto entre el usuario %d y el %d (%d saltos):\n",
               X, Y, distancia[Y]);
        for (int i = largo - 1; i >= 0; i--) {
            printf("%d", ruta[i]);
            if (i > 0) printf(" -> ");
        }
    }
    printf("\n");
    free(ruta);
}

/*
 * Cuenta cuantas distancias difieren entre las dos versiones. Se comparan las
 * distancias y no los predecesores porque cual hilo reclama primero a un amigo
 * compartido cambia entre ejecuciones; la distancia minima, en cambio, no.
 */
int verificarDistancias(const int *a, const int *b, int n) {
    int errores = 0;
    for (int i = 0; i < n; i++) if (a[i] != b[i]) errores++;
    return errores;
}

/* Repite la red de diez usuarios del secuencial para comparar las salidas. */
int modoDemo(void) {
    /* Asocia cada identificador numerico con un nombre facil de reconocer. */
    const char *nombres[] = {
        "Ana",   /* 0 */
        "Beto",  /* 1 */
        "Carla", /* 2 */
        "Diego", /* 3 */
        "Elena", /* 4 */
        "Fito",  /* 5 */
        "Gaby",  /* 6 */
        "Hugo",  /* 7 */
        "Ivan",  /* 8 */
        "Juana"  /* 9 */
    };
    int numUsuarios = 10;

    /* Las mismas amistades del secuencial, con el grupo aislado al final. */
    int amistades[][2] = { {0,1},{0,2},{1,3},{2,4},{3,5},{4,5},{4,6},{5,7},{6,7},{8,9} };
    int numAmistades = 10;

    Grafo *g = (Grafo *)malloc(sizeof(Grafo));
    g->numNodos   = numUsuarios;
    g->numAristas = numAmistades;
    g->numAmigos  = (int *)calloc(numUsuarios, sizeof(int));
    for (int e = 0; e < numAmistades; e++) {
        g->numAmigos[amistades[e][0]]++;
        g->numAmigos[amistades[e][1]]++;
    }
    g->bloque = (int  *)malloc((size_t)(2 * numAmistades) * sizeof(int));
    g->amigos = (int **)malloc((size_t)numUsuarios * sizeof(int *));
    int off = 0;
    for (int i = 0; i < numUsuarios; i++) {
        g->amigos[i] = g->bloque + off;
        off += g->numAmigos[i];
    }
    int *cursor = (int *)calloc(numUsuarios, sizeof(int));
    for (int e = 0; e < numAmistades; e++) {
        int u = amistades[e][0], v = amistades[e][1];
        g->amigos[u][cursor[u]++] = v;
        g->amigos[v][cursor[v]++] = u;
    }
    free(cursor);

    int *distancia   = (int *)malloc((size_t)numUsuarios * sizeof(int));
    int *invitadoPor = (int *)malloc((size_t)numUsuarios * sizeof(int));

    printf("=== Red social: busqueda de ruta minima (BFS paralela con OpenMP) ===\n\n");

    /* Primer caso: existe una ruta entre el origen y el destino. */
    printf("[Consulta 1] Ana -> Hugo\n");
    bfsParalelo(g, 0, 7, distancia, invitadoPor, omp_get_max_threads());
    imprimirCamino(0, 7, invitadoPor, distancia, nombres, numUsuarios);
    printf("\n");

    /* Segundo caso: los usuarios pertenecen a componentes diferentes. */
    printf("[Consulta 2] Ana -> Ivan\n");
    bfsParalelo(g, 0, 8, distancia, invitadoPor, omp_get_max_threads());
    imprimirCamino(0, 8, invitadoPor, distancia, nombres, numUsuarios);
    printf("\n");

    free(distancia);
    free(invitadoPor);
    liberarGrafo(g);
    return 0;
}

/* Construye la red grande, mide las dos versiones y reporta las metricas. */
int main(int argc, char **argv) {

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) return modoDemo();

    int numUsuarios   = (argc > 1) ? atoi(argv[1]) : 8000000;
    int gradoMedio    = (argc > 2) ? atoi(argv[2]) : 16;
    int repeticiones  = (argc > 3) ? atoi(argv[3]) : 3;
    int maxHilos      = omp_get_max_threads();

    printf("=== Ruta minima en una red social: version secuencial contra OpenMP ===\n\n");
    printf("Usuarios           : %d\n", numUsuarios);
    printf("Amistades promedio : %d\n", gradoMedio);
    printf("Repeticiones       : %d (se reporta el mejor tiempo)\n", repeticiones);
    printf("Hilos disponibles  : %d\n\n", maxHilos);

    printf("Generando la red social de prueba...\n");
    double t0 = omp_get_wtime();
    Grafo *g = generarRedSocial(numUsuarios, gradoMedio, 88172645463325252ULL);
    double tGeneracion = omp_get_wtime() - t0;

    /* El usuario mas popular evidencia el desbalance que justifica el reparto dinamico. */
    int  amigosMax = 0, usuarioMax = 0;
    long suma = 0;
    for (int i = 0; i < numUsuarios; i++) {
        suma += g->numAmigos[i];
        if (g->numAmigos[i] > amigosMax) { amigosMax = g->numAmigos[i]; usuarioMax = i; }
    }
    printf("Red lista en %.2f s | amistades: %ld | promedio real: %.1f | "
           "usuario mas popular: %d amigos (usuario %d)\n\n",
           tGeneracion, g->numAristas, (double)suma / numUsuarios, amigosMax, usuarioMax);

    /*
     * Se busca al usuario aislado para que el recorrido nunca termine antes de
     * tiempo y siempre visite la red completa: asi cada medicion corresponde a
     * la misma cantidad de trabajo.
     */
    int X = 0;
    int Y = numUsuarios - 1;

    int *distanciaSec   = (int *)malloc((size_t)numUsuarios * sizeof(int));
    int *invitadoPorSec = (int *)malloc((size_t)numUsuarios * sizeof(int));
    int *distanciaPar   = (int *)malloc((size_t)numUsuarios * sizeof(int));
    int *invitadoPorPar = (int *)malloc((size_t)numUsuarios * sizeof(int));

    /* Referencia secuencial: se conserva el mejor de todos los intentos. */
    double tSecuencial = 1e30;
    for (int rep = 0; rep < repeticiones; rep++) {
        double t = omp_get_wtime();
        bfsSecuencial(g, X, Y, distanciaSec, invitadoPorSec);
        t = omp_get_wtime() - t;
        if (t < tSecuencial) tSecuencial = t;
    }

    long alcanzados = 0, nivelMax = 0;
    for (int i = 0; i < numUsuarios; i++)
        if (distanciaSec[i] >= 0) {
            alcanzados++;
            if (distanciaSec[i] > nivelMax) nivelMax = distanciaSec[i];
        }
    printf("Recorrido de referencia: %ld usuarios alcanzados desde el usuario %d, "
           "repartidos en %ld niveles\n\n", alcanzados, X, nivelMax);

    printf("%-8s %-14s %-12s %-12s %-10s\n",
           "Hilos", "Tiempo (s)", "Speedup", "Eficiencia", "Correcto");
    printf("---------------------------------------------------------------\n");
    printf("%-8s %-14.4f %-12s %-12s %-10s\n", "1 (sec)", tSecuencial, "1.00", "100.0%", "-");

    /* Version paralela con una cantidad creciente de hilos. */
    for (int h = 1; h <= maxHilos; h = (h == 1) ? 2 : h + 2) {
        double tParalelo = 1e30;
        for (int rep = 0; rep < repeticiones; rep++) {
            double t = omp_get_wtime();
            bfsParalelo(g, X, Y, distanciaPar, invitadoPorPar, h);
            t = omp_get_wtime() - t;
            if (t < tParalelo) tParalelo = t;
        }
        int    errores    = verificarDistancias(distanciaSec, distanciaPar, numUsuarios);
        double speedup    = tSecuencial / tParalelo;
        double eficiencia = speedup / h * 100.0;

        char etiqueta[16], textoEficiencia[16];
        snprintf(etiqueta, sizeof(etiqueta), "%d", h);
        snprintf(textoEficiencia, sizeof(textoEficiencia), "%.1f%%", eficiencia);
        printf("%-8s %-14.4f %-12.2f %-12s %-10s\n",
               etiqueta, tParalelo, speedup, textoEficiencia,
               errores == 0 ? "si" : "NO");
        if (errores)
            printf("   *** %d distancias no coinciden con la version secuencial ***\n", errores);
    }
    printf("---------------------------------------------------------------\n");
    printf("Speedup = tiempo secuencial entre tiempo paralelo\n");
    printf("Eficiencia = speedup entre la cantidad de hilos\n\n");

    /* Consulta de ejemplo sobre la red grande, entre dos usuarios si conectados. */
    int destino = numUsuarios / 2;
    printf("[Consulta de ejemplo] usuario %d -> usuario %d\n", X, destino);
    bfsParalelo(g, X, destino, distanciaPar, invitadoPorPar, maxHilos);
    imprimirCamino(X, destino, invitadoPorPar, distanciaPar, NULL, numUsuarios);

    free(distanciaSec);
    free(invitadoPorSec);
    free(distanciaPar);
    free(invitadoPorPar);
    liberarGrafo(g);
    return 0;
}
