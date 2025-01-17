/* ---------------------------------------------------------------
Práctica 3.
Código fuente: CalcArbolesConcurrente.c
Grau Informàtica
78098453K - Marc Junyent Moreno
48053932D - Boris Llona Alonso
--------------------------------------------------------------- */
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <ConvexHull.h>
#include <semaphore.h>
#include <ctype.h>
#define DMaxArboles 	25
#define DMaximoCoste 999999
#define S 10000
#define DDebug 0
#define DefaultThreads 3
#define M 250000

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
	int numThread;
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
int min_cost = 99999, bestComb = 0, Mcomb = M;

/* Sincronixación */
pthread_mutex_t Mutex;
pthread_cond_t CondPartial;
pthread_barrier_t Barrera;
sem_t SemMutex;

//Variables para el desbalanceo//
double tiempo_mas_lento;
int numThreadLento;

//numero total de threads//
int NumTotalThreads=0;

//contador global para hacer el "join"
int contadorSincro = 0;
int sincronizado = 0;
//Estadisticas globales//
int GlobEstCombinaciones, GlobEstCombValidas, GlobEstCombInvalidas;
long GlobEstCosteTotal;
int GlobEstMejorCosteCombinacion, GlobEstPeorCosteCombinacion, GlobEstMejorArboles, GlobEstMejorArbolesCombinacion, GlobEstPeorArboles, GlobEstPeorArbolesCombinacion;
float GlobEstMejorCoste, GlobEstPeorCoste;

  //////////////////////////
 // Prototipos funciones //
//////////////////////////

bool LeerFicheroEntrada(char *PathFicIn);
bool GenerarFicheroSalida(TListaArboles optimo, char *PathFicOut);
bool CalcularCercaOptima(PtrListaArboles Optimo);
int cercaCostMinim(int d[], int *j);
void OrdenarArboles();
void CalcularCombinacionOptima(PtrRang Rangs);
void mostrar_estadistiques_parciales(int numThread,int evalued,int mejor,int peor, int validas, int CosteTotal, int PeorCombinacion, int MejorCombinacion, int mejoresArboles,\
	int mejorArbolesComb, int peorArboles, int peorArbolesComp);
int mostrar_estadistiques_globales();
void mostrar_desbalanceo(double tiempo, int numThread);
void threadmeslent(double time, int numThread);
int EvaluarCombinacionListaArboles(int Combinacion, int *numCombValidas, int *costeCombValidas, int *mejoresArboles, int *mejorArbolesComb,\
	int *peorArboles, int *peorArbolesComp, int *peorcomb, int *costepeorComb);
int ConvertirCombinacionToArboles(int Combinacion, PtrListaArboles CombinacionArboles);
int ConvertirCombinacionToArbolesTalados(int Combinacion, PtrListaArboles CombinacionArbolesTalados);
void ObtenerListaCoordenadasArboles(TListaArboles CombinacionArboles, TVectorCoordenadas Coordenadas);
float CalcularLongitudCerca(TVectorCoordenadas CoordenadasCerca, int SizeCerca);
float CalcularDistancia(int x1, int y1, int x2, int y2);
int CalcularMaderaArbolesTalados(TListaArboles CombinacionArboles);
int CalcularCosteCombinacion(TListaArboles CombinacionArboles);
void MostrarArboles(TListaArboles CombinacionArboles);
void InicializarEstadisticas();

int main(int argc, char *argv[])
{
	TListaArboles Optimo;
	
	if (argc<2 || argc>5)
		printf("Error Argumentos. Usage: CalcArboles <Fichero_Entrada> [<Est_Parciales>] [<Fichero_Salida>]");

	if (!LeerFicheroEntrada(argv[1]))
	{
		printf("Error lectura fichero entrada.\n");
		exit(1);
	}
	if (argc>2)
		NumTotalThreads = atoi(argv[2]);
	else 
		NumTotalThreads = 3;

	if (argc>3)
		Mcomb = atoi(argv[3]);
	else 
		Mcomb = 25000;

	if (!CalcularCercaOptima(&Optimo))
	{
		printf("Error CalcularCercaOptima.\n");
		exit(1);
	}

	if (argc<5)
	{
		if (!GenerarFicheroSalida(Optimo, "./Valla.res"))
		{
			printf("Error GenerarFicheroSalida.\n");
			exit(1);
		}
	}
	else
	{
		if (!GenerarFicheroSalida(Optimo, argv[4]))
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
		// Escribir numero arbol.
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


bool CalcularCercaOptima(PtrListaArboles Optimo)
{
	int MaxCombinaciones=0, NumArboles=0;

	int  CombinacionesThread, thread, PuntosCerca;
	pthread_t *Tids;
	PtrRang Rangs;
	TListaArboles CombinacionArboles;
	TVectorCoordenadas CoordArboles, CercaArboles;
	float MaderaArbolesTalados;
	//int *combParcial = malloc(sizeof(int));//Valor retornado por cada thread

	//int combParciales[NumTotalThreads];
	//�culo Máximo Combinaciones */
	MaxCombinaciones = (int) pow(2.0,ArbolesEntrada.NumArboles);
	CombinacionesThread = MaxCombinaciones/NumTotalThreads; //numero de combinaciones asignadas a cada thread
	// Ordenar Arboles por segun coordenadas crecientes de x,y
	OrdenarArboles();
	InicializarEstadisticas();
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
	printf("NUM THREADS, %i\n", NumTotalThreads );
	
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
		Rangs[thread].numThread = thread;
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

	pthread_mutex_lock(&Mutex);
	while(contadorSincro < NumTotalThreads){
		pthread_cond_wait(&CondPartial,&Mutex);
	}
	pthread_mutex_unlock(&Mutex);

	pthread_mutex_destroy(&Mutex);
	pthread_cond_destroy(&CondPartial);
	pthread_barrier_destroy(&Barrera);
	sem_destroy(&SemMutex);

	printf("\n");
	printf("Evaluacion Combinaciones posibles: \n");

	int combOpt = GlobEstMejorCosteCombinacion; //la combinacion optima sera la que se encuentre en el index devuelto por la funcion cercaCostMin
	int min_cost = GlobEstMejorCoste;

	GlobEstCombValidas--;

	ConvertirCombinacionToArbolesTalados(combOpt, &OptimoParcial);
	printf("\rOptimo %d-> Coste %d, %d Arboles talados:", combOpt, min_cost, OptimoParcial.NumArboles);
	MostrarArboles(OptimoParcial);
	printf("\n");

	mostrar_estadistiques_globales();

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
	//free(combParcial);

	return true;
	
	
}

void InicializarEstadisticas()
{
	GlobEstCombinaciones=0;
	GlobEstCombValidas=0; 
	GlobEstCombInvalidas=0; 
	GlobEstCosteTotal=0;
	GlobEstMejorCosteCombinacion=DMaximoCoste;
	GlobEstPeorCosteCombinacion=0;
	GlobEstMejorArboles=ArbolesEntrada.NumArboles;
	GlobEstPeorArboles=0;
	GlobEstMejorArbolesCombinacion=0;
	GlobEstPeorArbolesCombinacion=0;
	GlobEstMejorCoste=DMaximoCoste;
	GlobEstPeorCoste=0;
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

	int Combinacion,  CosteMejorCombinacion, CostePeorCombinacion=0,PrimeraCombinacion, UltimaCombinacion;
	int  MejorCombinacion=0, PeorCombinacion=0;
	int Coste, cont=0;
	PtrListaArboles Optimo;
	TListaArboles OptimoParcial;
	int NumArboles, numThread, numCombValidas=0, costeTotal=0, costeCombValidas=0, mejoresArboles=9999, mejorArbolesComb=0;
	int peorArboles=0, peorArbolesComb=0;
	PrimeraCombinacion = Rangs -> inici; //guarda el valor d'inici del thread
	UltimaCombinacion = Rangs -> final; //guarda el valor de final del thread
	Optimo = Rangs->ArbresOpt; //guarda a optimo l'estructura d'arbres de l'estructura Rangs
	numThread = Rangs->numThread;
	CosteMejorCombinacion = Optimo->Coste;


	for (Combinacion=PrimeraCombinacion; Combinacion<=UltimaCombinacion; Combinacion++)
	{
		pthread_mutex_lock(&Mutex);
		GlobEstCombinaciones++;
		pthread_mutex_unlock(&Mutex);

    	clock_t start = clock();	//inicio del tiempo para calcular cuanto tarda el thread a realizar las tareas
//    	printf("\tC%d -> \t",Combinacion);
		Coste = EvaluarCombinacionListaArboles(Combinacion, &numCombValidas, &costeCombValidas, &mejoresArboles, &mejorArbolesComb,\
			&peorArboles, &peorArbolesComb, &PeorCombinacion, &CostePeorCombinacion);
		//printf("%i\n", numCombValidas );
		if ( Coste < CosteMejorCombinacion )
		{
			CosteMejorCombinacion = Coste;
			MejorCombinacion = Combinacion;
//      	printf("***");
		}



		if((Combinacion%S)==0){
			ConvertirCombinacionToArbolesTalados(MejorCombinacion, &OptimoParcial);
			//printf("\r[%d] OptimoParcial %d-> Coste %d, %d Arboles talados:", Combinacion, MejorCombinacion, CosteMejorCombinacion, OptimoParcial.NumArboles);
			//MostrarArboles(OptimoParcial);
		}
		cont++;
		if (cont==Mcomb) //si el thread llega a la combinación M, mostrar estadisticas y desbalanceo
		{
				
			clock_t end = clock();	//final del tiempo que ha tardado el thread en realizar las tareas.
			double cpu_time = ((double) (end - start)) / CLOCKS_PER_SEC;

			pthread_mutex_lock(&Mutex);
			threadmeslent(cpu_time, numThread);	//buscamos el thread más lento a partir del tiempo registrado
			pthread_mutex_unlock(&Mutex);

			pthread_mutex_lock(&Mutex);
    		mostrar_estadistiques_parciales(numThread,Combinacion-PrimeraCombinacion+1,MejorCombinacion,PeorCombinacion,numCombValidas, costeCombValidas, CostePeorCombinacion, CosteMejorCombinacion, \
    			mejoresArboles, mejorArbolesComb, peorArboles, peorArbolesComb);	//mostramos estadisticas parciales de cada thread
			mostrar_desbalanceo(cpu_time, numThread);
			pthread_mutex_unlock(&Mutex);

			pthread_barrier_wait(&Barrera); //Esperamos a que todos los threads lleguen a la barrera para seguir

			if(numThread==0){				//EL thread 0 muestra las estadisticas globales
				mostrar_estadistiques_globales();
			}	


			sem_wait(&SemMutex); //sincronizamos con un semaforo para ractualizar la variable global de tiempo_mas_lento
			tiempo_mas_lento = 0;	
			sem_post(&SemMutex);
			

			cont=0; //contador de numero de combinaciones a 0
			
		}
	}
	//una vez han terminado los threads incrementan la variable contadorSincro para controlar des de la clase principal si han terminado
	//Realiza la funcion del join
	pthread_mutex_lock(&Mutex);
	contadorSincro++;
	pthread_cond_signal(&CondPartial);	//manda la señal una vez el thread ha incrementado el valor de la variable
	pthread_mutex_unlock(&Mutex);

}

void mostrar_estadistiques_parciales(int numThread,int evalued,int mejor,int peor, int validas, int CosteTotal, int CostePeorCombinacion, int CosteMejorCombinacion, int mejorArboles,\
	int mejorArbolesComb, int peorArboles, int peorArbolesComb){
	
	printf("++++++++++++++++++++++++++++ESTADISTICAS PARCIALES THREAD NUMERO %i ++++++++++++++++++++++++++++++", numThread);
	printf("\n++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
	printf("++ Eval Comb: %i \tValidas: %i \tInvalidas: %i\tCoste Validas: %.3f\n", evalued, validas, evalued-validas, (float)CosteTotal/(float)validas);
	printf("++ Mejor Comb (coste): %i Coste: %i  \tPeor Comb (coste): %i Coste: %i\n",mejor, CosteMejorCombinacion, peor, CostePeorCombinacion);
	printf("++ Mejor Comb (árboles): %i Arboles: %i  \tPeor Comb (árboles): %i Arboles %i\n",mejorArbolesComb, mejorArboles, peorArbolesComb, peorArboles);
	printf("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

	
}
int mostrar_estadistiques_globales(){
	
	GlobEstCombInvalidas = GlobEstCombinaciones - GlobEstCombValidas;
	printf("++++++++++++++++++++++++++++++++++ ESTADISTICAS GLOBALES ++++++++++++++++++++++++++++++++++");
	printf("\n++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
	printf("++ Eval Comb: %d \tValidas: %d \tInvalidas: %d\tCoste Validas: %.3f\n", GlobEstCombinaciones, GlobEstCombValidas, GlobEstCombInvalidas, (float)GlobEstCosteTotal/(float)GlobEstCombValidas);
	printf("++ Mejor Comb (coste): %d Coste: %.3f  \tPeor Comb (coste): %d Coste: %.3f\n",GlobEstMejorCosteCombinacion, GlobEstMejorCoste, GlobEstPeorCosteCombinacion, GlobEstPeorCoste);
	printf("++ Mejor Comb (árboles): %d Arboles: %d  \tPeor Comb (árboles): %d Arboles %d\n",GlobEstMejorArbolesCombinacion, GlobEstMejorArboles, GlobEstPeorArbolesCombinacion, GlobEstPeorArboles);
	printf("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

}

void mostrar_desbalanceo(double tiempo, int numThread){
	//printf("EL TIEMPO MAS LENTO ES %f del thread %i\n", tiempo_mas_lento, numThreadLento);
	//printf("EL TIEMPO ES %f del thread %i\n", tiempo, numThread);
	printf("El desbalanceo del hijo %i respeto el thread más lento %i es: %f \n",numThread,numThreadLento, tiempo-tiempo_mas_lento);
	printf("\n");


}

void threadmeslent(double time, int numThread){
	if(time>tiempo_mas_lento){
		tiempo_mas_lento = time;
		numThreadLento = numThread;
	}
}

int EvaluarCombinacionListaArboles(int Combinacion, int *numCombValidas, int *costeCombValidas, int *mejoresArboles, int *mejorArbolesComb,\
	int *peoresArboles, int *peoresArbolesComb, int *PeorCombinacion, int *CostePeorCombinacion)
{
	TVectorCoordenadas CoordArboles, CercaArboles;
	TListaArboles CombinacionArboles, CombinacionArbolesTalados;

	int NumArboles, NumArbolesTalados, PuntosCerca, CosteCombinacion;
	int combValidas=*numCombValidas, costeVal=*costeCombValidas, mejArboles = *mejoresArboles, mejorArbComb = *mejorArbolesComb;
	int peorcomb = *PeorCombinacion, costepeorComb = *CostePeorCombinacion;
	int peorArboles = *peoresArboles, peorArbolesComb = *peoresArbolesComb;
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

	//estadisticas globales//
	pthread_mutex_lock(&Mutex);
	GlobEstCombValidas++;
	GlobEstCosteTotal+=CosteCombinacion;

	if (CosteCombinacion<GlobEstMejorCoste)
	{
		GlobEstMejorCoste = CosteCombinacion;
		GlobEstMejorCosteCombinacion =  Combinacion;
	}
	else if (CosteCombinacion>GlobEstPeorCoste)
	{
		GlobEstPeorCoste = CosteCombinacion;
		GlobEstPeorCosteCombinacion =  Combinacion;
	}
	if (NumArboles<GlobEstMejorArboles)
	{
		GlobEstMejorArboles = NumArboles;
		GlobEstMejorArbolesCombinacion =  Combinacion;
	}
	else if (NumArboles>GlobEstPeorArboles)
	{
		GlobEstPeorArboles = NumArboles;
		GlobEstPeorArbolesCombinacion =  Combinacion;
	}
	pthread_mutex_unlock(&Mutex);
	/////////////////////////////////////////

	/////////////////////////////////////////
	//estadisticas parciales de cada thread//
	combValidas++;
	costeVal=costeVal+CosteCombinacion;
	
	if(NumArboles<mejArboles){
		mejArboles = NumArboles;
		mejorArbComb = Combinacion;
	}else if(NumArboles>peorArboles){
		peorArboles = NumArboles;
		peorArbolesComb = Combinacion;
	}
	if(CosteCombinacion > costepeorComb){
			costepeorComb = CosteCombinacion;
			peorcomb = Combinacion;
	}
	*mejoresArboles = mejArboles;
	*numCombValidas = combValidas;
	*costeCombValidas = costeVal;
	*mejorArbolesComb = mejorArbComb;
	*peoresArboles = peorArboles;
	*peoresArbolesComb = peorArbolesComb;
	*PeorCombinacion = peorcomb;
	*CostePeorCombinacion = costepeorComb;
	////////////////////////////////////////

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
	printf("\n");
}
	
