#ifndef PROYECTO_PROTOS_DEBUG_H
#define PROYECTO_PROTOS_DEBUG_H
#include <stdio.h>
#define STDOUT_DEBUG 1
#define FILE_DEBUG 2
FILE * debugging_file;
void debug_init(int setting);
void debug_file_close();
void debug(char * etiqueta, int codigo,char * mensaje, int extra);
#endif //PROYECTO_PROTOS_DEBUG_H
