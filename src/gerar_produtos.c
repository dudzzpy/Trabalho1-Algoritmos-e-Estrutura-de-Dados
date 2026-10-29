/*
 * gerar_produtos.c - Gera produtos.bin a partir dos CSVs do dataset do Kaggle.
 *
 * Uso: ./gerar_produtos <dir_saida> <max_linhas|0> <arquivo.csv> [outro.csv ...]
 *
 * CSV: event_time,event_type,product_id,category_id,category_code,brand,price,user_id,user_session
 *
 * Um registro por product_id (a 1a ocorrencia vence), ordenado por id_produto.
 * O gerador usa RAM para os produtos distintos (executa uma unica vez); a restricao
 * de RAM do enunciado vale para as operacoes de busca/insercao/remocao.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "produto.h"
#include "indice.h"

#define NUM_CAMPOS 9
#define TAM_LINHA  2048

typedef struct {
    Produto *itens;
    size_t   n, cap;
    int32_t *tabela;       /* hash aberta: indice em itens ou -1 */
    size_t   tam_tabela;   /* potencia de 2 */
} Colecao;

static size_t hash_id(int64_t id, size_t mascara)
{
    return (size_t)(((uint64_t)id * 11400714819323198485ull) >> 32) & mascara;
}

static void zerar_tabela(Colecao *c)
{
    memset(c->tabela, 0xFF, c->tam_tabela * sizeof(int32_t));   /* todos -1 */
}

static void colecao_init(Colecao *c)
{
    c->n = 0;
    c->cap = 4096;
    c->tam_tabela = 1u << 16;
    c->itens = malloc(c->cap * sizeof(Produto));
    c->tabela = malloc(c->tam_tabela * sizeof(int32_t));
    if (!c->itens || !c->tabela) { perror("malloc"); exit(1); }
    zerar_tabela(c);
}

static void colecao_rehash(Colecao *c)
{
    free(c->tabela);
    c->tam_tabela *= 2;
    c->tabela = malloc(c->tam_tabela * sizeof(int32_t));
    if (!c->tabela) { perror("malloc"); exit(1); }
    zerar_tabela(c);
    for (size_t i = 0; i < c->n; i++) {
        size_t h = hash_id(c->itens[i].id, c->tam_tabela - 1);
        while (c->tabela[h] != -1) h = (h + 1) & (c->tam_tabela - 1);
        c->tabela[h] = (int32_t)i;
    }
}

static void colecao_inserir(Colecao *c, const Produto *p)
{
    size_t mascara = c->tam_tabela - 1, h = hash_id(p->id, mascara);
    while (c->tabela[h] != -1) {
        if (c->itens[c->tabela[h]].id == p->id) return;      /* ja existe */
        h = (h + 1) & mascara;
    }
    if (c->n == c->cap) {
        c->cap *= 2;
        c->itens = realloc(c->itens, c->cap * sizeof(Produto));
        if (!c->itens) { perror("realloc"); exit(1); }
    }
    c->itens[c->n] = *p;
    c->tabela[h] = (int32_t)c->n++;
    if (c->n * 2 > c->tam_tabela) colecao_rehash(c);
}

static int cmp_por_id(const void *a, const void *b)
{
    int64_t x = ((const Produto *)a)->id, y = ((const Produto *)b)->id;
    return (x > y) - (x < y);
}

/*
 * Divide uma linha CSV, preservando campos vazios e respeitando aspas.
 * Virgulas dentro de aspas nao separam campos e aspas duplicadas ("") viram ".
 * Retorna a quantidade de campos ou -1 se o CSV estiver malformado/exceder max.
 * A funcao altera a linha recebida e nao suporta campos com quebras de linha.
 */
static int dividir_csv(char *linha, char *campos[], int max)
{
    char *leitura = linha;
    char *escrita = linha;
    int n = 0;

    if (max <= 0) return -1;

    while (1) {
        if (n >= max) return -1;
        campos[n++] = escrita;

        if (*leitura == '"') {
            /* Campo entre aspas: remove as aspas e desescapa "". */
            leitura++;
            int fechou = 0;
            while (*leitura) {
                if (*leitura == '"') {
                    if (leitura[1] == '"') {
                        *escrita++ = '"';
                        leitura += 2;
                    } else {
                        leitura++;
                        fechou = 1;
                        break;
                    }
                } else {
                    *escrita++ = *leitura++;
                }
            }
            if (!fechou) return -1;

            /* Aceita espacos depois da aspa de fechamento. */
            while (*leitura == ' ' || *leitura == '\t') leitura++;
            if (*leitura != ',' && *leitura != '\0' &&
                *leitura != '\n' && *leitura != '\r') return -1;
        } else {
            /* Campo sem aspas: virgula encerra o campo. */
            while (*leitura && *leitura != ',' && *leitura != '\n' && *leitura != '\r') {
                if (*leitura == '"') return -1; /* aspa no meio de campo nao citado */
                *escrita++ = *leitura++;
            }
        }

        char delimitador = *leitura;
        if (delimitador == '\r' && leitura[1] == '\n') leitura += 2;
        else if (delimitador == ',' || delimitador == '\r' || delimitador == '\n') leitura++;
        else if (delimitador != '\0') return -1;

        *escrita++ = '\0';

        if (delimitador != ',') return n;
        /* A proxima iteracao tambem trata campos vazios, inclusive o ultimo. */
    }
}

int main(int argc, char *argv[])
{
    if (argc < 4) {
        fprintf(stderr, "Uso: %s <dir_saida> <max_linhas|0> <arquivo.csv> [...]\n", argv[0]);
        return 1;
    }
    long long max_linhas = atoll(argv[2]), lidas = 0, ignoradas = 0, truncados = 0;
    Colecao col;
    colecao_init(&col);
    char linha[TAM_LINHA];

    for (int i = 3; i < argc && (max_linhas == 0 || lidas < max_linhas); i++) {
        FILE *csv = fopen(argv[i], "r");
        if (!csv) { perror(argv[i]); continue; }
        if (!fgets(linha, sizeof linha, csv)) { fclose(csv); continue; }   /* cabecalho */

        while ((max_linhas == 0 || lidas < max_linhas) && fgets(linha, sizeof linha, csv)) {
            char *c[NUM_CAMPOS];
            if (dividir_csv(linha, c, NUM_CAMPOS) != NUM_CAMPOS) { ignoradas++; continue; }
            lidas++;

            Produto p;
            memset(&p, 0, sizeof p);
            p.id = strtoll(c[2], NULL, 10);
            p.preco = strtod(c[6], NULL);
            p.status = STATUS_ATIVO;
            if (strlen(c[5]) > TAM_MARCA || strlen(c[4]) > TAM_CATEGORIA) truncados++;
            snprintf(p.marca, sizeof p.marca, "%.*s", TAM_MARCA, c[5]);
            snprintf(p.categoria, sizeof p.categoria, "%.*s", TAM_CATEGORIA, c[4]);
            if (p.id <= 0 || !produto_valido(&p)) { ignoradas++; continue; }
            colecao_inserir(&col, &p);
        }
        fclose(csv);
    }

    qsort(col.itens, col.n, sizeof(Produto), cmp_por_id);

    char caminho[1024];
    snprintf(caminho, sizeof caminho, "%s/produtos.bin", argv[1]);
    FILE *out = fopen(caminho, "wb");
    if (!out) { perror(caminho); return 1; }
    for (size_t i = 0; i < col.n; i++)
        if (produto_gravar_seq(out, &col.itens[i]) != 0) { perror("gravar"); return 1; }
    if (fclose(out) != 0) { perror("fechar produtos.bin"); return 1; }

    char caminho_idx[1024];
    snprintf(caminho_idx, sizeof caminho_idx, "%s/produtos.idx", argv[1]);
    if (indice_construir(caminho, caminho_idx) != 0) {
        perror("construir produtos.idx");
        return 1;
    }

    printf("Linhas lidas: %lld | ignoradas: %lld | campos truncados: %lld\n", lidas, ignoradas, truncados);
    printf("%s: %zu produtos distintos x %d bytes = %zu bytes\n",
           caminho, col.n, TAM_REGISTRO, col.n * TAM_REGISTRO);
    printf("Indice gerado: %s\n", caminho_idx);
    free(col.itens);
    free(col.tabela);
    return 0;
}
