#include "produto.h"
#include <stdlib.h>
#include <string.h>

long produto_leituras = 0;

static void copiar_preenchendo(char *dst, size_t n, const char *src)
{
    size_t len = strlen(src);
    if (len > n) len = n;
    memcpy(dst, src, len);
    memset(dst + len, ' ', n - len);
}

static void copiar_sem_espacos_finais(char *dst, const char *src, size_t n)
{
    memcpy(dst, src, n);
    dst[n] = '\0';
    while (n > 0 && dst[n - 1] == ' ') dst[--n] = '\0';
}

int produto_valido(const Produto *p)
{
    return p->id >= 0 && p->id <= ID_MAXIMO &&
           p->preco >= 0.0 && p->preco <= PRECO_MAXIMO;
}

void produto_codificar(const Produto *p, char buf[TAM_REGISTRO])
{
    char tmp[64];
    char *cur = buf;

    snprintf(tmp, sizeof tmp, "%0*lld", TAM_ID, (long long)p->id);
    memcpy(cur, tmp, TAM_ID);                        cur += TAM_ID;
    copiar_preenchendo(cur, TAM_MARCA, p->marca);    cur += TAM_MARCA;
    copiar_preenchendo(cur, TAM_CATEGORIA, p->categoria); cur += TAM_CATEGORIA;
    snprintf(tmp, sizeof tmp, "%0*.2f", TAM_PRECO, p->preco);
    memcpy(cur, tmp, TAM_PRECO);                     cur += TAM_PRECO;
    *cur++ = p->status;
    *cur   = '\n';
}

int produto_decodificar(const char buf[TAM_REGISTRO], Produto *p)
{
    char tmp[TAM_ID + 1 > TAM_PRECO + 1 ? TAM_ID + 1 : TAM_PRECO + 1];
    const char *cur = buf;

    if (buf[TAM_REGISTRO - 1] != '\n') return -1;    /* registro corrompido */

    memcpy(tmp, cur, TAM_ID); tmp[TAM_ID] = '\0'; cur += TAM_ID;
    p->id = strtoll(tmp, NULL, 10);
    copiar_sem_espacos_finais(p->marca, cur, TAM_MARCA);          cur += TAM_MARCA;
    copiar_sem_espacos_finais(p->categoria, cur, TAM_CATEGORIA);  cur += TAM_CATEGORIA;
    memcpy(tmp, cur, TAM_PRECO); tmp[TAM_PRECO] = '\0'; cur += TAM_PRECO;
    p->preco  = strtod(tmp, NULL);
    p->status = *cur;
    return 0;
}

long produto_num_registros(FILE *f)
{
    if (fseek(f, 0, SEEK_END) != 0) return -1;
    long tam = ftell(f);
    return tam < 0 ? -1 : tam / TAM_REGISTRO;
}

int produto_posicionar(FILE *f, long pos)
{
    return fseek(f, pos * (long)TAM_REGISTRO, SEEK_SET);
}

int produto_ler_seq(FILE *f, Produto *p)
{
    char buf[TAM_REGISTRO];
    if (fread(buf, TAM_REGISTRO, 1, f) != 1) return -1;
    produto_leituras++;
    return produto_decodificar(buf, p);
}

int produto_ler(FILE *f, long pos, Produto *p)
{
    if (produto_posicionar(f, pos) != 0) return -1;
    return produto_ler_seq(f, p);
}

int produto_gravar_seq(FILE *f, const Produto *p)
{
    char buf[TAM_REGISTRO];
    produto_codificar(p, buf);
    return fwrite(buf, TAM_REGISTRO, 1, f) == 1 ? 0 : -1;
}

int produto_gravar(FILE *f, long pos, const Produto *p)
{
    if (produto_posicionar(f, pos) != 0) return -1;
    return produto_gravar_seq(f, p);
}

void produto_imprimir(const Produto *p, long pos, const char *origem)
{
    printf("[%s #%ld] id=%lld | marca=%-12s | categoria=%-28s | preco=%9.2f | %s\n",
           origem, pos, (long long)p->id, p->marca, p->categoria, p->preco,
           p->status == STATUS_ATIVO ? "ativo" : "REMOVIDO");
}
