/* Teste automatizado do arquivo de produtos: busca, indice, insercao, remocao, reorganizacao. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "tabela.h"

#define N_INICIAL 1234
#define MAX_ID    20000
static int falhas = 0, verificacoes = 0;
static char esperado[MAX_ID + 1];       /* modelo: esperado[id] = produto deve existir */

#define CHECK(cond, msg) do { verificacoes++; if (!(cond)) { falhas++; printf("FALHOU: %s (linha %d)\n", msg, __LINE__); } } while (0)

static Produto novo_produto(int64_t id)
{
    Produto p; memset(&p, 0, sizeof p);
    p.id = id; p.preco = (double)(id % 1000) + 0.5; p.status = STATUS_ATIVO;
    snprintf(p.marca, sizeof p.marca, "marca%lld", (long long)(id % 17));
    snprintf(p.categoria, sizeof p.categoria, "cat.sub%lld", (long long)(id % 5));
    return p;
}

static void verificar_tudo(TabelaProdutos *t, const char *fase)
{
    int erros_antes = falhas;
    for (int64_t id = 0; id <= MAX_ID; id++) {
        Produto a, b; long pa, pb;
        Local la = tabela_buscar(t, id, METODO_BINARIA, &a, &pa);
        Local lb = tabela_buscar(t, id, METODO_INDICE, &b, &pb);
        CHECK((la != NAO_ENCONTRADO) == esperado[id], "binaria: existencia");
        CHECK((lb != NAO_ENCONTRADO) == esperado[id], "indice: existencia");
        CHECK(la == lb && (la == NAO_ENCONTRADO || (pa == pb && a.id == b.id)), "metodos concordam");
        if (la != NAO_ENCONTRADO) CHECK(a.id == id && a.preco == (double)(id % 1000) + 0.5, "conteudo");
    }
    printf("  %-34s %s\n", fase, falhas == erros_antes ? "ok" : "COM FALHAS");
}

static void verificar_ordenacao(TabelaProdutos *t)
{
    Produto p; int64_t anterior = -1; long ativos = 0;
    produto_posicionar(t->dados, 0);
    for (long i = 0; i < t->n_dados; i++) {
        produto_ler_seq(t->dados, &p);
        CHECK(p.id > anterior, "principal ordenado e sem duplicatas");
        anterior = p.id; ativos += p.status == STATUS_ATIVO;
    }
    CHECK(ativos == t->n_dados, "principal sem removidos apos reorganizar");
}

int main(void)
{
    const char *dir = "/tmp/teste_produtos";
    char cmd[128], caminho[300];
    snprintf(cmd, sizeof cmd, "rm -rf %s && mkdir -p %s", dir, dir);
    if (system(cmd) != 0) return 1;

    /* arquivo inicial: ids 10*i+5, com lacunas */
    snprintf(caminho, sizeof caminho, "%s/produtos.bin", dir);
    FILE *f = fopen(caminho, "wb");
    for (int i = 0; i < N_INICIAL; i++) {
        Produto p = novo_produto(10 * i + 5);
        produto_gravar_seq(f, &p);
        esperado[p.id] = 1;
    }
    fclose(f);

    TabelaProdutos t;
    CHECK(tabela_abrir(&t, dir) == 0, "abrir");
    CHECK(!t.indice_veio_do_arquivo, "1a abertura constroi indice");
    CHECK(t.indice.cab.n_nivel1 == (N_INICIAL + 6) / 7, "tamanho do nivel 1");
    printf("Testes (K=7, M=5, overflow=%d):\n", LIMITE_OVERFLOW);
    verificar_tudo(&t, "busca em arquivo recem-criado");

    /* insercoes: inicio, meio, fim, duplicata, invalido */
    int64_t novos[] = { 1, 6, 5000 + 1, 12346, 19999 };
    for (size_t i = 0; i < sizeof novos / sizeof *novos; i++) {
        Produto p = novo_produto(novos[i]);
        CHECK(tabela_inserir(&t, &p) == 0, "inserir");
        esperado[novos[i]] = 1;
    }
    Produto dup = novo_produto(15), inv = novo_produto(-4);
    CHECK(tabela_inserir(&t, &dup) == -1, "rejeita duplicata no principal");
    dup = novo_produto(6);
    CHECK(tabela_inserir(&t, &dup) == -1, "rejeita duplicata no overflow");
    CHECK(tabela_inserir(&t, &inv) == -2, "rejeita id invalido");
    verificar_tudo(&t, "apos insercoes (overflow)");

    /* remocoes: principal e overflow */
    int64_t rem[] = { 5, 15, 6, 55, 12346 };
    for (size_t i = 0; i < sizeof rem / sizeof *rem; i++) {
        int existia = esperado[rem[i]];
        CHECK(tabela_remover(&t, rem[i]) == existia, "remover");
        esperado[rem[i]] = 0;
    }
    CHECK(tabela_remover(&t, 7) == 0, "remover inexistente");
    verificar_tudo(&t, "apos remocoes (tombstones)");

    CHECK(tabela_reorganizar(&t) == 0, "reorganizar");
    CHECK(t.n_ovf == 0, "overflow vazio");
    verificar_ordenacao(&t);
    verificar_tudo(&t, "apos reorganizacao manual");

    /* reinsere um id removido e enche o overflow para disparar a reorganizacao automatica */
    Produto p = novo_produto(5);
    CHECK(tabela_inserir(&t, &p) == 0, "reinserir id removido");
    esperado[5] = 1;
    int disparou = 0;
    for (int64_t id = 2000; id < 2000 + 3 * LIMITE_OVERFLOW; id += 2) {
        if (esperado[id]) continue;
        p = novo_produto(id);
        int r = tabela_inserir(&t, &p);
        CHECK(r == 0 || r == 1, "inserir em lote");
        esperado[id] = 1; disparou |= r == 1;
    }
    CHECK(disparou, "reorganizacao automatica ao encher o overflow");
    verificar_tudo(&t, "apos lote com auto-reorganizacao");

    /* fechar com overflow pendente e reabrir: indice deve vir do arquivo */
    p = novo_produto(18881); tabela_inserir(&t, &p); esperado[18881] = 1;
    tabela_fechar(&t);
    CHECK(tabela_abrir(&t, dir) == 0, "reabrir");
    CHECK(t.indice_veio_do_arquivo, "indice carregado de produtos.idx");
    CHECK(t.n_ovf == 0, "overflow reorganizado ao fechar");
    verificar_ordenacao(&t);
    verificar_tudo(&t, "apos fechar/reabrir");

    /* economia de leituras do indice */
    Produto q; long pos;
    produto_leituras = indice_leituras = 0;
    tabela_buscar(&t, 6000 + 5, METODO_INDICE, &q, &pos);
    printf("  indice: %ld leituras de dados + %ld de indice (principal tem %ld registros)\n",
           produto_leituras, indice_leituras, t.n_dados);
    tabela_fechar(&t);

    printf("\n%d verificacoes, %d falhas\n", verificacoes, falhas);
    return falhas != 0;
}
