#ifndef	__DRIVER_EXT__H__
#define	__DRIVER_EXT__H__

/************************
 *			*
 *     Constantes	*
 *			*
 ************************/
/* Posibles códigos de error */
#define	CODERROR_FEATURE_DESCONOCIDO	(CODERROR_ALUMNO	- 101)

/* Valor mínimo */
#define	min(a, b)	(((a)<(b))?a:b)

/* Features de filesystems EXT */
#define EXT4_FEATURE_COMPAT_DIR_PREALLOC	0x0001
#define EXT4_FEATURE_COMPAT_IMAGIC_INODES	0x0002
#define EXT3_FEATURE_COMPAT_HAS_JOURNAL		0x0004
#define EXT2_FEATURE_COMPAT_EXT_ATTR		0x0008
#define EXT3_FEATURE_COMPAT_RESIZE_INODE	0x0010
#define EXT3_FEATURE_COMPAT_DIR_INDEX		0x0020
#define EXT4_FEATURE_COMPAT_SPARSE_SUPER2	0x0200

/* Incompat features de filesystems EXT (Si no conocemos un bit de este campo no se puede montar el FS) */
#define EXT4_FEATURE_INCOMPAT_COMPRESSION	0x0001
#define EXT2_FEATURE_INCOMPAT_FILETYPE		0x0002
#define EXT3_FEATURE_INCOMPAT_RECOVER		0x0004
#define EXT3_FEATURE_INCOMPAT_JOURNAL_DEV	0x0008
#define EXT2_FEATURE_INCOMPAT_META_BG		0x0010
#define EXT4_FEATURE_INCOMPAT_EXTENTS		0x0040
#define EXT4_FEATURE_INCOMPAT_64BIT		0x0080
#define EXT4_FEATURE_INCOMPAT_MMP		0x0100
#define EXT4_FEATURE_INCOMPAT_FLEX_BG		0x0200
#define EXT4_FEATURE_INCOMPAT_EA_INODE		0x0400
#define EXT4_FEATURE_INCOMPAT_DIRDATA		0x1000
#define EXT4_FEATURE_INCOMPAT_BG_USE_META_CSUM	0x2000
#define EXT4_FEATURE_INCOMPAT_LARGEDIR		0x4000
#define EXT4_FEATURE_INCOMPAT_INLINE_DATA	0x8000

/* Read-Only features de filesystems EXT  (Si no conocemos un bit de este campo se puede montar el FS sólo como RO) */
#define EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER	0x0001
#define EXT2_FEATURE_RO_COMPAT_LARGE_FILE	0x0002
#define EXT2_FEATURE_RO_COMPAT_BTREE_DIR	0x0004
#define EXT4_FEATURE_RO_COMPAT_HUGE_FILE	0x0008
#define EXT4_FEATURE_RO_COMPAT_GDT_CSUM		0x0010
#define EXT4_FEATURE_RO_COMPAT_DIR_NLINK	0x0020
#define EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE	0x0040
#define EXT4_FEATURE_RO_COMPAT_QUOTA		0x0100
#define EXT4_FEATURE_RO_COMPAT_BIGALLOC		0x0200
#define EXT4_FEATURE_RO_COMPAT_METADATA_CSUM	0x0400

/* Números de Index Nodes reservados (sacados de ext4.h) */
#define	EXT_BAD_INO		 1	/* Bad blocks inode */
#define EXT_ROOT_INO		 2	/* Root inode */
#define EXT_USR_QUOTA_INO	 3	/* User quota inode */
#define EXT_GRP_QUOTA_INO	 4	/* Group quota inode */
#define EXT_BOOT_LOADER_INO	 5	/* Boot loader inode */
#define EXT_UNDEL_DIR_INO	 6	/* Undelete directory inode */
#define EXT_RESIZE_INO		 7	/* Reserved group descriptors inode */
#define EXT_JOURNAL_INO		 8	/* Journal inode */

/* Macros para determinar qué representa una entrada en un INode basado en el campo i_mode (sacados de stat.h) */
#define 	S_IFMT   	0xF000
#define 	S_IFSOCK   	0xC000
#define 	S_IFLNK   	0xA000
#define 	S_IFREG   	0x8000
#define 	S_IFBLK   	0x6000
#define 	S_IFDIR   	0x4000
#define 	S_IFCHR   	0x2000
#define 	S_IFIFO   	0x1000
#define 	S_ISLNK(m)   	(((m) & S_IFMT) == S_IFLNK)
#define 	S_ISREG(m)   	(((m) & S_IFMT) == S_IFREG)
#define 	S_ISDIR(m)   	(((m) & S_IFMT) == S_IFDIR)
#define 	S_ISCHR(m)   	(((m) & S_IFMT) == S_IFCHR)
#define 	S_ISBLK(m)   	(((m) & S_IFMT) == S_IFBLK)
#define 	S_ISFIFO(m)   	(((m) & S_IFMT) == S_IFIFO)
#define 	S_ISSOCK(m)   	(((m) & S_IFMT) == S_IFSOCK)

/* Bots para determinar las propiedades del INode basado en el campo i_flags (sacados de stat.h) */
#define EXT3_INDEX_FL			0x00001000
#define EXT4_EXTENTS_FL			0x00080000


/************************
 *			*
 *     Estructuras	*
 *			*
 ************************/

/* Descriptor de grupos para EXT2 y EXT3 */
typedef	struct __attribute__((packed))
    {
	__le32		block_bitmap;
	__le32		inode_bitmap;
	__le32		inode_table;
	__le16		free_blocks_count;
	__le16		free_inodes_count;
	__le16		used_dirs_count;
	__u8		bg_reserved[14];
    }	TEntradaDescGrupoEXT23;

/* Descriptor de grupos para EXT4 (sacada de ext4.h) */
typedef struct __attribute__((packed))
    {
	__le32	bg_block_bitmap_lo;			/* Blocks bitmap block */
	__le32	bg_inode_bitmap_lo;			/* Inodes bitmap block */
	__le32	bg_inode_table_lo;			/* Inodes table block */
	__le16	bg_free_blocks_count_lo;		/* Free blocks count */
	__le16	bg_free_inodes_count_lo;		/* Free inodes count */
	__le16	bg_used_dirs_count_lo;			/* Directories count */
	__le16	bg_flags;				/* EXT4_BG_flags (INODE_UNINIT, etc) */
	__le32  bg_exclude_bitmap_lo;   		/* Exclude bitmap for snapshots */
	__le16  bg_block_bitmap_csum_lo;		/* crc32c(s_uuid+grp_num+bbitmap) LE */
	__le16  bg_inode_bitmap_csum_lo;		/* crc32c(s_uuid+grp_num+ibitmap) LE */
	__le16  bg_itable_unused_lo;			/* Unused inodes count */
	__le16  bg_checksum;				/* crc16(sb_uuid+group+desc) */
	__le32	bg_block_bitmap_hi;			/* Blocks bitmap block MSB */
	__le32	bg_inode_bitmap_hi;			/* Inodes bitmap block MSB */
	__le32	bg_inode_table_hi;			/* Inodes table block MSB */
	__le16	bg_free_blocks_count_hi;		/* Free blocks count MSB */
	__le16	bg_free_inodes_count_hi;		/* Free inodes count MSB */
	__le16	bg_used_dirs_count_hi;			/* Directories count MSB */
	__le16  bg_itable_unused_hi;    		/* Unused inodes count MSB */
	__le32  bg_exclude_bitmap_hi;   		/* Exclude bitmap block MSB */
	__le16  bg_block_bitmap_csum_hi;		/* crc32c(s_uuid+grp_num+bbitmap) BE */
	__le16  bg_inode_bitmap_csum_hi;		/* crc32c(s_uuid+grp_num+ibitmap) BE */
	__u32   bg_reserved;
    }	TEntradaDescGrupoEXT4;

/* Descriptor de INode (sacada de ext4.h) */
typedef struct __attribute__((packed))
    {
	__le16	i_mode;					/* File mode */
	__le16	i_uid;					/* Low 16 bits of Owner Uid */
	__le32	i_size_lo;				/* Size in bytes */
	__le32	i_atime;				/* Access time */
	__le32	i_ctime;				/* Inode Change time */
	__le32	i_mtime;				/* Modification time */
	__le32	i_dtime;				/* Deletion Time */
	__le16	i_gid;					/* Low 16 bits of Group Id */
	__le16	i_links_count;				/* Links count */
	__le32	i_blocks_lo;				/* Blocks count */
	__le32	i_flags;				/* File flags */
	union
	    {
		struct
		    {
			__le32  l_i_version;
		    } linux1;
		struct
		    {
			__u32  h_i_translator;
		    } hurd1;
		struct
		    {
			__u32  m_i_reserved1;
		    } masix1;
	    } osd1;					/* OS dependent 1 */
	__le32	i_block[15];				/* Pointers to blocks */
	__le32	i_generation;				/* File version (for NFS) */
	__le32	i_file_acl_lo;				/* File ACL */
	__le32	i_size_high;
	__le32	i_obso_faddr;				/* Obsoleted fragment address */
	union
	    {
		struct
		    {
			__le16	l_i_blocks_high; 	/* were l_i_reserved1 */
			__le16	l_i_file_acl_high;
			__le16	l_i_uid_high;		/* these 2 fields */
			__le16	l_i_gid_high;		/* were reserved2[0] */
			__le16	l_i_checksum_lo;	/* crc32c(uuid+inum+inode) LE */
			__le16	l_i_reserved;
		    } linux2;
		struct
		    {
			__le16	h_i_reserved1;		/* Obsoleted fragment number/size which are removed in ext4 */
			__u16	h_i_mode_high;
			__u16	h_i_uid_high;
			__u16	h_i_gid_high;
			__u32	h_i_author;
		    } hurd2;
		struct
		    {
			__le16	h_i_reserved1;		/* Obsoleted fragment number/size which are removed in ext4 */
			__le16	m_i_file_acl_high;
			__u32	m_i_reserved2[2];
		    } masix2;
	    } osd2;					/* OS dependent 2 */
	__le16	i_extra_isize;
	__le16	i_checksum_hi;				/* crc32c(uuid+inum+inode) BE */
	__le32  i_ctime_extra;  			/* extra Change time      (nsec << 2 | epoch) */
	__le32  i_mtime_extra;  			/* extra Modification time(nsec << 2 | epoch) */
	__le32  i_atime_extra;  			/* extra Access time      (nsec << 2 | epoch) */
	__le32  i_crtime;       			/* File Creation time */
	__le32  i_crtime_extra; 			/* extra FileCreationtime (nsec << 2 | epoch) */
	__le32  i_version_hi;				/* high 32 bits for 64-bit version */
    }	TINodeEXT;


/* Descriptor de entrada en un directorio (sacado de ext4.h) */
typedef struct __attribute__((packed))
    {
	__le32		inode;		/* Inode number */
	__le16		rec_len;	/* Directory entry length */
	__u8		name_len;	/* Name length */
	__u8		file_type;
	char		name[];		/* File name */
    }	TDirEntryEXT;

/* Base del árbo BTree para manejo de extents (sacado de internet) */
typedef struct __attribute__((packed))
    {
	__u16		eh_magic;       /* probably will support different formats */
	__le16		eh_entries;     /* number of valid entries */
	__le16		eh_max;         /* capacity of store in entries */
	__le16		eh_depth;       /* has tree real underlying blocks? */
	__le32		eh_generation;	/* generation of the tree */
    }	TExtentHeaderEXT4;

typedef struct __attribute__((packed))
    {
	__le32		ee_block;	/* first logical block extent covers */
	__le16		ee_len;		/* number of blocks covered by extent */
	__le16		ee_start_hi;	/* high 16 bits of physical block */
	__le32		ee_start_lo;	/* low 32 bits of physical block */
    }	TExtentNodeEXT4;

typedef struct __attribute__((packed))
    {
	__le32		ei_block;       /* index covers logical blocks from 'block' */
	__le32		ei_leaf_lo;     /* pointer to the physical block of the next level. leaf or next index could be there */
	__le16		ei_leaf_hi;     /* high 16 bits of physical block */
	__le16		ei_unused;
    }	TExtentIndexEXT4;



/********************************
 *				*
 *	 Clase TDriverEXT	*
 *				*
 ********************************/
class TDriverEXT : public TDriverBase
{
public:
					TDriverEXT(const unsigned char *DiskData, unsigned LongitudDiskData);
	virtual				~TDriverEXT();

	/* Funciones a implementar por el alumno */
	virtual int			LevantarDatosSuperbloque();
	virtual int 			ListarDirectorio(const char *Path, std::vector<TEntradaDirectorio> &Entradas);
	virtual int 			LeerArchivo(const char *Path, unsigned char *&Data, unsigned &DataLen);

};

#endif
