#include "debug.h"
#include <stdio.h>
FILE * debugging_file;
static int d;
void debug_init(int setting){
    d = setting;
    if(setting == FILE_DEBUG) {
        debugging_file = fopen("debugging.txt", "w");
    }
}
void debug(char * etiqueta, int codigo,char * mensaje, int extra){
    if(d == STDOUT_DEBUG)
        printf("DEBUG, %s: Código: %d. Mensaje: %s, %d\n", etiqueta, codigo, mensaje, extra);
    if(d == FILE_DEBUG) {
        fprintf(debugging_file, "DEBUG, %s: Código: %d. Mensaje: %s, %d\n", etiqueta, codigo, mensaje, extra);
    }
}
void debug_file_close(){
    fclose(debugging_file);
}
