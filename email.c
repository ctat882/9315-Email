/*
 * src/tutorial/complex.c
 *
 ******************************************************************************
  This file contains routines that can be bound to a Postgres backend and
  called by the backend in the process of processing queries.  The calling
  format for these routines is dictated by Postgres architecture.
******************************************************************************/

#include "postgres.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

#include "fmgr.h"
#include "libpq/pqformat.h"		/* needed for send/recv functions */

#define MAX_CHARS  128
#define TRUE 1
#define FALSE 0

PG_MODULE_MAGIC;

typedef struct EmailAddress
{
	int32		length;              // Variable length data types need this
	char		local[MAX_CHARS];    // the 'local' string
	char		domain[MAX_CHARS];   // the 'domain' string
}	EmailAddress;

/*
 * Since we use V1 function calling convention, all these functions have
 * the same signature as far as C is concerned.  We provide these prototypes
 * just to forestall warnings when compiled with gcc -Wmissing-prototypes.
 */

/* Functions concerning the reading and displaying of type EmailAddress */ 
Datum		email_in(PG_FUNCTION_ARGS);
Datum		email_out(PG_FUNCTION_ARGS);
Datum		email_recv(PG_FUNCTION_ARGS);
Datum		email_send(PG_FUNCTION_ARGS);

/* Functions concerning the operators on EmailAddress */ 

Datum		email_lt(PG_FUNCTION_ARGS);
Datum		email_le(PG_FUNCTION_ARGS);
Datum		email_eq(PG_FUNCTION_ARGS);
Datum		email_ne(PG_FUNCTION_ARGS);
Datum		email_ge(PG_FUNCTION_ARGS);
Datum		email_gt(PG_FUNCTION_ARGS);
Datum		email_cmp(PG_FUNCTION_ARGS);
Datum		email_de(PG_FUNCTION_ARGS);
Datum		email_dne(PG_FUNCTION_ARGS);


/* Function Prototypes */

int isValidCharacter (char c);
int getLocalStringEnd (char *str, int len);
int getDomainStringEnd (int start, char *str, int len);
int checkLocalIsValid (char *local);
int checkDomainIsValid (char *domain);
int regexMatch (char *string, char *pattern);
int email_cmp_internal(EmailAddress * a, EmailAddress * b);
int domain_cmp_internal(EmailAddress * a, EmailAddress * b);
void print_error (char *string);

/*****************************************************************************
 * Input/Output functions
 *****************************************************************************/

PG_FUNCTION_INFO_V1(email_in);

Datum
email_in(PG_FUNCTION_ARGS)
{
   // Get input string
	char *str = PG_GETARG_CSTRING(0);
	/* Create pointer to EmailAddress data structure, allocating
	   memory to it. Then set the length element of the structure
	   using the macro SET_VARSIZE.
	   NOTE: VARHDRSZ = sizeof(int32) = 4 bytes.
	*/
	EmailAddress *result = (EmailAddress *) palloc(VARHDRSZ + 256);
	SET_VARSIZE(result,VARHDRSZ + 256);

	/* Scan string and split into substrings local and domain*/
	
	int str_len = strlen(str);    // Get the length of the input string
	int i = 0;                    // array index placeholder
	// Iterate through the input string to the '@' symbol.
	i = getLocalStringEnd(str, str_len);
	// If i == 0, then there is no local part.
	if (!i) print_error(str);
	
	// create temporary local string
	char local[i];
	memcpy(local, &str[0],i - 1);
	local[i - 1] = '\0';
	
   // store the position of the '@' (one to the right of)
	int at_pos = i;
	i = getDomainStringEnd (i,str,str_len);
	// If an error occured in getDomainStringEnd, send error.
	if (!i) print_error(str);

	// Create temporary domain string and copy content.
	char domain[i - (at_pos + 1)];
	memcpy(domain, &str[at_pos],i - 1);
	domain[i - 1] = '\0';

   /* Validate local and domain strings */
   if( ! checkLocalIsValid(local)) print_error(local);
   if( ! checkDomainIsValid(domain)) print_error(domain);
	/* Initialise EmailAddress data structure and copy in content */ 	
	memset(result->local,0,MAX_CHARS);
	memset(result->domain,0,MAX_CHARS);
	strcpy(result->local,local);
	strcpy(result->domain,domain);
	PG_RETURN_POINTER(result);
}


/**
   Send error message to psql, and print error message.
   @PARAMS string: the string containing the error
*/
void print_error (char *string) {
   ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("invalid input syntax for email address: \"%s\"",
						string)));
   return;
}


/**
   Verify that email address rules are satisfied for local.
   @PARAMS local: The string to validate.
   @RETURN: Returns TRUE (1) if the string is valid,
            FALSE (0) otherwise. 
*/
int checkLocalIsValid (char *local) {
	int valid = TRUE;    // valid is set to TRUE by default.

	//TODO: insert regex tests here:
//	if(! regexMatch(local,"((^[a-zA-Z])+(([-]*[a-zA-z0-9]))*(([\.])([a-zA-Z])+(([-]*[a-zA-z0-9]))*)*)$")) valid = FALSE;

	if (! regexMatch(local,"^[A-Z]")) valid = FALSE;
//	if (regexMatch(local,"\b[A-Z]+[A-Z0-9]*\b")) valid = FALSE;

	return valid;
}


/**
   Verify that email address rules are satisfied for domain.
   @PARAMS domain: The string to validate.
   @RETURN: Returns TRUE (1) if the string is valid,
            FALSE (0) otherwise. 
*/
int checkDomainIsValid (char *domain) {
	int valid = TRUE;
	// TODO: Put your domain validation regex here:

//	if (! regexMatch(local,"((^[a-zA-Z])+(([-]*[a-zA-z0-9]))*(([\.])([a-zA-Z])+(([-]*[a-zA-z0-9]))*)+)$")) valid = FALSE;
	
	return valid;
}
/**
	Regular Expression Match. Uses extended regex and ignores case.
	@PARAMS  string: The string to check.
	         pattern: The regex pattern to check against.
	@RETURN: If there is a match, the function will return TRUE (int 1),
	         otherwise FALSE (0).
*/
int regexMatch (char *string, char *pattern) {
	int status;					// determine if error in compilation or execution
	regex_t regex;				// store compiled regex
	int match = FALSE;
	// Compile regular expression
	status = regcomp(&regex,pattern, REG_EXTENDED|REG_ICASE);
	if (status) return (-1); 	// if not 0, error occured;
	// Execute regular expression
	status = regexec(&regex, string, (size_t) 0, NULL, 0);
	regfree(&regex);
	// if status == 0, then there is a match.
	if (!status) match = TRUE;
	return match;	
}

/**
	Does basic character validation for the domain substring.
	If an error is detected, 0 is returned.
*/
int getDomainStringEnd (int start, char *str, int len) {
	int i = start;
	int end_pos = 0;
	int error = FALSE;
	char c = 0;
	for (i = i; i < len; i += 1) {
		c = str[i];
		printf("%c",c);
		if (!isValidCharacter(c)) {
			printf("Invalid Character\n");
			error = TRUE;
			break;
		}
		else if (c == '@') {
			printf("Invalid Character\n");
			error = TRUE;
			break;
		}		
	}
	if (!error) {
		end_pos = i;
	}
	return end_pos;
}


/**
	Given a string, this function iterates over it until
	the first '@' symbol is found, return it's position.
	If an error is detected, 0 will be returned.
*/

int getLocalStringEnd (char *str, int len) {
	int i = 0;
	int end_pos = 0;
	int error = FALSE;
	char c = 0;
	for (i = 0; i < len && c != '@'; i++)
	{
		c = str[i];
		printf("%c",c);
		if (!isValidCharacter(c)) {
			printf("Invalid Character\n");
			error = TRUE;
			break;
		}
		else if (i == 0 && c == '@') {
			printf("No Local\n");
			error = TRUE;
			break;
		}			  	
	}
	if (c == '@' && i >= len) {
		printf("No Domain\n");
		error = TRUE;
	}
	if (!error) {
		end_pos = i;
	}
	return end_pos;
}

/**
   Checks ASCII values for invalid characters.
   @PARAMS c: the character to check.
   @RETURN: Returns TRUE (1) if character is valid,
            FALSE (0) otherwise.
*/
int isValidCharacter (char c) {
	int valid = TRUE;
	if ( c < 48 && c != 46 && c != 45) valid = FALSE;	
	else if (c > 57 && c < 64)	valid = FALSE;
	else if (c > 90 && c < 97)	valid = FALSE;
	else if (c > 122)	valid = FALSE;
	return valid;
}


PG_FUNCTION_INFO_V1(email_out);

Datum
email_out(PG_FUNCTION_ARGS)
{
	EmailAddress    *email = (EmailAddress *) PG_GETARG_POINTER(0);
	char	   *result;

	result = (char *) palloc(256);
	snprintf(result, 256, "%s@%s", email->local, email->domain);
	PG_RETURN_CSTRING(result);
}

/*****************************************************************************
 * Binary Input/Output functions
 *
 * These are optional.
 *****************************************************************************/

PG_FUNCTION_INFO_V1(email_recv);

Datum
email_recv(PG_FUNCTION_ARGS)
{
	StringInfo	buf = (StringInfo) PG_GETARG_POINTER(0);
	EmailAddress    *result = (EmailAddress *) palloc(VARHDRSZ + 256);

	const char *local = pq_getmsgstring(buf);
	const char *domain = pq_getmsgstring(buf);

   SET_VARSIZE(result,VARHDRSZ + 256);
   memset(result->local,0,MAX_CHARS);
	memset(result->domain,0,MAX_CHARS);

	strcpy(result->local,local);
	strcpy(result->domain,domain);
	PG_RETURN_POINTER(result);
}

PG_FUNCTION_INFO_V1(email_send);

Datum
email_send(PG_FUNCTION_ARGS)
{
	EmailAddress    *email = (EmailAddress *) PG_GETARG_POINTER(0);
	StringInfoData buf;

	pq_begintypsend(&buf);
	pq_sendstring(&buf, email->local);
	pq_sendstring(&buf, email->domain);
	PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

/*****************************************************************************
 * New Operators
 *
 * A practical Complex datatype would provide much more than this, of course.
 *****************************************************************************/

/*PG_FUNCTION_INFO_V1(complex_add);*/

/*Datum*/
/*complex_add(PG_FUNCTION_ARGS)*/
/*{*/
/*	Complex    *a = (Complex *) PG_GETARG_POINTER(0);*/
/*	Complex    *b = (Complex *) PG_GETARG_POINTER(1);*/
/*	Complex    *result;*/

/*	result = (Complex *) palloc(sizeof(Complex));*/
/*	result->x = a->x + b->x;*/
/*	result->y = a->y + b->y;*/
/*	PG_RETURN_POINTER(result);*/
/*}*/


/*****************************************************************************
 * Operator class for defining B-tree index
 *
 * It's essential that the comparison operators and support function for a
 * B-tree index opclass always agree on the relative ordering of any two
 * data values.  Experience has shown that it's depressingly easy to write
 * unintentionally inconsistent functions.	One way to reduce the odds of
 * making a mistake is to make all the functions simple wrappers around
 * an internal three-way-comparison function, as we do here.
 *****************************************************************************/

/*#define Mag(c)	((c)->x*(c)->x + (c)->y*(c)->y)*/

/*static int*/
/*email_cmp_internal(EmailAddress * a, EmailAddress * b)*/
/*{*/
/*	double		amag = Mag(a),*/
/*				bmag = Mag(b);*/

/*	if (amag < bmag)*/
/*		return -1;*/
/*	if (amag > bmag)*/
/*		return 1;*/
/*	return 0;*/
/*}*/

/**
   Compares two EmailAddresses, domain first then local.
*/
int email_cmp_internal(EmailAddress * a, EmailAddress * b)
{
	int result = 0;

	if (strcasecmp(a->domain,b->domain) < 0)
		result = -1;
	else if (strcasecmp(a->domain,b->domain) > 0)
		result = 1;
	else if (strcasecmp(a->domain,b->domain) == 0) {
      if (strcasecmp(a->local,b->local) < 0)
         result = -1;
      else if (strcasecmp(a->local,b->local) > 0)
         result = 1;
	}
	return result;
}

/**
   Compares two EmailAddress' domains.
*/
int domain_cmp_internal(EmailAddress * a, EmailAddress * b)
{
   int result = 0;
   if (strcasecmp(a->domain,b->domain) < 0)
		result = -1;
	else if (strcasecmp(a->domain,b->domain) > 0)
		result = 1;

   return result;
}


PG_FUNCTION_INFO_V1(email_lt);

Datum
email_lt(PG_FUNCTION_ARGS)
{
	EmailAddress    *a = (EmailAddress *) PG_GETARG_POINTER(0);
	EmailAddress    *b = (EmailAddress *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(email_cmp_internal(a, b) < 0);
}

PG_FUNCTION_INFO_V1(email_le);

Datum
email_le(PG_FUNCTION_ARGS)
{
	EmailAddress    *a = (EmailAddress *) PG_GETARG_POINTER(0);
	EmailAddress    *b = (EmailAddress *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(email_cmp_internal(a, b) <= 0);
}

PG_FUNCTION_INFO_V1(email_eq);

Datum
email_eq(PG_FUNCTION_ARGS)
{
	EmailAddress    *a = (EmailAddress *) PG_GETARG_POINTER(0);
	EmailAddress    *b = (EmailAddress *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(email_cmp_internal(a, b) == 0);
}



PG_FUNCTION_INFO_V1(email_ge);

Datum
email_ge(PG_FUNCTION_ARGS)
{
	EmailAddress    *a = (EmailAddress *) PG_GETARG_POINTER(0);
	EmailAddress    *b = (EmailAddress *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(email_cmp_internal(a, b) >= 0);
}

PG_FUNCTION_INFO_V1(email_gt);

Datum
email_gt(PG_FUNCTION_ARGS)
{
	EmailAddress    *a = (EmailAddress *) PG_GETARG_POINTER(0);
	EmailAddress    *b = (EmailAddress *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(email_cmp_internal(a, b) > 0);
}

PG_FUNCTION_INFO_V1(email_cmp);

Datum
email_cmp(PG_FUNCTION_ARGS)
{
	EmailAddress    *a = (EmailAddress *) PG_GETARG_POINTER(0);
	EmailAddress    *b = (EmailAddress *) PG_GETARG_POINTER(1);

	PG_RETURN_INT32(email_cmp_internal(a, b));
}


/* The Not Equal function <> */

PG_FUNCTION_INFO_V1(email_ne);

Datum
email_ne(PG_FUNCTION_ARGS)
{
	EmailAddress    *a = (EmailAddress *) PG_GETARG_POINTER(0);
	EmailAddress    *b = (EmailAddress *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(email_cmp_internal(a, b) != 0);
}


/* The domain equal function ~ */

PG_FUNCTION_INFO_V1(email_de);

Datum
email_de(PG_FUNCTION_ARGS)
{
	EmailAddress    *a = (EmailAddress *) PG_GETARG_POINTER(0);
	EmailAddress    *b = (EmailAddress *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(domain_cmp_internal(a, b) == 0);
}

/* The domain not equal function !~ */

PG_FUNCTION_INFO_V1(email_dne);

Datum
email_dne(PG_FUNCTION_ARGS)
{
	EmailAddress    *a = (EmailAddress *) PG_GETARG_POINTER(0);
	EmailAddress    *b = (EmailAddress *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(domain_cmp_internal(a, b) != 0);
}



