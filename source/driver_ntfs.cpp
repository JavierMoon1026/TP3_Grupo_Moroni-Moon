#include "all_heads.h"


/********************************
 *				*
 *	 Clase TDriverNTFS	*
 *				*
 ********************************/
/****************************************************************************************************************************************
 *																	*
 *							TDriverNTFS :: TDriverNTFS							*
 *																	*
 * OBJETIVO: Inicializar la clase recién creada.											*
 *																	*
 * ENTRADA: DiskData: Puntero a un bloque de memoria con la imágen del disco a analizar.						*
 *	    LongitudDiskData: Tamaño, en bytes, de la imágen a analizar.								*
 *																	*
 * SALIDA: Nada.															*
 *																	*
 ****************************************************************************************************************************************/
TDriverNTFS::TDriverNTFS(const unsigned char *DiskData, unsigned LongitudDiskData) : TDriverBase(DiskData, LongitudDiskData)
{
}


/****************************************************************************************************************************************
 *																	*
 *							TDriverNTFS :: ~TDriverNTFS							*
 *																	*
 * OBJETIVO: Liberar recursos alocados.													*
 *																	*
 * ENTRADA: Nada.															*
 *																	*
 * SALIDA: Nada.															*
 *																	*
 ****************************************************************************************************************************************/
TDriverNTFS::~TDriverNTFS()
{
}


/****************************************************************************************************************************************
 *																	*
 *						   TDriverNTFS :: LevantarDatosSuperbloque						*
 *																	*
 * OBJETIVO: Esta función analiza el superbloque y completa la estructura DatosFS con los datos levantados.				*
 *																	*
 * ENTRADA: Nada.															*
 *																	*
 * SALIDA: En el nombre de la función CODERROR_NINGUNO si no hubo errores. Sino uno de los siguientes valores:				*
 *		CODERROR_SUPERBLOQUE_INVALIDO   : El superbloque está dañado o no corresponde a un disco con ningún formato.		*
 *		CODERROR_FILESYSTEM_DESCONOCIDO : El superbloque es válido, pero no corresponde a un FyleSystem soportado por esta	*
 *						  clase.										*
 *																	*
 ****************************************************************************************************************************************/
int TDriverNTFS::LevantarDatosSuperbloque()
{
	/* El boot sector de NTFS se encuentra en el primer sector de la imagen. */
	const NTFS_BOOT_SECTOR *Boot =
		reinterpret_cast<const NTFS_BOOT_SECTOR *>(PunteroASector(0));

	if (!Boot)
		return(CODERROR_SUPERBLOQUE_INVALIDO);

	/* OemSystem ocupa ocho bytes y no termina en '\0'. */
	if (memcmp(Boot->OemSystem, "NTFS    ", 8) != 0)
		return(CODERROR_FILESYSTEM_DESCONOCIDO);

	/* Validar los campos necesarios para realizar los cálculos. */
	if (Boot->BytesPerSector == 0 ||
	    Boot->SectorsPerCluster == 0 ||
	    Boot->SectorCount64 == 0)
		return(CODERROR_SUPERBLOQUE_INVALIDO);

	DatosFS.TipoFilesystem = tfsNTFS;
	DatosFS.BytesPorSector = Boot->BytesPerSector;
	DatosFS.BytesPorCluster = Boot->BytesPerSector * Boot->SectorsPerCluster;
	DatosFS.NumeroDeClusters = Boot->SectorCount64 / Boot->SectorsPerCluster;

	TDatosFSNTFS &NTFS = DatosFS.DatosEspecificos.NTFS;
	NTFS.TotalSectores = Boot->SectorCount64;
	NTFS.SectoresPorCluster = Boot->SectorsPerCluster;
	NTFS.ClusterMFT = Boot->MasterFileTableCluster;
	NTFS.ClusterMFTMirror = Boot->MasterFileTableMirrorCluster;
	NTFS.OffsetParticionEnSectores = Boot->HiddenSectorCount;

	/*
	 * Un valor positivo expresa una cantidad de clusters. Un valor
	 * negativo expresa una potencia de dos; por ejemplo, -10 es 2^10.
	 */
	auto CalcularTamano = [&](CHAR Valor) -> int {
		signed char ValorConSigno = static_cast<signed char>(Valor);

		if (ValorConSigno > 0)
			return(ValorConSigno * DatosFS.BytesPorCluster);

		if (ValorConSigno < 0 && ValorConSigno >= -30)
			return(1U << (-ValorConSigno));

		return(0);
	};

	NTFS.BytesPorFileRecordSegment =
		CalcularTamano(Boot->ClustersPerRecord);
	NTFS.BytesPorIndexBuffer =
		CalcularTamano(Boot->ClustersPerIndexBuffer);

	if (NTFS.BytesPorFileRecordSegment == 0 ||
	    NTFS.BytesPorIndexBuffer == 0)
		return(CODERROR_SUPERBLOQUE_INVALIDO);

	/* Salir */
	return(CODERROR_NINGUNO);
}


/****************************************************************************************************************************************
 *																	*
 *						  TDriverNTFS :: ListarDirectorio							*
 *																	*
 * OBJETIVO: Esta función enumera las entradas en un directorio y retorna un arreglo de elementos, uno por cada entrada.		*
 *																	*
 * ENTRADA: Path: Path al directorio enumerar (cadena de nombres de directorio separados por '/').					*
 *																	*
 * SALIDA: En el nombre de la función CODERROR_NINGUNO si no hubo errores, caso contrario el código de error.				*
 *	   Entradas: Arreglo con cada una de las entradas.										*
 *																	*
 ****************************************************************************************************************************************/
int TDriverNTFS::ListarDirectorio(const char *Path, std::vector<TEntradaDirectorio> &Entradas)
{
/* Salir */
return(CODERROR_NO_IMPLEMENTADO);
}


/****************************************************************************************************************************************
 *																	*
 *						     TDriverNTFS :: LeerArchivo								*
 *																	*
 * OBJETIVO: Esta función levanta de la imágen un archivo dada su ruta.									*
 *																	*
 * ENTRADA: Path: Ruta al archivo a levantar.												*
 *																	*
 * SALIDA: En el nombre de la función CODERROR_NINGUNO si no hubo errores, caso contrario el código de error.				*
 *	   Data: Buffer alocado con malloc() con los datos del archivo.									*
 *	   DataLen: Tamaño en bytes del buffer devuelto.										*
 *																	*
 * OBSERVACIONES: Los valores Data y DataLen sólo devuelven valores válidos si se retorna CODERROR_NINGUNO.				*
 *																	*
 ****************************************************************************************************************************************/
int TDriverNTFS::LeerArchivo(const char *Path, unsigned char *&Data, unsigned &DataLen)
{
/* Salir */
return(CODERROR_NO_IMPLEMENTADO);
}

