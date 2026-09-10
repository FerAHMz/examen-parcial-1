#!/usr/bin/env python3
"""
Consultoria Hilo Conductor
Pruebas de rendimiento del Examen Parcial 1 con OpenMP.

Cada integrante corre este archivo en su propia maquina. El programa imprime
en pantalla el detalle completo de la corrida (esa pantalla es la prueba de
ejecucion que hay que capturar) y deja cuatro graficas PNG dentro de
docs/resultados/:

    speedup_histograma-masivo.png       eficiencia_histograma-masivo.png
    speedup_ruta-minima-bfs.png         eficiencia_ruta-minima-bfs.png

Uso:

    python3 pruebas_rendimiento.py --integrante "Nombre Completo"

Prueba corta para verificar que todo compila y corre:

    python3 pruebas_rendimiento.py --integrante "Nombre" --rapido

No hay que instalar nada. Las graficas PNG se generan con la libreria estandar
de Python, de modo que el script se comporta igual en macOS y en Linux.
"""

import argparse
import math
import os
import platform
import re
import shutil
import struct
import subprocess
import sys
import unicodedata
import zlib
from datetime import datetime

RAIZ = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(RAIZ, "bin")

HISTOGRAMA = "Histograma masivo"
BFS = "Ruta minima (BFS)"

BLANCO = (255, 255, 255)
AZUL = (32, 118, 210)
GRIS = (150, 158, 168)
TINTA = (28, 34, 43)
SUAVE = (110, 120, 132)
REJILLA = (228, 232, 236)


# ----------------------------------------------------------------------------
# Utilidades
# ----------------------------------------------------------------------------

def sin_acentos(texto):
    """Quita tildes y deja solo ASCII; la fuente de las graficas es ASCII."""
    plano = unicodedata.normalize("NFKD", texto)
    return "".join(c for c in plano if not unicodedata.combining(c))


def apodo(texto):
    """Convierte 'Joel Jaquez' en 'joel-jaquez' para nombrar carpetas."""
    limpio = re.sub(r"[^A-Za-z0-9]+", "-", sin_acentos(texto)).strip("-").lower()
    return limpio or "integrante"


def marco(texto=""):
    """Una linea del recuadro del encabezado, con el ancho ya ajustado."""
    print("#   %-65s#" % texto)


def abortar(mensaje):
    print("\nERROR: %s\n" % mensaje, file=sys.stderr)
    sys.exit(1)


def correr(comando, hilos=None):
    """Ejecuta un binario y devuelve su salida."""
    entorno = dict(os.environ)
    if hilos is not None:
        entorno["OMP_NUM_THREADS"] = str(hilos)
    try:
        proceso = subprocess.run(comando, capture_output=True, text=True,
                                 env=entorno, check=False)
    except FileNotFoundError:
        abortar("No se encontro %s. Corra 'make' primero." % comando[0])
    if proceso.returncode != 0:
        abortar("%s termino con codigo %d.\n%s"
                % (" ".join(comando), proceso.returncode, proceso.stderr.strip()))
    return proceso.stdout


def lista_hilos(maximo):
    """Misma progresion que usa nodos-paralelo.c: 1, 2, 4, 6, 8, ..."""
    hilos, h = [], 1
    while h <= maximo:
        hilos.append(h)
        h = 2 if h == 1 else h + 2
    return hilos


def datos_compilador():
    compilador = os.environ.get("CC", "cc")
    ruta = shutil.which(compilador)
    if not ruta:
        return "desconocido"
    try:
        salida = subprocess.run([ruta, "--version"], capture_output=True,
                                text=True, check=False).stdout
        return salida.splitlines()[0].strip() if salida else compilador
    except Exception:
        return compilador


# ----------------------------------------------------------------------------
# Fuente de 5x7 pixeles usada en las graficas
# ----------------------------------------------------------------------------
# Cada glifo son 7 filas; cada fila usa los 5 bits menos significativos.

FUENTE = {
    " ": (0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00),
    "0": (0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E),
    "1": (0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E),
    "2": (0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F),
    "3": (0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E),
    "4": (0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02),
    "5": (0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E),
    "6": (0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E),
    "7": (0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08),
    "8": (0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E),
    "9": (0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C),
    "A": (0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11),
    "B": (0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E),
    "C": (0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E),
    "D": (0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E),
    "E": (0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F),
    "F": (0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10),
    "G": (0x0E, 0x11, 0x10, 0x16, 0x11, 0x11, 0x0E),
    "H": (0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11),
    "I": (0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E),
    "J": (0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E),
    "K": (0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11),
    "L": (0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F),
    "M": (0x11, 0x1B, 0x15, 0x11, 0x11, 0x11, 0x11),
    "N": (0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11),
    "O": (0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E),
    "P": (0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10),
    "Q": (0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D),
    "R": (0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11),
    "S": (0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E),
    "T": (0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04),
    "U": (0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E),
    "V": (0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04),
    "W": (0x11, 0x11, 0x11, 0x11, 0x15, 0x1B, 0x11),
    "X": (0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11),
    "Y": (0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04),
    "Z": (0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F),
    "a": (0x00, 0x00, 0x0E, 0x01, 0x0F, 0x11, 0x0F),
    "b": (0x10, 0x10, 0x1E, 0x11, 0x11, 0x11, 0x1E),
    "c": (0x00, 0x00, 0x0E, 0x10, 0x10, 0x10, 0x0E),
    "d": (0x01, 0x01, 0x0F, 0x11, 0x11, 0x11, 0x0F),
    "e": (0x00, 0x00, 0x0E, 0x11, 0x1F, 0x10, 0x0E),
    "f": (0x06, 0x09, 0x08, 0x1E, 0x08, 0x08, 0x08),
    "g": (0x00, 0x00, 0x0F, 0x11, 0x0F, 0x01, 0x0E),
    "h": (0x10, 0x10, 0x1E, 0x11, 0x11, 0x11, 0x11),
    "i": (0x04, 0x00, 0x0C, 0x04, 0x04, 0x04, 0x0E),
    "j": (0x02, 0x00, 0x02, 0x02, 0x02, 0x12, 0x0C),
    "k": (0x10, 0x10, 0x12, 0x14, 0x18, 0x14, 0x12),
    "l": (0x0C, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E),
    "m": (0x00, 0x00, 0x1A, 0x15, 0x15, 0x11, 0x11),
    "n": (0x00, 0x00, 0x1E, 0x11, 0x11, 0x11, 0x11),
    "o": (0x00, 0x00, 0x0E, 0x11, 0x11, 0x11, 0x0E),
    "p": (0x00, 0x00, 0x1E, 0x11, 0x11, 0x1E, 0x10),
    "q": (0x00, 0x00, 0x0F, 0x11, 0x11, 0x0F, 0x01),
    "r": (0x00, 0x00, 0x16, 0x19, 0x10, 0x10, 0x10),
    "s": (0x00, 0x00, 0x0F, 0x10, 0x0E, 0x01, 0x1E),
    "t": (0x08, 0x08, 0x1E, 0x08, 0x08, 0x09, 0x06),
    "u": (0x00, 0x00, 0x11, 0x11, 0x11, 0x11, 0x0F),
    "v": (0x00, 0x00, 0x11, 0x11, 0x11, 0x0A, 0x04),
    "w": (0x00, 0x00, 0x11, 0x11, 0x15, 0x15, 0x0A),
    "x": (0x00, 0x00, 0x11, 0x0A, 0x04, 0x0A, 0x11),
    "y": (0x00, 0x00, 0x11, 0x11, 0x0F, 0x01, 0x0E),
    "z": (0x00, 0x00, 0x1F, 0x02, 0x04, 0x08, 0x1F),
    ".": (0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C),
    ",": (0x00, 0x00, 0x00, 0x00, 0x0C, 0x04, 0x08),
    ":": (0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00),
    "-": (0x00, 0x00, 0x00, 0x0E, 0x00, 0x00, 0x00),
    "(": (0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02),
    ")": (0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08),
    "/": (0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10),
    "%": (0x19, 0x1A, 0x02, 0x04, 0x08, 0x0B, 0x13),
    "+": (0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00),
    "|": (0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04),
}


# ----------------------------------------------------------------------------
# Lienzo de pixeles y escritura de PNG
# ----------------------------------------------------------------------------

class Lienzo:
    """
    Mapa de bits RGB con dibujo suavizado. Se escribe el PNG a mano con zlib,
    que ya viene con Python, para no depender de matplotlib ni de gnuplot.
    """

    def __init__(self, ancho, alto, fondo=BLANCO):
        self.ancho = ancho
        self.alto = alto
        self.px = bytearray(bytes(fondo) * (ancho * alto))

    def mezclar(self, x, y, color, alfa):
        if alfa <= 0.0:
            return
        x, y = int(x), int(y)
        if not (0 <= x < self.ancho and 0 <= y < self.alto):
            return
        if alfa >= 1.0:
            i = (y * self.ancho + x) * 3
            self.px[i] = color[0]
            self.px[i + 1] = color[1]
            self.px[i + 2] = color[2]
            return
        i = (y * self.ancho + x) * 3
        resto = 1.0 - alfa
        self.px[i] = int(self.px[i] * resto + color[0] * alfa + 0.5)
        self.px[i + 1] = int(self.px[i + 1] * resto + color[1] * alfa + 0.5)
        self.px[i + 2] = int(self.px[i + 2] * resto + color[2] * alfa + 0.5)

    def rectangulo(self, x, y, ancho, alto, color):
        x0, y0 = max(0, int(x)), max(0, int(y))
        x1 = min(self.ancho, int(x + ancho))
        y1 = min(self.alto, int(y + alto))
        if x1 <= x0 or y1 <= y0:
            return
        franja = bytes(color) * (x1 - x0)
        for fila in range(y0, y1):
            inicio = (fila * self.ancho + x0) * 3
            self.px[inicio:inicio + len(franja)] = franja

    def disco(self, cx, cy, radio, color):
        """Circulo relleno con borde suavizado."""
        x0, x1 = int(cx - radio - 1), int(cx + radio + 1)
        y0, y1 = int(cy - radio - 1), int(cy + radio + 1)
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                distancia = math.hypot(x + 0.5 - cx, y + 0.5 - cy)
                self.mezclar(x, y, color, min(radio + 0.5 - distancia, 1.0))

    def linea(self, x0, y0, x1, y1, color, grosor=2.0, guion=None):
        """
        Segmento suavizado. Las lineas horizontales y verticales sin guiones
        usan un relleno directo, que es mucho mas rapido para la rejilla.
        """
        if guion is None and abs(y1 - y0) < 0.5:
            self.rectangulo(min(x0, x1), y0 - grosor / 2.0,
                            abs(x1 - x0) + 1, max(1, round(grosor)), color)
            return
        if guion is None and abs(x1 - x0) < 0.5:
            self.rectangulo(x0 - grosor / 2.0, min(y0, y1),
                            max(1, round(grosor)), abs(y1 - y0) + 1, color)
            return

        largo = math.hypot(x1 - x0, y1 - y0)
        if largo < 0.01:
            self.disco(x0, y0, grosor / 2.0, color)
            return
        pasos = int(largo * 2) + 1
        ciclo = (guion[0] + guion[1]) if guion else 0
        for i in range(pasos + 1):
            t = i / float(pasos)
            if guion and (t * largo) % ciclo >= guion[0]:
                continue
            self.disco(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t,
                       grosor / 2.0, color)

    def texto(self, x, y, cadena, color, escala=2, anclaje="izquierda"):
        """Dibuja texto con la fuente de 5x7. 'y' es el borde superior."""
        cadena = sin_acentos(cadena)
        avance = 6 * escala
        total = len(cadena) * avance - escala
        if anclaje == "centro":
            x -= total / 2.0
        elif anclaje == "derecha":
            x -= total
        for caracter in cadena:
            glifo = FUENTE.get(caracter)
            if glifo:
                for fila in range(7):
                    bits = glifo[fila]
                    for columna in range(5):
                        if bits & (1 << (4 - columna)):
                            self.rectangulo(x + columna * escala,
                                            y + fila * escala,
                                            escala, escala, color)
            x += avance

    def texto_vertical(self, x, y, cadena, color, escala=2):
        """Texto girado 90 grados, leyendose de abajo hacia arriba."""
        cadena = sin_acentos(cadena)
        avance = 6 * escala
        y += (len(cadena) * avance - escala) / 2.0
        for caracter in cadena:
            glifo = FUENTE.get(caracter)
            if glifo:
                for fila in range(7):
                    bits = glifo[fila]
                    for columna in range(5):
                        if bits & (1 << (4 - columna)):
                            self.rectangulo(x + fila * escala,
                                            y - columna * escala,
                                            escala, escala, color)
            y -= avance

    def guardar(self, ruta):
        crudo = bytearray()
        paso = self.ancho * 3
        for fila in range(self.alto):
            crudo.append(0)                       # filtro 0: sin filtrar
            inicio = fila * paso
            crudo += self.px[inicio:inicio + paso]

        def bloque(tipo, datos):
            return (struct.pack(">I", len(datos)) + tipo + datos +
                    struct.pack(">I", zlib.crc32(tipo + datos) & 0xFFFFFFFF))

        with open(ruta, "wb") as archivo:
            archivo.write(b"\x89PNG\r\n\x1a\n")
            archivo.write(bloque(b"IHDR", struct.pack(">IIBBBBB", self.ancho,
                                                      self.alto, 8, 2, 0, 0, 0)))
            archivo.write(bloque(b"IDAT", zlib.compress(bytes(crudo), 9)))
            archivo.write(bloque(b"IEND", b""))


def escalones(maximo, objetivo=6):
    """Paso 'redondo' para las marcas del eje vertical."""
    if maximo <= 0:
        return 1.0
    bruto = maximo / float(objetivo)
    exponente = math.floor(math.log10(bruto))
    base = bruto / (10 ** exponente)
    for candidato in (1, 2, 2.5, 5, 10):
        if base <= candidato:
            return candidato * (10 ** exponente)
    return 10 ** (exponente + 1)


# ----------------------------------------------------------------------------
# Graficas: solo speedup y eficiencia
# ----------------------------------------------------------------------------

def dibujar_grafica(ruta, titulo, subtitulo, etiqueta_y, hilos, medido, ideal,
                    nombre_medido, nombre_ideal, decimales, sufijo=""):
    ancho, alto = 1200, 700
    izq, der, arriba, abajo = 120, 54, 168, 96
    ancho_util = ancho - izq - der
    alto_util = alto - arriba - abajo
    respiro = 30

    lienzo = Lienzo(ancho, alto)

    tope = max(max(medido), max(ideal))
    paso = escalones(tope * 1.10)
    tope_eje = math.ceil(tope * 1.10 / paso) * paso or paso

    min_x, max_x = min(hilos), max(hilos)
    rango_x = (max_x - min_x) or 1

    def px(h):
        return (izq + respiro
                + (h - min_x) / float(rango_x) * (ancho_util - 2 * respiro))

    def py(v):
        return arriba + alto_util - (v / tope_eje) * alto_util

    def formatear(valor):
        return ("%." + str(decimales) + "f") % valor + sufijo

    # Encabezado
    lienzo.texto(izq - 8, 34, titulo, TINTA, escala=4)
    lienzo.texto(izq - 8, 82, subtitulo, SUAVE, escala=2)

    # Leyenda horizontal, debajo del subtitulo
    ly = 118
    lienzo.linea(izq - 8, ly + 7, izq + 30, ly + 7, AZUL, grosor=4)
    lienzo.disco(izq + 11, ly + 7, 6, AZUL)
    lienzo.texto(izq + 42, ly, nombre_medido, TINTA, escala=2)
    lx2 = izq + 42 + len(sin_acentos(nombre_medido)) * 12 + 34
    lienzo.linea(lx2, ly + 7, lx2 + 38, ly + 7, GRIS, grosor=3, guion=(8, 6))
    lienzo.texto(lx2 + 50, ly, nombre_ideal, TINTA, escala=2)

    # Rejilla y marcas del eje vertical
    marca = 0.0
    while marca <= tope_eje + paso * 0.001:
        y = py(marca)
        lienzo.linea(izq, y, izq + ancho_util, y, REJILLA, grosor=1)
        lienzo.texto(izq - 16, y - 7, formatear(marca), SUAVE,
                     escala=2, anclaje="derecha")
        marca += paso

    # Ejes
    lienzo.linea(izq, py(0), izq + ancho_util, py(0), SUAVE, grosor=2)
    lienzo.linea(izq, arriba, izq, py(0), SUAVE, grosor=2)

    # Marcas del eje horizontal
    for h in hilos:
        x = px(h)
        lienzo.linea(x, py(0), x, py(0) + 8, SUAVE, grosor=2)
        lienzo.texto(x, py(0) + 18, str(h), TINTA, escala=2, anclaje="centro")

    lienzo.texto(izq + ancho_util / 2.0, alto - 44, "Cantidad de hilos",
                 TINTA, escala=2, anclaje="centro")
    lienzo.texto_vertical(30, arriba + alto_util / 2.0, etiqueta_y,
                          TINTA, escala=2)

    # Referencia ideal
    for i in range(len(hilos) - 1):
        lienzo.linea(px(hilos[i]), py(ideal[i]), px(hilos[i + 1]),
                     py(ideal[i + 1]), GRIS, grosor=3, guion=(9, 7))

    # Serie medida
    for i in range(len(hilos) - 1):
        lienzo.linea(px(hilos[i]), py(medido[i]), px(hilos[i + 1]),
                     py(medido[i + 1]), AZUL, grosor=4)
    for h, valor in zip(hilos, medido):
        lienzo.disco(px(h), py(valor), 6.5, AZUL)
        lienzo.texto(px(h), py(valor) - 30, formatear(valor), AZUL,
                     escala=2, anclaje="centro")

    lienzo.guardar(ruta)


def graficar_problema(carpeta, problema, filas, integrante):
    paralelas = sorted([f for f in filas if f["version"] == "OpenMP"],
                       key=lambda f: f["hilos"])
    hilos = [f["hilos"] for f in paralelas]
    if len(hilos) < 2:
        return []

    nombre = apodo(problema)
    detalle = "%s  |  %s elementos" % (integrante,
                                       "{:,}".format(paralelas[0]["tamano"]))
    generadas = []

    ruta = os.path.join(carpeta, "speedup_%s.png" % nombre)
    dibujar_grafica(ruta, "Speedup - %s" % problema, detalle,
                    "Speedup (T secuencial / T paralelo)", hilos,
                    [f["speedup"] for f in paralelas], [float(h) for h in hilos],
                    "Speedup medido", "Speedup ideal (lineal)", 2)
    generadas.append(ruta)

    ruta = os.path.join(carpeta, "eficiencia_%s.png" % nombre)
    dibujar_grafica(ruta, "Eficiencia - %s" % problema, detalle,
                    "Eficiencia (%)", hilos,
                    [f["eficiencia"] for f in paralelas],
                    [100.0 for _ in hilos],
                    "Eficiencia medida", "Eficiencia ideal (100%)", 0, "%")
    generadas.append(ruta)
    return generadas


# ----------------------------------------------------------------------------
# Problema 1: histograma masivo
# ----------------------------------------------------------------------------

def leer_resultado(salida):
    """RESULTADO,histograma,version,N,hilos,reps,internas,tiempo,total,checksum"""
    for linea in salida.splitlines():
        if linea.startswith("RESULTADO,"):
            c = linea.strip().split(",")
            return {"n": int(c[3]), "tiempo": float(c[7]),
                    "total": int(c[8]), "checksum": int(c[9])}
    abortar("El programa de histograma no imprimio la linea RESULTADO.")


def encabezado_tabla():
    print("   %-9s %-16s %-11s %-13s %s"
          % ("HILOS", "TIEMPO (s)", "SPEEDUP", "EFICIENCIA", "CORRECTO"))
    print("   " + "-" * 64)


def fila_tabla(etiqueta, tiempo, speedup, eficiencia, correcto):
    print("   %-9s %-16.9f %-11.4f %-13s %s"
          % (etiqueta, tiempo, speedup, "%.1f%%" % eficiencia, correcto))


def medir_histograma(n, repeticiones, semilla, hilos):
    print()
    print("=" * 70)
    print(" PROBLEMA 1:  %s" % HISTOGRAMA)
    print("=" * 70)
    print("   Datos      : %s temperaturas   (semilla %d)"
          % ("{:,}".format(n), semilla))
    print("   Se mide    : la fase paralelizada, el conteo por cubetas")
    print("   Cada dato  : mejor tiempo de %d repeticiones" % repeticiones)
    print()

    base = leer_resultado(correr([os.path.join(BIN, "histograma-secuencial"),
                                  "--benchmark", str(n), str(repeticiones),
                                  str(semilla)]))
    tiempo_base = base["tiempo"]

    encabezado_tabla()
    fila_tabla("1 (sec)", tiempo_base, 1.0, 100.0, "-")

    filas = [{"problema": HISTOGRAMA, "version": "Secuencial", "hilos": 1,
              "tamano": n, "tiempo": tiempo_base, "speedup": 1.0,
              "eficiencia": 100.0, "correcto": "si"}]

    for h in hilos:
        actual = leer_resultado(correr(
            [os.path.join(BIN, "histograma-paralelo"), "--benchmark",
             str(n), str(h), str(repeticiones), str(semilla)], hilos=h))
        speedup = tiempo_base / actual["tiempo"]
        eficiencia = speedup / h * 100.0
        # El checksum pondera cada cubeta: detecta un conteo mal repartido
        # aunque el total cuadre. Es la prueba de que no hubo race condition.
        correcto = (actual["total"] == n and actual["checksum"] == base["checksum"])
        fila_tabla(str(h), actual["tiempo"], speedup, eficiencia,
                   "si" if correcto else "NO")
        filas.append({"problema": HISTOGRAMA, "version": "OpenMP", "hilos": h,
                      "tamano": n, "tiempo": actual["tiempo"],
                      "speedup": speedup, "eficiencia": eficiencia,
                      "correcto": "si" if correcto else "NO"})
    print("   " + "-" * 64)
    return filas


# ----------------------------------------------------------------------------
# Problema 2: ruta minima con BFS
# ----------------------------------------------------------------------------

FILA_SEC = re.compile(r"^1 \(sec\)\s+([0-9.]+)")
FILA_PAR = re.compile(r"^(\d+)\s+([0-9.]+)\s+([0-9.]+)\s+([0-9.]+)%\s+(si|NO)")


def medir_bfs(n, grado, repeticiones, max_hilos):
    print()
    print("=" * 70)
    print(" PROBLEMA 2:  %s" % BFS)
    print("=" * 70)
    print("   Datos      : %s usuarios, %d amistades promedio"
          % ("{:,}".format(n), grado))
    print("   Se mide    : el recorrido completo de la red")
    print("   Cada dato  : mejor tiempo de %d repeticiones" % repeticiones)
    print()
    print("   Construyendo la red social y midiendo...")

    salida = correr([os.path.join(BIN, "nodos-paralelo"), str(n), str(grado),
                     str(repeticiones)], hilos=max_hilos)

    tiempo_base, filas = None, []
    for linea in salida.splitlines():
        limpia = linea.strip()
        coincide = FILA_SEC.match(limpia)
        if coincide:
            tiempo_base = float(coincide.group(1))
            filas.append({"problema": BFS, "version": "Secuencial", "hilos": 1,
                          "tamano": n, "tiempo": tiempo_base, "speedup": 1.0,
                          "eficiencia": 100.0, "correcto": "-"})
            continue
        coincide = FILA_PAR.match(limpia)
        if coincide and tiempo_base is not None:
            h = int(coincide.group(1))
            tiempo = float(coincide.group(2))
            speedup = tiempo_base / tiempo
            filas.append({"problema": BFS, "version": "OpenMP", "hilos": h,
                          "tamano": n, "tiempo": tiempo, "speedup": speedup,
                          "eficiencia": speedup / h * 100.0,
                          "correcto": coincide.group(5)})

    if tiempo_base is None or len(filas) < 2:
        abortar("No se pudo leer la tabla de resultados de nodos-paralelo.")

    print()
    encabezado_tabla()
    for f in filas:
        etiqueta = "1 (sec)" if f["version"] == "Secuencial" else str(f["hilos"])
        fila_tabla(etiqueta, f["tiempo"], f["speedup"], f["eficiencia"],
                   f["correcto"])
    print("   " + "-" * 64)
    return filas


# ----------------------------------------------------------------------------
# Programa principal
# ----------------------------------------------------------------------------

def opciones():
    p = argparse.ArgumentParser(
        description="Mide speedup y eficiencia de los dos problemas del parcial.")
    p.add_argument("--integrante", help="Nombre completo de quien corre la prueba.")
    p.add_argument("--hist-n", type=int, default=1000000, dest="hist_n",
                   help="Temperaturas del histograma (maximo 1000000).")
    p.add_argument("--graph-n", type=int, default=2000000, dest="graph_n",
                   help="Usuarios de la red social del BFS.")
    p.add_argument("--grado", type=int, default=16,
                   help="Amistades promedio por usuario.")
    p.add_argument("--repeticiones", type=int, default=5,
                   help="Repeticiones por medicion (se reporta el mejor tiempo).")
    p.add_argument("--max-hilos", type=int, default=None, dest="max_hilos",
                   help="Tope de hilos. Por omision, el menor entre 8 y los nucleos.")
    p.add_argument("--semilla", type=int, default=20260909,
                   help="Semilla de los datos, igual para secuencial y paralelo.")
    p.add_argument("--rapido", action="store_true",
                   help="Corrida corta para verificar que todo funciona.")
    p.add_argument("--no-compilar", action="store_true", dest="no_compilar",
                   help="No ejecutar 'make' antes de medir.")
    return p.parse_args()


def main():
    args = opciones()

    integrante = args.integrante
    if not integrante:
        try:
            integrante = input("Nombre completo del integrante: ").strip()
        except (EOFError, KeyboardInterrupt):
            integrante = ""
    if not integrante:
        abortar('Se necesita el nombre: --integrante "Nombre Completo"')

    if args.rapido:
        args.hist_n = min(args.hist_n, 200000)
        args.graph_n = min(args.graph_n, 300000)
        args.repeticiones = 2

    if not 0 < args.hist_n <= 1000000:
        abortar("--hist-n debe estar entre 1 y 1000000 (limite MAX del programa en C).")

    nucleos = os.cpu_count() or 4
    if args.max_hilos is None:
        args.max_hilos = min(8, nucleos)
    hilos = lista_hilos(args.max_hilos)
    if not hilos:
        abortar("--max-hilos debe ser al menos 1.")

    if not args.no_compilar:
        print("Compilando con make...")
        if subprocess.run(["make"], cwd=RAIZ).returncode != 0:
            abortar("Fallo la compilacion. Revise la salida de make.")

    for programa in ("histograma-secuencial", "histograma-paralelo", "nodos-paralelo"):
        if not os.path.isfile(os.path.join(BIN, programa)):
            abortar("Falta bin/%s. Corra 'make'." % programa)

    # ---- Encabezado: identifica la corrida y la maquina de quien la ejecuta --
    print()
    print("#" * 70)
    marco()
    marco("CONSULTORIA HILO CONDUCTOR")
    marco("Examen Parcial 1  |  Pruebas de speedup y eficiencia con OpenMP")
    marco()
    print("#" * 70)
    print()
    print("   INTEGRANTE : %s" % integrante)
    print("   FECHA      : %s" % datetime.now().strftime("%d/%m/%Y  %H:%M:%S"))
    print("   EQUIPO     : %s" % platform.platform())
    print("   PROCESADOR : %s, %d nucleos logicos" % (platform.machine(), nucleos))
    print("   COMPILADOR : %s" % datos_compilador())
    print("   HILOS      : %s" % ", ".join(str(h) for h in hilos))
    if args.rapido:
        print()
        print("   *** MODO RAPIDO: tamanos reducidos, no usar en el informe ***")

    filas = []
    filas += medir_histograma(args.hist_n, args.repeticiones, args.semilla, hilos)
    filas += medir_bfs(args.graph_n, args.grado, args.repeticiones, args.max_hilos)

    # ---- Graficas -----------------------------------------------------------
    carpeta = os.path.join(RAIZ, "docs", "resultados", "%s-%s"
                           % (apodo(integrante),
                              datetime.now().strftime("%Y%m%d-%H%M%S")))
    os.makedirs(carpeta, exist_ok=True)

    print()
    print("   Generando las graficas...")
    graficas = []
    for problema in (HISTOGRAMA, BFS):
        graficas += graficar_problema(
            carpeta, problema,
            [f for f in filas if f["problema"] == problema], integrante)

    # ---- Resumen ------------------------------------------------------------
    print()
    print("=" * 70)
    print(" RESUMEN DE LA CORRIDA DE %s" % sin_acentos(integrante).upper())
    print("=" * 70)
    for problema in (HISTOGRAMA, BFS):
        paralelas = [f for f in filas
                     if f["problema"] == problema and f["version"] == "OpenMP"]
        if not paralelas:
            continue
        mejor = max(paralelas, key=lambda f: f["speedup"])
        fallos = [f for f in paralelas if f["correcto"] != "si"]
        print("   %s" % problema)
        print("      Mejor speedup    : %.2fx con %d hilos"
              % (mejor["speedup"], mejor["hilos"]))
        print("      Eficiencia ahi   : %.1f%%" % mejor["eficiencia"])
        print("      Resultado igual al secuencial: %s"
              % ("si, en todos los casos" if not fallos
                 else "NO en %d casos" % len(fallos)))
    print()
    print("   GRAFICAS GENERADAS en %s" % os.path.relpath(carpeta, RAIZ))
    for ruta in graficas:
        print("      %s" % os.path.basename(ruta))
    print()
    print("=" * 70)
    print()


if __name__ == "__main__":
    main()
