#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define _ERRO_ARGUMENTO -10
#define _ERRO_ARQUIVO   -20

int main(int argc, char *argv[])
{
    FILE* arquivo;
    char linha[512];

    unsigned int tam;
    unsigned long endr;

    /* Verifica argumentos */
    if(argc<2) {
        fprintf(stderr, "Argumento faltando: nome de arquivo\n");
        return _ERRO_ARGUMENTO;
    }
    
    arquivo = fopen(argv[1], "r");

    if(!arquivo){
        fprintf(stderr, "Erro ao abrir arquivo %s: %s\n", argv[1], strerror(errno));
        return _ERRO_ARQUIVO;
    }

    while(fgets(linha, 512, arquivo) != NULL) {
        if(linha[1]=='S' || linha[1]=='L' || linha[1]=='M') {
            sscanf(linha+3, "%lx,%u", &endr, &tam);
      
            printf("%c %lx,%u\n", linha[1], endr, tam);
        }
    }
    fclose(arquivo);
    
    return 0;
}

    
