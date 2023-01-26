all: backend frontend

frontend: Source_Files/frontend.c
	gcc Source_Files/frontend.c -pthread -o Executable_Files/frontend

backend: Source_Files/backend.c
	gcc Source_Files/backend.c Source_Files/users_lib.o -pthread -o Executable_Files/backend

clean:
	rm Executable_Files/token.txt Executable_Files/frontend Executable_Files/backend
