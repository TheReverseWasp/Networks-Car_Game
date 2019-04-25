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
#include <ncurses.h>

#include <chrono>
#include <functional>

using namespace std;

//exemplary key codes for ncurses getch():
#define K_ENTER	0x0A
#define K_ENTER2	0x0D //ncurses only (and very strange :|)
#define K_ESC	0x1B
#define K_DOWN	0x102
#define K_UP	0x103
#define K_LEFT	0x104
#define K_RIGHT	0x105


#define LIM_X 80
#define LIM_Y 24
#define MAP_SIZE 1920 //LIM_X * LIM_Y

bool track_time = true;

//string mapa;
char *mapa = new char[MAP_SIZE];





void timer_start(std::function<void(void)> func, unsigned int interval)
{
    std::thread([func, interval]() {
        while (true)
        {
            func();
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        }
    }).detach();
}

void do_something()
{
    std::cout << "I am doing something" << std::endl;
}






//Funcion que dibuja la pantalla
void dibujar()
{
	string punto;
	bool print_track = track_time;
	string track;

	clear();
	for(int i=0; i<LIM_Y; i++)
	{
		if(print_track)
		{
			track = "|";
		}
		else
		{
			track = " ";
		}
		mvprintw(i, 0, track.c_str()); //borde izquierdo de la pista
		for(int j=0; j<LIM_X; j++)
		{
			if(mapa[LIM_X*i+j]=='-')
			{
				mvprintw(i, j+1, " "); //mapa[i][j]
			}
			else
			{
				punto = mapa[LIM_X*i+j];
				mvprintw(i, j+1, punto.c_str()); //mapa[i][j]
			}
			refresh();
		}
		mvprintw(i, LIM_X+1, track.c_str()); //borde derecho de la pista
		print_track = not print_track; //la siguiente linea se imprime, o viceversa
	}
	track_time = not track_time;
}


//Funcion de thread que envia el movimiento del usuario
void write_socket(int my_socket, bool &stop)
{
	int key;
	do{

		key = getch();
		//Abajo - DOWN
		if(key == K_DOWN)
		{
			write(my_socket, "MD", 2);
		}
		//Arriba - UP
		else if(key == K_UP)
		{
			write(my_socket, "MT", 2);
		}
		//Izquierda - LEFT
		else if(key == K_LEFT)
		{
			write(my_socket, "ML", 2);
		}
		//Derecha - RIGHT
		else if(key == K_RIGHT)
		{
			write(my_socket, "MR", 2);
		}
	}while( stop==false );
}

//Funcion de thread que lee la respuesta del server
void read_socket(int my_socket, bool &stop)
{
	char buffer[MAP_SIZE+1]; //espacio adicional para fin de linea
	int n;
	while(stop==false)
	{
		n = read(my_socket, buffer ,1); //read just the first byte to check the message's type
		if(n==0)
		{
			continue;
		}
		else if (buffer[0]=='G')
		{
			cout<<"GAME OVER"<<endl;
			stop = true;
			continue;
		}
		if (buffer[0]=='U') //Mensaje del server, envia el mapa de juego
		{

			bzero(buffer,1); //cleaning the buffer of the header-command
			read(my_socket,buffer,MAP_SIZE); //leemos el mapa
			for(int i=0; i<MAP_SIZE; i++)
			{
				mapa[i] = buffer[i];
			}
			bzero(buffer, MAP_SIZE); //cleaning buffer
		}
	}
}



int main()
{
	struct sockaddr_in stSockAddr;
	int Res;
	int SocketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (-1 == SocketFD)
	{
		perror("cannot create socket");
		exit(EXIT_FAILURE);
	}

	memset(&stSockAddr, 0, sizeof(struct sockaddr_in));
	stSockAddr.sin_family = AF_INET;
	stSockAddr.sin_port = htons(1100);
	Res = inet_pton(AF_INET, "0.0.0.0", &stSockAddr.sin_addr);

	if (0 > Res)
	{
		perror("error: first parameter is not a valid address family");
		close(SocketFD);
		exit(EXIT_FAILURE);
	}
	else if (0 == Res)
	{
		perror("char string (second parameter does not contain valid ipaddress");
		close(SocketFD);
		exit(EXIT_FAILURE);
	}

	if (-1 == connect(SocketFD, (const struct sockaddr *)&stSockAddr, sizeof(struct sockaddr_in)))
	{
		perror("connect failed");
		close(SocketFD);
		exit(EXIT_FAILURE);
	}

	//LOG IN
	char usuario;
	string mensaje;
	char answer_buffer[1];
	bool stop = false; //loop's control
	while(stop==false)
	{
		cout<<"Login: ";
		cin>>usuario;
		cout<<"Usuario: "<<usuario<<endl;
		mensaje = "A";
		mensaje += usuario;
		write(SocketFD, mensaje.c_str(), 2); //mandamos el mensaje de login al server
		//comprueba respuesta del log in
		read(SocketFD, answer_buffer, 1);

    if(answer_buffer[0] == 'O')//login ok
		{
			cout<<"Login is Ok"<<endl;
			stop = true;
		}
		else //error
		{
			cout<<"Login error"<<endl;
			stop = false;
		}
	}


	stop = false;

	//Iniciamos ventana de juego
	initscr();
	cbreak();
	noecho();
	keypad( stdscr, TRUE );
	move( 0, 0 );

	resize_term(LIM_Y, LIM_X+2);

	//This is what I send/write
	thread(write_socket, SocketFD, ref(stop)).detach();

	//This is what I receive/read
	thread(read_socket, SocketFD, ref(stop)).detach();

	//Dibujo el mapa cada 100 milisegundos
	timer_start(dibujar, 100);

	//waiting for the end
	while(stop==false)
	{
	}


	shutdown(SocketFD, SHUT_RDWR);
	close(SocketFD);
	keypad(stdscr, FALSE);
	endwin();
	return 0;
}
