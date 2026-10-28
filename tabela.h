/*
 * tabela.h - Operacoes sobre o arquivo de produtos (sequencial-indexado).
 *
 * Arquivos:
 *   produtos.bin  principal, ordenado por id_produto
 *   produtos.ovf  area de overflow: insercoes recentes (nao ordenadas, pequeno)
 *   produtos.idx  indice parcial do arquivo principal
 *
 * Estrategias:
 *   REMOCAO   logica: o status do registro vira 'R' (principal ou overflow).
 *   INSERCAO  anexada ao overflow; as buscas consultam o principal e depois o overflow.
 *   REORGANIZACAO (merge): quando o overflow chega a LIMITE_OVERFLOW registros, por
 *     comando do usuario, ou ao fechar o programa com overflow nao vazio, o overflow
 *     e ordenado e mesclado em streaming com o principal (descartando removidos),
 *     gerando um novo produtos.bin; entao o indice e RECONSTRUIDO e gravado.
 *   Como o principal so muda na reorganizacao, o indice permanece valido entre elas.
 */
#ifndef TABELA_H
#define TABELA_H

#include "produto.h"
#include "indice.h"

#ifndef LIMITE_OVERFLOW
#define LIMITE_OVERFLOW 100
#endif

typedef enum { METODO_BINARIA, METODO_INDICE } Metodo;
typedef enum { NAO_ENCONTRADO = 0, NO_PRINCIPAL = 1, NO_OVERFLOW = 2 } Local;

typedef struct {
    char  caminho_dados[256], caminho_ovf[256], caminho_idx[256];
    FILE *dados, *ovf;
    long  n_dados, n_ovf;
    Indice indice;
    int   indice_veio_do_arquivo;   /* 1 = carregado de produtos.idx, 0 = reconstruido */
} TabelaProdutos;

int  tabela_abrir(TabelaProdutos *t, const char *dir);
void tabela_fechar(TabelaProdutos *t);

long tabela_busca_binaria_dados(FILE *f, long n, int64_t chave, Produto *out);
Local tabela_buscar(TabelaProdutos *t, int64_t chave, Metodo metodo, Produto *out, long *pos);

void tabela_mostrar(TabelaProdutos *t, long inicio, long quantidade);
void tabela_mostrar_overflow(TabelaProdutos *t);

/* Retorno: 0 ok, 1 ok e disparou reorganizacao, -1 chave ja existe, -2 dados invalidos */
int  tabela_inserir(TabelaProdutos *t, const Produto *novo);
int  tabela_remover(TabelaProdutos *t, int64_t chave);   /* 1 removeu, 0 nao achou */
int  tabela_reorganizar(TabelaProdutos *t);

#endif
