#include "tabela.h"
#include <stdlib.h>
#include <string.h>

/* ---------- abertura / fechamento ---------- */

static int abrir_arquivos(TabelaProdutos *t)
{
    t->dados = fopen(t->caminho_dados, "rb+");
    if (!t->dados) return -1;
    t->ovf = fopen(t->caminho_ovf, "rb+");
    if (!t->ovf) t->ovf = fopen(t->caminho_ovf, "wb+");
    if (!t->ovf) { fclose(t->dados); t->dados = NULL; return -1; }
    t->n_dados = produto_num_registros(t->dados);
    t->n_ovf   = produto_num_registros(t->ovf);
    return 0;
}

static int carregar_ou_reconstruir_indice(TabelaProdutos *t)
{
    if (indice_carregar(&t->indice, t->caminho_idx, t->n_dados) == 0) {
        t->indice_veio_do_arquivo = 1;
        return 0;
    }
    t->indice_veio_do_arquivo = 0;
    if (indice_construir(t->caminho_dados, t->caminho_idx) != 0) return -1;
    return indice_carregar(&t->indice, t->caminho_idx, t->n_dados);
}

int tabela_abrir(TabelaProdutos *t, const char *dir)
{
    memset(t, 0, sizeof *t);
    snprintf(t->caminho_dados, sizeof t->caminho_dados, "%s/produtos.bin", dir);
    snprintf(t->caminho_ovf,   sizeof t->caminho_ovf,   "%s/produtos.ovf", dir);
    snprintf(t->caminho_idx,   sizeof t->caminho_idx,   "%s/produtos.idx", dir);

    if (abrir_arquivos(t) != 0) return -1;
    if (carregar_ou_reconstruir_indice(t) != 0) { tabela_fechar(t); return -1; }
    return 0;
}

void tabela_fechar(TabelaProdutos *t)
{
    /* Fim da execucao: se ha insercoes pendentes, reorganiza (regrava dados e indice). */
    if (t->dados && t->n_ovf > 0) tabela_reorganizar(t);
    indice_fechar(&t->indice);
    if (t->dados) fclose(t->dados);
    if (t->ovf)   fclose(t->ovf);
    t->dados = t->ovf = NULL;
}

/* ---------- buscas ---------- */

long tabela_busca_binaria_dados(FILE *f, long n, int64_t chave, Produto *out)
{
    long ini = 0, fim = n - 1;
    while (ini <= fim) {
        long meio = ini + (fim - ini) / 2;
        if (produto_ler(f, meio, out) != 0) return -1;
        if (out->id == chave) return meio;
        if (out->id < chave) ini = meio + 1;
        else                 fim = meio - 1;
    }
    return -1;
}

/* Varredura linear do overflow (pequeno, limitado por LIMITE_OVERFLOW). */
static long buscar_overflow(TabelaProdutos *t, int64_t chave, Produto *out)
{
    if (produto_posicionar(t->ovf, 0) != 0) return -1;
    for (long i = 0; i < t->n_ovf; i++) {
        if (produto_ler_seq(t->ovf, out) != 0) return -1;
        if (out->id == chave && out->status == STATUS_ATIVO) return i;
    }
    return -1;
}

Local tabela_buscar(TabelaProdutos *t, int64_t chave, Metodo metodo, Produto *out, long *pos)
{
    long p = (metodo == METODO_BINARIA)
           ? tabela_busca_binaria_dados(t->dados, t->n_dados, chave, out)
           : indice_buscar(&t->indice, t->dados, chave, out);
    if (p >= 0 && out->status == STATUS_ATIVO) { *pos = p; return NO_PRINCIPAL; }

    p = buscar_overflow(t, chave, out);
    if (p >= 0) { *pos = p; return NO_OVERFLOW; }
    return NAO_ENCONTRADO;
}

/* ---------- exibicao ---------- */

void tabela_mostrar(TabelaProdutos *t, long inicio, long quantidade)
{
    if (inicio < 0 || inicio >= t->n_dados) { printf("Posicao fora do arquivo (0..%ld).\n", t->n_dados - 1); return; }
    if (quantidade <= 0 || inicio + quantidade > t->n_dados) quantidade = t->n_dados - inicio;
    if (produto_posicionar(t->dados, inicio) != 0) return;
    for (long i = 0; i < quantidade; i++) {
        Produto p;
        if (produto_ler_seq(t->dados, &p) != 0) break;
        produto_imprimir(&p, inicio + i, "principal");
    }
}

void tabela_mostrar_overflow(TabelaProdutos *t)
{
    if (t->n_ovf == 0) { printf("Overflow vazio.\n"); return; }
    produto_posicionar(t->ovf, 0);
    for (long i = 0; i < t->n_ovf; i++) {
        Produto p;
        if (produto_ler_seq(t->ovf, &p) != 0) break;
        produto_imprimir(&p, i, "overflow");
    }
}

/* ---------- insercao / remocao ---------- */

int tabela_inserir(TabelaProdutos *t, const Produto *novo)
{
    Produto existente;
    long pos;
    if (!produto_valido(novo)) return -2;
    if (tabela_buscar(t, novo->id, METODO_INDICE, &existente, &pos) != NAO_ENCONTRADO) return -1;

    Produto p = *novo;
    p.status = STATUS_ATIVO;
    if (fseek(t->ovf, 0, SEEK_END) != 0 || produto_gravar_seq(t->ovf, &p) != 0) return -2;
    fflush(t->ovf);
    t->n_ovf++;

    if (t->n_ovf >= LIMITE_OVERFLOW) return tabela_reorganizar(t) == 0 ? 1 : -2;
    return 0;
}

int tabela_remover(TabelaProdutos *t, int64_t chave)
{
    Produto p;
    long pos;
    Local onde = tabela_buscar(t, chave, METODO_INDICE, &p, &pos);
    if (onde == NAO_ENCONTRADO) return 0;

    p.status = STATUS_REMOVIDO;
    FILE *f = (onde == NO_PRINCIPAL) ? t->dados : t->ovf;
    if (produto_gravar(f, pos, &p) != 0) return 0;
    fflush(f);
    return 1;
}

/* ---------- reorganizacao (merge) + reconstrucao do indice ---------- */

static int cmp_por_id(const void *a, const void *b)
{
    int64_t x = ((const Produto *)a)->id, y = ((const Produto *)b)->id;
    return (x > y) - (x < y);
}

/* Le o proximo registro ATIVO do arquivo (posicao atual). 1 se leu, 0 se acabou. */
static int proximo_ativo(FILE *f, Produto *p)
{
    while (produto_ler_seq(f, p) == 0)
        if (p->status == STATUS_ATIVO) return 1;
    return 0;
}

int tabela_reorganizar(TabelaProdutos *t)
{
    char caminho_tmp[300];
    int rc = -1;
    long n_novos = 0;
    Produto *novos = malloc((t->n_ovf > 0 ? t->n_ovf : 1) * sizeof(Produto));
    if (!novos) return -1;

    /* 1) registros ativos do overflow, ordenados (poucos: <= LIMITE_OVERFLOW) */
    produto_posicionar(t->ovf, 0);
    for (long i = 0; i < t->n_ovf; i++) {
        Produto p;
        if (produto_ler_seq(t->ovf, &p) != 0) break;
        if (p.status == STATUS_ATIVO) novos[n_novos++] = p;
    }
    qsort(novos, n_novos, sizeof(Produto), cmp_por_id);

    /* 2) merge em streaming: principal (sem removidos) + overflow -> arquivo temporario */
    snprintf(caminho_tmp, sizeof caminho_tmp, "%s.tmp", t->caminho_dados);
    FILE *saida = fopen(caminho_tmp, "wb");
    if (!saida) { free(novos); return -1; }

    Produto a;
    long j = 0;
    produto_posicionar(t->dados, 0);
    int tem_a = proximo_ativo(t->dados, &a);
    int erro = 0;
    while (!erro && (tem_a || j < n_novos)) {
        if (tem_a && (j >= n_novos || a.id < novos[j].id)) {
            erro = produto_gravar_seq(saida, &a) != 0;
            tem_a = proximo_ativo(t->dados, &a);
        } else {
            erro = produto_gravar_seq(saida, &novos[j++]) != 0;
        }
    }
    free(novos);
    if (fclose(saida) != 0) erro = 1;
    if (erro) { remove(caminho_tmp); return -1; }

    /* 3) troca os arquivos, esvazia o overflow e reconstroi o indice */
    indice_fechar(&t->indice);
    fclose(t->dados);
    fclose(t->ovf);
    remove(t->caminho_dados);
    if (rename(caminho_tmp, t->caminho_dados) != 0) return -1;
    remove(t->caminho_ovf);

    if (abrir_arquivos(t) != 0) return -1;
    if (indice_construir(t->caminho_dados, t->caminho_idx) == 0 &&
        indice_carregar(&t->indice, t->caminho_idx, t->n_dados) == 0)
        rc = 0;
    return rc;
}
