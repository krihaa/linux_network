#include "network.c"
#include <signal.h>
#include <stdlib.h>

/* Enkel signal handler for sigint
*/
volatile sig_atomic_t running = 1;
void SignalHandler(int a_Signal)
{
	if(a_Signal == SIGINT)
		running = 0;
}

/*	main vil først åpne filen i argumentet for lesning, sette sigint for så å prøve å koble seg til localhost og porten spesifisert i argumentet.
*	Etter det vil main lese pakker sent fra klienten over netverket
*
*	Input:
*		Filen som skal leses inn
*		Porten til serveren 
*/
int main(int argc, char *argv[])
{
	if(argc < 3)
	{
		printf("For få argumentet, kjør programmet med: ./Server <Jobfile> <portnumber>\n");
		return 0;
	}


	struct sigaction sa;
	sa.sa_handler = SignalHandler;
	sa.sa_flags = 0;
	sigemptyset( &sa.sa_mask );
	sigaction( SIGINT, &sa, NULL );


	FILE *f = fopen(argv[1], "rb");
	if (!f)
	{
		printf("Kunne ikke åpne filen: %s\n", argv[1]);
		printf("Avsluttet servern\n");
		return 0;
	}

	int socket = SetupConnection("localhost",argv[2],1);
	if(socket < 0)
	{
		printf("Kunne ikke koble til localhost:%s\n", argv[2]);
		running = 0;
	}
	while(running)
	{
		char msgType = 'U';
		read(socket, &msgType, sizeof(msgType));
		if(msgType == 'G')
		{
			int antJobbs = 0;
			read(socket, &antJobbs, sizeof(antJobbs));
			antJobbs = ntohl(antJobbs);
			while(antJobbs)
			{
				char fileType = fgetc(f);
				int fileLength = fgetc(f);

				struct packet *p = malloc(sizeof * p);

				//Hvis det ikke er mer å lese inn
				if(fileLength <= 0)
				{
					//Send en melding som ber klienten om å avslutte
					p->T = 'Q';
					p->L = 0;
					p->Jobbtekst = NULL;
					SendPacket(socket, p);
					free(p);
					break;
				}
				else
				{
					p->T = fileType;
					p->L = fileLength;
					p->Jobbtekst = calloc(fileLength+1, sizeof(char));
					
					for (int y = 0; y < fileLength; y++)
					{
						char c = fgetc(f);
						if(c != -1)
							p->Jobbtekst[y] = c;
					}
					SendPacket(socket, p);
					free(p->Jobbtekst);
				}

				free(p);
				antJobbs--;
			}
		}else if(msgType == 'T')
		{
			printf("Klienten terminerte normalt\n");
			running = 0;
		}else if(msgType == 'E')
		{
			printf("Klienten terminerte paa grunn av feil\n");
			running = 0;
		}
	}
	if(socket > -1)
	{
		close(socket);
	}
	fclose(f);
	printf("Avsluttet serveren\n");
}