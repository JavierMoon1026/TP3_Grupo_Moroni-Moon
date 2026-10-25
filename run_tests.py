#!/usr/bin/env python3

import subprocess
import sys
import difflib
import os
import re

RED   = "\033[31m"
GREEN = "\033[32m"
CYAN  = "\033[36m"
RESET = "\033[0m"

_colores = sys.stdout.isatty()


def ejecutar(binario, imagen):
    if not os.path.isfile(binario):
        print(f"Error: no se encontró el binario '{binario}'", file=sys.stderr)
        sys.exit(1)
    resultado = subprocess.run([binario, imagen], capture_output=True)
    return resultado.stdout.decode("utf-8", errors="replace")


def parsear_secciones(salida):
    """Split output into named sections.

    Keys: "superbloque", "ls:/path", "cat:/path".
    Each value is a list of lines that follow the section header.
    """
    secciones   = {}
    clave_actual = None

    for linea in salida.splitlines():
        if linea.strip() == "Datos del superbloque:":
            clave_actual = "superbloque"
            secciones[clave_actual] = []
        elif m := re.match(r"^Leyendo directorio '(.+)' \.\.\.", linea):
            clave_actual = f"ls:{m.group(1)}"
            secciones[clave_actual] = []
        elif m := re.match(r"^Leyendo archivo '(.+)' \.\.\.", linea):
            clave_actual = f"cat:{m.group(1)}"
            secciones[clave_actual] = []
        elif clave_actual is not None:
            secciones[clave_actual].append(linea)

    return secciones


def parsear_campos_superbloque(lineas):
    campos = {}
    for linea in lineas:
        m = re.match(r"^\t(.+?)\s*:\s*(.+)$", linea)
        if m:
            campos[m.group(1).strip()] = m.group(2).strip()
    return campos


def comparar_superbloque(campos_ref, campos):
    diferencias = []
    for clave in sorted(campos_ref.keys() | campos.keys()):
        val_ref = campos_ref.get(clave)
        val     = campos.get(clave)
        if val_ref is None:
            diferencias.append(("extra", clave, None,    val))
        elif val is None:
            diferencias.append(("falta", clave, val_ref, None))
        elif val_ref != val:
            diferencias.append(("diff",  clave, val_ref, val))
    return diferencias


def colorear(linea):
    if not _colores:
        return linea
    if linea.startswith("+"):
        return GREEN + linea + RESET
    if linea.startswith("-"):
        return RED   + linea + RESET
    if linea.startswith("@@"):
        return CYAN  + linea + RESET
    return linea


def mostrar_diff_seccion(titulo, lineas_ref, lineas):
    """Print a unified diff for one section. Returns True if there were differences."""
    diff = list(difflib.unified_diff(
        [l + "\n" for l in lineas_ref],
        [l + "\n" for l in lineas],
        lineterm="",
        n=2,
    ))
    if not diff:
        return False

    if _colores:
        print(f"=== {titulo}: diferencias ({RED}-ref{RESET} = referencia, {GREEN}+tpfs{RESET} = propio) ===")
    else:
        print(f"=== {titulo}: diferencias (-ref = referencia, +tpfs = propio) ===")
    for linea in diff[2:]:   # skip the --- / +++ header lines
        print(colorear(linea))
    print()
    return True


def etiqueta(clave):
    if clave.startswith("ls:"):
        return "LS " + clave[3:]
    if clave.startswith("cat:"):
        return "CAT " + clave[4:]
    return clave.upper()


def main():
    if len(sys.argv) != 2:
        print(f"Uso: {sys.argv[0]} <imagen>", file=sys.stderr)
        sys.exit(1)

    imagen = sys.argv[1]
    if not os.path.isfile(imagen):
        print(f"Error: imagen '{imagen}' no encontrada", file=sys.stderr)
        sys.exit(1)

    salida     = ejecutar("./tpfs",     imagen)
    salida_ref = ejecutar("./tpfs_ref", imagen)

    secciones_ref = parsear_secciones(salida_ref)
    secciones     = parsear_secciones(salida)

    hay_diferencias = False

    # --- Superbloque ---
    lineas_sb_ref = secciones_ref.get("superbloque", [])
    lineas_sb     = secciones.get("superbloque", [])

    if lineas_sb_ref or lineas_sb:
        campos_ref = parsear_campos_superbloque(lineas_sb_ref)
        campos     = parsear_campos_superbloque(lineas_sb)
        diffs_sb   = comparar_superbloque(campos_ref, campos)
        if diffs_sb:
            hay_diferencias = True
            if _colores:
                print(f"=== SUPERBLOQUE: diferencias ({RED}ref{RESET} = referencia, {GREEN}tpfs{RESET} = propio) ===")
            else:
                print("=== SUPERBLOQUE: diferencias (ref = referencia, tpfs = propio) ===")
            c = _colores
            for tipo, clave, val_ref, val in diffs_sb:
                if tipo == "extra":
                    v = f"{GREEN}{val}{RESET}" if c else val
                    print(f"  + {clave}: {v}  (no está en ref)")
                elif tipo == "falta":
                    v = f"{RED}{val_ref}{RESET}" if c else val_ref
                    print(f"  - {clave}: {v}  (falta en tpfs)")
                else:
                    ref_label  = f"{RED}ref{RESET}"   if c else "ref"
                    tpfs_label = f"{GREEN}tpfs{RESET}" if c else "tpfs"
                    print(f"  ~ {clave}: {ref_label}={val_ref}  {tpfs_label}={val}")
            print()
        else:
            print("=== SUPERBLOQUE: OK ===\n")

    # --- ls y cat (orden independiente) ---
    claves_otras = sorted(
        (secciones_ref.keys() | secciones.keys()) - {"superbloque"}
    )

    for clave in claves_otras:
        titulo = etiqueta(clave)
        if clave not in secciones_ref:
            print(f"=== {titulo}: solo en tpfs (no está en ref) ===\n")
            hay_diferencias = True
        elif clave not in secciones:
            print(f"=== {titulo}: solo en ref (falta en tpfs) ===\n")
            hay_diferencias = True
        elif mostrar_diff_seccion(titulo, secciones_ref[clave], secciones[clave]):
            hay_diferencias = True
        else:
            print(f"=== {titulo}: OK ===\n")

    sys.exit(1 if hay_diferencias else 0)


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print(f"\nError inesperado: el script no pudo procesar la salida de los binarios.", file=sys.stderr)
        print(f"Detalle: {e}", file=sys.stderr)
        sys.exit(2)
