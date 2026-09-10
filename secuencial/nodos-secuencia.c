/*
* nodos-secuencia.c
* Solucion secuencial para encontrar la menor cantidad de conexiones entre
* dos personas de una red social. El recorrido BFS examina el grafo por niveles
* y utiliza una cola para conservar los usuarios pendientes de visitar.
*
* Grupo: Joel Jaquez - 23369, Fernando Hernandez - 23645, Carlos Alburez - 23311
* Compilacion: gcc -O2 -Wall -o nodos-secuencia nodos-secuencia.c
* Ejecucion: ./nodos-secuencia
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
/* Representacion del grafo mediante una lista de amigos para cada usuario. */
typedef struct {
int numNodos; /* Numero total de usuarios almacenados. */
int *numAmigos; /* Cantidad actual de amistades de cada usuario. */
int *capAmigos; /* Espacio disponible en la lista de cada usuario. */
int **amigos; /* Identificadores de los vecinos conectados a cada usuario. */
} Grafo;
/* Reserva e inicializa un grafo sin amistades para la cantidad indicada. */
Grafo *crearGrafo(int numNodos) {
Grafo *g = (Grafo *)malloc(sizeof(Grafo));
g->numNodos = numNodos;
g->numAmigos = (int *)calloc(numNodos, sizeof(int));
g->capAmigos = (int *)calloc(numNodos, sizeof(int));
g->amigos = (int **)calloc(numNodos, sizeof(int *));
return g;
}
/* Registra a v como vecino de u; esta funcion agrega solamente una direccion. */
static void agregarAdyacencia(Grafo *g, int u, int v) {
if (g->numAmigos[u] == g->capAmigos[u]) {
int nuevaCap = (g->capAmigos[u] == 0) ? 4 : g->capAmigos[u] * 2;
g->amigos[u] = (int *)realloc(g->amigos[u], nuevaCap * sizeof(int));
g->capAmigos[u] = nuevaCap;
}
g->amigos[u][g->numAmigos[u]++] = v;
}
/* Crea una amistad mutua almacenando la conexion tanto en u como en v. */
void agregarAmistad(Grafo *g, int u, int v) {
agregarAdyacencia(g, u, v);
agregarAdyacencia(g, v, u);
}
/* Libera primero las listas individuales y despues la estructura completa. */
void liberarGrafo(Grafo *g) {
for (int i = 0; i < g->numNodos; i++)
free(g->amigos[i]);
free(g->amigos);
free(g->numAmigos);
free(g->capAmigos);
free(g);
}
/*
 * Recorre la red por niveles desde X mediante Busqueda en Amplitud (BFS).
 * Si alcanza a Y, reconstruye e imprime la ruta minima usando el predecesor
 * guardado en invitadoPor. Devuelve false cuando no existe una conexion.
 */
bool bfsRutaMinima(const Grafo *g, int X, int Y,
const char *nombres[]) {
int n = g->numNodos;
/* Datos necesarios para controlar el avance y reconstruir el resultado. */
bool *visitado = (bool *)calloc(n, sizeof(bool)); /* Indica si el nodo ya fue descubierto. */
int *invitadoPor = (int *)malloc(n * sizeof(int)); /* Predecesor del nodo dentro del recorrido. */
int *distancia = (int *)malloc(n * sizeof(int)); /* Numero de conexiones recorridas desde X. */
/* Cola FIFO representada por un arreglo y dos indices de control. */
int *cola = (int *)malloc(n * sizeof(int));
int frente = 0; /* Posicion del siguiente usuario que se atendera. */
int final = 0; /* Posicion donde se insertara el proximo usuario. */
for (int i = 0; i < n; i++) {
invitadoPor[i] = -1; /* El valor -1 significa que aun no tiene predecesor. */
distancia[i] = -1;
}
/* El recorrido comienza colocando al usuario de origen en la cola. */
cola[final++] = X; /* X es el primer elemento pendiente. */
visitado[X] = true; /* Evita que el origen vuelva a agregarse. */
invitadoPor[X] = -1; /* El origen no procede de ningun otro nodo. */
distancia[X] = 0; /* Se necesitan cero saltos para llegar al origen. */
bool encontrado = false; /* Permanece falso hasta localizar el destino. */
/* Procesa usuarios mientras queden elementos pendientes en la cola. */
while (frente < final) { /* La cola contiene elementos si frente es menor que final. */
int u = cola[frente++]; /* Extrae el usuario situado al frente. */
if (u == Y) { /* Finaliza en cuanto BFS alcanza el destino. */
encontrado = true;
break; /* No es necesario explorar los niveles posteriores. */
}
/* Examina todas las conexiones directas del usuario actual. */
for (int k = 0; k < g->numAmigos[u]; k++) {
int v = g->amigos[u][k]; /* Obtiene el siguiente vecino de u. */
if (!visitado[v]) { /* Solo procesa usuarios que aun no se han descubierto. */
visitado[v] = true; /* Lo marca al encontrarlo para no repetirlo. */
invitadoPor[v] = u; /* Guarda desde cual usuario se llego hasta v. */
distancia[v] = distancia[u] + 1; /* Suma una conexion a la distancia de u. */
cola[final++] = v; /* Deja al nuevo usuario pendiente de exploracion. */
}
/* Los vecinos ya visitados se omiten y el ciclo continua con el siguiente. */
}
}
/* Presenta la ruta encontrada o informa que los usuarios estan separados. */
if (encontrado) {
/* Recorre los predecesores desde Y y luego imprime la secuencia al reves. */
int *ruta = (int *)malloc(n * sizeof(int));
int largo = 0;
for (int nodo = Y; nodo != -1; nodo = invitadoPor[nodo])
ruta[largo++] = nodo;
printf("Camino mas corto entre %s y %s (%d saltos):\n",
nombres[X], nombres[Y], distancia[Y]);
for (int i = largo - 1; i >= 0; i--) {
printf("%s", nombres[ruta[i]]);
if (i > 0) printf(" -> ");
}
printf("\n");
free(ruta);
} else {
printf("%s y %s no estan conectados.\n", nombres[X], nombres[Y]);
}
free(visitado);
free(invitadoPor);
free(distancia);
free(cola);
return encontrado;
}
/* Construye una red de ejemplo y ejecuta dos casos de prueba. */
int main(void) {
/* Asocia cada identificador numerico con un nombre facil de reconocer. */
const char *nombres[] = {
"Ana", /* 0 */
"Beto", /* 1 */
"Carla", /* 2 */
"Diego", /* 3 */
"Elena", /* 4 */
"Fito", /* 5 */
"Gaby", /* 6 */
"Hugo", /* 7 */
"Ivan", /* 8 */
"Juana" /* 9 */
};
int numUsuarios = 10;
Grafo *g = crearGrafo(numUsuarios);
/* Estas conexiones forman el grupo principal, integrado por los nodos 0 a 7. */
agregarAmistad(g, 0, 1); /* Conexion directa entre Ana y Beto. */
agregarAmistad(g, 0, 2); /* Conexion directa entre Ana y Carla. */
agregarAmistad(g, 1, 3); /* Conexion directa entre Beto y Diego. */
agregarAmistad(g, 2, 4); /* Conexion directa entre Carla y Elena. */
agregarAmistad(g, 3, 5); /* Conexion directa entre Diego y Fito. */
agregarAmistad(g, 4, 5); /* Conexion directa entre Elena y Fito. */
agregarAmistad(g, 4, 6); /* Conexion directa entre Elena y Gaby. */
agregarAmistad(g, 5, 7); /* Conexion directa entre Fito y Hugo. */
agregarAmistad(g, 6, 7); /* Conexion directa entre Gaby y Hugo. */
/* Ivan y Juana forman otro grupo, sin enlaces hacia el componente principal. */
agregarAmistad(g, 8, 9); /* Unica conexion del componente aislado. */
printf("=== Red social: busqueda de ruta minima (BFS secuencial) ===\n\n");
/* Primer caso: existe una ruta entre el origen y el destino. */
printf("[Consulta 1] Ana -> Hugo\n");
bfsRutaMinima(g, 0, 7, nombres);
printf("\n");
/* Segundo caso: los usuarios pertenecen a componentes diferentes. */
printf("[Consulta 2] Ana -> Ivan\n");
bfsRutaMinima(g, 0, 8, nombres);
printf("\n");
liberarGrafo(g);
return 0;
}
