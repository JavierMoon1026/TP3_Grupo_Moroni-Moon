#include "all_heads.h"
#define FUSE_USE_VERSION 29
#define __ASSEMBLY__
#include "fuse/fuse.h"


/********************************
 *				*
 *     Variables Globales	*
 *				*
 ********************************/
/* Datos de la imágen del disco cargada en memoria */
unsigned			LongitudDiskData = 0;
const unsigned char		*DiskData = NULL;

/* Puntero al driver */
TDriverBase			*DriverFS = NULL;




/****************************************************************************************************************************************
 *																	*
 *								     Init								*
 *																	*
 * OBJETIVO: Esta función es llamada para inicializar todo previo a la operación.							*
 *																	*
 * ENTRADA: pImagen: Puntero al nombre de la imágen de disco a cargar.									*
 *																	*
 * SALIDA: Esta función puede devolver un puntero que luego se recibe en DeInit(), pero no lo uso.					*
 *																	*
 ****************************************************************************************************************************************/
int Init(const char *pImagen)
{
int	CodError;
FILE	*f;

/* Abrir el archivo con la imágen */
if ( (f=fopen(pImagen, "rb")) != NULL )
    {
	/* Determinar el tamaño */
	fseek(f, 0, SEEK_END);
	LongitudDiskData=ftell(f);
	fseek(f, 0, SEEK_SET);

	/* Alocar memoria para almacenarlo */
	if ( (DiskData=(const unsigned char *)malloc(LongitudDiskData)) != NULL )
	    {
		/* Levantar el archivo a memoria */
		if ( fread((void *)DiskData, 1, LongitudDiskData, f) != LongitudDiskData )
		    {
			/* Error al levantar el archivo */
			free((void *)DiskData);
			DiskData=NULL;
			LongitudDiskData=0;
		    }
	    }
	
	/* Cerrar el archivo con la imágen */
	fclose(f);
    }

/* Si hubo errores, salir */
if (!DiskData)
    {
	printf("Error al levantar la imágen del archivo %s, saliendo...\n", pImagen);
	return(-1);
    }

/* Ver si la imágen es FAT */
printf("Analizando imágen con driver FAT12/FAT16/FAT32 ...\n");
DriverFS=new TDriverFAT(DiskData, LongitudDiskData);
CodError=DriverFS->LevantarDatosSuperbloque();
if ( (CodError==CODERROR_SUPERBLOQUE_INVALIDO) || (CodError==CODERROR_FILESYSTEM_DESCONOCIDO) )
    {
	/* No es FAT, ver si es EXT */
	printf("ERROR: La imágen no es FAT12/FAT16/FAT32.\n");
	delete DriverFS;

	printf("Analizando imágen con driver EXT2/EXT3/EXT4 ...\n");
	DriverFS=new TDriverEXT(DiskData, LongitudDiskData);
	CodError=DriverFS->LevantarDatosSuperbloque();
	if ( (CodError==CODERROR_SUPERBLOQUE_INVALIDO) || (CodError==CODERROR_FILESYSTEM_DESCONOCIDO) )
	    {
		/* No es FAT, ver si es EXT */
		printf("ERROR: La imágen no es EXT2/EXT3/EXT4.\n");
		delete DriverFS;

		printf("Analizando imágen con driver NTFS ...\n");
		DriverFS=new TDriverNTFS(DiskData, LongitudDiskData);
		CodError=DriverFS->LevantarDatosSuperbloque();
		if ( (CodError==CODERROR_SUPERBLOQUE_INVALIDO) || (CodError==CODERROR_FILESYSTEM_DESCONOCIDO) )
			printf("ERROR: La imágen no es NTFS.\n");
	    }
    }
if (CodError!=CODERROR_NINGUNO)
	return(CodError);
printf("ÉXITO: Imágen válida.\n");

/* Salir indicando éxito */
return(0);
}


/****************************************************************************************************************************************
 *																	*
 *								   Destroy								*
 *																	*
 * OBJETIVO: Esta función es llamada al terminar para liberar los recursos alocados.							*
 *																	*
 * ENTRADA: PrivateData: Valor devuelto por Init(), pero no lo uso.									*
 *																	*
 * SALIDA: Nada.															*
 *																	*
 ****************************************************************************************************************************************/
void Destroy(void *PrivateData)
{
/* Ver si hay driver cargado */
if (DriverFS)
    {
	/* Sí, liberarlo */
	delete(DriverFS);
	DriverFS=NULL;
    }

/* Ver si tengo cargada una imágen de disco */
if (DiskData)
    {
	/* Sí, liberarla */
	free((void *)DiskData);
	DiskData=NULL;
	LongitudDiskData=0;
    }
}


/****************************************************************************************************************************************
 *																	*
 *								 GetAttr								*
 *																	*
 * OBJETIVO: Esta función es llamada por el SO para obtener los permisos y atributos de un archivo.					*
 *																	*
 * ENTRADA: Path: Ruta al archivo cuyos atributos hay que devolver.									*
 *																	*
 * SALIDA: En el nombre de la función 0 si no hubo errores. Sino alguna de las constantes Exxxx definidas en errno.h			*
 *	   FileStat: Si se devuelve 0 estructura completa con los datos del archivo.							*
 *																	*
 ****************************************************************************************************************************************/
int GetAttr(const char *Path, struct stat *FileStat)
{
int				CodError;
unsigned			i;
TString				DirName, FileName;
std::vector<TEntradaDirectorio> Entradas;


/* Ver si quieren los datos del directorio raíz */
if (!strcasecmp(Path, "/"))
    {
	/* Sí, devolverlos */
	FileStat->st_uid = getuid();
	FileStat->st_gid = getgid();
	FileStat->st_mode = S_IFDIR | 0755;
	return(0);
    }

/* Separar el path en directorio + archivo + stream */
DirName=Path;
i=DirName.rfind('/');
if (i!=UINT_MAX)
    {
	FileName = DirName.c_str()+i+1;
	DirName  = DirName.substr(0, i);
    }
if (!DirName.length())
	DirName="/";

/* Leer el contenido del directorio */
if ( (CodError=DriverFS->ListarDirectorio(DirName.c_str(), Entradas)) != CODERROR_NINGUNO )
	return(-EIO);

/* Buscar el archivo */
for(i=0;i<Entradas.size();i++)
    {
	/* Ver si este es el archivo que busco */
	if (!strcasecmp(Entradas[i].Nombre.c_str(), FileName.c_str()))
	    {
		/* Sí, como no tenemos ninguna función que devuelva permisos, asumir que son los datos del usuario actual */
		FileStat->st_uid = getuid();
		FileStat->st_gid = getgid();

		/* Colocar los flags de tipo de archivo */
		if (Entradas[i].Flags&fedDIRECTORIO)
			FileStat->st_mode = S_IFDIR | 0755;
		else if (Entradas[i].Flags&fedACCESO_DIRECTO)
			FileStat->st_mode = S_IFLNK | 0755;
		else
			FileStat->st_mode = S_IFREG | 0644;
		
		/* Colocar la longitud */
		FileStat->st_size = Entradas[i].Bytes;

		/* Salir */
		return(0);
	    }
    }

/* El archivo no existe */
return(-ENOENT);
}


/****************************************************************************************************************************************
 *																	*
 *								 ReadDir								*
 *																	*
 * OBJETIVO: Esta función es llamada por el SO para obtener un listado de las entradas en un directorio.				*
 *																	*
 * ENTRADA: Path: Ruta al directorio cuyas entradas devolver.										*
 *	    Buffer: Parmámetro interno que hay que pasarle a la función Filler() cuando la llamo.					*
 *	    Filler: Función a llamar para reportar cada entrada en el directorio.							*
 *	    Offset: Índice de la entrada del directorio a devolver. Se puede optar por devolver una sola pasándole este valor a Filler()*
 *		    al llamarla, o devolverlas todas pasándole a Filler() cero en el parámetro Offset. Si se opta por la primera, el SO	*
 *		    va a llamar reiteradas veces a esta función para que vaya devolviendo las entradas de a una.			*
 *																	*
 * SALIDA: En el nombre de la función 0 si no hubo errores. Sino alguna de las constantes Exxxx definidas en errno.h			*
 *	   FuseFileInfo: Flags que se pueden devolver para indicar qué opciones se soportan, cache, flush, etc. Son para escritura,	*
 *			 no devuelvo nada.												*
 *																	*
 ****************************************************************************************************************************************/
int ReadDir(const char *Path, void *Buffer, fuse_fill_dir_t Filler, off_t Offset, struct fuse_file_info *FuseFileInfo)
{
int				CodError;
unsigned			i;
std::vector<TEntradaDirectorio> Entradas;

printf("ReadDir(): 1 (Path=%s)\n", Path);
/* Buscar el contenido del directorio */
CodError=DriverFS->ListarDirectorio(Path, Entradas);
switch(CodError)
    {
	case CODERROR_DIRECTORIO_INEXISTENTE:
		return(ENOENT);

	case CODERROR_NINGUNO:
		for(i=0;i<Entradas.size();i++)
		    {
			/* Colocarle los flags */
			Filler(Buffer, Entradas[i].Nombre.c_str(), NULL, 0);
		    }
		break;
	default:
		return(EIO);
    }
	
/* Salir indicando éxito */
return(0);
}


/****************************************************************************************************************************************
 *																	*
 *								   Read									*
 *																	*
 * OBJETIVO: Esta función es llamada por el SO para leer el contenido de un archivo.							*
 *																	*
 * ENTRADA: Path: Ruta al archivo a leer.												*
 *	    BufferLen: Longitud del buffer a llenar.											*
 *	    Offset: Punto a partir del cual comenzar a devolver datos.									*
 *	    FileInfo: Estructura fuse_file_info con los datos del archivo a leer.							*
 *																	*
 * SALIDA: En el nombre de la función 0 si no hubo errores. Sino alguna de las constantes Exxxx definidas en errno.h			*
 *	   Buffer: Buffer donde colocar los datos.											*
 *																	*
 ****************************************************************************************************************************************/
int Read(const char *Path, char *Buffer, size_t BufferLen, off_t Offset, struct fuse_file_info *FileInfo)
{
int		CodError;
unsigned	DataLen;
unsigned char	*Data;

/* Buscar el contenido del archivo */
CodError=DriverFS->LeerArchivo(Path, Data, DataLen);
switch(CodError)
    {
	case CODERROR_ARCHIVO_INEXISTENTE:
		return(-ENOENT);

	case CODERROR_NINGUNO:
		/* Ver si tengo los datos que buscan */
		if (Offset>=DataLen)
		    {
			/* No tengo nada de lo que buscan */
			return(0);
		    }
		
		/* Si el archivo tiene más caracteres que el buffer, devolver sólo lo que pueden recibir */
		if ( (DataLen-Offset) > BufferLen )
			DataLen = BufferLen;
		
		/* Pasarle a Linux los datos del archivo */
		memcpy(Buffer, Data, DataLen);
		
		/* Liberar el bloque alocado */
		free(Data);
		
		/* Devolver la cantidad de bytes en la salida */
		return(DataLen);

	default:
		return(-EIO);
    }
}

/****************************************************************************************************************************************
 *																	*
 *								   main									*
 *																	*
 * OBJETIVO: Punto de entrada al programa.												*
 *																	*
 * ENTRADA: argc: Cantidad de parámetros recibidos de la línea de comandos.								*
 *	    argv: Arreglo con los punteros a cada uno de los parámetros recibidos.							*
 *																	*
 * SALIDA: En el nombre de la función el código de retorno del programa al SO.								*
 *																	*
 ****************************************************************************************************************************************/
int main(int argc, char **argv)
{
int				CodError;
const char			*pImagen;
FILE				*f;
static struct fuse_operations	OperacionesImplementadas;

/* Ver si pasaron parámetros */
if (argc>1)
    {
	/* El último parámetro debe ser el nombre de la imágen */
	pImagen = (const char *)(argv[argc-1]);
	argc--;
    }

/* Inicializar todo */
if ( (CodError=Init(pImagen)) < 0 )
	return(CodError);

/* Inicializar la estructura con las funciones soportadas */
memset(&OperacionesImplementadas, 0, sizeof(OperacionesImplementadas));
OperacionesImplementadas.destroy = Destroy;
OperacionesImplementadas.getattr = GetAttr;
OperacionesImplementadas.readdir = ReadDir;
OperacionesImplementadas.read    = Read;

/* Invocar al FUSE */
return(fuse_main(argc, argv, &OperacionesImplementadas, NULL));
}

