#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h> 
#include <stdio.h> 
#include <string.h> 
#include <errno.h> 
#include <string>
#include <iostream>
#include <unistd.h>
#include <thread>
#include <queue> 
#include <map>
#include <random>
#include <time.h>

using namespace std;

#define LIM_X 80
#define LIM_Y 24
#define MAP_SIZE 1920 //LIM_X * LIM_Y

class Player;

map<char, int> client_sym; //sockets de cada usuario(por avatar)
map<char, int>::iterator sym_it; //iterador en la lista de sockets

map<char, pair<int,int>> car_pos; //posiciones de los carros de los usuarios

map<char, Player*> players; //datos de los usuarios


char mapa[LIM_Y][LIM_X];




class Player
{
public:
	char nickname; //simbolo o avatar del jugador
	pair<int,int> car; //posicion del auto del jugador

	//Constructor
	Player(char nick, int pos_y, int pos_x)
	{
		nickname = nick;
		car = make_pair(pos_y,pos_x);	
	}

	//Mueve el carro a la nueva posicion
	void move_car(int new_y, int new_x)
	{		
		mapa[car.first][car.second] = '-'; //al avanzar, se borra la anterior posicion		
		mapa[new_y][new_x] = nickname; //actualizamos el mapa, colocando el auto en la nueva posicion
		car = make_pair(new_y,new_x);	
	}

	
};




//Generador de aleatorios
int intRand(const int & min, const int & max)
{
    static thread_local std::mt19937 generator;
    std::uniform_int_distribution<int> distribution(min,max);
    return distribution(generator);
}



//convierte el mapa en un string
string stringify_map()
{
	string resultado = "";
	for(int i=0; i<LIM_Y; i++)
	{
		for(int j=0; j<LIM_X; j++)
		{
			resultado += mapa[i][j];
		}
	}
	return resultado;
}


//Devuelve el avatar o sibolo que pertence al cliente de ese socket
char nick(int client)
{
	char resultado;
	for(sym_it=client_sym.begin(); sym_it!=client_sym.end(); ++sym_it)
	{
		if(sym_it->second==client)
		{
			resultado = sym_it->first;
		}
	}
	return resultado;	
}


//Revisa si ese avatar esta disponible
bool nick_ok(char nickname)
{
	//simbolos de espacio vacio y worm
	if(nickname=='|' or nickname=='-')
	{
		return false;
	}
	//nickname repetido
	for(sym_it=client_sym.begin(); sym_it!=client_sym.end(); ++sym_it)
	{
		if(sym_it->first == nickname)
		{
			return false;
		}
	}
	return true;
}





//Colisiones
void check_player_collision(char nickname, int new_x, int new_y, int old_x, int old_y)
{
	//si no hubo movimiento	
	if(new_y==old_y and new_x==old_x) 
	{
		return;
	}
	Player *myself = players[nickname]; //datos de este usuario
	//si la posicion destino esta libre...
	if( mapa[new_y][new_x]=='-' )
	{
		myself->move_car(new_y,new_x); //hacemos que el carro se mueva
	}
	//si choca con otro jugador...
	else
	{
		//no puede avanzar
	}

}

//mueve al jugador, aplicando las reglas de juego
void actualizar(int cliente, char direccion)
{
	Player *myself = players[nick(cliente)]; //datos de este usuario
	//pair<int,int> my_pos = car_pos[ nick(cliente) ]; //posicion actual del auto del jugador
	pair<int,int> my_pos = myself->car; //posicion actual del auto del jugador
	int old_x = my_pos.second;
	int old_y = my_pos.first;
	int new_x = old_x;
	int new_y = old_y;
	if(direccion=='D')
	{
		new_y++;
		if( new_y >= LIM_Y)
		{
			new_y = LIM_Y - 1;
		}
	}
	else if(direccion=='T')
	{
		new_y--;
		if( new_y < 0)
		{
			new_y = 0;
		}
	}
	else if(direccion=='L')
	{
		new_x--;
		if( new_x < 0)
		{
			new_x = 0;
		}
	}
	else if(direccion=='R')
	{
		new_x++;
		if( new_x >= LIM_X)
		{
			new_x = LIM_X - 1;
		}
	}
	check_player_collision(nick(cliente), new_x, new_y, old_x, old_y);
	 
}

//Receive instructions from client
void processClient_thread(int socketClient)
{
	char buffer[MAP_SIZE+2];
	string message;
	int n;
	int pos_x = 0;
	int pos_y = 0;
	for(;;)
	{
		buffer[0] = 'X';
		n = read(socketClient, buffer, 1);
		if(n==0)
		{
			continue;
		}
		//Login
		if(buffer[0] == 'A')
		{

			bzero(buffer,1); //cleaning the buffer of the header-size
			read(socketClient, buffer, 1);
			//check if there is not another user with the same nickname
			if(nick_ok(buffer[0]))
			{
				client_sym[buffer[0]] = socketClient; //add user's symbol
				while(mapa[pos_y][pos_x]!='-')
				{
					pos_x = intRand(0,LIM_X-1);
					pos_y = intRand(0,LIM_Y-1);
				}
				car_pos[buffer[0]] = make_pair(pos_y,pos_x); //add user's position
				mapa[pos_y][pos_x] = buffer[0]; //colocamos el auto del jugador en el mapa
				players[buffer[0]] = new Player(buffer[0],pos_y,pos_x); //informacion del jugador
				cout<< "Se unio el jugador: "<<buffer[0]<<endl;
				write(socketClient,"O",1); //acknowledgment ok
				//enviamos el mapa
				message = "U" + stringify_map(); //mapa actualizado
				write(socketClient, message.c_str(), message.size());
			}
			else
			{
				write(socketClient,"V",1); //error: nickname is already being used
			}
			bzero(buffer,1); //cleaning the buffer of the header-size
			
		}
		//Movimiento realizado por el usuario
		else if(buffer[0] == 'M')
		{
			bzero(buffer,1); //cleaning the buffer of the header-size
			read(socketClient, buffer, 1); //direccion de movimiento
			actualizar(socketClient, buffer[0]); //mueve al jugador, aplicando las reglas de juego			
			message = "U" + stringify_map(); //mapa actualizado
			//enviamos a todo usuario el mapa actualizado
			for(sym_it=client_sym.begin(); sym_it!=client_sym.end(); ++sym_it)
			{
				write(sym_it->second, message.c_str(), message.size());
			}
			bzero(buffer, message.size());			
		}		
	}
}

int main(void)
{
	struct sockaddr_in stSockAddr;
	int SocketClient = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	char buffer[1000];
	int n;

	if(-1 == SocketClient)
	{
		perror("can not create socket");
		exit(EXIT_FAILURE);
	}

	memset(&stSockAddr, 0, sizeof(struct sockaddr_in));

	stSockAddr.sin_family = AF_INET;
	stSockAddr.sin_port = htons(1100);
	stSockAddr.sin_addr.s_addr = INADDR_ANY;

	if(-1 == bind(SocketClient,(const struct sockaddr *)&stSockAddr, sizeof(struct sockaddr_in)))
	{
		perror("error bind failed");
		close(SocketClient);
		exit(EXIT_FAILURE);
	}

	if(-1 == listen(SocketClient, 10))
	{
		perror("error listen failed");
		close(SocketClient);
		exit(EXIT_FAILURE);
	}

	//Iniciando el mapa
	for(int i=0; i<LIM_Y; i++)
	{
		for(int j=0; j<LIM_X; j++)
		{
			mapa[i][j] = '-';
		}
	}

	cout<<"JUEGO INICIADO"<<endl;

	for(;;)
	{
		int ConnectFD = accept(SocketClient, NULL, NULL);
		if(0 > ConnectFD)
		{
			perror("error accept failed");
			close(SocketClient);
			exit(EXIT_FAILURE);
		}
		else
		{
			thread(processClient_thread,ConnectFD).detach();
		}
	}	
		
	close(SocketClient);
	return 0;
}
