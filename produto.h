/*
 * produto.h - Registro de produto (tamanho fixo, campos textuais) e E/S em disco.
 *
 * Layout de um registro em produtos.bin (93 bytes):
 *   id_produto (10) | marca (30) | categoria (40) | preco (11) | status (1) | '\n' (1)
 *
 *  - id_produto: numero com zeros a esquerda (ex.: 0000000123)  -> CHAVE UNICA
 *  - marca/categoria: texto preenchido com espacos a direita
 *  - preco: "%011.2f" (ex.: 00001299.90)
 *  - status: 'A' = ativo, 'R' = removido (remocao logica / lapide)
 *
 * A posicao em bytes do registro n e n * TAM_REGISTRO. A leitura e sempre por
 * tamanho fixo (fread), nunca por linha.
 */
#ifndef PRODUTO_H
#define PRODUTO_H

#include <stdio.h>
#include <stdint.h>

#define TAM_ID        10
#define TAM_MARCA     30
#define TAM_CATEGORIA 40
#define TAM_PRECO     11
#define TAM_STATUS    1
#define TAM_REGISTRO  (TAM_ID + TAM_MARCA + TAM_CATEGORIA + TAM_PRECO + TAM_STATUS + 1)

#define STATUS_ATIVO    'A'
#define STATUS_REMOVIDO 'R'

#define ID_MAXIMO    9999999999LL
#define PRECO_MAXIMO 99999999.99

typedef struct {
    int64_t id;
    char    marca[TAM_MARCA + 1];          /* sem espacos finais, terminada em '\0' */
    char    categoria[TAM_CATEGORIA + 1];
    double  preco;
    char    status;
} Produto;

/* Contador de registros lidos do disco (para comparar metodos de busca). */
extern long produto_leituras;

int  produto_valido(const Produto *p);
void produto_codificar(const Produto *p, char buf[TAM_REGISTRO]);
int  produto_decodificar(const char buf[TAM_REGISTRO], Produto *p);

long produto_num_registros(FILE *f);
int  produto_posicionar(FILE *f, long pos);        /* fseek para o registro 'pos' */
int  produto_ler(FILE *f, long pos, Produto *p);   /* fseek + leitura           */
int  produto_ler_seq(FILE *f, Produto *p);         /* le na posicao atual       */
int  produto_gravar(FILE *f, long pos, const Produto *p);
int  produto_gravar_seq(FILE *f, const Produto *p);
void produto_imprimir(const Produto *p, long pos, const char *origem);

#endif
