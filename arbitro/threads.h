#ifndef THREADS_H
#define THREADS_H

#include "includes.h"

DWORD WINAPI connectionListener(LPVOID param);
DWORD WINAPI playerHandler(LPVOID index);

#endif // THREADS_H