#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX 1000000
#define CUBETAS 100

// Variables globales

float A[MAX]; // Arreglo original con las temperaturas generadas aleatoriamente
float temp[MAX];    // Arreglo donde se ordenan las temperaturas (copia de A[])
float tempAux[MAX]; // Arreglo auxiliar usado por Merge() para fusionar los
                    // subarreglos

int histograma[CUBETAS]; // Contador de frecuencias por cada cubeta del
                         // histograma

int N;      // Cantidad total de temperaturas
int i;      // Índice general de recorrido
int cubeta; // Cubeta actual que se está llenando en el histograma

int izq; // Límite izquierdo (se reutiliza en distintos contextos)
int der; // Límite derecho (se reutiliza en distintos contextos)

float limInf; // Límite inferior de la cubeta actual del histograma
float limSup; // Límite superior de la cubeta actual del histograma

// MERGE
// Combina dos mitades ya ordenadas (arreglo[izq..centro]
// y arreglo[centro+1..der]) en una sola sección ordenada.
// Se apoya en tempAux[] para no perder datos mientras
// se van intercalando los elementos de ambas mitades.

void Merge(float arreglo[], int izq, int centro, int der) {
  int i = izq;        // Puntero que recorre la mitad izquierda
  int j = centro + 1; // Puntero que recorre la mitad derecha
  int k = izq;        // Puntero de escritura en tempAux[]

  // Compara elemento por elemento entre ambas mitades
  // y va colocando en tempAux[] el menor de los dos.
  while (i <= centro && j <= der) {
    if (arreglo[i] <= arreglo[j]) {
      tempAux[k] = arreglo[i];
      i++;
    } else {
      tempAux[k] = arreglo[j];
      j++;
    }

    k++;
  }

  // Si ya se terminó la mitad derecha pero quedan
  // elementos en la izquierda, se copian directamente
  // (ya están ordenados entre sí).
  while (i <= centro) {
    tempAux[k] = arreglo[i];
    i++;
    k++;
  }

  // Igual, pero para el caso de que sobren elementos
  // en la mitad derecha.
  while (j <= der) {
    tempAux[k] = arreglo[j];
    j++;
    k++;
  }

  // Se copia el resultado fusionado desde tempAux[]
  // de regreso al arreglo original, en el mismo rango
  // [izq..der] que se estaba trabajando.
  for (i = izq; i <= der; i++) {
    arreglo[i] = tempAux[i];
  }
}

// MERGESORT
// Ordena arreglo[izq..der] de forma recursiva:
// divide el rango en dos mitades, ordena cada mitad
// por separado, y al final las combina con Merge().

void MergeSort(float arreglo[], int izq, int der) {
  // Si izq >= der, el subarreglo tiene 0 o 1 elementos,
  // así que ya está ordenado y no hay nada que hacer
  // (caso base de la recursión).
  if (izq < der) {
    int centro = (izq + der) / 2;

    // Ordena recursivamente la mitad izquierda [izq..centro]
    MergeSort(arreglo, izq, centro);

    // Ordena recursivamente la mitad derecha [centro+1..der]
    MergeSort(arreglo, centro + 1, der);

    // Aquí es donde realmente se hace el trabajo de "mezclar":
    // una vez que ambas mitades ya están ordenadas por separado,
    // se combinan en una sola sección ordenada.
    Merge(arreglo, izq, centro, der);
  }
}

// HISTOGRAMA
// Recibe temp[] ya completamente ordenado (por MergeSort)
// y cuenta cuántas temperaturas caen en cada una de las
// 100 cubetas, recorriendo el arreglo una sola vez porque
// ya está ordenado (no hace falta buscar, solo avanzar).

void Histograma() {
  // Se inicializan todas las cubetas en cero
  for (i = 0; i < CUBETAS; i++)
    histograma[i] = 0;

  i = 0;
  cubeta = 0;

  // Se determinan el valor mínimo y máximo del arreglo
  // ordenado (el primero y el último elemento)
  float minimo = temp[0];
  float maximo = temp[N - 1];

  // Ancho de cada cubeta: el rango total dividido entre
  // la cantidad de cubetas
  float ancho = (maximo - minimo) / CUBETAS;

  // Si todos los valores son iguales, el ancho daría 0,
  // así que se fuerza a 1 para evitar división por cero
  // más adelante o un ciclo infinito
  if (ancho == 0)
    ancho = 1;

  // Límites de la primera cubeta
  limInf = minimo;
  limSup = minimo + ancho;

  // Recorre las cubetas una por una, y dentro de cada
  // cubeta va avanzando en temp[] mientras los valores
  // sigan perteneciendo a su rango
  while (cubeta < CUBETAS && i < N) {

    // Caso especial de la última cubeta: se usa <= en
    // el límite superior para no dejar fuera al valor
    // máximo del arreglo (el último elemento de temp[])
    if (cubeta == CUBETAS - 1) {
      if (temp[i] >= limInf && temp[i] <= limSup) {
        histograma[cubeta]++;
        i++;
      } else {
        cubeta++;
      }
    } else {
      // Mientras la temperatura actual pertenezca al
      // rango de la cubeta activa, se cuenta y se avanza
      // al siguiente elemento
      if (temp[i] >= limInf && temp[i] < limSup) {
        histograma[cubeta]++;
        i++;
      } else {
        // Si ya no pertenece a esta cubeta, se pasa a la
        // siguiente y se recalculan sus límites
        cubeta++;

        limInf = limSup;
        limSup = limInf + ancho;
      }
    }
  }
}

int main() {
  srand(time(NULL));

  printf("Cantidad de temperaturas: ");
  scanf("%d", &N);

  if (N > MAX) {
    printf("\nError: MAX = %d\n", MAX);
    return 0;
  }

  // Generar temperaturas aleatorias entre -100 y 100

  for (i = 0; i < N; i++) {
    A[i] = -100.0 + (rand() % 20001) / 100.0;
  }

  printf("\nPrimeras 20 temperaturas generadas:\n\n");

  for (i = 0; i < 20 && i < N; i++)
    printf("%.2f ", A[i]);

  // Se copia A[] en temp[] porque MergeSort ordena in-place
  // el arreglo que recibe, y se quiere conservar A[] intacto
  // con los datos originales sin ordenar

  for (i = 0; i < N; i++) {
    temp[i] = A[i];
  }

  printf("\n\nOrdenando...\n");

  // Aquí arranca el ordenamiento: se llama a MergeSort sobre

  MergeSort(temp, 0, N - 1);

  // Al terminar esta llamada, temp[] queda completamente
  // ordenado de menor a mayor

  printf("Ordenamiento terminado.\n");

  // Guardar temperaturas ordenadas en CSV
  // (esto ocurre justo después de terminar el ordenamiento,
  // antes de construir el histograma)

  FILE *csv = fopen("temperaturas_ordenadas.csv", "w");

  if (csv == NULL) {
    printf("No se pudo crear temperaturas_ordenadas.csv\n");
    return 1;
  }

  fprintf(csv, "Indice,Temperatura\n");

  for (i = 0; i < N; i++) {
    fprintf(csv, "%d,%.2f\n", i, temp[i]);
  }

  fclose(csv);

  printf("Archivo generado: temperaturas_ordenadas.csv\n");

  printf("\nPrimeras 20 temperaturas ordenadas:\n\n");

  for (i = 0; i < 20 && i < N; i++)
    printf("%.2f ", temp[i]);

  // A partir de aquí empieza la construcción del histograma,
  // usando el arreglo temp[] ya ordenado por MergeSort

  Histograma();

  printf("\n\nHistograma\n\n");

  for (i = 0; i < CUBETAS; i++) {
    printf("Cubeta %2d : %d\n", i, histograma[i]);
  }

  // Guardar los datos del histograma en un archivo .dat,
  // con el valor central de cada cubeta y su frecuencia

  FILE *archivo = fopen("histograma.dat", "w");

  if (archivo == NULL) {
    printf("\nNo se pudo crear histograma.dat\n");
    return 1;
  }

  float minimo = temp[0];
  float maximo = temp[N - 1];
  float ancho = (maximo - minimo) / CUBETAS;

  for (i = 0; i < CUBETAS; i++) {
    float temperatura = minimo + (i * ancho);

    fprintf(archivo, "%.2f %d\n", temperatura, histograma[i]);
  }

  fclose(archivo);

  // Generar automáticamente la gráfica del histograma

  FILE *gp = popen("gnuplot", "w");

  if (gp == NULL) {
    printf("\nNo se pudo ejecutar gnuplot.\n");
    return 1;
  }

  fprintf(gp, "set terminal pngcairo size 1400,700 enhanced font 'Arial,12'\n");
  fprintf(gp, "set output 'histograma.png'\n");

  fprintf(gp, "set title 'Histograma de Temperaturas'\n");

  fprintf(gp, "set xlabel 'Temperatura (°C)'\n");

  fprintf(gp, "set ylabel 'Frecuencia'\n");

  fprintf(gp, "set xrange [-100:100]\n");

  fprintf(gp, "set xtics -100,20,100\n");

  fprintf(gp, "set grid ytics\n");

  fprintf(gp, "set style fill solid 1.0\n");

  fprintf(gp, "set boxwidth 1.8\n");

  fprintf(gp, "plot 'histograma.dat' using 1:2 "
              "with boxes lc rgb '#2E86DE' "
              "title 'Frecuencia'\n");

  fprintf(gp, "exit\n");

  pclose(gp);

  printf("\n");
  printf("Archivo generado : temperaturas_ordenadas.csv\n");
  printf("Archivo de datos : histograma.dat\n");
  printf("Grafica generada : histograma.png\n");

  return 0;
}