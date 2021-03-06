#include <stdio.h>
#include <stdlib.h>

#include <termios.h>

void input_clean_stdin (void) {

	int c = 0;
	do {
		c = getchar ();
	} while (c != '\n' && c != EOF);

}

// returns a newly allocated c string
char *input_get_line (void) {

	size_t lenmax = 128, len = lenmax;
	char *line = (char *) malloc (lenmax), *linep = line;

	if (line) {
		int c = 0;

		for (;;) {
			c = fgetc (stdin);

			if (c == EOF || c == '\n') break;

			if (--len == 0) {
				len = lenmax;
				char * linen = (char *) realloc (linep, lenmax *= 2);

				if(linen == NULL) {
					free (linep);
					return NULL;
				}

				line = linen + (line - linep);
				linep = linen;
			}

			if ((*line++ = c) == '\n') break;
		}

		*line = '\0';
	}

	return linep;

}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"

unsigned int input_password (char *password) {

	unsigned int retval = 1;

	struct termios old_term = { 0 }, new_term = { 0 };

	/* Turn echoing off and fail if we can't. */
	if (!tcgetattr (fileno (stdin), &old_term)) {
		new_term = old_term;
		new_term.c_lflag &= ~ECHO;
		if (!tcsetattr (fileno (stdin), TCSAFLUSH, &new_term) != 0) {
			(void) scanf ("%128s", password);

			/* Restore terminal. */
			(void) tcsetattr (fileno (stdin), TCSAFLUSH, &old_term);

			retval = 0;
		}
	}

	return retval;

}

#pragma GCC diagnostic pop