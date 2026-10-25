#include "all_heads.h"

/********************************
 *				*
 *      Clase TAnalizadorFS	*
 *				*
 ********************************/
/****************************************************************************************************************************************
 *																	*
 *						    TAnalizadorFS :: TAnalizadorFS							*
 *																	*
 * OBJETIVO: Inicializar la clase recién creada.											*
 *																	*
 * ENTRADA: Nada.															*
 *																	*
 * SALIDA: Nada.															*
 *																	*
 ****************************************************************************************************************************************/
TAnalizadorFS::TAnalizadorFS(const std::string &ConfigFile) : ConfigFile(ConfigFile)
{
	struct winsize WinSize = {};

	/* Inicialziar variables */
	LongitudDiskData=0;
	DiskData=NULL;
	DriverFS=NULL;

	/* Levantar el ancho de la pantalla */
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &WinSize);

	/* Determinar el ancho de las impresiones de buffers en base al ancho de la pantalla */
	if (WinSize.ws_col<80)
		PrintWidth=8;
	else if (WinSize.ws_col<144)
		PrintWidth=16;
	else if (WinSize.ws_col<272)
		PrintWidth=32;
	else
		PrintWidth=64;
}


/****************************************************************************************************************************************
 *																	*
 *						    TAnalizadorFS :: ~TAnalizadorFS							*
 *																	*
 * OBJETIVO: Liberar recursos alocados.													*
 *																	*
 * ENTRADA: Nada.															*
 *																	*
 * SALIDA: Nada.															*
 *																	*
 ****************************************************************************************************************************************/
TAnalizadorFS::~TAnalizadorFS()
{
	/* Liberar el driver del filesystem si está cargado */
	if (DriverFS) {
		delete(DriverFS);
		DriverFS=NULL;
	}

	/* Liberar todos los recursos alocados */
	BorrarTodoYReinicializar();
}


/****************************************************************************************************************************************
 *																	*
 *						      TAnalizadorFS :: Ejecutar								*
 *																	*
 * OBJETIVO: Esta función ejecuta todos los tests e imprime los resultados.								*
 *																	*
 * ENTRADA: Ruta: Ruta al archivo binario a cargar.											*
 *																	*
 * SALIDA: Nada.															*
 *																	*
 ****************************************************************************************************************************************/
int TAnalizadorFS::CargarConfig()
{
	std::ifstream Archivo(ConfigFile);
	if (!Archivo)
		return(CODERROR_FALTA_ARCHIVO_DE_COMANDOS);

	std::string Linea;
	while (std::getline(Archivo, Linea)) {
		if (!Linea.empty() && Linea.back() == '\r') Linea.pop_back();
		if (Linea.empty() || Linea[0] == '#') continue;
		auto Pos = Linea.find('=');
		if (Pos == std::string::npos) continue;
		Config[Linea.substr(0, Pos)] = Linea.substr(Pos + 1);
	}

	return(CODERROR_NINGUNO);
}


int TAnalizadorFS::Ejecutar(const char *Ruta)
{
	int CodError;

	/* Cargar configuración */
	printf("Cargando configuración ...\n");
	CodError = CargarConfig();
	if (CodError != CODERROR_NINGUNO) return(CodError);

	/* Cargar la imágen de disco */
	printf("Cargando imágen de disco ...\n");
	CodError = CargarImagen(Ruta);
	if (CodError != CODERROR_NINGUNO) return(CodError);

	/* Probar cada driver hasta encontrar uno que reconozca la imágen */
	auto ImagenCompatible = [](int cod) {
		return cod != CODERROR_SUPERBLOQUE_INVALIDO && cod != CODERROR_FILESYSTEM_DESCONOCIDO;
	};

	const std::function<TDriverBase*()> Drivers[] = {
		 [&]{ return (TDriverBase*)new TDriverFAT(DiskData, LongitudDiskData);  },
		[&]{ return (TDriverBase*)new TDriverEXT(DiskData, LongitudDiskData); },
		[&]{ return (TDriverBase*)new TDriverNTFS(DiskData, LongitudDiskData);},
	};

	printf("Analizando imágenes de FS...");
	for (const auto &MakeDriver : Drivers) {
		DriverFS = MakeDriver();
		CodError = DriverFS->LevantarDatosSuperbloque();
		if (ImagenCompatible(CodError))
			break;
		delete DriverFS;
		DriverFS = NULL;
	}

	if (DriverFS == NULL)
	{
		printf("ERROR: La imágen no es compatible con Fat, Ext o NTFS.\n");
		return(CodError);
	}

	printf("ÉXITO: Imágen válida.\n");

	/* Mostrar los datos del Filesystem */
	DriverFS->MostrarDatosSuperbloque();

	/* Ejecutar los tests */
	CodError=EjecutarTests();
	if (CodError != CODERROR_NINGUNO) return(CodError);

	/* Salir */
	return(CODERROR_NINGUNO);
}


/****************************************************************************************************************************************
 *																	*
 *						      TAnalizadorFS :: EjecutarTests							*
 *																	*
 * OBJETIVO: Esta función carga el archivo <nombre-ejecutable>_tests.txt y ejecuta cada uno de los comandos.				*
 *																	*
 * ENTRADA: Ruta: Ruta al archivo binario a cargar.											*
 *																	*
 * SALIDA: En el nombre de la función CODERROR_NINGUNO si no hubo errores, caso contrario el código de error.				*
 *																	*
 ****************************************************************************************************************************************/
int TAnalizadorFS::EjecutarTests()
{
	/* Obtener la ruta al archivo de tests desde la configuración */
	auto it = Config.find("tests_file");
	if (it == Config.end())
		return(CODERROR_FALTA_ARCHIVO_DE_COMANDOS);

	std::ifstream Archivo(it->second);
	if (!Archivo)
		return(CODERROR_FALTA_ARCHIVO_DE_COMANDOS);

	/* Tabla de comandos disponibles */
	const std::unordered_map<std::string, std::function<int(const char*)>> Comandos = {
		{ "ls", [&](const char *Arg) { return MostrarContenidoDirectorio(Arg); } },
		{ "cat", [&](const char *Arg) { return MostrarContenidoArchivo(Arg); } },
	};

	std::string Linea;
	int CodError = CODERROR_NINGUNO;

	while ((CodError == CODERROR_NINGUNO) && std::getline(Archivo, Linea)) {
		if (!Linea.empty() && Linea.back() == '\r') Linea.pop_back();
		if (Linea.empty() || Linea[0] == '#') continue;

		std::istringstream ss(Linea);
		std::string Cmd, ArgStr;
		ss >> Cmd >> ArgStr;

		std::transform(Cmd.begin(), Cmd.end(), Cmd.begin(), ::tolower);

		auto it = Comandos.find(Cmd);
		if (it == Comandos.end()) return(CODERROR_COMANDO_DESCONOCIDO);
		if (ArgStr.empty())       return(CODERROR_COMANDO_CON_ERRORES);

		CodError = it->second(ArgStr.c_str());
	}

	return(CODERROR_NINGUNO);
}


/****************************************************************************************************************************************
 *																	*
 *						      TAnalizadorFS :: CargarImagen							*
 *																	*
 * OBJETIVO: Esta función carga una nueva imágen de disco para analizar.								*
 *																	*
 * ENTRADA: Ruta: Ruta al archivo binario a cargar.											*
 *																	*
 * SALIDA: En el nombre de la función CODERROR_NINGUNO si no hubo errores, caso contrario el código de error.				*
 *																	*
 ****************************************************************************************************************************************/
int TAnalizadorFS::CargarImagen(const char *Ruta)
{
	BorrarTodoYReinicializar();

	std::ifstream Archivo(Ruta, std::ios::binary | std::ios::ate);
	if (!Archivo)
		return(CODERROR_ARCHIVO_INEXISTENTE);

	const unsigned Longitud = static_cast<unsigned>(Archivo.tellg());
	if (Longitud % 512 != 0)
		return(CODERROR_ARCHIVO_INVALIDO);

	auto *Buffer = new(std::nothrow) unsigned char[Longitud];
	if (!Buffer)
		return(CODERROR_FALTA_MEMORIA);

	Archivo.seekg(0);
	if (!Archivo.read(reinterpret_cast<char*>(Buffer), Longitud)) {
		delete[] Buffer;
		return(CODERROR_LECTURA_DISCO);
	}

	DiskData = Buffer;
	LongitudDiskData = Longitud;
	printf("Se cargaron %u bytes de %s sin errores.\n", LongitudDiskData, Ruta);
	return(CODERROR_NINGUNO);
}


/****************************************************************************************************************************************
 *																	*
 *						 TAnalizadorFS :: BorrarTodoYReinicializar						*
 *																	*
 * OBJETIVO: Reinicializar la clase, liberando cualquier recurso alocado, de forma que se pueda iniciar el proceso de otra imágen de	*
 *	     disco.															*
 *																	*
 * ENTRADA: Nada.															*
 *																	*
 * SALIDA: Nada.															*
 *																	*
 ****************************************************************************************************************************************/
void TAnalizadorFS::BorrarTodoYReinicializar(void)
{
	if (DiskData) {
		delete[] const_cast<unsigned char*>(DiskData);
		LongitudDiskData = 0;
		DiskData = NULL;
	}
}


/****************************************************************************************************************************************
 *																	*
 *					   TAnalizadorFS :: MostrarContenidoDirectorio							*
 *																	*
 * OBJETIVO: Esta función usa el driver cargado para listar el contenido de un determinado directorio.					*
 *																	*
 * ENTRADA: Path: Ruta al directorio cuyo contenido listar.										*
 *																	*
 * SALIDA: En el nombre de la función el código de error.										*
 *																	*
 ****************************************************************************************************************************************/
int TAnalizadorFS::MostrarContenidoDirectorio(const char *Path)
{
	printf("Leyendo directorio '%s' ...\n", Path);

	std::vector<TEntradaDirectorio> Entradas;
	int CodError = DriverFS->ListarDirectorio(Path, Entradas);

	if (CodError == CODERROR_DIRECTORIO_INEXISTENTE) {
		printf("\tError, el directorio NO EXISTE!\n");
		return(CODERROR_NINGUNO);
	}
	if (CodError != CODERROR_NINGUNO)
		return(CodError);

	DriverFS->MostrarDatosDirectorio(Entradas);
	return(CODERROR_NINGUNO);
}


/****************************************************************************************************************************************
 *																	*
 *					      TAnalizadorFS :: MostrarContenidoArchivo							*
 *																	*
 * OBJETIVO: Esta función usa el driver cargado para listar el contenido de un determinado directorio.					*
 *																	*
 * ENTRADA: Path: Ruta al archivo cuyo contenido listar.										*
 *																	*
 * SALIDA: En el nombre de la función el código de error.										*
 *																	*
 ****************************************************************************************************************************************/
int TAnalizadorFS::MostrarContenidoArchivo(const char *Path)
{
	printf("Leyendo archivo '%s' ...\n", Path);

	unsigned char *Data  = nullptr;
	unsigned       DataLen = 0;
	int CodError = DriverFS->LeerArchivo(Path, Data, DataLen);

	if (CodError == CODERROR_ARCHIVO_INEXISTENTE) {
		printf("\tError, el archivo NO EXISTE!\n");
		return(CODERROR_NINGUNO);
	}
	if (CodError != CODERROR_NINGUNO)
		return(CodError);

	printf("\tLeído, %u bytes\n", DataLen);
	DriverFS->PrintBuffer(Data, DataLen, PrintWidth);
	free(Data);
	return(CODERROR_NINGUNO);
}


