#ifndef UTILS_H
#define UTILS_H

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include "../data.h"

void fecharPipe(HANDLE hPipe);
void getAndPrintLetrasFromSharedMemory();

#endif