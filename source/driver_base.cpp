#include "all_heads.h"


/********************************
 *				*
 *      Clase TDriverBase	*
 *				*
 ********************************/
/****************************************************************************************************************************************
 *																	*
 *						      TDriverBase :: TDriverBase							*
 *																	*
 * OBJETIVO: Inicializar la clase recién creada.											*
 *																	*
 * ENTRADA: DiskData: Puntero a un bloque de memoria con la imágen del disco a analizar.						*
 *	    LongitudDiskData: Tamaño, en bytes, de la imágen a analizar.								*
 *																	*
 * SALIDA: Nada.															*
 *																	*
 ****************************************************************************************************************************************/
TDriverBase::TDriverBase(const unsigned char *DiskData, unsigned LongitudDiskData)
{
	/* Tomar los valores recibidos */
	TDriverBase::DiskData=DiskData;
	TDriverBase::LongitudDiskData=LongitudDiskData;

	DatosFS = {};
}


/****************************************************************************************************************************************
 *																	*
 *						     TDriverBase :: ~TDriverBase							*
 *																	*
 * OBJETIVO: Liberar recursos alocados.													*
 *																	*
 * ENTRADA: Nada.															*
 *																	*
 * SALIDA: Nada.															*
 *																	*
 ****************************************************************************************************************************************/
TDriverBase::~TDriverBase()
{
}


/****************************************************************************************************************************************
 *																	*
 *						   TDriverBase :: PunteroASector							*
 *																	*
 * OBJETIVO: Esta función devuelve un puntero al sector pedido.										*
 *																	*
 * ENTRADA: NroSector: Número de sector (el primero es el sector es el 0).								*
 *																	*
 * SALIDA: En el nombre de la función el puntero a los datos del sector.								*
 *																	*
 * OBSERVACIONES: IMPORTANTE: Esta función sólo puede usarse para acceder al sector 0 hasta tanto se inicialice la variable 		*
 *			      DatosFS.BytesPorSector.											*
 *																	*
 ****************************************************************************************************************************************/
const unsigned char *TDriverBase::PunteroASector(__u64 NroSector)
{
	/* Ver si tengo imágen cargada */
	if (!DiskData) return NULL ;

	/* Ver si el sector existe */
	if (((NroSector + 1) * DatosFS.BytesPorSector) >= LongitudDiskData ) return NULL;
		
	/* Retornar el puntero solicitado */
	return(DiskData + NroSector * DatosFS.BytesPorSector);
}


/****************************************************************************************************************************************
 *																	*
 *						    TDriverBase :: MostrarDatosSuperbloque						*
 *																	*
 * OBJETIVO: Esta función muestra los datos del superbloque.										*
 *																	*
 * ENTRADA: Nada.															*
 *																	*
 * SALIDA: En el nombre de la función CODERROR_NINGUNO si no hubo errores, caso contrario el código de error.				*
 *																	*
 * OBSERVACIONES: Si llega acá LevantarDatosSuperbloque() retornó sin errores, por lo que el TipoFilesystem es uno de los tipos 	*
 *		  soportados.														*
 *																	*
 ****************************************************************************************************************************************/
int TDriverBase::MostrarDatosSuperbloque(void)
{
	printf("Datos del superbloque:\n");

	/* Nombre del filesystem */
	static const std::unordered_map<TipoFilsystem, const char*> NombresFS = {
		{ tfsFAT12, "FAT12" }, { tfsFAT16, "FAT16" }, { tfsFAT32, "FAT32" },
		{ tfsEXT2,  "EXT2"  }, { tfsEXT3,  "EXT3"  }, { tfsEXT4,  "EXT4"  },
		{ tfsNTFS,  "NTFS"  },
	};
	auto itFS = NombresFS.find(DatosFS.TipoFilesystem);
	const char *NombreFS = (itFS != NombresFS.end()) ? itFS->second : "Desconocido";
	printf("\tFormato                 : %s\n", NombreFS);

	/* Valores comunes */
	printf("\tBytes/Sector            : %d\n", DatosFS.BytesPorSector);
	printf("\tBytes/Cluster           : %d\n", DatosFS.BytesPorCluster);
	printf("\tNro Clusters            : %d\n", DatosFS.NumeroDeClusters);

	/* Valores propios de cada filesystem */
	switch (DatosFS.TipoFilesystem) {
		case tfsFAT12:
		case tfsFAT16:
		case tfsFAT32:  MostrarDatosSuperbloqueFAT();  break;
		case tfsEXT2:
		case tfsEXT3:
		case tfsEXT4:   MostrarDatosSuperbloqueEXT();  break;
		case tfsNTFS:   MostrarDatosSuperbloqueNTFS(); break;
		default:        break;
	}

	return(CODERROR_NINGUNO);
}


void TDriverBase::MostrarDatosSuperbloqueFAT(void)
{
	auto &fat = DatosFS.DatosEspecificos.FAT;
	printf("\tNro Sectores            : %d\n", fat.TotalSectores);
	printf("\tNro Sectores Ocultos    : %d\n", fat.SectoresOcultos);
	printf("\tNro Sectores Reservados : %d\n", fat.SectoresReservados);
	printf("\tNro copias FAT          : %d\n", fat.CopiasFAT);
	printf("\tNro entradas RootDir    : %d\n", fat.EntradasRootDir);
	printf("\tSectores/Cluster        : %d\n", fat.SectoresPorCluster);
	printf("\tSectores/FAT            : %d\n", fat.SectoresPorFAT);
	printf("\tNro Clusters RootDir    : %d\n", fat.ClustersRootDir);
	printf("\t1er Cluster RootDir     : %d\n", fat.PrimerClusterRootDir);
}


void TDriverBase::MostrarDatosSuperbloqueEXT(void)
{
	auto &ext = DatosFS.DatosEspecificos.EXT;
	printf("\tCaract. Compatibles     : %04X\n", ext.CaracteristicasCompatibles);
	printf("\tCaract. Incompatibles   : %04X\n", ext.CaracteristicasIncompatibles);
	printf("\tCaract. Sólo Lectura    : %04X\n", ext.CaracteristicasSoloLectura);
	printf("\tClusters/Grupo          : %d\n",   ext.ClustersPorGrupo);
	printf("\tINodes/Grupo            : %d\n",   ext.INodesPorGrupo);
	printf("\tBytes/INode             : %d\n",   ext.BytesPorINode);
	printf("\tNro INodes              : %d\n",   ext.NumeroDeINodes);
	printf("\tNro Grupos              : %d\n",   ext.NroGrupos);
	printf("\tPeríodo Agrupado Flex   : %d\n",   ext.PeriodoAgrupadoFlex);
	printf("\tNro Clust. reserv. GDT  : %d\n",   ext.ClustersReservadosGDT);

	auto ImprimirGrupos = [&](const char *Label, auto Selector) {
		printf("\t%-24s: ", Label);
		for (int i = 0; i < ext.NroGrupos; i++) {
			if (i > 0) printf(", ");
			printf("%llu", Selector(ext.DatosGrupo[i]));
		}
		printf("\n");
	};
	ImprimirGrupos("Cluster Bitmap INodes",  [](auto &g){ return g.ClusterBitmapINodes;  });
	ImprimirGrupos("Cluster Tabla INodes",   [](auto &g){ return g.ClusterTablaINodes;   });
	ImprimirGrupos("Cluster Bitmap Bloques", [](auto &g){ return g.ClusterBitmapBloques; });
	ImprimirGrupos("Cluster Tabla Bloques",  [](auto &g){ return g.ClusterTablaBloques;  });
}


void TDriverBase::MostrarDatosSuperbloqueNTFS(void)
{
	auto &ntfs = DatosFS.DatosEspecificos.NTFS;
	printf("\tOffset Part en Sectores : %d\n",   ntfs.OffsetParticionEnSectores);
	printf("\tNro Sectores            : %lld\n", ntfs.TotalSectores);
	printf("\tSectores/Cluster        : %d\n",   ntfs.SectoresPorCluster);
	printf("\tBytes/FileRecordSegment : %d\n",   ntfs.BytesPorFileRecordSegment);
	printf("\tBytes/IndexBuffer       : %d\n",   ntfs.BytesPorIndexBuffer);
	printf("\tNro Cluster $MFT        : %llu\n", ntfs.ClusterMFT);
	printf("\tNro Cluster $MFT Mirror : %llu\n", ntfs.ClusterMFTMirror);
}


/****************************************************************************************************************************************
 *																	*
 *						     TDriverBase :: MostrarDatosDirectorio						*
 *																	*
 * OBJETIVO: Esta función muestra en contenido de un directorio.									*
 *																	*
 * ENTRADA: Entradas: Arreglo de estructuras con las entradas (archivos/directorios/etc) a mostrar.					*
 *																	*
 * SALIDA: En el nombre de la función CODERROR_NINGUNO si no hubo errores, caso contrario el código de error.				*
 *																	*
 ****************************************************************************************************************************************/
int TDriverBase::MostrarDatosDirectorio(std::vector<TEntradaDirectorio> &Entradas)
{
	/* Encabezado — columna FS-específica varía según el filesystem */
	printf(" Fecha Creación   Fecha Ult Acceso  Fecha Ult Modif                                Nombre                                 Flags     Tamaño  ");
	switch (DatosFS.TipoFilesystem) {
		case tfsFAT12: case tfsFAT16: case tfsFAT32: printf("   1º Clu  ");          break;
		case tfsEXT2:  case tfsEXT3:  case tfsEXT4:  printf("    INode   ");         break;
		case tfsNTFS:                                printf("   Índice MFT     Sec "); break;
		default: break;
	}
	printf("\n");
	printf("----------------- ----------------- ----------------- ---------------------------------------------------------------- ----------- ----------");
	switch (DatosFS.TipoFilesystem) {
		case tfsFAT12: case tfsFAT16: case tfsFAT32: printf(" ----------");           break;
		case tfsEXT2:  case tfsEXT3:  case tfsEXT4:  printf(" -----------");          break;
		case tfsNTFS:                                printf(" --------------- ----"); break;
		default: break;
	}
	printf("\n");

	/* Lambda para imprimir una fecha o espacios en blanco si no está disponible */
	auto ImprimirFecha = [](time_t t) {
		if (!t) {
			printf("                  ");
			return;
		}
		tm *lt = localtime(&t);
		printf("%02d/%02d/%04d %02d:%02d  ",
		       lt->tm_mday, 1 + lt->tm_mon, 1900 + lt->tm_year,
		       lt->tm_hour, lt->tm_min);
	};

	/* Tabla de flags: bit → carácter a mostrar */
	static const std::pair<unsigned, char> FlagDefs[] = {
		{ fedSOLO_LECTURA,     'R' }, { fedOCULTO,           'H' },
		{ fedSISTEMA,          'S' }, { fedETIQUETA_VOLUMEN, 'V' },
		{ fedDIRECTORIO,       'D' }, { fedARCHIVAR,         'A' },
		{ fedACCESO_DIRECTO,   'L' }, { fedCOMPRIMIDO,       'C' },
		{ fedENCRIPTADO,       'E' }, { fedDISPERSO,         'P' },
	};

	for (const auto &Entrada : Entradas) {
		ImprimirFecha(Entrada.FechaCreacion);
		ImprimirFecha(Entrada.FechaUltimoAcceso);
		ImprimirFecha(Entrada.FechaUltimaModificacion);

		printf("%64.64s", Entrada.Nombre.c_str());

		std::string Flags;
		for (auto &[Bit, Char] : FlagDefs) {
			char c = (Entrada.Flags & Bit) ? Char : ' ';
			Flags += c;
		}
		printf(" %s ", Flags.c_str());

		printf("%10llu", Entrada.Bytes);

		switch (DatosFS.TipoFilesystem) {
			case tfsFAT12: case tfsFAT16: case tfsFAT32:
				printf(" %10u",  Entrada.DatosEspecificos.FAT.PrimerCluster);
				break;
			case tfsEXT2: case tfsEXT3: case tfsEXT4:
				printf(" %11u",  Entrada.DatosEspecificos.EXT.INode);
				break;
			case tfsNTFS:
				printf(" %15llu %04X", Entrada.DatosEspecificos.NTFS.IndiceMFT,
				                      Entrada.DatosEspecificos.NTFS.NroSecuencia);
				break;
			default: break;
		}

		printf("\n");
	}

	return(CODERROR_NINGUNO);
}


/****************************************************************************************************************************************
 *																	*
 *						     TDriverBase :: MostrarDatosDirectorio						*
 *																	*
 *  OBJETIVO: Esta función imprime en contenido de un buffer binario en pantalla.							*
 *																	*
 *  ENTRADA: Buffer: Puntero al bloque binario.												*
 *	     BufferLen: Longitud del bloque a imprimir.											*
 *																	*
 *  SALIDA: Nada.															*
 *																	*
 ****************************************************************************************************************************************/
void TDriverBase::PrintBuffer(const unsigned char *Buffer, unsigned BufferLen, unsigned BytesPorLinea)
{
	for (unsigned i = 0; i < BufferLen; i += BytesPorLinea) {
		printf("    %08x    ", i);

		for (unsigned j = i; j < i + BytesPorLinea; j++) {
			if (j < BufferLen)  printf("%02X ", Buffer[j]);
			else                printf("   ");
		}

		for (unsigned j = i; j < i + BytesPorLinea; j++) {
			if (j >= BufferLen) {
				printf(" ");
			} else {
				char c = (Buffer[j] >= ' ') ? Buffer[j] : '.';
				printf("%c", c);
			}
		}

		printf("\n");
	}
}









