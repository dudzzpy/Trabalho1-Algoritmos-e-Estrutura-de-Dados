#ifndef ACESSOS_H
#define ACESSOS_H

#include <stdint.h>
#include <stdio.h>

/* Campos de tamanho fixo: 12+19+16+12+12+40+1+1 = 113 bytes. */
#define AC_TAM_ID 12
#define AC_TAM_DATA 19
#define AC_TAM_TIPO 16
#define AC_TAM_PROD 12
#define AC_TAM_USUARIO 12
#define AC_TAM_SESSAO 40
#define AC_TAM_REGISTRO 113
#define AC_ID_MAX 999999999999LL

#ifndef ACESSOS_K
#define ACESSOS_K 100
#endif
#ifndef ACESSOS_M
#define ACESSOS_M 100
#endif

typedef struct {
    int64_t id_evento;
    char data_hora[AC_TAM_DATA + 1];
    char tipo[AC_TAM_TIPO + 1];
    int64_t id_produto;
    int64_t id_usuario;
    char sessao[AC_TAM_SESSAO + 1];
    char status;
} Acesso;

/* Indice: [cabecalho de 48 bytes][nivel 2][nivel 1]. */
typedef struct {
    char magic[8];
    int64_t k, m, n_registros, n_nivel1, n_nivel2;
} CabecalhoIndiceAcesso;

typedef struct {
    int64_t chave;
    int64_t pos; /* nivel 2: indice da entrada do nivel 1; nivel 1: numero do registro */
} EntradaIndiceAcesso;

typedef struct {
    char dados[512];
    char indice[512];
    int64_t quantidade;
    FILE *arquivo_dados;
    FILE *arquivo_indice;
    CabecalhoIndiceAcesso cab;
    int indice_carregado;
} TabelaAcessos;

int acesso_valido(const Acesso *a);
int acesso_gravar(FILE *f, const Acesso *a);
int acesso_ler(FILE *f, int64_t pos, Acesso *a);
int acesso_abrir(TabelaAcessos *t, const char *dir);
void acesso_fechar(TabelaAcessos *t);
int acesso_reconstruir_indice(TabelaAcessos *t);
int64_t acesso_buscar_binaria(TabelaAcessos *t, int64_t id, Acesso *saida);
int64_t acesso_buscar_indice(TabelaAcessos *t, int64_t id, Acesso *saida);
int acesso_listar(TabelaAcessos *t, int64_t limite);
int acesso_inserir(TabelaAcessos *t, const Acesso *novo); /* 0 ok, 1 duplicado, -1 erro */
int acesso_remover(TabelaAcessos *t, int64_t id);       /* 0 ok, 1 nao existe, -1 erro */
int acesso_criar_diretorio(const char *dir);
void acesso_imprimir(const Acesso *a);

#endif
