#ifndef NODEBUG /* NODEBUG inhibits debugging */
#ifndef DEBUG		/* and if DEBUG not already defined */
#ifndef MINSIZE /* MINSIZE inhibits debugging */
#ifndef DEBUG
//#define DEBUG
#define EKERMITDIRECTDEBUG
#endif /* DEBUG */
#endif /* MINSIZE */
#endif /* DEBUG */
#endif /* NODEBUG */
#include <stdio.h>

#define COLOR_OFF "\x1B[0m"
#define COLOR_RED "\x1B[0;91m"
#define COLOR_GREEN "\x1B[0;92m"
#define COLOR_YELLOW "\x1B[0;93m"
#define COLOR_BLUE "\x1B[0;94m"
#define COLOR_MAGENTA "\x1B[0;95m"
#define COLOR_BOLDGRAY "\x1B[1;30m"
#define COLOR_BOLDWHITE "\x1B[1;37m"

#ifdef DEBUG /* Debugging included... */
/* dodebug() function codes... */
#define DB_OPN 1	/* Open log */
#define DB_LOG 2	/* Write label+string or int to log */
#define DB_MSG 3	/* Write message to log */
#define DB_CHR 4	/* Write label + char to log */
#define DB_PKT 5	/* Record a Kermit packet in log */
#define DB_CLS 6	/* Close log */
#define DB_OPNT 7 /* Open log with time stamps */
#define DB_HEX 8	/* Record a buffer in hex */

int dodebug(int, UCHAR *, UCHAR *, long); /* Prototype */
/*
  dodebug() is accessed throug a macro that:
   . Coerces its args to the required types.
   . Accesses dodebug() directly or thru a pointer according to context.
   . Makes it disappear entirely if DEBUG not defined.
*/
#ifdef KERMIT_C
/* In kermit.c we debug only through a function pointer */
#define debug(a, b, c, d) \
	if (k->dbf)             \
	k->dbf(a, (UCHAR *)b, (UCHAR *)c, (long)(d))
#else /* KERMIT_C */
/* Elsewhere we can call the debug function directly */
#define debug(a, b, c, d) dodebug(a, (UCHAR *)b, (UCHAR *)c, (long)(d))
#endif /* KERMIT_C */

#ifdef EKERMITDIRECTDEBUG
#define PRINT_DDEBUG(msg)                 \
	printf(COLOR_GREEN "[DDBG]" COLOR_OFF); \
	printf(msg);                            \
	printf("\n");
#define PRINT_DDEBUG_ARG(msg, val)        \
	printf(COLOR_GREEN "[DDBG]" COLOR_OFF); \
	printf(msg, val);                       \
	printf("\n");
#define PRINT_DDEBUG_TIMESTAMP(msg)       \
	printf(COLOR_GREEN "[DDBG]" COLOR_OFF); \
	printf(msg);                            \
	printf("\n");
#endif

#else /* Debugging not included... */

#define debug(a, b, c, d)
#define PRINT_DDEBUG(msg)                 \
	printf(COLOR_GREEN "[DDBG]" COLOR_OFF); \
	printf(msg);                            \
	printf("\n");
#define PRINT_DDEBUG_ARG(msg, val)        \
	printf(COLOR_GREEN "[DDBG]" COLOR_OFF); \
	printf(msg, val);                       \
	printf("\n");
#define PRINT_DDEBUG_TIMESTAMP(msg)       \
	printf(COLOR_GREEN "[DDBG]" COLOR_OFF); \
	printf(msg);                            \
	printf("\n");

#endif /* DEBUG */
