/*
 * indice.h - Indice parcial (esparso) de 2 niveis para produtos.bin.
 *
 * Nivel 1: uma entrada (chave, n do registro) a cada K registros do arquivo de dados.
 * Nivel 2: uma entrada (chave, n da entrada do nivel 1) a cada M entradas do nivel 1.
 *
 * Arquivo produtos.idx:  [cabecalho][nivel 2][nivel 1]   (entradas de 16 bytes)
 *
 * A consulta faz busca binaria DIRETO NO ARQUIVO DE INDICE (fseek/fread), sem
 * carregar o indice na RAM; so o cabecalho fica em memoria.
 *   nivel 2 (busca binaria) -> bloco de ate M entradas do nivel 1 (busca binaria)
 *   -> bloco de ate K registros no arquivo de dados (varredura sequencial).
 */
#ifndef INDICE_H
#define INDICE_H

#include "produto.h"

#ifndef INTERVALO_NIVEL1
#define INTERVALO_NIVEL1 100   /* K */
#endif
#ifndef INTERVALO_NIVEL2
#define INTERVALO_NIVEL2 100   /* M */
#endif

typedef struct {
    char    magic[8];
    int64_t k;            /* intervalo do nivel 1 */
    int64_t m;            /* intervalo do nivel 2 */
    int64_t n_registros;  /* registros do arquivo de dados quando o indice foi gerado */
    int64_t n_nivel1;
    int64_t n_nivel2;
} CabecalhoIndice;

typedef struct {
    int64_t chave;
    int64_t pos;
} EntradaIndice;

typedef struct {
    CabecalhoIndice cab;
    FILE *arq;            /* arquivo de indice aberto para leitura */
} Indice;

/* Contador de entradas de indice lidas do disco. */
extern long indice_leituras;

int  indice_construir(const char *arq_dados, const char *arq_indice);
/* Retorna 0 se carregou um indice valido e consistente com o arquivo de dados. */
int  indice_carregar(Indice *ix, const char *arq_indice, long n_registros_dados);
void indice_fechar(Indice *ix);
/* Retorna o n do registro encontrado em 'dados' (preenche *out) ou -1. */
long indice_buscar(const Indice *ix, FILE *dados, int64_t chave, Produto *out);

#endif
