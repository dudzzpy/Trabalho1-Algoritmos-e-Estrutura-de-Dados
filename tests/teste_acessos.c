#include "acessos.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static Acesso criar(int64_t id) {
    Acesso a = {0};
    a.id_evento = id;
    snprintf(a.data_hora, sizeof a.data_hora, "2019-11-01 12:00:00");
    snprintf(a.tipo, sizeof a.tipo, "%s", id % 3 ? "view" : "purchase");
    a.id_produto = 123;
    a.id_usuario = 456;
    snprintf(a.sessao, sizeof a.sessao, "teste");
    a.status = 'A';
    return a;
}
static void conferir_buscas(TabelaAcessos *t, int64_t id, int existe) {
    Acesso a, b;
    int64_t p1 = acesso_buscar_binaria(t, id, &a);
    int64_t p2 = acesso_buscar_indice(t, id, &b);
    assert((p1 >= 0) == existe);
    assert(p1 == p2);
    if (existe) { assert(a.id_evento == id); assert(b.id_evento == id); }
}
int main(void) {
    const char *dir = "teste_dados_acessos";
    assert(acesso_criar_diretorio(dir) == 0);
    FILE *f = fopen("teste_dados_acessos/acessos.bin", "wb");
    assert(f);
    for (int i = 0; i < 37; ++i) {
        Acesso a = criar(10 + 10 * i);
        assert(acesso_gravar(f, &a) == 0);
    }
    assert(fclose(f) == 0);
    remove("teste_dados_acessos/acessos.idx");
    TabelaAcessos t;
    assert(acesso_abrir(&t, dir) == 0);
    assert(t.indice_carregado == 0);
    assert(t.quantidade == 37);
    assert(t.cab.n_nivel2 >= 2); /* exercita os dois niveis */
    for (int i = 0; i < 37; ++i) conferir_buscas(&t, 10 + 10 * i, 1);
    for (int i = 0; i <= 380; ++i) if (i < 10 || (i - 10) % 10 != 0) conferir_buscas(&t, i, 0);
    conferir_buscas(&t, 999999, 0);

    Acesso a = criar(5);
    assert(acesso_inserir(&t, &a) == 0);  /* insere no inicio */
    a = criar(25);
    assert(acesso_inserir(&t, &a) == 0); /* insere no meio */
    a = criar(500);
    assert(acesso_inserir(&t, &a) == 0); /* insere no fim */
    assert(acesso_inserir(&t, &a) == 1); /* chave duplicada */
    assert(t.quantidade == 40);
    conferir_buscas(&t, 5, 1);
    conferir_buscas(&t, 25, 1);
    conferir_buscas(&t, 500, 1);
    assert(acesso_remover(&t, 5) == 0);
    assert(acesso_remover(&t, 25) == 0);
    assert(acesso_remover(&t, 500) == 0);
    assert(acesso_remover(&t, 500) == 1); /* inexistente */
    assert(t.quantidade == 37);
    conferir_buscas(&t, 5, 0);
    conferir_buscas(&t, 25, 0);
    conferir_buscas(&t, 500, 0);
    for (int i = 0; i < 37; ++i) conferir_buscas(&t, 10 + 10 * i, 1);
    acesso_fechar(&t);

    assert(acesso_abrir(&t, dir) == 0);
    assert(t.indice_carregado == 1); /* indice persistido */
    conferir_buscas(&t, 190, 1);
    acesso_fechar(&t);
    puts("OK: testes de acessos passaram (buscas, indice, insercao, remocao e persistencia).");
    return 0;
}
