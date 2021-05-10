#include "network.c"
#include <signal.h>
#include <sys/wait.h>



/* Enkel signal handler for sigint
*/
volatile sig_atomic_t running = 1;
volatile sig_atomic_t sigint = 0;
void SignalHandler(int a_Signal)
{
	if(a_Signal == SIGINT)
	{
		sigint = 1;
		running = 0;
	}
}



//Pipes til barneprossesser
int pipe_out[2];
int pipe_err[2];


/*	Denne funksjonen leser fra pipe_out og skriver det til stdout.
*	Jeg legger også til en '\0' på slutten av teksten
*/
void StdoutChild()
{
	close(pipe_out[1]);
	int l = 0;
	char* buffer;
	while(running)
	{
		if(read(pipe_out[0], &l, sizeof(l))>0)
		{
			buffer = calloc(l+2,sizeof(char));
			read(pipe_out[0], buffer, l);
			buffer[l+1] = '\0';
			if(strcmp(buffer,"Q") == 0) //Hvis vi skal avslutte
				running = 0;
			else
			{
				fprintf(stdout, "%s", buffer);
				fflush(stdout);
			}
			free(buffer);
		}
	}
}
/*	Denne funksjonen leser fra pipe_err og skriver det til stderr.
*	Jeg legger også til en '\0' på slutten av teksten
*/
void StderrChild()
{
	close(pipe_err[1]);
	int l = 0;
	char* buffer;
	while(running)
	{
		if(read(pipe_err[0], &l, sizeof(l))>0)
		{
			buffer = calloc(l+2,sizeof(char));
			read(pipe_err[0], buffer, l);
			buffer[l+1] = '\0';
			if(strcmp(buffer,"Q") == 0)//Hvis vi skal avslutte
				running = 0;
			else
			{
				fprintf(stderr, "%s", buffer);
				fflush(stderr);
			}
			free(buffer);
		}
	}
}


/*	Denne funksjonen lager 2 barneprosesser en som kaller på StdoutChild(); og en som kaller på StderrChild();
*	
*	Input:
*		StdoutPid: Dette nummeret vil bli pid til et av barna
*		StderrPid: Dette nummeret vil bli pid til et av barna
*/
void CreateChildProsess(int *StdoutPid, int *StderrPid)
{
	*StdoutPid = fork();
	if(*StdoutPid == -1)
	{
		printf("Noe gikk feil med fork()\n");
	}
	if(*StdoutPid == 0)
		StdoutChild();
	else
	{
		*StderrPid = fork();
		if(*StderrPid == -1)
		{
			printf("Noe gikk feil med fork()\n");
		}
		if(*StderrPid == 0)
			StderrChild();
	}
}

/*	Denne funksjonen venter på x antall pakker fra serveren og hånterer pakkene den mottar
*	enten ved å sende dem til barneprosesser eller avslutte programmet
*
*	Input:
*		a_Socket: socket å lese fra
*		a_NbrOfPackets: antall pakker den forventer at serveren sender
*/

void waitForPackage(int a_Socket, int a_NbrOfPackets)
{
	while(a_NbrOfPackets && running)
	{
		char type = 'U';
		read(a_Socket, &type, sizeof(type));
		int length = 0;
		read(a_Socket, &length, sizeof(length));
		length = ntohl(length);
		char* text = calloc(length, sizeof(char));
		read(a_Socket, text, length);
		switch (type)
		{
			case 'O':
				write(pipe_out[1], &length, sizeof(length));
				write(pipe_out[1], text, length);
				break;
			case 'E':
				write(pipe_err[1], &length, sizeof(length));
				write(pipe_err[1], text, length);
				break;
			case 'Q':
				running = 0;
				printf("Ingen flere jobber tilgjengerlig\n");
				free(text);
				return;
			default:
				break;
		}
		free(text);
		a_NbrOfPackets--;
	}
}


/*	Ordreløkke som spør brukeren om valg og sender melding til serveren.
*	istedenfor å sende write(a_Socket, "G", sizeof(char)) for hver jobb valgte jeg her å bruke
*	den samme packet structen serveren bruker og lengde-variablen blir da brukt til antall jobber
*	klienten vil ha:
*	1: en jobb, -1: alle jobber, 1++: x antall jobber
*
*	Input:
*		a_Socket: socket til serveren
*/

void orderLoop(int a_Socket)
{
	int o = -1;
	printf("-------Meny-------\n");
	printf("1) : Hent en jobb fra serveren \n");
	printf("2) : Hent x antall jobber fra serveren \n");
	printf("3) : Hent alle jobber fra serveren \n");
	printf("4) : Avslutt programmet \n");
	fscanf(stdin,"%d", &o);

	//Bruker samme melding som serveren isteden for å sende en get-melding

	struct packet *p = malloc(sizeof * p);
	p->T = 'G';
	p->L = 1;
	p->Jobbtekst = NULL;
	int ant = 0;
	switch (o)
	{
	case 1:
		SendPacket(a_Socket, p);
		waitForPackage(a_Socket, 1);
		break;
	case 2:
		printf("Skriv inn antall jobber:\n");
		fscanf(stdin,"%d", &ant);
		if(ant < 2)
		{
			printf("Skriv inn en heltall storre en en\n");
			break;
		}
		printf("Henter %i jobber fra serveren\n
			(hvis serveren har mindre en %i antall jobber\n
			får klienten alle jobbene serveren har)\n", ant, ant);
		p->L = ant;
		SendPacket(a_Socket, p);
		waitForPackage(a_Socket, ant);
		break;
	case 3:
		printf("Henter alle jobber fra serveren\n");
		p->L = -1;
		SendPacket(a_Socket, p);
		waitForPackage(a_Socket, -1);
		break;
	case 4:
		running = 0;
		break;
	default:
		printf("Ukjent kommando. Skriv bare et nummer\n");
		break;
	}
	free(p);
}

/*	main setter opp to barneprossesser, sigint og en tilkobling til serveren
*	Etter det vil main kjøre i en loop hvor den vil kalle på ordrelokke() og lese inn
*	meldinger fra serveren
*
*	Input:
*		Addresse til serveren
*		Porten til serveren 
*/

int main(int argc, char *argv[])
{

	if(argc < 3)
	{
		printf("For få argumentet, kjør programmet med: ./klient <server-address> <portnumber>\n");
		return 0;
	}

	struct sigaction sa;
	sa.sa_handler = SignalHandler;
	sa.sa_flags = 0;
	sigemptyset( &sa.sa_mask );
	sigaction( SIGINT, &sa, NULL );

	pipe(pipe_out);
  	pipe(pipe_err);

  	int StdoutPid = -1;
  	int StderrPid = -1;

	CreateChildProsess(&StdoutPid, &StderrPid);
	if(StdoutPid == -1 || StderrPid == -1)
	{
		running = 0;
	}

	//Hovedprossesen trenger ikke å lese, dette sørger også for at barneprossesene lukker når de returnerer
	close(pipe_out[0]);
  	close(pipe_err[0]);
  	int socket = -1;
	if(StdoutPid > 0 && StderrPid > 0)
	{
		socket = SetupConnection(argv[1],argv[2], 0);
		if(socket < 0)
		{
			printf("Kunne ikke koble til %s:%s\n", argv[1],argv[2]);
			running = 0;
		}
		while(running)
		{
			orderLoop(socket);
		}
		//Be barneprosessene avslutte
		//Jeg har valgt å ta med sigint i barneprossessene så hvis de blir terminert av sigint
		//vil jeg få sigpipe (og krasj) her hvis jeg ikke sjekker
		if(!sigint)
		{
			//Jeg kan ikke bare sende 'Q' til barneprossessene hvis en tekst fra serveren også inneholder Q
			char q = 'Q';
			int l = 1;
			write(pipe_out[1], &l, sizeof(int));
			write(pipe_out[1], &q, 1);

			write(pipe_err[1], &l, sizeof(int));
			write(pipe_err[1], &q, 1);
		}

		//Lukke resisterende pipes
		close(pipe_out[1]);
		close(pipe_err[1]);

		//Vente på barneprossessene
		int a;
		waitpid(StdoutPid, &a, 0);
		waitpid(StderrPid, &a, 0);

		if(socket > -1)
		{
			if(!sigint)
				write(socket, "T", sizeof(char));
			//Jeg vet ikke helt hvor jeg skal sende at klienten terminerte på grunn av feil da
			//de fleste feilene skjer før klienten er tilkoblet serveren
			//så jeg sender det hvis klienten blir avbrutt av ctrl+c (sigint)
			else
				write(socket, "E", sizeof(char));
			close(socket);
		}
		
		printf("\nHovedprosses avsluttet\n");
	}
	else
	{
		printf("\nBarneprosses avsluttet\n");
	}
	return 0;
}