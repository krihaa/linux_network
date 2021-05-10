#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
/*	Melding struct som server og klient sender til hverandre
*	char T: meldings type
*	int L: lengde på text / klienten bruker denne for antall jobber
*	char* Jobbtekst: teksten serveren sender
*/
struct packet
{
	char T;
	int L;
	char* Jobbtekst;
};

/*	Denne funksjonen sender innholdet i en struct packet over en socket
*
*	Input:
*		a_Socket: socket som det skal sendes over
*	  * a_Packet: peker til pakken som skal sendes
*/
void SendPacket(int a_Socket, struct packet *a_Packet)
{
	int s = htonl(a_Packet->L);
	write(a_Socket, &a_Packet->T, sizeof(a_Packet->T));
	write(a_Socket, &s, sizeof(s));
	if(a_Packet->Jobbtekst != NULL)
		write(a_Socket, a_Packet->Jobbtekst, a_Packet->L);
}

/*	Skriver ut egne feilmeldinger og errno
*	Input:
*		a_Info: Egne feilmeldinger
*/
void PrintError(char* a_Info)
{
	int err = errno;
	printf("%s: %s\n",a_Info, strerror(err));
}


/*	Denne funksjonen setter opp en socket tilkobling på addressen og porten gitt som char* argumenter
*	Hvis a_IsServer er 1 blir en server satt opp isteden og addressen ignorert (localhost blir brukt istedet)
*
*
*	Input:
*		a_Address: ip-addressen man skal koble til
*		a_Port: Port nummeret man skal koble til
*		a_IsServer: Hvis man skal sette opp en server isteden for å koble til en (1 for server, 0 for klient)
*
*	Return:
*		Funksjonen returnerer en socket hvis vellykket og -1 hvis noe gikk galt
*/
int SetupConnection(char* a_Address, char* a_Port, int a_IsServer)
{
	errno = 0;
	int error;

	if(a_IsServer)
		a_Address = "localhost";


	printf("Setter opp tilkobling paa: %s:%s\n", a_Address, a_Port);
	struct addrinfo hints;
	struct addrinfo *result, *tmp;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;
	
	error = getaddrinfo(a_Address, a_Port, &hints, &result);
	if(error != 0)
	{
		PrintError("Feilet aa skaffe address info");
		return -1;
	}

	int sock;
	for (tmp = result; tmp != NULL; tmp = tmp->ai_next) {
		sock = socket(tmp->ai_family, tmp->ai_socktype,tmp->ai_protocol);
		if (socket < 0)
			continue;
		if(a_IsServer) //Server
		{
			error = bind(sock, tmp->ai_addr, tmp->ai_addrlen);
			if(error == 0)
				break;
		}else //Client
		{
			error = connect(sock, tmp->ai_addr, tmp->ai_addrlen);
			if(error == 0)
				break;
		}
		if(a_IsServer)
			PrintError("Feilet aa binde");
		else
			PrintError("Feilet aa koble");
		freeaddrinfo(result);
		close(sock);
		return -1;
	}
	
	freeaddrinfo(result);

	printf("Tilkobling laget\n");

	//Ferdig med client siden
	if(!a_IsServer)
		return sock;

	error = listen(sock, SOMAXCONN);
	if(error == -1)
	{
		PrintError("Feil paa listen");
		return -1;
	}

	//accept lager en ny socket for en klient så man kan ha flere klienter tilkoblet til en server
	int accepted = accept(sock, NULL, NULL);
	//Vi skulle bare tillate en klient-tilkobling så lukker
	close(sock);

	if(accepted < 0)
	{
		PrintError("Feil paa accept");
		return -1;
	}
	return accepted;
}