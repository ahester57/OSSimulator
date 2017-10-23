/*
$Id: filehelper.c,v 1.1 2017/10/23 07:27:24 o1-hester Exp o1-hester $
$Date: 2017/10/23 07:27:24 $
$Revision: 1.1 $
$Log: filehelper.c,v $
Revision 1.1  2017/10/23 07:27:24  o1-hester
Initial revision

Revision 1.2  2017/10/10 20:18:57  o1-hester
formats

Revision 1.1  2017/10/04 07:44:59  o1-hester
Initial revision

$Author: o1-hester $
*/
#include <stdio.h>
#include <stdlib.h>
#include "filehelper.h"

// Set a char** to a list of strings (by line) from a file
int
setArrayFromFile(const char* filename, char** list)
{
	FILE* fp;
	fp = fopen(filename, "r");
	if (fp == NULL) {
		return -1;
	}
	int n = 0;
	char* line = malloc(LINESIZE*sizeof(char));
	if (line == NULL)
		return -1;
	rewind(fp);
	while (fgets(line, LINESIZE, (FILE*)fp))
	{
		list[n] = malloc(LINESIZE*sizeof(char));
		// copy line into array, remove newline
		memcpy(list[n], line, LINESIZE);
		list[n][strcspn(list[n], "\n")] = '\0';
		n++;
	}
	free(line);
	fclose(fp);
	if (errno != 0)
		return -1;
	return 0;
}

// Count how  many lines a file has
int
countLines(const char* filename)
{
	FILE* fp;
	fp = fopen(filename, "r");
	if (fp == NULL) {
		return -1;
	}
	int n = 0;
	char* line = malloc(LINESIZE*sizeof(char));
	if (line == NULL)
		return -1;
	rewind(fp);
	while (fgets(line, LINESIZE, (FILE*)fp))
	{
		n++;
	}
	free(line);
	fclose(fp);
	if (errno != 0)
		return -1;
	return n;
}

// writes to file, returns -1 on error, 0 otherwise
int
writeToFile(const char* filename, const long pid,
		const int index, const char* text)
{
	FILE* fp;
	fp = fopen(filename, "a+");
	if (fp == NULL) {
		return -1;
	}
	fprintf((FILE*)fp, "%ld\t%d\t%s\n", pid, index, text);
	fclose(fp);
	return 0;
}
