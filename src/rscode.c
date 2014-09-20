/* rscode.c - rscode(1) source code
 * rscode - en- or decode funky chars in filenames like rsync would do
 * See README for contact-, COPYING for license information.  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>

#include <err.h>

#include <getopt.h>

/* If no arguments are present, or if -f is given, we read file
 *   names from stdin (or from the file specified by -f).
 *   Each line corresponds to one file name, unless -0 is given,
 *   in which case file names are expected to be \0 terminated.
 * If arguments are provided, stdin is ignored, and each argument
 *   corresponds to one file name
 * If both arguments are present and -f is given, we bail out. */

static bool s_decode;
static FILE *s_inpstr;
static bool s_nulterm;


static void encode(const char *s);
static void decode(const char *s);
static int isodigit(int c);
static bool iseseq(const char *s, bool strict);
static int parseeseq(const char *s);
static void process_args(int *argc, char ***argv);
static void init(int *argc, char ***argv);
static void usage(FILE *str, const char *a0, int ec);


static void
process_args(int *argc, char ***argv)
{
	char *a0 = (*argv)[0];

	for (int ch; (ch = getopt(*argc, *argv, "edf:0hV")) != -1;) {
		switch (ch) {
		case 'e':
		case 'd':
			s_decode = ch == 'd';
			break;
		case 'f':
			if (strcmp(optarg, "-") == 0)
				s_inpstr = stdin;
			else if (!(s_inpstr = fopen(optarg, "r")))
				err(EXIT_FAILURE, "fopen %s", optarg);
		case '0':
			s_nulterm = true;
			break;
		case 'h':
			usage(stdout, a0, EXIT_SUCCESS);
			break;
		case 'V':
			puts(PACKAGE_NAME " v" PACKAGE_VERSION);
			exit(EXIT_SUCCESS);
		default:
			usage(stderr, a0, EXIT_FAILURE);
		}
	}

	*argc -= optind;
	*argv += optind;

	if (*argc && s_inpstr)
		errx(EXIT_FAILURE, "error: arguments and -f present, wat do?!");
	
	if (!*argc && !s_inpstr)
		s_inpstr = stdin;

	if (*argc && !s_decode && s_nulterm) {
		warnx("warning: ignoring -0 because arguments are provided");
		s_nulterm = false;
	}
}

static void
init(int *argc, char ***argv)
{
	process_args(argc, argv);
}

static void
encode(const char *s)
{
	size_t rem = strlen(s);
	while (*s) {
		if (iseseq(s, false) || !isprint(*s) || !isascii(*s))
			fprintf(stdout, "\\#%03o", (unsigned char)*s);
		else
			putchar(*s);

		s++;
	}
	putchar('\n');
}

static void
decode(const char *s)
{
	size_t rem = strlen(s);
	while (*s) {
		if (iseseq(s, true)) {
			putchar(parseeseq(s));
			s += 4;
		} else
			putchar(*s);

		s++;
	}

	putchar(s_nulterm ? '\0' : '\n');
}

static int
isodigit(int c)
{
	return c >= '0' && c <= '7';
}

/* tell whether the string pointed to by s starts with a valid,
 * or valid-looking rsync-style escape sequence, i.e. \#ooo
 * if ``strict'' is false, we allow '0'-'9' as "octal" digits
 * (because that's what rsync does). Otherwise, only real octal
 * digits are tolerated ('0'-'7') */
static bool
iseseq(const char *s, bool strict)
{
	if (*s != '\\')
		return false;

	size_t len = strlen(s);
	if (len < 5)
		return false;

	if (s[1] != '#')
		return false;
	
	int (*fn)(int) = strict ? isodigit : isdigit;

	return fn(s[2]) && fn(s[3]) && fn(s[4]);
}

/* Parse the rsync-style escape sequence at the beginning of ``s''.
 * This is only safe to call if iseseq(s, true) returned true. */
static int
parseeseq(const char *s)
{
	char tmp[4];
	memcpy(tmp, s+2, 3);
	tmp[3] = '\0';
	return (int)strtol(tmp, NULL, 8);
}


static void
usage(FILE *str, const char *a0, int ec)
{
	#define I(STR) fputs(STR "\n", str)
	I("==================================================================");
	I("==           rscode - en/decode filenames rsync-style           ==");
	I("==================================================================");
	fprintf(str, "usage: %s [-de0hV -f <file>] [ <input> ... ]\n", a0);
	I("");
	I("Converts funky chars the way rsync does for filenames.");
	I("The encoding process translates nonprintable characters");
	I("  into ``\\#ooo'', were ``ooo'' is the octal codepoint value.");
	I("  In literal ``\\#ooo'' tokens, the backslash is in turn encoded");
	I("  in the same way, i.e. ''\\#123'' becomes ``\\#134#123'', even if");
	I("  ``ooo'' wasn't a legitimate octal number (i.e. may contain 8, 9)");
	I("The decoding process does the inverse operation.");
	I("");
	I("Parameter summary:");
	I("\t-d: Decode.");
	I("\t-e: Encode.  This is the default.");
	I("\t-f <file>: Read input to en/decode from <file> instead of stdin");
	I("\t-0: When encoding, expect input strings to be \\0-terminated");
	I("\t    rather than by \\n.  When decoding, terminate output strings");
	I("\t    by \\0 instead of \\n");
	I("\t-h: Display this usage statement and terminate");
	I("\t-V: Print version information");
	I("");
	I("If no arguments and no -f is given, we read from stdin.");
	I("If arguments are given, we ignore stdin and use the args as input");
	I("");
	I("Version: " PACKAGE_VERSION);
	I("(C) 2014, Timo Buhrmester (contact: #fstd @ irc.freenode.org)");
	#undef I
	exit(ec);
}


int
main(int argc, char **argv)
{
	init(&argc, &argv);

	void (*encdec)(const char *) = s_decode ? decode : encode;


	if (argc) {
		while (argc--) {
			encdec(argv[0]);
			argv++;
		}

		return EXIT_SUCCESS;
	}

	size_t bufsz = 1024;
	size_t bufpos = 0;
	char *buf = malloc(bufsz);

	if (!buf)
		err(EXIT_FAILURE, "malloc");

	int c;
	while ((c = getc(s_inpstr)) != EOF) {
		if (bufpos >= bufsz) {
			char *nbuf = realloc(buf, bufsz *= 2);
			if (!nbuf)
				err(EXIT_FAILURE, "realloc");
			buf = nbuf;
		}

		if (c == (!s_decode && s_nulterm ? '\0' : '\n')) {
			buf[bufpos] = '\0';
			encdec(buf);
			bufpos = 0;
		} else
			buf[bufpos++] = c;
	}

	if (ferror(s_inpstr))
		err(EXIT_FAILURE, "fgetc");

	if (bufpos) {
		warnx("warning: terminator missing on last input entry");
		buf[bufpos] = '\0';
		encdec(buf);
	}

	return EXIT_SUCCESS;
}
