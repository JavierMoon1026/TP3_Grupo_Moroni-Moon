#include "all_heads.h"

int main(int argc, char *argv[])
{
	int		CodError;
	TAnalizadorFS	AnalizadorFS("./config/tpfs.conf");

	/* Analizar los parámetros */
	if (argc != 2) {
		if (argc < 2)
			fprintf(stderr, "Error: falta la imagen de disco.\n");
		else
			fprintf(stderr, "Error: demasiados argumentos.\n");
		fprintf(stderr, "Uso: %s <imagen>\n", argv[0]);
		fprintf(stderr, "  imagen  Ruta al archivo binario de disco a analizar (FAT12/16/32, EXT2/3/4, NTFS)\n");
		return(CODERROR_PARAMETROS_INVALIDOS);
	}

	/* Ejeuctar la clase que busca el driver adecuado y luego analiza la imágen */
	CodError=AnalizadorFS.Ejecutar(argv[1]);

	/* Imprimir un mensaje final */
	static const std::unordered_map<int, const char*> MensajesError = {
		{ CODERROR_NINGUNO,                  "OK"                        		},
		{ CODERROR_PARAMETROS_INVALIDOS,     "Parámetros inválidos"      		},
		{ CODERROR_ARCHIVO_INEXISTENTE,      "Archivo a cargar no encontrado"   },
		{ CODERROR_ARCHIVO_INVALIDO,         "Archivo inválido"          		},
		{ CODERROR_FALTA_MEMORIA,            "Falta memoria"             		},
		{ CODERROR_LECTURA_DISCO,            "Error de lectura de disco" 		},
		{ CODERROR_FALTA_ARCHIVO_DE_COMANDOS,"Falta archivo de comandos" 		},
		{ CODERROR_COMANDO_DESCONOCIDO,      "Comando desconocido"       		},
		{ CODERROR_COMANDO_CON_ERRORES,      "Comando con errores"       		},
		{ CODERROR_NO_IMPLEMENTADO,          "No implementado"           		},
		{ CODERROR_SUPERBLOQUE_INVALIDO,     "Superbloque inválido"      		},
		{ CODERROR_FILESYSTEM_DESCONOCIDO,   "Filesystem desconocido"    		},
		{ CODERROR_FILESYSTEM_CORRUPTO,      "Filesystem corrupto"       		},
		{ CODERROR_DIRECTORIO_INEXISTENTE,   "Directorio inexistente"    		},
		{ CODERROR_RUTA_NO_ABSOLUTA,         "Ruta no absoluta"          		},
		{ CODERROR_ALUMNO,                   "Error del alumno"          		},
	};
	auto it = MensajesError.find(CodError);
	const char *MensajeError = (it != MensajesError.end()) ? it->second : "Error desconocido";
	printf("El programa termina con resultado: %s (Codigo de error: %d).\r\n", MensajeError, CodError);

	/* Salir */
	return(CodError);
}
