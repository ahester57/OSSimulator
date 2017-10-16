/*
$Id: filehelper.h,v 1.1 2017/10/04 07:44:59 o1-hester Exp o1-hester $
$Date: 2017/10/04 07:44:59 $
$Revision: 1.1 $
$Log: filehelper.h,v $
Revision 1.1  2017/10/04 07:44:59  o1-hester
Initial revision

$Author: o1-hester $
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
