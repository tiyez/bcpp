

#include <stdio.h>
#include <string.h>

int main () {
	const char	*filename = "main.c";

	printf ("%.*so\n", (int) strlen (filename) - 1, filename);
}



