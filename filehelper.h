/*
$Id$
$Date$
$Revision$
$Log$
$Author$
*/
#ifndef FILEHELPER_H_
#define FILEHELPER_H_

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#define LINESIZE 256

int setArrayFromFile(const char* filename, char** list);
int countLines(const char* filename);
int writeToFile(const char* filename, long pid, int index, const char* text); 

#endif
