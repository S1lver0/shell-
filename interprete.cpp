#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string>
#include <vector>
#include <strings.h>
#include <assert.h>
#include <fcntl.h> // Para funciones de manejo de archivos

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#define READ_END 0	/* index pipe extremo escritura */
#define WRITE_END 1 /* index pipe extremo lectura */

#define FILE_NAME "file.txt"

using namespace std;

bool flag = true;

int main()
{

	int fd1[2]; // definir los extramos de escritura y lectura
	int fd2;	// corresponde al archivo
	int status;

	while (true)
	{
		string programa;
		string ruta;
		if (flag == true)
		{
			cout << "Escriba el nombre del programa (o 'salir' para terminar): ";
			getline(cin, programa);
		}

		if (programa == "salir")
		{
			flag = false;
			break; // Salir del bucle si se ingresa "salir"
		}

		cout << programa << endl;

		if (programa[0] != '/')
		{
			ruta = "/bin/" + programa; // asumimos a /bin/
		}

		cout << ruta << endl;
		
		int pid = fork();
		if (pid == 0)
		{

			// Proceso hijo - interprete de comandos

			if (programa.find(">") != string::npos)
			{
				cout << "*-----Se encontró '>' en la cadena.----*" << endl;
				size_t posicion = programa.find(">");

				string comando = programa.substr(0, posicion - 1);
				string nombreArchivoSalida = programa.substr(posicion + 2);

				cout << "nombre:" << comando << endl;
				cout << "archivo:" << nombreArchivoSalida << endl;

				vector<string> argumentos; // creamos un vector de strings para almancenar los argumentos
				size_t pos = 0;			   // iniciamos la posicion inicial en 0

				// string::npos se devuelve en caso no ingresar una cadena
				// programa.find(" ") devuelve un numero entero de la posicion si encuentra un espacio
				while ((pos = comando.find(" ")) != string::npos)
				{
					argumentos.push_back(comando.substr(0, pos)); // se agrega a argumentos, el primer argumento
					comando.erase(0, pos + 1);					  // elimina el argumento que ya tomamos
				}

				argumentos.push_back(comando); // se agrega el ultimo argumento ya que no tendra espacios

				for (const string &arg : argumentos)
				{
					cout << "Argumento:" << arg << endl;
				}

				// USARMOS execvp que recibe (el nombre del comando , una matriz de cadenas char*.)

				// tenemos que crear un vector de punteros tipo char* teniendoe en cuenta que
				vector<char *> args;
				// el último elemento de args debe ser un puntero NULL.

				// recorremos el vector argumentos dividos previamente
				//  - token.str convierte el objeto string a (un puntero de caracteres)
				//  - const_cast<char*> convertir la variable constante a una no constante
				for (const string &token : argumentos)
				{
					args.push_back(const_cast<char *>(token.c_str()));
				}

				args.push_back(NULL); // Agregar NULL al final

				// Abrir el archivo de salida en modo escritura (creándolo si no existe)
				int file = open(nombreArchivoSalida.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
				assert(file != -1); // Verificar si la apertura del archivo fue exitosa

				// Redirigir la salida estándar al archivo
				dup2(file, STDOUT_FILENO);

				// Cerrar el descriptor de archivo que ya no necesitamos
				close(file);

				// Ejecutar el programa
				execvp(args[0], &args[0]);
				// execl("/bin/ls", "ls",(char *)0);
				// execlp("/bin/ls","ls", (char *)0);

				// Si execlp falla, imprimir un mensaje de error
				cerr << "Error al ejecutar el programa. Intente nuevamente. > " << endl;
				exit(1);
			}
			else if (programa.find("|") != string::npos)
			{
				cout << "-----Se encontró '|' en la cadena.----" << endl;

				size_t posicion = programa.find("|");
				string comando1 = programa.substr(0, posicion);
				string comando2 = programa.substr(posicion + 1);

				// Eliminar espacios en blanco alrededor de los comandos
				comando1 = comando1.erase(0, comando1.find_first_not_of(" \t\n\r\f\v"));
				comando1 = comando1.erase(comando1.find_last_not_of(" \t\n\r\f\v") + 1);
				comando2 = comando2.erase(0, comando2.find_first_not_of(" \t\n\r\f\v"));
				comando2 = comando2.erase(comando2.find_last_not_of(" \t\n\r\f\v") + 1);

				cout << "comando1: " << comando1 << endl;
				cout << "comando2: " << comando2 << endl;

				int pipe_fd[2];
				if (pipe(pipe_fd) == -1)
				{
					cerr << "Error al crear la tubería." << endl;
					exit(1);
				}

				pid_t pid_hijo = fork();
				if (pid_hijo < 0)
				{
					cerr << "Error al crear el proceso hijo." << endl;
					exit(1);
				}

				if (pid_hijo == 0)
				{
					// Proceso hijo (primer comando)
					close(pipe_fd[READ_END]); /* Cerramos el extremo de lectura de la tubería */

					// Redirige la salida estándar al extremo de escritura de la tubería
					dup2(pipe_fd[WRITE_END], STDOUT_FILENO);
					close(pipe_fd[WRITE_END]);

					// Ejecuta el primer comando
					execl("/bin/sh", "sh", "-c", comando1.c_str(), NULL);
					cerr << "Error al ejecutar el primer comando." << endl;
					exit(1);
				}
				else
				{
					// Proceso padre
					waitpid(pid_hijo, NULL, 0); /* Espera a que el primer comando termine */

					close(pipe_fd[WRITE_END]); /* Cerramos el extremo de escritura de la tubería en el padre */

					// Redirige la entrada estándar desde el extremo de lectura de la tubería
					dup2(pipe_fd[READ_END], STDIN_FILENO);
					close(pipe_fd[READ_END]);

					// Ejecuta el segundo comando (sort)
					execl("/usr/bin/sort", "sort", NULL);
					cerr << "Error al ejecutar sort." << endl;
					exit(1);
				}
			}
			else if (programa.find("<") != string::npos)
			{
				cout << "*-----Se encontró '<' en la cadena.----*" << endl;

				size_t posicion = programa.find("<");
				string comando = programa.substr(0, posicion);
				string nombreArchivoEntrada = programa.substr(posicion + 1);

				// Eliminar espacios en blanco alrededor del nombre del archivo
				nombreArchivoEntrada = nombreArchivoEntrada.erase(0, nombreArchivoEntrada.find_first_not_of(" \t\n\r\f\v"));
				nombreArchivoEntrada = nombreArchivoEntrada.erase(nombreArchivoEntrada.find_last_not_of(" \t\n\r\f\v") + 1);

				cout << "comando: " << comando << endl;
				cout << "archivo: " << nombreArchivoEntrada << endl;

					// Proceso hijo

					// Abre el archivo de entrada en modo lectura
					int file = open(nombreArchivoEntrada.c_str(), O_RDONLY);
					if (file == -1)
					{
						cerr << "Error al abrir el archivo de entrada." << endl;
						exit(1);
					}

					// Redirigir la entrada estándar desde el archivo
					if (dup2(file, STDIN_FILENO) == -1)
					{
						cerr << "Error al redirigir la entrada desde el archivo." << endl;
						exit(1);
					}

					// Cerrar el descriptor de archivo que ya no necesitas
					close(file);

					// Ejecutar el comando externo
					execl("/bin/sh", "sh", "-c", comando.c_str(), (char *)0);
					cerr << "Error al ejecutar el comando externo." << endl;
					exit(1);
				
			}
			else
			{
				cout << "*-----no hay > -----*" << endl;

				vector<string> argumentos; // creamos un vector de strings para almancenar los argumentos
				size_t pos = 0;			   // iniciamos la posicion inicial en 0

				// string::npos se devuelve en caso no ingresar una cadena
				// programa.find(" ") devuelve un numero entero de la posicion si encuentra un espacio
				while ((pos = programa.find(" ")) != string::npos)
				{
					argumentos.push_back(programa.substr(0, pos)); // se agrega a argumentos, el primer argumento
					programa.erase(0, pos + 1);					   // elimina el argumento que ya tomamos
				}

				argumentos.push_back(programa); // se agrega el ultimo argumento ya que no tendra espacios

				// USARMOS execvp que recibe (el nombre del comando , una matriz de cadenas char*.)

				// tenemos que crear un vector de punteros tipo char* teniendoe en cuenta que
				vector<char *> args;
				// el último elemento de args debe ser un puntero NULL.

				// recorremos el vector argumentos dividos previamente
				//  - token.str convierte el objeto string a (un puntero de caracteres)
				//  - const_cast<char*> convertir la variable constante a una no constante
				for (const string &token : argumentos)
				{
					args.push_back(const_cast<char *>(token.c_str()));
				}

				args.push_back(NULL); // Agregar NULL al final

				execvp(args[0], &args[0]);

				execl(ruta.c_str(), programa.c_str(), (char *)0);
				cerr << "Error al ejecutar el programa-intente nuevamente no hay >" << endl;
				exit(1); // Salir con un código de error
			}
		}
		else if (pid > 0)
		{ // Proceso padre

			wait(0);
			cout << "El programa ha finalizado." << endl;
		}
		else
		{
			// Error al crear el proceso hijo
			cerr << "Error al crear el proceso hijo." << endl;
		}
	}

	return 0;
}

// asdasd
