/* ---------------------------------------------------------------
Práctica 1.
Código fuente: CalcArbolesConcurrente.c
Grau Informàtica
78098453K - Marc Junyent Moreno
41624352X - Francesc Gaya Piña
--------------------------------------------------------------- */
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <ConvexHull.h>
#include <semaphore.h>

#define DMaxArboles 	25
#define DMaximoCoste 999999
#define S 10000
#define DDebug 0
#define DefaultThreads 3


  //////////////////////////
 // Estructuras de datos //
//////////////////////////


// Definicin estructura arbol entrada (Conjunto �boles).
struct  Arbol
{
	int	  IdArbol;
	Point Coord;			// Posicin �bol
	int Valor;				// Valor / Coste �bol.
	int Longitud;			// Cantidad madera �bol
};
typedef struct Arbol TArbol, *PtrArbol;



// Definicin estructura Bosque entrada (Conjunto �boles).
struct Bosque
{
	int 		NumArboles;
	TArbol 	Arboles[DMaxArboles];
};
typedef struct Bosque TBosque, *PtrBosque;



// Combinacin .
struct ListaArboles
{
	int 		NumArboles;
 	float		Coste;
	float		CosteArbolesCortados;
	float		CosteArbolesRestantes;
	float		LongitudCerca;
	float		MaderaSobrante;
	int 		Arboles[DMaxArboles];
};
typedef struct ListaArboles TListaArboles, *PtrListaArboles;

struct TRang //Estructura Rangos asignados a un thread
{
	long int inici;
	long int final;
	PtrListaArboles ArbresOpt;
};
typedef struct TRang TRang, *PtrRang;

// Vector est�ico Coordenadas.
typedef Point TVectorCoordenadas[DMaxArboles], *PtrVectorCoordenadas;


typedef enum {false, true} bool;


  ////////////////////////
 // Variables Globales //
////////////////////////

TBosque ArbolesEntrada;
TListaArboles OptimoParcial;
int min_cost = 99999;
int bestComb = 0;
/* Mutex */
pthread_mutex_t Mutex;
pthread_cond_t CondPartial;
pthread_barrier_t Barrera;
sem_t SemMutex;


  //////////////////////////
 // Prototipos funciones //
//////////////////////////

bool LeerFicheroEntrada(char *PathFicIn);
bool GenerarFicheroSalida(TListaArboles optimo, char *PathFicOut);
bool CalcularCercaOptima(PtrListaArboles Optimo, int argc, char *argv[]);
int cercaCostMinim(int d[], int *j);
void OrdenarArboles();
void CalcularCombinacionOptima(PtrRang Rangs);
int EvaluarCombinacionListaArboles(int Combinacion);
int ConvertirCombinacionToArboles(int Combinacion, PtrListaArboles CombinacionArboles);
int ConvertirCombinacionToArbolesTalados(int Combinacion, PtrListaArboles CombinacionArbolesTalados);
void ObtenerListaCoordenadasArboles(TListaArboles CombinacionArboles, TVectorCoordenadas Coordenadas);
float CalcularLongitudCerca(TVectorCoordenadas CoordenadasCerca, int SizeCerca);
float CalcularDistancia(int x1, int y1, int x2, int y2);
int CalcularMaderaArbolesTalados(TListaArboles CombinacionArboles);
int CalcularCosteCombinacion(TListaArboles CombinacionArboles);
void MostrarArboles(TListaArboles CombinacionArboles);




int main(int argc, char *argv[])
{
	TListaArboles Optimo;
	
	if (argc<3 || argc>4)
		printf("Error Argumentos. Usage: CalcArboles <Fichero_Entrada> <Max_threads> [<Fichero_Salida>]");

	if (!LeerFicheroEntrada(argv[1]))
	{
		printf("Error lectura fichero entrada.\n");
		exit(1);
	}

	if (!CalcularCercaOptima(&Optimo, argc, argv))
	{
		printf("Error CalcularCercaOptima.\n");
		exit(1);
	}

	if (argc==3)
	{
		if (!GenerarFicheroSalida(Optimo, "./Valla.res"))
		{
			printf("Error GenerarFicheroSalida.\n");
			exit(1);
		}
	}
	else
	{
		if (!GenerarFicheroSalida(Optimo, argv[3]))
		{
			printf("Error GenerarFicheroSalida.\n");
			exit(1);
		}
	}
	exit(0);
}



bool LeerFicheroEntrada(char *PathFicIn)
{
	FILE *FicIn;
	int a;

	FicIn=fopen(PathFicIn,"r");
	if (FicIn==NULL)
	{
		perror("Lectura Fichero entrada.");
		return false;
	}
	printf("Datos Entrada:\n");

	// Leemos el nmero de arboles del bosque de entrada.
	if (fscanf(FicIn, "%d", &(ArbolesEntrada.NumArboles))<1)
	{
		perror("Lectura arboles entrada");
		return false;
	}
	printf("\tÁrboles: %d.\n",ArbolesEntrada.NumArboles);

	// Leer atributos arboles.
	for(a=0;a<ArbolesEntrada.NumArboles;a++)
	{
		ArbolesEntrada.Arboles[a].IdArbol=a+1;
		// Leer x, y, Coste, Longitud.
		if (fscanf(FicIn, "%d %d %d %d",&(ArbolesEntrada.Arboles[a].Coord.x), &(ArbolesEntrada.Arboles[a].Coord.y), &(ArbolesEntrada.Arboles[a].Valor), &(ArbolesEntrada.Arboles[a].Longitud))<4)
		{
			perror("Lectura datos arbol.");
			return false;
		}
		printf("\tÁrbol %d-> (%d,%d) Coste:%d, Long:%d.\n",a+1,ArbolesEntrada.Arboles[a].Coord.x, ArbolesEntrada.Arboles[a].Coord.y, ArbolesEntrada.Arboles[a].Valor, ArbolesEntrada.Arboles[a].Longitud);
	}

	return true;
}




bool GenerarFicheroSalida(TListaArboles Optimo, char *PathFicOut)
{
	FILE *FicOut;
	int a;

	FicOut=fopen(PathFicOut,"w+");
	if (FicOut==NULL)
	{
		perror("Escritura fichero salida.");
		return false;
	}

	// Escribir arboles a talartalado.
		// Escribimos nmero de arboles a talar.
	if (fprintf(FicOut, "Cortar %d árbol/es: ", Optimo.NumArboles)<1)
	{
		perror("Escribir nmero de arboles a talar");
		return false;
	}

	for(a=0;a<Optimo.NumArboles;a++)
	{
		// Escribir nmero arbol.
		if (fprintf(FicOut, "%d ",ArbolesEntrada.Arboles[Optimo.Arboles[a]].IdArbol)<1)
		{
			perror("Escritura nmero �bol.");
			return false;
		}
	}

	// Escribimos coste arboles a talar.
	if (fprintf(FicOut, "\nMadera Sobrante: \t%4.2f (%4.2f)", Optimo.MaderaSobrante,  Optimo.LongitudCerca)<1)
	{
		perror("Escribir coste arboles a talar.");
		return false;
	}

	// Escribimos coste arboles a talar.
	if (fprintf(FicOut, "\nValor árboles cortados: \t%4.2f.", Optimo.CosteArbolesCortados)<1)
	{
		perror("Escribir coste arboles a talar.");
		return false;
	}

		// Escribimos coste arboles a talar.
	if (fprintf(FicOut, "\nValor árboles restantes: \t%4.2f\n", Optimo.CosteArbolesRestantes)<1)
	{
		perror("Escribir coste arboles a talar.");
		return false;
	}

	return true;



}



bool CalcularCercaOptima(PtrListaArboles Optimo, int argc, char *argv[])
{
	int MaxCombinaciones=0, NumArboles=0;
	int NumTotalThreads=0, CombinacionesThread, thread, PuntosCerca;
	pthread_t *Tids;
	PtrRang Rangs;
	TListaArboles CombinacionArboles;
	TVectorCoordenadas CoordArboles, CercaArboles;
	float MaderaArbolesTalados;
	int *combParcial = malloc(sizeof(int));//Valor retornado por cada thread

	if(argc > 2){
		NumTotalThreads = atoi(argv[2]); //Almacenamos numero threads introducido como parametro
	}else{
		NumTotalThreads = DefaultThreads;
	}

	int combParciales[NumTotalThreads];
	//�culo Máximo Combinaciones */
	MaxCombinaciones = (int) pow(2.0,ArbolesEntrada.NumArboles);
	CombinacionesThread = MaxCombinaciones/NumTotalThreads; //numero de combinaciones asignadas a cada thread
	// Ordenar Arboles por segun coordenadas crecientes de x,y
	OrdenarArboles();

	Tids = malloc(sizeof(pthread_t)*NumTotalThreads);
	if(Tids == NULL){
		perror("Error en reserva de memoria tids.");
		exit(1);
	}

	Rangs = malloc(sizeof(struct TRang)*NumTotalThreads);
	if (Rangs == NULL){
		perror("Error en reserva de memoria rangs.");
		exit(1);
	}
	
	pthread_mutex_init(&Mutex,NULL);
	pthread_cond_init(&CondPartial,NULL);
	pthread_barrier_init(&Barrera,NULL,NumTotalThreads);
	sem_init (&SemMutex,0,1);

	/* C�culo �timo */
	Optimo->NumArboles = 0;
	Optimo->Coste = DMaximoCoste;
	Rangs[0].inici = 1;

	for(thread = 0; thread < NumTotalThreads; thread++){ //creamos n threads
		Rangs[thread].ArbresOpt = Optimo;
		if (thread < (NumTotalThreads-1)){
			Rangs[thread].final = Rangs[thread].inici + (CombinacionesThread-1);
		}else{
			Rangs[thread].final = MaxCombinaciones - 1;
		}

		if (pthread_create(&(Tids[thread]), NULL, (void *(*) (void *)) CalcularCombinacionOptima, &(Rangs[thread])) != 0){ //cada thread ejecutara la funcion CalcularCombinacionOptima(Rangs)
			perror("Creacio threads.");
			exit(1);
		}
		if(thread < (NumTotalThreads -1 )){
			Rangs[thread + 1].inici = Rangs[thread].final + 1;
		}

	}
	
	for( thread = 0; thread<NumTotalThreads; thread++){
		
		if(pthread_join(Tids[thread], NULL)){ //sincronitzacion de todos los threads i guardar valor de retorno a combParcial
			perror("Error join threads");
			exit(1);
		}
		//combParciales[thread] = *combParcial; //guardamos en el array el valor retornado de cada thread
	}
	pthread_mutex_destroy(&Mutex);
	pthread_cond_destroy(&CondPartial);
	pthread_barrier_destroy(&Barrera);
	sem_destroy(&SemMutex);

	printf("\n");
	printf("Evaluacion Combinaciones posibles: \n");
	int index = 0;
	//min_cost = cercaCostMinim(combParciales, &index); //buscamos el minimo coste de todas las posibles soluciones de combParciales
	int combOpt = bestComb; //la combinacion optima sera la que se encuentre en el index devuelto por la funcion cercaCostMin


	ConvertirCombinacionToArbolesTalados(combOpt, &OptimoParcial);
	printf("\rOptimo %d-> Coste %d, %d Arboles talados:", combOpt, min_cost, OptimoParcial.NumArboles);
	MostrarArboles(OptimoParcial);
	printf("\n");

	if (min_cost == Optimo->Coste)
		return false;  // No se ha encontrado una combinacin mejor.

	 // Asignar combinacin encontrada.
	ConvertirCombinacionToArbolesTalados(combOpt, Optimo);
	Optimo->Coste = min_cost;
	// Calcular estadisticas óptimo.
	NumArboles = ConvertirCombinacionToArboles(combOpt, &CombinacionArboles);
	ObtenerListaCoordenadasArboles(CombinacionArboles, CoordArboles);
	PuntosCerca = chainHull_2D( CoordArboles, NumArboles, CercaArboles );

	Optimo->LongitudCerca = CalcularLongitudCerca(CercaArboles, PuntosCerca);
	MaderaArbolesTalados = CalcularMaderaArbolesTalados(*Optimo);
	Optimo->MaderaSobrante = MaderaArbolesTalados - Optimo->LongitudCerca;
	Optimo->CosteArbolesCortados = min_cost;
	Optimo->CosteArbolesRestantes = CalcularCosteCombinacion(CombinacionArboles);
	
	free(Tids); //Liberear memoria Tids 
	free(Rangs); //Liberar memoria Rangs
	free(combParcial);

	return true;
}

int cercaCostMinim(int combParciales[], int *index)
{
	size_t size = sizeof(combParciales, sizeof(int)); //size combParciales
	int Coste = EvaluarCombinacionListaArboles(combParciales[0]), newCoste; //Coste inicial el de la primera posible solucion

	for(int i = 1; i<size-1;i++){
		newCoste = EvaluarCombinacionListaArboles(combParciales[i]); //Coste de la posible solucion i
		if(Coste>newCoste){
			Coste = newCoste; //actualizamos valor del coste
			*index = i; //index de la solucion es actualizado al valor de indice actual
		}
	}
	return Coste; 
}

void OrdenarArboles()
{
  int a,b;
  
	for(a=0; a<(ArbolesEntrada.NumArboles-1); a++)
	{
		for(b=1; b<ArbolesEntrada.NumArboles; b++)
		{
			if ( ArbolesEntrada.Arboles[b].Coord.x < ArbolesEntrada.Arboles[a].Coord.x ||
				 (ArbolesEntrada.Arboles[b].Coord.x == ArbolesEntrada.Arboles[a].Coord.x && ArbolesEntrada.Arboles[b].Coord.y < ArbolesEntrada.Arboles[a].Coord.y) )
			{
				TArbol aux;

				// aux=a
				aux.Coord.x = ArbolesEntrada.Arboles[a].Coord.x;
				aux.Coord.y = ArbolesEntrada.Arboles[a].Coord.y;
				aux.IdArbol = ArbolesEntrada.Arboles[a].IdArbol;
				aux.Valor = ArbolesEntrada.Arboles[a].Valor;
				aux.Longitud = ArbolesEntrada.Arboles[a].Longitud;

				// a=b
				ArbolesEntrada.Arboles[a].Coord.x = ArbolesEntrada.Arboles[b].Coord.x;
				ArbolesEntrada.Arboles[a].Coord.y = ArbolesEntrada.Arboles[b].Coord.y;
				ArbolesEntrada.Arboles[a].IdArbol = ArbolesEntrada.Arboles[b].IdArbol;
				ArbolesEntrada.Arboles[a].Valor = ArbolesEntrada.Arboles[b].Valor;
				ArbolesEntrada.Arboles[a].Longitud = ArbolesEntrada.Arboles[b].Longitud;

				// b=aux
				ArbolesEntrada.Arboles[b].Coord.x = aux.Coord.x;
				ArbolesEntrada.Arboles[b].Coord.y = aux.Coord.y;
				ArbolesEntrada.Arboles[b].IdArbol = aux.IdArbol;
				ArbolesEntrada.Arboles[b].Valor = aux.Valor;
				ArbolesEntrada.Arboles[b].Longitud = aux.Longitud;
			}
		}
	}
}



// Calcula la combinacin ptima entre el rango de combinaciones PrimeraCombinacion-UltimaCombinacion.

void CalcularCombinacionOptima(PtrRang Rangs) 
{

	int Combinacion,  CosteMejorCombinacion, PrimeraCombinacion, UltimaCombinacion;
	int  MejorCombinacion=0,*MejorCombinacion_ret = malloc(sizeof(int));
	int Coste;
	PtrListaArboles Optimo;
	TListaArboles OptimoParcial;
	int NumArboles;

	PrimeraCombinacion = Rangs -> inici; //guarda el valor d'inici del thread
	UltimaCombinacion = Rangs -> final; //guarda el valor de final del thread
	Optimo = Rangs->ArbresOpt; //guarda a optimo l'estructura d'arbres de l'estructura Rangs

	CosteMejorCombinacion = Optimo->Coste;
	for (Combinacion=PrimeraCombinacion; Combinacion<=UltimaCombinacion; Combinacion++)
	{
//    	printf("\tC%d -> \t",Combinacion);
		Coste = EvaluarCombinacionListaArboles(Combinacion);
		if ( Coste < CosteMejorCombinacion )
		{
			CosteMejorCombinacion = Coste;
			MejorCombinacion = Combinacion;
//      	printf("***");
		}
		if ((Combinacion%S)==0)
		{
			 ConvertirCombinacionToArbolesTalados(MejorCombinacion, &OptimoParcial);
			 printf("\r[%d] OptimoParcial %d-> Coste %d, %d Arboles talados:", Combinacion, MejorCombinacion, CosteMejorCombinacion, OptimoParcial.NumArboles);
			 MostrarArboles(OptimoParcial);
		}
			
//    printf("\n");
	}
	sem_wait(&SemMutex);
	//*MejorCombinacion_ret=MejorCombinacion; //devolvemos un apuntador (actualizamos la variable de retorno a la buscada anteriormente)
	if (CosteMejorCombinacion<min_cost)
	{
		min_cost = CosteMejorCombinacion;
		bestComb = MejorCombinacion;
	}
	sem_post(&SemMutex);
	pthread_barrier_wait(&Barrera);
	//pthread_exit(MejorCombinacion_ret); //el thread termina y devuelve el puntero a la variable solucion

}



int EvaluarCombinacionListaArboles(int Combinacion)
{
	TVectorCoordenadas CoordArboles, CercaArboles;
	TListaArboles CombinacionArboles, CombinacionArbolesTalados;
	int NumArboles, NumArbolesTalados, PuntosCerca, CosteCombinacion;
	float LongitudCerca, MaderaArbolesTalados;

	// Convertimos la combinacin al vector de arboles no talados.
	NumArboles = ConvertirCombinacionToArboles(Combinacion, &CombinacionArboles);

	// Obtener el vector de coordenadas de arboles no talados.
	ObtenerListaCoordenadasArboles(CombinacionArboles, CoordArboles);

	// Calcular la cerca
	PuntosCerca = chainHull_2D( CoordArboles, NumArboles, CercaArboles );

	/* Evaluar si obtenemos suficientes �boles para construir la cerca */
	LongitudCerca = CalcularLongitudCerca(CercaArboles, PuntosCerca);

	// Evaluar la madera obtenida mediante los arboles talados.
	// Convertimos la combinacin al vector de arboles no talados.
	NumArbolesTalados = ConvertirCombinacionToArbolesTalados(Combinacion, &CombinacionArbolesTalados);
if (DDebug) printf(" %d arboles cortados: ",NumArbolesTalados);
if (DDebug) MostrarArboles(CombinacionArbolesTalados);
  MaderaArbolesTalados = CalcularMaderaArbolesTalados(CombinacionArbolesTalados);
if (DDebug) printf("  Madera:%4.2f  \tCerca:%4.2f ",MaderaArbolesTalados, LongitudCerca);
	if (LongitudCerca > MaderaArbolesTalados)
	{
		// Los arboles cortados no tienen suficiente madera para construir la cerca.
if (DDebug) printf("\tCoste:%d",DMaximoCoste);
    return DMaximoCoste;
	}

	// Evaluar el coste de los arboles talados.
	CosteCombinacion = CalcularCosteCombinacion(CombinacionArbolesTalados);
if (DDebug) printf("\tCoste:%d",CosteCombinacion);
  
	return CosteCombinacion;
}


int ConvertirCombinacionToArboles(int Combinacion, PtrListaArboles CombinacionArboles)
{
	int arbol=0;

	CombinacionArboles->NumArboles=0;
	CombinacionArboles->Coste=0;

	while (arbol<ArbolesEntrada.NumArboles)
	{
		if ((Combinacion%2)==0)
		{
			CombinacionArboles->Arboles[CombinacionArboles->NumArboles]=arbol;
			CombinacionArboles->NumArboles++;
			CombinacionArboles->Coste+= ArbolesEntrada.Arboles[arbol].Valor;
		}
		arbol++;
		Combinacion = Combinacion>>1;
	}

	return CombinacionArboles->NumArboles;
}


int ConvertirCombinacionToArbolesTalados(int Combinacion, PtrListaArboles CombinacionArbolesTalados)
{
	int arbol=0;

	CombinacionArbolesTalados->NumArboles=0;
	CombinacionArbolesTalados->Coste=0;

	while (arbol<ArbolesEntrada.NumArboles)
	{
		if ((Combinacion%2)==1)
		{
			CombinacionArbolesTalados->Arboles[CombinacionArbolesTalados->NumArboles]=arbol;
			CombinacionArbolesTalados->NumArboles++;
			CombinacionArbolesTalados->Coste+= ArbolesEntrada.Arboles[arbol].Valor;
		}
		arbol++;
		Combinacion = Combinacion>>1;
	}

	return CombinacionArbolesTalados->NumArboles;
}



void ObtenerListaCoordenadasArboles(TListaArboles CombinacionArboles, TVectorCoordenadas Coordenadas)
{
	int c, arbol;

	for (c=0;c<CombinacionArboles.NumArboles;c++)
	{
    arbol=CombinacionArboles.Arboles[c];
		Coordenadas[c].x = ArbolesEntrada.Arboles[arbol].Coord.x;
		Coordenadas[c].y = ArbolesEntrada.Arboles[arbol].Coord.y;
	}
}


	
float CalcularLongitudCerca(TVectorCoordenadas CoordenadasCerca, int SizeCerca)
{
	int x;
	float coste;
	
	for (x=0;x<(SizeCerca-1);x++)
	{
		coste+= CalcularDistancia(CoordenadasCerca[x].x, CoordenadasCerca[x].y, CoordenadasCerca[x+1].x, CoordenadasCerca[x+1].y);
	}
	
	return coste;
}



float CalcularDistancia(int x1, int y1, int x2, int y2)
{
	return(sqrt(pow((double)abs(x2-x1),2.0)+pow((double)abs(y2-y1),2.0)));
}



int 
CalcularMaderaArbolesTalados(TListaArboles CombinacionArboles)
{
	int a;
	int LongitudTotal=0;
	
	for (a=0;a<CombinacionArboles.NumArboles;a++)
	{
		LongitudTotal += ArbolesEntrada.Arboles[CombinacionArboles.Arboles[a]].Longitud;
	}
	
	return(LongitudTotal);
}



int 
CalcularCosteCombinacion(TListaArboles CombinacionArboles)
{
	int a;
	int CosteTotal=0;
	
	for (a=0;a<CombinacionArboles.NumArboles;a++)
	{
		CosteTotal += ArbolesEntrada.Arboles[CombinacionArboles.Arboles[a]].Valor;
	}
	
	return(CosteTotal);
}






void
MostrarArboles(TListaArboles CombinacionArboles)
{
	int a;

	for (a=0;a<CombinacionArboles.NumArboles;a++)
		printf("%d ",ArbolesEntrada.Arboles[CombinacionArboles.Arboles[a]].IdArbol);

  for (;a<ArbolesEntrada.NumArboles;a++)
    printf("  ");  
}
	