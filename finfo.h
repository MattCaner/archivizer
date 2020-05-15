#ifndef FINFO_H
#define FINFO_H

#include<stdio.h>
#include<stdlib.h>
#include<dirent.h>
#include<string.h>
#include<fcntl.h>
#include<unistd.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<time.h>
#include<sys/mman.h>

struct finfo{
	int type;			//0 = file, 1 = directory, 2 = end of a directory
	int name_size;		//0 in case of end of directory
	mode_t permissions;
	long content_size;	//in case of a file - size in bytes, in case of a directory = 0, in case of terminator = amount of should-be entries in a directory
};

#endif