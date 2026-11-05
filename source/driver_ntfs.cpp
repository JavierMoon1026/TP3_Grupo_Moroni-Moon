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
	// Acá había un error de reporte, estaba desfasado con 1 por debajo. Le sumás uno y chau.
	DatosFS.NumeroDeClusters = Boot->SectorCount64 / Boot->SectorsPerCluster + 1;

	TDatosFSNTFS &NTFS = DatosFS.DatosEspecificos.NTFS;
	// Igual que arriba; desfasado con 1 por debajo. De nuevo, sumás uno y chau.
	NTFS.TotalSectores = Boot->SectorCount64 + 1;
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

/*
	~~~ | ~~~
	Estas primeras funciones auxiliares son de propósito general. Son utilizadas para traducir formatos del File System
	a uno más amigable para el código. Algo así como un "traductor".
	~~~ | ~~~
*/

// Esta función convierte un nombre UTF-16LE a uno UTF-8. Es decir, de lo que usa NTFS a lo que usa el código.
static TString UTF16aUTF8(const WCHAR * Nombre, UCHAR Largo) {
	TString Resultado;
	Resultado.reserve(Largo);
	for (unsigned i = 0; i < Largo; ) {
		unsigned Punto = Nombre[i++];
		// Par sustituto, son dos WCHAR juntos codifican un punto de código mayor a 0xFFFF.
		if (Punto >= 0xD800 && Punto <= 0xDBFF && i < Largo && Nombre[i] >= 0xDC00 && Nombre[i] <= 0xDFFF)
			Punto = 0x10000 + ((Punto - 0xD800) << 10) + (Nombre[i++] - 0xDC00);
		if (Punto < 0x80) {
			Resultado += (char)Punto;
		} else if (Punto < 0x800) {
			Resultado += (char)(0xC0 | (Punto >> 6));
			Resultado += (char)(0x80 | (Punto & 0x3F));
		} else if (Punto < 0x10000) {
			Resultado += (char)(0xE0 | (Punto >> 12));
			Resultado += (char)(0x80 | ((Punto >> 6) & 0x3F));
			Resultado += (char)(0x80 | (Punto & 0x3F));
		} else {
			Resultado += (char)(0xF0 | (Punto >> 18));
			Resultado += (char)(0x80 | ((Punto >> 12) & 0x3F));
			Resultado += (char)(0x80 | ((Punto >> 6) & 0x3F));
			Resultado += (char)(0x80 | (Punto & 0x3F));
		}
	}
	return Resultado;
}

// Esta otra función se encarga de mostrar el tiempo correctamente, porque NTFS guarda las fechas de forma
// diferente, empezando alrededor del año 1600.
static time_t TicksAFecha(ULONGLONG Ticks) {
	// El número "mágico". Esta es la cantidad de segundos entre ese año y los 70s.
    const __s64 DiferenciaEntreEpochs=116444736000000000LL;
    return (time_t)(((__s64)Ticks - DiferenciaEntreEpochs) / 10000000LL);
}

// Esta función también es útil, porque traduce los atributos de un registro a flags que entienda nuestro programa.
static unsigned TraducirFlags(ULONG FileAttributes) {
	unsigned Flags = 0;
	if (FileAttributes & NTFS_READ_ONLY) Flags |= fedSOLO_LECTURA;
	if (FileAttributes & NTFS_HIDDEN) Flags |= fedOCULTO;
	if (FileAttributes & NTFS_SYSTEM) Flags |= fedSISTEMA;
	if (FileAttributes & NTFS_DIRECTORY) Flags |= fedDIRECTORIO;
	if (FileAttributes & NTFS_ARCHIVE) Flags |= fedARCHIVAR;
	if (FileAttributes & NTFS_COMPRESSED) Flags |= fedCOMPRIMIDO;
	if (FileAttributes & NTFS_ENCTRYPTED) Flags |= fedENCRIPTADO;
	if (FileAttributes & NTFS_SPARSE_FILE) Flags |= fedDISPERSO;
	return Flags;
}

/*
	~~~ | ~~~
	Ahora vamos con las funciones auxiliares del árbol B-Tree que usa internamente el sistema de NTFS para los
	directorios. Después, seguirán las funciones propiamente dichas de la búsqueda.
	~~~ | ~~~
*/

// Esta función simplemente compara dos nombres ingnorando mayúsculas o minúsculas.
// A NTFS no le importa si es mayúscula o minúscula, es case-insensitive.
static bool NombresIguales(const TString & A, const TString & B) {
	if (A.size() != B.size()) return false;
	for (size_t i = 0; i < A.size(); i++) {
		if (tolower((unsigned char)A[i]) != tolower((unsigned char)B[i])) return false;
	}
	return true;
}

// Si el atributo no tiene nombre o coincide con el pedido, esta función devuelve true.
static bool AtributoCoincideNombre(const ATTRIBUTE_RECORD_HEADER * Atributo, const char * Nombre) {
	if (!Nombre || !Nombre[0]) return true; // O no me dieron un atributo o no me dieron un nombre. Nada para buscar.
	if (Atributo->NameLength == 0) return false; // Me pidieron un nombre, pero el atributo no tiene nada.
	const WCHAR * NombreAtributo = reinterpret_cast<const WCHAR *>(reinterpret_cast<const unsigned char *>(Atributo) + Atributo->NameOffset);
	for (unsigned i = 0; i < Atributo->NameLength; i++) {
		if (!Nombre[i] || (unsigned char)Nombre[i] != (unsigned char)NombreAtributo[i]) return false;
	}
	return Nombre[Atributo->NameLength] == '\0';
}

// Y esta otra función te dice si el registro es un directorio o no (True/False).
static bool RegistroEsDirectorio(const unsigned char * Registro) {
	const FILE_RECORD_SEGMENT_HEADER * Encabezado = reinterpret_cast<const FILE_RECORD_SEGMENT_HEADER *>(Registro);
	return (Encabezado->Flags & NTFS_RECORD_ES_DIRECTORIO) != 0;
}

/*
	~~~ | ~~~
	Estas son las funciones para poder recorrer los paths propiamente dichos. El NTFS funciona como un B-Tree, entonces
	es necesario implementar ese mismo tipo de lógica.
	~~~ | ~~~
*/

// Es como la función para buscar un atributo, pero si no lo encuentra en el registro base, lo va a buscar en los
// registros de extensión. Devuelve el puntero al atributo.
ATTRIBUTE_RECORD_HEADER * TDriverNTFS::BuscarAtributoEnLista(unsigned char * Registro, unsigned TamanoRegistro,
					ATTRIBUTE_TYPE_CODE Tipo, const char * Nombre, std::vector<unsigned char> & BufferExtension) {

	// Primero, está en el registro base?
	ATTRIBUTE_RECORD_HEADER * Atributo = BuscarAtributo(Registro, TamanoRegistro, Tipo);
	if (Atributo && AtributoCoincideNombre(Atributo, Nombre)) return Atributo;

	// Si los archivos muy grandes guardan algunos atributos en registros de extensión.
	ATTRIBUTE_RECORD_HEADER * AttrLista = BuscarAtributo(Registro, TamanoRegistro, NTFS_STRUCT_ATTRIBUTE_LIST);
	if (!AttrLista) return NULL;

	std::vector<unsigned char> Contenido;
	if (LeerContenidoAtributo(AttrLista, Contenido) != CODERROR_NINGUNO) return NULL;
	for (unsigned Offset = 0; Offset + 26 <= Contenido.size(); ) {
		const ATTRIBUTE_LIST_ENTRY * Entrada = reinterpret_cast<const ATTRIBUTE_LIST_ENTRY *>(Contenido.data() + Offset);
		if (Entrada->TypeCode == NTFS_STRUCT_END || Entrada->RecordLength < 26 || Offset + Entrada->RecordLength > Contenido.size()) break;
		if (Entrada->TypeCode == Tipo) {
			// Carga el registro que contiene el atributo y lo busca en ese lugar.
			std::vector<unsigned char> RegistroExtension;
			if (LeerRegistroMFT(Entrada->SegmentReference.MFTIndex, RegistroExtension) == CODERROR_NINGUNO) {
				ATTRIBUTE_RECORD_HEADER * Hallado = BuscarAtributo(RegistroExtension.data(), RegistroExtension.size(), Tipo);
				if (Hallado && AtributoCoincideNombre(Hallado, Nombre)) {
					BufferExtension = std::move(RegistroExtension);
					return Hallado;
				}
			}
		}
		Offset += Entrada->RecordLength;
	}
	return NULL;
}

// Esta función consigue los DataRuns completos de un atributo que se no residente. Si tuviera un attribute list,
// se recorren todos sus fragmentos y se concatenan sus DataRuns.
int TDriverNTFS::ObtenerDataRunsAtributo(unsigned char * Registro, unsigned TamanoRegistro, ATTRIBUTE_TYPE_CODE Tipo,
										 const char * Nombre, std::vector<TDataRun> & DataRuns) {
	DataRuns.clear();

	// Este es el caso simple, no hay lista de atributos, entonces, todo está en el registro base.
	ATTRIBUTE_RECORD_HEADER * AttrLista = BuscarAtributo(Registro, TamanoRegistro, NTFS_STRUCT_ATTRIBUTE_LIST);
	if (!AttrLista) {
		ATTRIBUTE_RECORD_HEADER * Atributo = BuscarAtributo(Registro, TamanoRegistro, Tipo);
		if (!Atributo || !Atributo->NonResidentFlag || !AtributoCoincideNombre(Atributo, Nombre)) return CODERROR_NO_ENCONTRADO;
		return DecodificarDataRuns(Atributo, DataRuns);
	}

	// Este sería el caso más difícil, recorrer la lista de atributos.
	std::vector<unsigned char> Contenido;
	int CodError = LeerContenidoAtributo(AttrLista, Contenido);
	if (CodError != CODERROR_NINGUNO) return CodError;
	for (unsigned Offset = 0; Offset + 26 <= Contenido.size(); ) {
		const ATTRIBUTE_LIST_ENTRY * Entrada = reinterpret_cast<const ATTRIBUTE_LIST_ENTRY *>(Contenido.data() + Offset);
		if (Entrada->TypeCode == NTFS_STRUCT_END || Entrada->RecordLength < 26 || Offset + Entrada->RecordLength > Contenido.size()) break;

		// Sólo el fragmento con LowestVcn = 0 arranca desde el principio, si no, acumulan clusters y se procesan en el orden de la lstita.
		if (Entrada->TypeCode == Tipo) {
			std::vector<unsigned char> RegistroFragmento;
			if (LeerRegistroMFT(Entrada->SegmentReference.MFTIndex, RegistroFragmento) != CODERROR_NINGUNO) return CODERROR_FILESYSTEM_CORRUPTO;

			// El registro base también aparece en la lista, vovler a cargarlo no hace nada.
			ATTRIBUTE_RECORD_HEADER * Atributo = BuscarAtributo(RegistroFragmento.data(), RegistroFragmento.size(), Tipo);
			if (!Atributo || !Atributo->NonResidentFlag || !AtributoCoincideNombre(Atributo, Nombre)) return CODERROR_FILESYSTEM_CORRUPTO;
			std::vector<TDataRun> RunsFragmento;
			CodError = DecodificarDataRuns(Atributo, RunsFragmento);
			if (CodError != CODERROR_NINGUNO) return CodError;

			DataRuns.insert(DataRuns.end(), RunsFragmento.begin(), RunsFragmento.end());
		}
		Offset += Entrada->RecordLength;
	}
	if (DataRuns.empty()) return CODERROR_NO_ENCONTRADO;
	return CODERROR_NINGUNO;
}

// Esta función carga en memoria todo el nodo apuntado por el VCN de una entrada con subnodo. Ojo que el VCN está en unidades de cluster,
// entonces el offeset dentro del index allocation es con una operación matemática adicional.
int TDriverNTFS::CargarNodoIndice(__u64 VCN, const std::vector<TDataRun> & DataRunsAllocacion,
								 unsigned TamanoNodoIndice, std::vector<unsigned char> & Nodo) {
	if (TamanoNodoIndice<(unsigned)DatosFS.BytesPorSector || TamanoNodoIndice % (unsigned)DatosFS.BytesPorSector != 0 ||
		DatosFS.BytesPorCluster <= 0 || DataRunsAllocacion.empty()) return CODERROR_FILESYSTEM_CORRUPTO;
	Nodo.resize(TamanoNodoIndice);

	// No olvidar que el VCN está en unidades de cluster y no de buffer.
	int CodError = LeerDesdeDataRuns(DataRunsAllocacion, VCN * (__u64)DatosFS.BytesPorCluster, Nodo.data(), TamanoNodoIndice);
	if (CodError != CODERROR_NINGUNO) return CodError;

	CodError = AplicarFixups(Nodo.data(), TamanoNodoIndice);
	if (CodError != CODERROR_NINGUNO) return CodError;
	if (memcmp(Nodo.data(), "INDX", 4) != 0) return(CODERROR_FILESYSTEM_CORRUPTO);

	// El VCN declarado por el buffer debe ser el pedido.
	FILE_RECORD_INDEX_HEADER * Encabezado = reinterpret_cast<FILE_RECORD_INDEX_HEADER *>(Nodo.data());
	if (Encabezado->IndexVCN != VCN) return(CODERROR_FILESYSTEM_CORRUPTO);
	return(CODERROR_NINGUNO);
}

// Esta función recorre las entradas de un nodo del índice en preorder. Ojo que los nodos internos también guardan entradas reales.
int TDriverNTFS::EnumerarNodo(unsigned char * Nodo, unsigned TamanoNodo, unsigned OffsetEncabezado, const std::vector<TDataRun> & DataRunsAllocacion,
	unsigned TamanoNodoIndice, TpColectoraDatosIndice pFuncion, void * pParametroUsuario, unsigned Nivel) {

        if (!Nodo || Nivel > 64 || OffsetEncabezado + sizeof(IDX_HEADER)>TamanoNodo) return CODERROR_FILESYSTEM_CORRUPTO;

        IDX_HEADER * Encabezado = reinterpret_cast<IDX_HEADER *>(Nodo + OffsetEncabezado);
        unsigned Offset = OffsetEncabezado + Encabezado->OffsetFirstEntry;
        unsigned Fin = OffsetEncabezado + Encabezado->IndexEntriesUsed;
        while (Offset + offsetof(INDEX_RECORD, Data) <= Fin && Offset + offsetof(INDEX_RECORD, Data) <= TamanoNodo) {
			INDEX_RECORD * Entrada = reinterpret_cast<INDEX_RECORD *>(Nodo + Offset);
			if (Entrada->RecordLength<offsetof(INDEX_RECORD, Data) || Offset + Entrada->RecordLength>TamanoNodo) return CODERROR_FILESYSTEM_CORRUPTO;

			// Primero se reporta la entrada.
			if (!(Entrada->Flags & NTFS_INDEX_ENTRADA_FIN) && Entrada->StreamLength > 0) {
				int CodError = (this->*pFuncion)(Entrada, pParametroUsuario);
				if (CodError != CODERROR_NINGUNO) return CodError;
			}

			// Ahora, si la entrada tiene putnero, se baja al subnodo.
			if (Entrada->Flags & NTFS_INDEX_ENTRADA_TIENE_SUBNODO) {
				if (Entrada->RecordLength<sizeof(ULONGLONG)) return CODERROR_FILESYSTEM_CORRUPTO;

				ULONGLONG VCN;
				memcpy(&VCN, reinterpret_cast<unsigned char *>(Entrada) + Entrada->RecordLength-sizeof(ULONGLONG), sizeof(VCN));
				std::vector<unsigned char> Subnodo;
				int CodError = CargarNodoIndice(VCN, DataRunsAllocacion, TamanoNodoIndice, Subnodo);
				if (CodError != CODERROR_NINGUNO) return CodError;

				// En un buffer de índice, el idx header está dentro del offset 24.
				CodError = EnumerarNodo(Subnodo.data(), Subnodo.size(), offsetof(FILE_RECORD_INDEX_HEADER, Header), DataRunsAllocacion, TamanoNodoIndice,
										pFuncion, pParametroUsuario, Nivel + 1);
				if (CodError != CODERROR_NINGUNO) return CodError;
			}

			if (Entrada->Flags & NTFS_INDEX_ENTRADA_FIN) break;
			Offset += Entrada->RecordLength;
        }
        return CODERROR_NINGUNO;
}

// Esta función recorre el índice I30 de un directorio cuyo registro MFT ya fue leído y tiene los fixup aplicados. El nodo raíz del árbol
// vive en el index_root, como residete.
int TDriverNTFS::EnumerarIndice(unsigned char * Registro, unsigned TamanoRegistro, TpColectoraDatosIndice pFuncion, void * pParametroUsuario) {
	// Como el index_root siempre es residente, si no está en el registro base, se busca en los de extensión.
	std::vector<unsigned char> BufferExtension;
	unsigned char * NodoRaiz = Registro;
	unsigned TamanoRaiz = TamanoRegistro;
	ATTRIBUTE_RECORD_HEADER * IndexRoot = BuscarAtributo(Registro, TamanoRegistro, NTFS_STRUCT_INDEX_ROOT);

	if (!IndexRoot) {
		IndexRoot = BuscarAtributoEnLista(Registro, TamanoRegistro, NTFS_STRUCT_INDEX_ROOT, "$I30", BufferExtension);
		if (!IndexRoot) return CODERROR_FILESYSTEM_CORRUPTO;
		NodoRaiz = BufferExtension.data();
		TamanoRaiz = BufferExtension.size();
	}
	if (IndexRoot->NonResidentFlag) return CODERROR_FILESYSTEM_CORRUPTO;

	unsigned OffsetAtributo = (unsigned)(reinterpret_cast<unsigned char *>(IndexRoot) - NodoRaiz);
	unsigned OffsetValor = OffsetAtributo + IndexRoot->Form.Resident.ValueOffset;
	if (IndexRoot->Form.Resident.ValueOffset + IndexRoot->Form.Resident.ValueLength>IndexRoot->RecordLength ||
		OffsetValor + sizeof(IDX_ROOT) + sizeof(IDX_HEADER) > TamanoRaiz) return CODERROR_FILESYSTEM_CORRUPTO;

	const IDX_ROOT *Raiz=reinterpret_cast<const IDX_ROOT *>(NodoRaiz+OffsetValor); // Es de este índice nomás, no siempre coincide con el boot.
	unsigned TamanoNodoIndice = Raiz->IndexAllocationEntrySize;
	if (TamanoNodoIndice < (unsigned)DatosFS.BytesPorSector || TamanoNodoIndice % (unsigned)DatosFS.BytesPorSector != 0) return CODERROR_FILESYSTEM_CORRUPTO;

	// Si el índice es chico, el atributo no existe y sus entradas están en el index_root.
	std::vector<TDataRun> DataRunsAllocacion;
	int CodError = ObtenerDataRunsAtributo(Registro, TamanoRegistro, NTFS_STRUCT_INDEX_ALLOCATION, "$I30", DataRunsAllocacion);
	if (CodError == CODERROR_FILESYSTEM_CORRUPTO) return CodError;
	unsigned OffsetEncabezado = OffsetValor + sizeof(IDX_ROOT);
	return EnumerarNodo(NodoRaiz, TamanoRaiz, OffsetEncabezado, DataRunsAllocacion, TamanoNodoIndice, pFuncion, pParametroUsuario, 0);
}

// Por cada index records, arma una entrada. Todos los datos salen del file name que está en la propia entrada del índice.
int TDriverNTFS::ColectoraListarEntradas(INDEX_RECORD * EntradaIndice, void * pParametroUsuario) {
	std::vector<TEntradaDirectorio> * Entradas = reinterpret_cast<std::vector<TEntradaDirectorio> *>(pParametroUsuario);
	if (EntradaIndice->StreamLength < offsetof(FILE_NAME, FileName)) return CODERROR_FILESYSTEM_CORRUPTO;

	FILE_NAME * Nombre = reinterpret_cast<FILE_NAME *>(EntradaIndice->Data);
	if (EntradaIndice->StreamLength<offsetof(FILE_NAME, FileName) + 2 * Nombre->FileNameLength) return CODERROR_FILESYSTEM_CORRUPTO;
	const WCHAR * TextoNombre = reinterpret_cast<const WCHAR *>(reinterpret_cast<const unsigned char *>(Nombre) + offsetof(FILE_NAME, FileName));
	TEntradaDirectorio Entrada = {};

	// Y todo esto sale del file name, de la propia entrada del índice.
	Entrada.Nombre = UTF16aUTF8(TextoNombre, Nombre->FileNameLength);
	Entrada.Flags = TraducirFlags(Nombre->FileAttributes);
	Entrada.Bytes = Nombre->RealSize;
	Entrada.FechaCreacion = TicksAFecha(Nombre->UTCCreation);
	Entrada.FechaUltimoAcceso = TicksAFecha(Nombre->UTCRLastAccesed);
	Entrada.FechaUltimaModificacion = TicksAFecha(Nombre->UTCModification);
	Entrada.DatosEspecificos.NTFS.IndiceMFT = EntradaIndice->FileReference.MFTIndex;
	Entrada.DatosEspecificos.NTFS.NroSecuencia = EntradaIndice->FileReference.Sequence;
	
	Entradas->push_back(Entrada);
	return CODERROR_NINGUNO;
}

// Esta otra función compara el nombre de cada entrada del índice con el componente de path buscado. Igual que antes, es case-insensitve.
int TDriverNTFS::ColectoraBuscarNombre(INDEX_RECORD *EntradaIndice, void * pParametroUsuario) {
	TBusquedaNombre * Busqueda = reinterpret_cast<TBusquedaNombre *>(pParametroUsuario);
	if (Busqueda->Encontrado) return CODERROR_NINGUNO;
	if (EntradaIndice->StreamLength<offsetof(FILE_NAME, FileName)) return CODERROR_FILESYSTEM_CORRUPTO;

	FILE_NAME * Nombre = reinterpret_cast<FILE_NAME *>(EntradaIndice->Data);
	if (EntradaIndice->StreamLength<offsetof(FILE_NAME, FileName) + 2 * Nombre->FileNameLength) return CODERROR_FILESYSTEM_CORRUPTO;

	const WCHAR * TextoNombre = reinterpret_cast<const WCHAR *>(reinterpret_cast<const unsigned char *>(Nombre) + offsetof(FILE_NAME, FileName));
	TString Candidato = UTF16aUTF8(TextoNombre, Nombre->FileNameLength);
	if (NombresIguales(Candidato, Busqueda->Nombre)) {
		Busqueda->Encontrado = true;
		Busqueda->IndiceMFT = EntradaIndice->FileReference.MFTIndex;
	}
	return CODERROR_NINGUNO;
}

// Esta función resuelve un path absoluto en el número de registro de la MFT del elemento final. Empieza desde el registro 5 (La raíz)
// y por cada componente del path, busca el nombre en el índice del directorio actual, bajando al registro MFT encontrado.
int TDriverNTFS::ResolverPath(const char * Path, __u64 & IndiceMFT) {
	IndiceMFT = NTFS_ELEM_ROOT_DIR;
	if (!Path || Path[0] != '/') return CODERROR_RUTA_NO_ABSOLUTA;

	std::vector<unsigned char> Registro;
	int CodError = LeerRegistroMFT(IndiceMFT, Registro);
	if (CodError != CODERROR_NINGUNO) return CodError;
	
	const char * Inicio = Path;
	while (*Inicio) {
		// Nos saltamos las barras, sean iniciales, repetidas o finales.
		while (*Inicio == '/') Inicio++;
		if (!*Inicio) break;

		const char * Fin = Inicio;
		while (*Fin && *Fin != '/') Fin++;

		TString Componente(Inicio, Fin - Inicio);
		Inicio = Fin;

		// Ahora, buscamos el componente en el índice del directorio acutal.
		TBusquedaNombre Busqueda;
		Busqueda.Nombre = Componente;
		Busqueda.Encontrado = false;
		Busqueda.IndiceMFT = 0;
		CodError = EnumerarIndice(Registro.data(), Registro.size(), &TDriverNTFS::ColectoraBuscarNombre, &Busqueda);
		if (CodError != CODERROR_NINGUNO) return CodError;
		if (!Busqueda.Encontrado) return CODERROR_DIRECTORIO_INEXISTENTE;
		IndiceMFT = Busqueda.IndiceMFT;

		// Si quedan más componentes, seguimos bajando por este registro.
		const char * Resto = Inicio;
		while (*Resto == '/') Resto++;
		if (!*Resto) break;
		CodError = LeerRegistroMFT(IndiceMFT, Registro);
		if (CodError != CODERROR_NINGUNO) return CodError;

		// Para poder bajar a otro nivel, el componente encontrado tiene que ser un directorio.
		if (!RegistroEsDirectorio(Registro.data())) return CODERROR_DIRECTORIO_INEXISTENTE;
	}
	return(CODERROR_NINGUNO);
}

// Y así llegamos a la tan esperada función a implementar! ListarDirectorio()
int TDriverNTFS::ListarDirectorio(const char *Path, std::vector<TEntradaDirectorio> &Entradas)
{
	Entradas.clear();

	// 1. Resolvemos el path: arrancamos en la raíz y bajamos por los índices.
	__u64 IndiceMFT;
	int CodError = ResolverPath(Path, IndiceMFT);
	if (CodError != CODERROR_NINGUNO) return CodError;

	// 2. Leemos el registro MFT del directorio a listar.
	std::vector<unsigned char> Registro;
	CodError = LeerRegistroMFT(IndiceMFT, Registro);
	if (CodError != CODERROR_NINGUNO) return CodError;

	// 3. Revisamos que el archivo no tenga el índice I30, porque sólo se pueden listar directorios.
	if (!RegistroEsDirectorio(Registro.data())) return CODERROR_DIRECTORIO_INEXISTENTE;

	// 4. Ahora sí, recorremos el índice I30 completo, llenando una entrada por cada nombre.
	return EnumerarIndice(Registro.data(), Registro.size(), &TDriverNTFS::ColectoraListarEntradas, &Entradas);
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

