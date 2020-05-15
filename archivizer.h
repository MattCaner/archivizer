#ifndef ARCHIVIZER_H
#define ARCHIVIZER_H
#include"finfo.h"

//int buffer_size;

void pack(const char* archivePath, const char* directoryPath);
void unpack(const char* archivePath, const char* directoryPath);

void packDirectory(int archiveFD, const char* path, const char* name);
void unpackDirectory(int archiveFD, const char* path, const char* name);

#endif