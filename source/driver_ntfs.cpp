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
	MFTInicializada=false;
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
	/* El boot sector esta en el primer sector de la imagen */
	const NTFS_BOOT_SECTOR *Boot =
		reinterpret_cast<const NTFS_BOOT_SECTOR *>(PunteroASector(0));

	if (!Boot)
		return(CODERROR_SUPERBLOQUE_INVALIDO);

	/* OemSystem ocupa 8 bytes y no termina en '\0' */
	if (memcmp(Boot->OemSystem, "NTFS    ", 8) != 0)
		return(CODERROR_FILESYSTEM_DESCONOCIDO);

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

	/* Si es negativo el tamano se calcula como potencia de 2, por ejemplo -10 da 2^10 */
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

	return(CODERROR_NINGUNO);
}

/* Helpers usados por ListarDirectorio() y LeerArchivo() */
int TDriverNTFS::LeerBytesDeImagen(__u64 Offset, unsigned char *Destino, __u64 Cantidad)
{
	if (Cantidad==0) return(CODERROR_NINGUNO);
	if (!Destino || DatosFS.BytesPorSector<=0) return(CODERROR_SUPERBLOQUE_INVALIDO);

	/* Copiar por partes permite cruzar el limite de un sector */
	while (Cantidad>0) {
		__u64 NroSector=Offset/DatosFS.BytesPorSector;
		unsigned OffsetEnSector=Offset%DatosFS.BytesPorSector;
		const unsigned char *Sector=PunteroASector(NroSector);

		if (!Sector) return(CODERROR_FILESYSTEM_CORRUPTO);

		unsigned Disponibles=DatosFS.BytesPorSector-OffsetEnSector;
		unsigned ALeer=(Cantidad<Disponibles) ? Cantidad : Disponibles;
		memcpy(Destino, Sector+OffsetEnSector, ALeer);

		Offset+=ALeer;
		Destino+=ALeer;
		Cantidad-=ALeer;
	}

	return(CODERROR_NINGUNO);
}

int TDriverNTFS::AplicarFixups(unsigned char *Registro, unsigned TamanoRegistro)
{
	if (!Registro || TamanoRegistro<sizeof(MULTI_SECTOR_HEADER) || DatosFS.BytesPorSector<=0)
		return(CODERROR_FILESYSTEM_CORRUPTO);

	MULTI_SECTOR_HEADER *Encabezado=reinterpret_cast<MULTI_SECTOR_HEADER *>(Registro);
	unsigned OffsetArreglo=Encabezado->UpdateSequenceArrayOffset;
	unsigned CantidadElementos=Encabezado->UpdateSequenceArraySize;

	/* El arreglo tiene un numero de secuencia y un reemplazo por sector */
	if (CantidadElementos<2 || OffsetArreglo+CantidadElementos*sizeof(USHORT)>TamanoRegistro)
		return(CODERROR_FILESYSTEM_CORRUPTO);

	USHORT NumeroSecuencia;
	memcpy(&NumeroSecuencia, Registro+OffsetArreglo, sizeof(NumeroSecuencia));

	for (unsigned i=1; i<CantidadElementos; i++) {
		unsigned FinDeSector=i*DatosFS.BytesPorSector;
		if (FinDeSector<2 || FinDeSector>TamanoRegistro)
			return(CODERROR_FILESYSTEM_CORRUPTO);

		USHORT ValorGuardado;
		memcpy(&ValorGuardado, Registro+FinDeSector-2, sizeof(ValorGuardado));

		/* Si no coincide el registro esta incompleto o danado */
		if (ValorGuardado!=NumeroSecuencia)
			return(CODERROR_FILESYSTEM_CORRUPTO);

		USHORT ValorOriginal;
		memcpy(&ValorOriginal, Registro+OffsetArreglo+i*sizeof(USHORT), sizeof(ValorOriginal));
		memcpy(Registro+FinDeSector-2, &ValorOriginal, sizeof(ValorOriginal));
	}

	return(CODERROR_NINGUNO);
}

ATTRIBUTE_RECORD_HEADER *TDriverNTFS::BuscarAtributo(unsigned char *Registro, unsigned TamanoRegistro, ATTRIBUTE_TYPE_CODE Tipo)
{
	if (!Registro || TamanoRegistro<sizeof(FILE_RECORD_SEGMENT_HEADER)) return(NULL);

	FILE_RECORD_SEGMENT_HEADER *Encabezado=reinterpret_cast<FILE_RECORD_SEGMENT_HEADER *>(Registro);
	unsigned Offset=Encabezado->FirstAttributeOffset;

	/* Cada atributo indica su largo y 0xFFFFFFFF marca el final */
	while (Offset+sizeof(ATTRIBUTE_TYPE_CODE)<=TamanoRegistro) {
		ATTRIBUTE_RECORD_HEADER *Atributo=reinterpret_cast<ATTRIBUTE_RECORD_HEADER *>(Registro+Offset);

		if (Atributo->TypeCode==NTFS_STRUCT_END) return(NULL);
		if (Offset+16>TamanoRegistro || Atributo->RecordLength<16 || Offset+Atributo->RecordLength>TamanoRegistro)
			return(NULL);
		if (Atributo->TypeCode==Tipo) return(Atributo);

		Offset+=Atributo->RecordLength;
	}

	return(NULL);
}

int TDriverNTFS::DecodificarDataRuns(const ATTRIBUTE_RECORD_HEADER *Atributo, std::vector<TDataRun> &DataRuns)
{
	DataRuns.clear();
	if (!Atributo || !Atributo->NonResidentFlag || Atributo->RecordLength<64)
		return(CODERROR_FILESYSTEM_CORRUPTO);

	unsigned Offset=Atributo->Form.NonResident.DataRunsOffset;
	const unsigned char *Bytes=reinterpret_cast<const unsigned char *>(Atributo);
	__s64 ClusterAnterior=0;

	if (Offset>=Atributo->RecordLength) return(CODERROR_FILESYSTEM_CORRUPTO);

	while (Offset<Atributo->RecordLength && Bytes[Offset]!=0) {
		UCHAR Encabezado=Bytes[Offset++];
		unsigned BytesCantidad=Encabezado&0x0F;
		unsigned BytesInicio=Encabezado>>4;

		if (BytesCantidad==0 || BytesCantidad>8 || BytesInicio>8 ||
		    Offset+BytesCantidad+BytesInicio>Atributo->RecordLength)
			return(CODERROR_FILESYSTEM_CORRUPTO);

		ULONGLONG CantidadClusters=0;
		for (unsigned i=0; i<BytesCantidad; i++)
			CantidadClusters|=((ULONGLONG)Bytes[Offset+i])<<(8*i);
		Offset+=BytesCantidad;

		if (CantidadClusters==0 || CantidadClusters>UINT_MAX)
			return(CODERROR_FILESYSTEM_CORRUPTO);

		TDataRun DataRun={};
		DataRun.Cantidad=CantidadClusters;
		DataRun.Disperso=(BytesInicio==0);

		if (!DataRun.Disperso) {
			ULONGLONG ValorOffset=0;
			for (unsigned i=0; i<BytesInicio; i++)
				ValorOffset|=((ULONGLONG)Bytes[Offset+i])<<(8*i);

			/* El offset tiene signo y es relativo al run anterior */
			if (BytesInicio<8 && (Bytes[Offset+BytesInicio-1]&0x80))
				ValorOffset|=(~0ULL)<<(BytesInicio*8);

			__s64 Desplazamiento=static_cast<__s64>(ValorOffset);
			if ((Desplazamiento<0 && ClusterAnterior < -Desplazamiento) ||
			    (Desplazamiento>=0 && ClusterAnterior > LLONG_MAX-Desplazamiento))
				return(CODERROR_FILESYSTEM_CORRUPTO);

			ClusterAnterior+=Desplazamiento;
			if (ClusterAnterior<0) return(CODERROR_FILESYSTEM_CORRUPTO);
			DataRun.Inicio=ClusterAnterior;
		}

		Offset+=BytesInicio;
		DataRuns.push_back(DataRun);
	}

	if (Offset>=Atributo->RecordLength || Bytes[Offset]!=0 || DataRuns.empty())
		return(CODERROR_FILESYSTEM_CORRUPTO);

	return(CODERROR_NINGUNO);
}

int TDriverNTFS::LeerDesdeDataRuns(const std::vector<TDataRun> &DataRuns, __u64 Offset, unsigned char *Destino, __u64 Cantidad)
{
	if (Cantidad==0) return(CODERROR_NINGUNO);
	if (!Destino || DatosFS.BytesPorCluster<=0) return(CODERROR_FILESYSTEM_CORRUPTO);

	for (const TDataRun &DataRun : DataRuns) {
		__u64 BytesEnRun=(__u64)DataRun.Cantidad*DatosFS.BytesPorCluster;

		if (Offset>=BytesEnRun) {
			Offset-=BytesEnRun;
			continue;
		}

		__u64 Disponibles=BytesEnRun-Offset;
		__u64 ALeer=(Cantidad<Disponibles) ? Cantidad : Disponibles;

		if (DataRun.Disperso) {
			/* Un run disperso no ocupa clusters fisicos y se lee como ceros */
			memset(Destino, 0, ALeer);
		} else {
			__u64 OffsetFisico=(__u64)DataRun.Inicio*DatosFS.BytesPorCluster+Offset;
			int CodError=LeerBytesDeImagen(OffsetFisico, Destino, ALeer);
			if (CodError!=CODERROR_NINGUNO) return(CodError);
		}

		Destino+=ALeer;
		Cantidad-=ALeer;
		Offset=0;
		if (Cantidad==0) return(CODERROR_NINGUNO);
	}

	return(CODERROR_FILESYSTEM_CORRUPTO);
}

int TDriverNTFS::LeerContenidoAtributo(const ATTRIBUTE_RECORD_HEADER *Atributo, std::vector<unsigned char> &Contenido)
{
	Contenido.clear();
	if (!Atributo) return(CODERROR_NO_ENCONTRADO);

	if (!Atributo->NonResidentFlag) {
		unsigned Offset=Atributo->Form.Resident.ValueOffset;
		unsigned Tamano=Atributo->Form.Resident.ValueLength;

		if (Offset+Tamano>Atributo->RecordLength) return(CODERROR_FILESYSTEM_CORRUPTO);
		const unsigned char *Inicio=reinterpret_cast<const unsigned char *>(Atributo)+Offset;
		Contenido.assign(Inicio, Inicio+Tamano);
		return(CODERROR_NINGUNO);
	}

	/* Los tramos siguientes se encuentran usando $ATTRIBUTE_LIST */
	if (Atributo->Form.NonResident.FirstVCN!=0) return(CODERROR_FILESYSTEM_CORRUPTO);

	ULONGLONG TamanoReal=Atributo->Form.NonResident.RealSize;
	ULONGLONG TamanoInicializado=Atributo->Form.NonResident.InitializedSize;
	if (TamanoReal>UINT_MAX) return(CODERROR_FALTA_MEMORIA);
	if (TamanoInicializado>TamanoReal) return(CODERROR_FILESYSTEM_CORRUPTO);

	std::vector<TDataRun> DataRuns;
	int CodError=DecodificarDataRuns(Atributo, DataRuns);
	if (CodError!=CODERROR_NINGUNO) return(CodError);

	Contenido.resize(TamanoReal, 0);
	if (TamanoInicializado==0) return(CODERROR_NINGUNO);

	return(LeerDesdeDataRuns(DataRuns, 0, Contenido.data(), TamanoInicializado));
}

int TDriverNTFS::InicializarMFT()
{
	if (MFTInicializada) return(CODERROR_NINGUNO);
	if (DatosFS.DatosEspecificos.NTFS.BytesPorFileRecordSegment<=0 || DatosFS.BytesPorCluster<=0)
		return(CODERROR_SUPERBLOQUE_INVALIDO);

	unsigned TamanoRegistro=DatosFS.DatosEspecificos.NTFS.BytesPorFileRecordSegment;
	std::vector<unsigned char> RegistroMFT(TamanoRegistro);
	__u64 OffsetMFT=DatosFS.DatosEspecificos.NTFS.ClusterMFT*(__u64)DatosFS.BytesPorCluster;

	/* El boot sector permite encontrar el registro 0 de $MFT */
	int CodError=LeerBytesDeImagen(OffsetMFT, RegistroMFT.data(), TamanoRegistro);
	if (CodError!=CODERROR_NINGUNO) return(CodError);

	CodError=AplicarFixups(RegistroMFT.data(), TamanoRegistro);
	if (CodError!=CODERROR_NINGUNO) return(CodError);
	if (memcmp(RegistroMFT.data(), "FILE", 4)!=0) return(CODERROR_FILESYSTEM_CORRUPTO);

	ATTRIBUTE_RECORD_HEADER *AtributoDatos=
		BuscarAtributo(RegistroMFT.data(), TamanoRegistro, NTFS_STRUCT_DATA);
	if (!AtributoDatos || !AtributoDatos->NonResidentFlag)
		return(CODERROR_FILESYSTEM_CORRUPTO);

	CodError=DecodificarDataRuns(AtributoDatos, DataRunsMFT);
	if (CodError!=CODERROR_NINGUNO) return(CodError);

	MFTInicializada=true;
	return(CODERROR_NINGUNO);
}

int TDriverNTFS::LeerRegistroMFT(__u64 Indice, std::vector<unsigned char> &Registro)
{
	int CodError=InicializarMFT();
	if (CodError!=CODERROR_NINGUNO) return(CodError);

	unsigned TamanoRegistro=DatosFS.DatosEspecificos.NTFS.BytesPorFileRecordSegment;
	if (Indice>ULLONG_MAX/TamanoRegistro) return(CODERROR_FILESYSTEM_CORRUPTO);

	Registro.resize(TamanoRegistro);
	CodError=LeerDesdeDataRuns(DataRunsMFT, Indice*TamanoRegistro, Registro.data(), TamanoRegistro);
	if (CodError!=CODERROR_NINGUNO) return(CodError);

	CodError=AplicarFixups(Registro.data(), TamanoRegistro);
	if (CodError!=CODERROR_NINGUNO) return(CodError);
	if (memcmp(Registro.data(), "FILE", 4)!=0) return(CODERROR_FILESYSTEM_CORRUPTO);

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

