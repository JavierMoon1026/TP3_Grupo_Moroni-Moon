# Analizador de Filesystems

Herramienta de línea de comandos que analiza imágenes de disco en los formatos FAT12/16/32, EXT2/3/4 y NTFS. Detecta automáticamente el tipo de filesystem, muestra los datos del superbloque y puede listar directorios y leer archivos dentro de la imagen.

---

## Imágenes de disco

Las imágenes de disco son archivos binarios que representan el contenido completo de un disco o partición, byte por byte. Son la entrada principal del programa.

El archivo `fs-bin.zip` incluye imágenes de prueba para cada formato soportado:

| Archivo | Formato |
|---------|---------|
| `FAT12.bin` | FAT12 |
| `FAT16.bin` | FAT16 |
| `FAT32.bin` | FAT32 |
| `EXT2.bin`, `EXT2a.bin` | EXT2 |
| `EXT3.bin` | EXT3 |
| `EXT4.bin` | EXT4 |
| `NTFS.bin`, `NTFSa.bin`, `NTFSb.bin` | NTFS |

Descomprimir antes de usar:
```bash
unzip fs-bin.zip
```

---

## Compilación y uso

```bash
make
./tpfs <imagen>
```

---

## Configuración

El programa lee su configuración de `config/tpfs.conf` (formato `clave=valor`) y un archivo de tests que lista los comandos a ejecutar sobre la imagen.

### `config/tpfs.conf`
```
tests_file=./config/tests.txt
```

### `config/tests.txt`

Lista de operaciones a ejecutar sobre la imagen. Las líneas con `#` son comentarios.

```
LS   /
CAT  /DIR/archivo.txt
#LS  /DIR/DIR2          <- comentado, no se ejecuta
```

#### `LS <ruta>`

Lista el contenido del directorio indicado por `<ruta>`. Para cada entrada muestra: fecha de creación, fecha de último acceso, fecha de última modificación, nombre, flags (lectura, oculto, sistema, directorio, etc.), tamaño en bytes, y un campo específico del filesystem (primer cluster en FAT, número de INode en EXT, índice en la MFT en NTFS).

#### `CAT <ruta>`

Lee el contenido del archivo indicado por `<ruta>` y lo vuelca en pantalla en formato hexadecimal, con el offset, los bytes en hex y su representación ASCII en cada línea.

---

## Estructura del proyecto

```
.
├── Makefile
├── config/
│   ├── tpfs.conf               # configuración
│   └── tests.txt               # comandos a ejecutar sobre la imagen
├── include/                    # headers
├── source/
│   ├── main.cpp                # punto de entrada
│   ├── analizadorfs.cpp        # orquestador
│   ├── driver_base.cpp         # clase abstracta comun
│   ├── driver_fat.cpp          # ← completar
│   ├── driver_ext.cpp          # ← completar
│   └── driver_ntfs.cpp         # ← completar
├── object/
├── tpfs                        # binario compilado
├── tpfs_ref                    # binario de referencia
├── run_tests.py                # compara tpfs contra tpfs_ref (con colores)
└── run_tests_without_colours.py # ídem, sin colores
```

---

## Arquitectura

### `main.cpp`

Punto de entrada. Parsea los argumentos, instancia `TAnalizadorFS` y llama a `Ejecutar()`. Imprime el resultado final con su código de error.

### `analizadorfs.cpp`

Orquestador principal. Carga la configuración desde `tpfs.conf` y la imagen de disco en memoria. Luego prueba cada driver en orden (FAT → EXT → NTFS) llamando a `LevantarDatosSuperbloque()` hasta encontrar uno que reconozca la imagen. Una vez identificado el filesystem, muestra el superbloque y ejecuta los comandos de `tests.txt`.

### `driver_base.cpp`

Implementa toda la lógica de presentación que es común a los tres filesystems: impresión del superbloque, listado de directorios en formato tabular e impresión de buffers en hex. Los drivers derivados no necesitan ocuparse del formato de salida: solo rellenan las estructuras de datos y `driver_base` se encarga de mostrarlas.

### `driver_fat.cpp`, `driver_ext.cpp`, `driver_ntfs.cpp`

Esqueletos de los drivers de cada filesystem. Cada uno debe implementar tres métodos: `LevantarDatosSuperbloque()`, `ListarDirectorio()` y `LeerArchivo()` del filesystem correspondiente. Los headers correspondientes en `include/` describen las estructuras on-disk de cada formato.


---

## Comparación contra la referencia

Los scripts de Python ejecutan `./tpfs` y `./tpfs_ref` sobre una imagen y comparan sus salidas sección por sección.

```bash
python3 run_tests.py <imagen>
```

Si los colores se ven mal en la terminal, usar:

```bash
python3 run_tests_without_colours.py <imagen>
```

Exit code `0` si las salidas coinciden, `1` si hay diferencias.
