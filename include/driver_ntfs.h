#ifndef	__DRIVER_NTFS__H__
#define	__DRIVER_NTFS__H__

/************************
 *			*
 *     Constantes	*
 *			*
 ************************/
/* Códigos de error internos a este driver */
#define	CODERROR_NO_ENCONTRADO			(CODERROR_ALUMNO-1)

/* Elementos en posiciones específicas */
#define	NTFS_ELEM_MFT				 0
#define	NTFS_ELEM_MFT_MIRROR			 1
#define	NTFS_ELEM_LOG_FILE			 2
#define	NTFS_ELEM_VOLUME			 3
#define	NTFS_ELEM_ATTR_DEF			 4
#define	NTFS_ELEM_ROOT_DIR			 5
#define	NTFS_ELEM_BITMAP			 6
#define	NTFS_ELEM_BOOT				 7
#define	NTFS_ELEM_BAD_CLUS			 8
#define	NTFS_ELEM_SECURE			 9
#define	NTFS_ELEM_UPCASE			10
#define	NTFS_ELEM_EXTEND			11
#define	NTFS_ELEM_QUOTA				24
#define	NTFS_ELEM_OBJID				25
#define	NTFS_ELEM_REPARSE			26

/* Estructuras de los elementos que tengo definidas */
#define	NTFS_STRUCT_STANDARD_INFORMATION	0x00000010
#define	NTFS_STRUCT_ATTRIBUTE_LIST		0x00000020
#define	NTFS_STRUCT_FILE_NAME			0x00000030
#define	NTFS_STRUCT_DATA			0x00000080
#define	NTFS_STRUCT_INDEX_ROOT			0x00000090
#define	NTFS_STRUCT_INDEX_ALLOCATION		0x000000A0
#define NTFS_STRUCT_END				0xFFFFFFFF

/* Flags para el campo FILE_NAME.FileAttributes */
#define	NTFS_READ_ONLY				0x00000001
#define	NTFS_HIDDEN				0x00000002
#define	NTFS_SYSTEM				0x00000004
#define	NTFS_ARCHIVE				0x00000020
#define	NTFS_DEVICE				0x00000040
#define	NTFS_NORMAL				0x00000080
#define	NTFS_TEMPORARY				0x00000100
#define	NTFS_SPARSE_FILE			0x00000200
#define	NTFS_REPARSE_POINT			0x00000400
#define	NTFS_COMPRESSED				0x00000800
#define	NTFS_OFFLINE				0x00001000
#define	NTFS_NOT_CONTENT_INDEXED		0x00002000
#define	NTFS_ENCTRYPTED				0x00004000
#define	NTFS_DIRECTORY				0x10000000	// Directory (copy from corresponding bit in MFT record)
#define	NTFS_INDEX_VIEW				0x20000000	// Index View (copy from corresponding bit in MFT record)



/************************
 *			*
 *     Estructuras	*
 *			*
 ************************/
/* Declaraciones de tipos para estructuras sacadas de Microsoft */
typedef	char16_t			WCHAR;
typedef	char				CHAR;
typedef	unsigned char			UCHAR;
typedef short				SHORT;
typedef unsigned short			USHORT;
typedef int	 			LONG;
typedef unsigned int			ULONG;
typedef unsigned long long		LONGLONG;
typedef unsigned long long		ULONGLONG;

/* Encabezado de un elemento del $MFT (sacado de learn.microsoft.com) */
typedef struct __attribute__((packed))
    {
	UCHAR  				Signature[4];
	USHORT 				UpdateSequenceArrayOffset;
	USHORT 				UpdateSequenceArraySize;
    }	MULTI_SECTOR_HEADER;

typedef struct __attribute__((packed))
    {
	ULONGLONG			MFTIndex : 48;			// 6 primeros bytes índice en $MFT, último word sequence number
	ULONGLONG			Sequence : 16;
    }	FILE_REFERENCE;

typedef struct __attribute__((packed))
    {
	USHORT				UpdateSequenceNumber;
	USHORT				UpdateSequenceArray[1];		// La cantidad de elementos de esta estructura la determina MULTI_SECTOR_HEADER.UpdateSequenceArraySize
    }	UPDATE_SEQUENCE_ARRAY;

/* Estaestructura está al comienzo de cada elemento del $MTF */
typedef struct __attribute__((packed))
    {
	MULTI_SECTOR_HEADER   		MultiSectorHeader;
	ULONGLONG			LogFileSequenceNumber;
	USHORT                		SequenceNumber;
	USHORT                		HardLinkCount;
	USHORT                		FirstAttributeOffset;
	USHORT                		Flags;
	ULONG                 		RealSizeOfFileRecord;
	ULONG				AllocatedSizeOfFileRecord;
	FILE_REFERENCE			BaseFileRecordSegment;
	USHORT              		NextAttributeId;
	UCHAR				Padding[2];
	ULONG                		RecordId;
	UPDATE_SEQUENCE_ARRAY		UpdateSequenceArray;
    }	FILE_RECORD_SEGMENT_HEADER;

typedef ULONG				ATTRIBUTE_TYPE_CODE;

typedef	ULONGLONG			VCN;			// Clusters "virtuales"

typedef	ULONGLONG			LCN;			// Clusters "lógicos" (asignados?)

/* Esta estructura encabeza toda otra estructura debajo de FILE_RECORD_SEGMENT_HEADER */
typedef struct __attribute__((packed))
    {
	ATTRIBUTE_TYPE_CODE		TypeCode;
	ULONG               		RecordLength;
	UCHAR               		NonResidentFlag;
	UCHAR               		NameLength;
	USHORT              		NameOffset;
	USHORT              		Flags;
	USHORT              		AttributeId;
	union
	    {
		struct
		    {
			ULONG		ValueLength;
			USHORT		ValueOffset;
			UCHAR 		IndexedFlag;
			UCHAR		Padding;
		    }	Resident;
		struct
		    {
			VCN      	FirstVCN;
			VCN      	LastVCN;
			USHORT   	DataRunsOffset;
			USHORT   	CompressionUnitSize;
			UCHAR    	Padding[4];
			LONGLONG 	AllocatedLength;
			LONGLONG 	RealSize;
			LONGLONG 	InitializedSize;
		    }	NonResident;
	    }	Form;
    }	ATTRIBUTE_RECORD_HEADER;

typedef struct __attribute__((packed))
    {
	UCHAR				Length : 4;
	UCHAR				Start  : 4;
	UCHAR				DataRunInfo[1];			// Longitud: Length bytes seguidos de Start bytes.
    }	DATA_RUN;

typedef struct __attribute__((packed))
    {
	DATA_RUN			DataRuns[1];			// Longitud indefinida, hasta que DataRun[n].Length de cero.
    }	DATA_RUNS;

/****************************************************************************************************************************************
 *	Las siguientes estructuras (con TypeCodes 0x10 a 0xA0) se encuentan inmediatamente después de FILE_RECORD_SEGMENT_HEADER	*
 *	CUANDO ESTÁN RESIDENTES (ATTRIBUTE_RECORD_HEADER.NonResidentFlag=0). Si NO ESTÁN RESIDENTES, la sigue una estructura DATA_RUNS	*
 *	que especifica los rangos de clusers en los que está la estructura.								*
 *	En los clusters que contienen la información no-residente, encontramos dos casos:						*
 *		1) Algunas estructuras, como INDEX_ROOT, van precedidas por otras que permiten implementar fixups en esos clusters no	*
 *		   residentes (FILE_RECORD_INDEX_HEADER).										*
 *		2) Otras, como ATTRIBUTE_LIST simplemente contienen la estructura sola.							*
 ****************************************************************************************************************************************/

/* Estructura STANDARD_INFORMATION (TypeCode = 0x10) */
typedef struct __attribute__((packed))
    {
	ULONGLONG			UTCCreation;
	ULONGLONG			UTCModification;
	ULONGLONG			UTCRecModification;
	ULONGLONG			UTCRLastAccesed;
	ULONG				FilePremissions;
	ULONG				MaximumNumberOfVersions;
	ULONG				VersionNumber;
	ULONG				ClassId;
	ULONG 				OwnerId;
	ULONG				SecurityId;
    }	STANDARD_INFORMATION;

/* Estructura ATTRIBUTE_LIST (TypeCode = 0x20) */
typedef struct __attribute__((packed))
    {
	ATTRIBUTE_TYPE_CODE		TypeCode;
	USHORT                		RecordLength;
	UCHAR                 		AttributeNameLength;
	UCHAR                 		AttributeNameOffset;
	VCN                   		LowestVcn;
	FILE_REFERENCE 			SegmentReference;
	USHORT                		Reserved;
	WCHAR                 		AttributeName[1];
    }	ATTRIBUTE_LIST_ENTRY;						// La estructura se repite tantas veces como permitan ATTRIBUTE_RECORD_HEADER.RealSize bytes

/* Estructura FILE_NAME (TypeCode = 0x30) */
typedef struct __attribute__((packed))
    {
	FILE_REFERENCE 			ParentDirectory;
	ULONGLONG			UTCCreation;
	ULONGLONG			UTCModification;
	ULONGLONG			UTCRecModification;
	ULONGLONG			UTCRLastAccesed;
	ULONGLONG			AllocatedSize;
	ULONGLONG			RealSize;
	ULONG				FileAttributes;
	UCHAR          			Reserved[4];
	UCHAR          			FileNameLength;
	UCHAR          			Flags;
	WCHAR          			FileName[1];			// La cantidad de elementos de esta estructura la determina FILE_NAME.FileNameLength
    }	FILE_NAME;

typedef struct __attribute__((packed))
    {
	ULONG				IndexedAttributeType;
	ULONG				CollationRule;
	ULONG				IndexAllocationEntrySize;
	UCHAR				ClustersPerIndexRecord;
	UCHAR				Padding[3];
    }	IDX_ROOT;

typedef struct __attribute__((packed))
    {
	ULONG				OffsetFirstEntry;
	ULONG				IndexEntriesUsed;
	ULONG				IndexEntriesAllocated;
	UCHAR				Flags;				
	UCHAR				Padding[3];
    }	IDX_HEADER;

/* Estructura INDEX_ROOT (TypeCode = 0x90) */
typedef struct __attribute__((packed))
    {
	IDX_ROOT			Root;
	IDX_HEADER			Header;				// IDX_HEADER.Flags: 0=small, fits in IDX_ROOT. 1=large, uses $INDEX_ALLOCATION
    }	INDEX_ROOT;

/* Estructura INDEX_ROOT (TypeCode = 0xA0) (Prefijo - Encabeza todo DARA_RUN del índice) */
typedef struct __attribute__((packed))
    {
	MULTI_SECTOR_HEADER   		MultiSectorHeader;
	ULONGLONG			LogFileSequenceNumber;
	VCN				IndexVCN;
	IDX_HEADER			Header;				// IDX_HEADER.Flags: 0=leaf, 1=has children
	UPDATE_SEQUENCE_ARRAY		UpdateSequenceArray;
    }	FILE_RECORD_INDEX_HEADER;

/* Estructura INDEX_ROOT (TypeCode = 0xA0) (Registros de índice) */
typedef struct __attribute__((packed))
    {
	FILE_REFERENCE			FileReference;
	USHORT				RecordLength;			// Si Flags&1 los últimos 8 bytes tienen un VCN con el puntero
	USHORT				StreamLength;
	UCHAR				Flags;				// 0x00=Leaf node, 0x01=Pointer to SubNode, 0x02=Last Index Entry in node
	UCHAR				Padding[3];
	UCHAR 				Data[1];			// StreamLength bytes
    }	INDEX_RECORD;


/* Puntero a función usado por el enumerador de entrdas de índice */
class TDriverNTFS;
typedef	int				(TDriverNTFS::* TpColectoraDatosIndice)(INDEX_RECORD *IndexRecord, void *pParametroUsuario);

/* Data Runs tal como los guardo en memoria */
typedef struct
    {
	LCN		Inicio;
	unsigned	Cantidad;
    }	TDataRun;

/* Estructura usada para devolver bloques de memoria alocados */
typedef struct
    {
	__u64		NumBytes;
	void		*pDatos;
    }	TDatosAlocados;


/********************************
 *				*
 *	Clase TAutoLiberar	*
 *				*
 ********************************/
template <typename T> class TAutoLiberar
{
public:
					TAutoLiberar() 			{pGuardado=NULL;};
					~TAutoLiberar() 		{if (pGuardado) free((void *)pGuardado);};
	T				*AutoliberarPuntero(void *p) 	{pGuardado=(T *)(p); return(pGuardado);};
	void				LiberarAhora() 			{if (pGuardado) free((void *)pGuardado); pGuardado=NULL;};
protected:
	T				*pGuardado;
};


/********************************
 *				*
 *	 Clase TDriverNTFS	*
 *				*
 ********************************/
class TDriverNTFS : public TDriverBase
{
public:
					TDriverNTFS(const unsigned char *DiskData, unsigned LongitudDiskData);
	virtual				~TDriverNTFS();

	/* Funciones a implementar por el alumno */
	virtual int			LevantarDatosSuperbloque();
	virtual int 			ListarDirectorio(const char *Path, std::vector<TEntradaDirectorio> &Entradas);
	virtual int 			LeerArchivo(const char *Path, unsigned char *&Data, unsigned &DataLen);
	
};

#endif
