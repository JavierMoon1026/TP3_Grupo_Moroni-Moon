#ifndef	__DRIVER_FAT__H__
#define	__DRIVER_FAT__H__

/************************
 *			*
 *     Constantes	*
 *			*
 ************************/
/* Flags de cada entrada de directorio */
#define FAT_READ_ONLY	0x01
#define FAT_HIDDEN	0x02
#define FAT_SYSTEM	0x04
#define FAT_VOLUME_ID	0x08
#define FAT_DIRECTORY	0x10
#define FAT_ARCHIVE	0x20 
#define FAT_LFN		(FAT_READ_ONLY|FAT_HIDDEN|FAT_SYSTEM|FAT_VOLUME_ID)


/************************
 *			*
 *     Estructuras	*
 *			*
 ************************/

/* Entrada de directorio (sacado de Wikipedia) */
typedef	struct __attribute__((packed))
    {
	char		Name[8];
	char		Ext[3];
	__u8		FileAttributes;
	__u8		Reserverd;
	__u8		CerationTimeMS;
	__u16		CreationTime;
	__u16		CreationDate;
	__u16		LastAccessDate;
	__u16		StartClusterH;
	__u16		ModificationTime;
	__u16		ModificationDate;
	__u16		StartClusterL;
	__le32		FileSize;
    }	TDirEntryFAT;


/********************************
 *				*
 *	 Clase TDriverFAT	*
 *				*
 ********************************/
class TDriverFAT : public TDriverBase
{
public:
					TDriverFAT(const unsigned char *DiskData, unsigned LongitudDiskData);
	virtual				~TDriverFAT();

	/* Funciones a implementar por el alumno */
	virtual int			LevantarDatosSuperbloque();
	virtual int 			ListarDirectorio(const char *Path, std::vector<TEntradaDirectorio> &Entradas);
	virtual int 			LeerArchivo(const char *Path, unsigned char *&Data, unsigned &DataLen);
};

#endif
