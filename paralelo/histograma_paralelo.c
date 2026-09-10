#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX 1000000
#define CUBETAS 100

// VARIABLES GLOBALES

float A[MAX];
float temp[MAX];
float tempAux[MAX];

int histograma[CUBETAS];

int N;

// MERGE

// Fusiona dos mitades previamente ordenadas.
// Esta parte sigue siendo SECUENCIAL.

void Merge(float arreglo[], int izq, int centro, int der) {
  int i = izq;
  int j = centro + 1;
  int k = izq;

  // Comparar elementos de ambas mitades
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

  // Copiar elementos restantes de la mitad izquierda
  while (i <= centro) {
    tempAux[k] = arreglo[i];
    i++;
    k++;
  }

  // Copiar elementos restantes de la mitad derecha
  while (j <= der) {
    tempAux[k] = arreglo[j];
    j++;
    k++;
  }

  // Copiar resultado al arreglo original
  for (i = izq; i <= der; i++) {
    arreglo[i] = tempAux[i];
  }
}

// MERGESORT

// Ordenamiento secuencial mediante Divide y Venceras.

void MergeSort(float arreglo[], int izq, int der) {
  if (izq < der) {
    int centro = (izq + der) / 2;

    // Ordenar mitad izquierda
    MergeSort(arreglo, izq, centro);

    // Ordenar mitad derecha
    MergeSort(arreglo, centro + 1, der);

    // Fusionar ambas mitades
    Merge(arreglo, izq, centro, der);
  }
}

//==========================================================
// BUSQUEDA BINARIA

// Como temp[] esta ordenado, esta funcion permite localizar
// rapidamente donde comienza un determinado rango.
//
// Retorna la primera posicion donde temp[pos] >= valor.

int BuscarInicio(float valor) {
  int izq = 0;
  int der = N;

  while (izq < der) {
    int centro = izq + (der - izq) / 2;

    if (temp[centro] < valor)
      izq = centro + 1;
    else
      der = centro;
  }

  return izq;
}

// HISTOGRAMA PARALELO

void HistogramaParalelo(int numHilos) {
  int h;
  int c;

  // El arreglo ya esta ordenado.
  float minimo = temp[0];
  float maximo = temp[N - 1];

  float ancho = (maximo - minimo) / CUBETAS;

  if (ancho == 0)
    ancho = 1;

  //------------------------------------------------------
  // histogramaLocal
  //
  // Cada trabajador tiene su propio histograma.
  // De esta forma evitamos que dos hilos escriban
  // simultaneamente sobre el mismo contador.
  //------------------------------------------------------

  int *histogramasLocales = calloc(numHilos * CUBETAS, sizeof(int));

  if (histogramasLocales == NULL) {
    printf("Error al reservar memoria.\n");
    exit(1);
  }

  // Inicializar histograma global
  for (c = 0; c < CUBETAS; c++)
    histograma[c] = 0;

#pragma omp parallel for num_threads(numHilos) schedule(static)                \
    shared(histogramasLocales) firstprivate(minimo, maximo, ancho)

  for (h = 0; h < numHilos; h++) {

    int primeraCubeta = (h * CUBETAS) / numHilos;

    int ultimaCubeta = ((h + 1) * CUBETAS) / numHilos - 1;

    // Cada hilo trabaja con SU histograma local.
    int *histogramaLocal = &histogramasLocales[h * CUBETAS];

    // Calcular el rango completo de temperaturas
    float inicioTemperatura = minimo + primeraCubeta * ancho;

    float finTemperatura;

    if (ultimaCubeta == CUBETAS - 1)
      finTemperatura = maximo;
    else
      finTemperatura = minimo + (ultimaCubeta + 1) * ancho;

    // PASO DEL DIAGRAMA:
    // "Localizar en temp[] el inicio y fin del rango"
    // Aprovechamos que temp[] esta ordenado.

    int inicioRango = BuscarInicio(inicioTemperatura);

    int finRango;

    if (ultimaCubeta == CUBETAS - 1)
      finRango = N;
    else
      finRango = BuscarInicio(finTemperatura);

    // i = inicioRango
    // cubeta = primeraCubetaAsignada

    int iLocal = inicioRango;
    int cubetaLocal = primeraCubeta;

    // Calcular limites de la primera cubeta asignada
    float limInf = minimo + cubetaLocal * ancho;

    float limSup = limInf + ancho;
    // CICLO PRINCIPAL DEL TRABAJADOR

    while (cubetaLocal <= ultimaCubeta && iLocal < finRango) {
      // ¿temp[i] pertenece a la cubeta actual?

      int pertenece;

      // La ultima cubeta incluye el maximo
      if (cubetaLocal == CUBETAS - 1) {
        pertenece = temp[iLocal] >= limInf && temp[iLocal] <= maximo;
      } else {
        pertenece = temp[iLocal] >= limInf && temp[iLocal] < limSup;
      }

      if (pertenece) {
        // histogramaLocal[cubeta]++

        histogramaLocal[cubetaLocal]++;

        iLocal++;
      } else {

        // PASO DEL DIAGRAMA:
        // cubeta = cubeta + 1

        cubetaLocal++;

        // Calcular nuevos limInf y limSup

        limInf = limSup;
        limSup = limInf + ancho;
      }
    }
  }

  for (h = 0; h < numHilos; h++) {
    for (c = 0; c < CUBETAS; c++) {
      histograma[c] += histogramasLocales[h * CUBETAS + c];
    }
  }

  free(histogramasLocales);
}

/*
 * Modo no interactivo utilizado por pruebas_rendimiento.py. Emplea la misma
 * semilla que el secuencial y mide solo la fase que fue paralelizada.
 */
static int ejecutarBenchmark(int cantidad, int numHilos, int repeticiones,
                             unsigned int semilla) {
  if (cantidad <= 0 || cantidad > MAX || numHilos <= 0 || repeticiones <= 0) {
    fprintf(stderr, "Parametros de benchmark invalidos.\n");
    return 1;
  }

  N = cantidad;
  /* Sin esto OpenMP puede ajustar el tamano del equipo por su cuenta y la
   * medicion no correria con la cantidad de hilos que se pidio. */
  omp_set_dynamic(0);
  srand(semilla);
  for (int i = 0; i < N; i++) {
    A[i] = -100.0 + (rand() % 20001) / 100.0;
    temp[i] = A[i];
  }
  MergeSort(temp, 0, N - 1);

  /* Promedia varias llamadas para que la fase corta supere la resolucion del reloj. */
  int iteracionesInternas = 100000000 / N;
  if (iteracionesInternas < 1) iteracionesInternas = 1;
  if (iteracionesInternas > 1000) iteracionesInternas = 1000;
  double mejorFase = 1e30;
  for (int repeticion = 0; repeticion < repeticiones; repeticion++) {
    double inicio = omp_get_wtime();
    for (int interna = 0; interna < iteracionesInternas; interna++)
      HistogramaParalelo(numHilos);
    double promedio = (omp_get_wtime() - inicio) / iteracionesInternas;
    if (promedio < mejorFase)
      mejorFase = promedio;
  }

  /*
   * total confirma que no se perdio ningun dato. checksum pondera cada cubeta
   * por su indice, de modo que delata un conteo mal repartido aunque el total
   * cuadre: es la comprobacion de que los histogramas locales se combinaron
   * bien y ningun hilo piso el contador de otro.
   */
  long long total = 0;
  long long checksum = 0;
  for (int c = 0; c < CUBETAS; c++) {
    total += histograma[c];
    checksum += (long long)(c + 1) * histograma[c];
  }

  /* Linea que lee pruebas_rendimiento.py. Campos: problema, version, N,
   * hilos, repeticiones, iteraciones internas, tiempo de una llamada,
   * datos contabilizados y checksum. Debe coincidir con el del secuencial. */
  printf("RESULTADO,histograma,paralelo,%d,%d,%d,%d,%.9f,%lld,%lld\n",
         N, numHilos, repeticiones, iteracionesInternas, mejorFase, total,
         checksum);
  return total == N ? 0 : 1;
}

int main(int argc, char *argv[]) {
  /* Con --benchmark ejecuta la medicion y termina. Sin argumentos sigue de
   * largo al modo interactivo original, que no fue modificado. */
  if (argc > 1 && strcmp(argv[1], "--benchmark") == 0) {
    int cantidad = (argc > 2) ? atoi(argv[2]) : MAX;
    int numHilos = (argc > 3) ? atoi(argv[3]) : omp_get_max_threads();
    int repeticiones = (argc > 4) ? atoi(argv[4]) : 10;
    unsigned int semilla = (argc > 5) ? (unsigned int)strtoul(argv[5], NULL, 10)
                                       : 20260909U;
    return ejecutarBenchmark(cantidad, numHilos, repeticiones, semilla);
  }

  int i;
  int numHilos;

  srand(time(NULL));

  printf("Cantidad de temperaturas: ");
  scanf("%d", &N);

  if (N <= 0 || N > MAX) {
    printf("\nError: N debe estar entre 1 y %d\n", MAX);
    return 1;
  }

  printf("Cantidad de trabajadores (hilos): ");
  scanf("%d", &numHilos);

  if (numHilos <= 0) {
    printf("\nNumero de hilos invalido.\n");
    return 1;
  }

  for (i = 0; i < N; i++) {
    A[i] = -100.0 + (rand() % 20001) / 100.0;
  }

  printf("\nPrimeras 20 temperaturas generadas:\n\n");

  for (i = 0; i < 20 && i < N; i++)
    printf("%.2f ", A[i]);

  // COPIAR A[] EN temp[]

  for (i = 0; i < N; i++) {
    temp[i] = A[i];
  }

  // MERGE SORT SECUENCIAL

  printf("\n\nOrdenando con Merge Sort...\n");

  double inicioOrdenamiento = omp_get_wtime();

  MergeSort(temp, 0, N - 1);

  double finOrdenamiento = omp_get_wtime();

  printf("Ordenamiento terminado.\n");

  // En este punto temp[] ya esta completamente ordenado.

  printf("\nPrimeras 20 temperaturas ordenadas:\n\n");

  for (i = 0; i < 20 && i < N; i++)
    printf("%.2f ", temp[i]);

  // GUARDAR ARREGLO ORDENADO

  FILE *csv = fopen("temperaturas_ordenadas.csv", "w");

  if (csv == NULL) {
    printf("\nNo se pudo crear temperaturas_ordenadas.csv\n");
    return 1;
  }

  fprintf(csv, "Indice,Temperatura\n");

  for (i = 0; i < N; i++) {
    fprintf(csv, "%d,%.2f\n", i, temp[i]);
  }

  fclose(csv);

  // HISTOGRAMA PARALELO

  printf("\n\nConstruyendo histograma con %d hilos...\n", numHilos);

  double inicioHistograma = omp_get_wtime();

  HistogramaParalelo(numHilos);

  double finHistograma = omp_get_wtime();

  printf("Histograma terminado.\n");

  // MOSTRAR HISTOGRAMA GLOBAL

  printf("\nHistograma Global\n\n");

  for (i = 0; i < CUBETAS; i++) {
    printf("Cubeta %2d : %d\n", i, histograma[i]);
  }

  long long total = 0;

#pragma omp parallel for reduction(+ : total) num_threads(numHilos)

  for (i = 0; i < CUBETAS; i++) {
    total += histograma[i];
  }

  printf("\nTemperaturas originales : %d\n", N);
  printf("Temperaturas contabilizadas: %lld\n", total);

  if (total == N)
    printf("Conteo correcto.\n");
  else
    printf("ERROR: faltan temperaturas por contabilizar.\n");

  // GUARDAR DATOS DEL HISTOGRAMA

  FILE *archivo = fopen("histograma.dat", "w");

  if (archivo == NULL) {
    printf("\nNo se pudo crear histograma.dat\n");
    return 1;
  }

  float minimo = temp[0];
  float maximo = temp[N - 1];
  float ancho = (maximo - minimo) / CUBETAS;

  if (ancho == 0)
    ancho = 1;

  for (i = 0; i < CUBETAS; i++) {
    float temperatura = minimo + (i * ancho);

    fprintf(archivo, "%.2f %d\n", temperatura, histograma[i]);
  }

  fclose(archivo);

  // GENERAR GRAFICA CON GNUPLOT

  FILE *gp = popen("gnuplot", "w");

  if (gp == NULL) {
    printf("\nNo se pudo ejecutar gnuplot.\n");
    return 1;
  }

  fprintf(gp, "set terminal pngcairo size 1400,700 "
              "enhanced font 'Arial,12'\n");

  fprintf(gp, "set output 'histograma.png'\n");

  fprintf(gp, "set title 'Histograma de Temperaturas - Paralelo'\n");

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

  // TIEMPOS

  double tiempoMerge = finOrdenamiento - inicioOrdenamiento;
  double tiempoHisto = finHistograma - inicioHistograma;
  double tiempoTotal = tiempoMerge + tiempoHisto;

  printf("\n----------------------------------------\n");
  printf("Tiempo Merge Sort : %.6f segundos\n", tiempoMerge);
  printf("Tiempo Histograma : %.6f segundos\n", tiempoHisto);
  printf("Tiempo Total      : %.6f segundos\n", tiempoTotal);
  printf("----------------------------------------\n");

  printf("\nArchivo generado : temperaturas_ordenadas.csv\n");
  printf("Archivo de datos : histograma.dat\n");
  printf("Grafica generada : histograma.png\n");

  return 0;
}
