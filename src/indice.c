#include "indice.h"
#include <string.h>

_Static_assert(sizeof(CabecalhoIndice) == 48, "cabecalho do indice deve ter 48 bytes");
_Static_assert(sizeof(EntradaIndice) == 16, "entrada do indice deve ter 16 bytes");

static const char MAGIC[8] = { 'I', 'D', 'X', 'P', 'R', 'O', 'D', '1' };

long indice_leituras = 0;

static long base_nivel2(void)
{
    return (long)sizeof(CabecalhoIndice);
}

static long base_nivel1(const CabecalhoIndice *c)
{
    return (long)sizeof(CabecalhoIndice) + (long)(c->n_nivel2 * sizeof(EntradaIndice));
}

static int ler_entrada(FILE *f, long base, long i, EntradaIndice *e)
{
    if (fseek(f, base + i * (long)sizeof *e, SEEK_SET) != 0) return -1;
    if (fread(e, sizeof *e, 1, f) != 1) return -1;
    indice_leituras++;
    return 0;
}

static int gravar_entrada(FILE *f, long base, long i, const EntradaIndice *e)
{
    if (fseek(f, base + i * (long)sizeof *e, SEEK_SET) != 0) return -1;
    return fwrite(e, sizeof *e, 1, f) == 1 ? 0 : -1;
}

int indice_construir(const char *arq_dados, const char *arq_indice)
{
    FILE *d = fopen(arq_dados, "rb");
    if (!d) return -1;
    FILE *x = fopen(arq_indice, "w+b");
    if (!x) { fclose(d); return -1; }

    int rc = -1;
    long n = produto_num_registros(d);
    CabecalhoIndice c;
    memcpy(c.magic, MAGIC, sizeof MAGIC);
    c.k = INTERVALO_NIVEL1;
    c.m = INTERVALO_NIVEL2;
    c.n_registros = n;
    c.n_nivel1 = (n + c.k - 1) / c.k;
    c.n_nivel2 = (c.n_nivel1 + c.m - 1) / c.m;

    if (n < 0 || fwrite(&c, sizeof c, 1, x) != 1) goto fim;

    /* Nivel 1: le a chave de 1 registro a cada K (seek direto, sem varrer o arquivo) */
    for (long i = 0; i < c.n_nivel1; i++) {
        Produto p;
        if (produto_ler(d, i * c.k, &p) != 0) goto fim;
        EntradaIndice e = { p.id, i * c.k };
        if (gravar_entrada(x, base_nivel1(&c), i, &e) != 0) goto fim;
    }
    /* Nivel 2: 1 entrada do nivel 1 a cada M */
    for (long j = 0; j < c.n_nivel2; j++) {
        EntradaIndice e1;
        if (ler_entrada(x, base_nivel1(&c), j * c.m, &e1) != 0) goto fim;
        EntradaIndice e2 = { e1.chave, j * c.m };
        if (gravar_entrada(x, base_nivel2(), j, &e2) != 0) goto fim;
    }
    rc = 0;
fim:
    fclose(x);
    fclose(d);
    return rc;
}

int indice_carregar(Indice *ix, const char *arq_indice, long n_registros_dados)
{
    ix->arq = fopen(arq_indice, "rb");
    if (!ix->arq) return -1;

    CabecalhoIndice *c = &ix->cab;
    int ok = fread(c, sizeof *c, 1, ix->arq) == 1
          && memcmp(c->magic, MAGIC, sizeof MAGIC) == 0
          && c->k > 0 && c->m > 0
          && c->n_registros == n_registros_dados;
    if (ok) {
        fseek(ix->arq, 0, SEEK_END);
        long esperado = (long)sizeof *c + (long)((c->n_nivel1 + c->n_nivel2) * sizeof(EntradaIndice));
        ok = ftell(ix->arq) == esperado;
    }
    if (!ok) { fclose(ix->arq); ix->arq = NULL; return -1; }
    return 0;
}

void indice_fechar(Indice *ix)
{
    if (ix->arq) fclose(ix->arq);
    ix->arq = NULL;
}

/* Maior entrada com chave <= 'chave' em [lo, hi) (busca binaria no arquivo). -1 se nao ha. */
static long maior_menor_igual(FILE *f, long base, long lo, long hi,
                              int64_t chave, EntradaIndice *res)
{
    long ini = lo, fim = hi - 1, achou = -1;
    while (ini <= fim) {
        long meio = ini + (fim - ini) / 2;
        EntradaIndice e;
        if (ler_entrada(f, base, meio, &e) != 0) return -1;
        if (e.chave <= chave) { achou = meio; *res = e; ini = meio + 1; }
        else                  { fim = meio - 1; }
    }
    return achou;
}

long indice_buscar(const Indice *ix, FILE *dados, int64_t chave, Produto *out)
{
    const CabecalhoIndice *c = &ix->cab;
    EntradaIndice e2, e1;

    if (c->n_registros == 0) return -1;
    if (maior_menor_igual(ix->arq, base_nivel2(), 0, c->n_nivel2, chave, &e2) < 0)
        return -1;                                   /* chave menor que a primeira */

    long ini1 = e2.pos;
    long fim1 = ini1 + c->m < c->n_nivel1 ? ini1 + c->m : c->n_nivel1;
    if (maior_menor_igual(ix->arq, base_nivel1(c), ini1, fim1, chave, &e1) < 0)
        return -1;

    long pos = e1.pos;
    long limite = pos + c->k < c->n_registros ? pos + c->k : c->n_registros;
    if (produto_posicionar(dados, pos) != 0) return -1;
    for (; pos < limite; pos++) {
        if (produto_ler_seq(dados, out) != 0) return -1;
        if (out->id == chave) return pos;
        if (out->id > chave) break;
    }
    return -1;
}
