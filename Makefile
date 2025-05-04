CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -fsanitize=address -g -Wl,-z,stack-size=16777216

mysh: mysh.c
	$(CC) $(CFLAGS) -o mysh mysh.c

test: mysh
	echo "Running basic commands test..."
	./mysh test_basic_commands.mysh
	echo "Running redirection test..."
	./mysh test_redirection.mysh
	echo "Running pipe test..."
	./mysh test_pipe.mysh
	echo "Running wildcard test..."
	./mysh test_wildcard.mysh
	echo "Running builtin commands test..."
	./mysh test_builtin_commands.mysh

clean:
	rm -f mysh
