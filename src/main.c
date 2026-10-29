/*
 * main.c - Menu do arquivo de produtos.
 * Uso: ./produtos [diretorio_dos_dados]   (padrao: data)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "tabela.h"

static int ler_linha(const char *prompt, char *buf, size_t n)
{
    printf("%s", prompt);
    fflush(stdout);
    if (!fgets(buf, (int)n, stdin)) return 0;
    buf[strcspn(buf, "\r\n")] = '\0';
    return 1;
}

static long ler_numero(const char *prompt, long padrao)
{
    char linha[64];
    if (!ler_linha(prompt, linha, sizeof linha) || linha[0] == '\0') return padrao;
    return atol(linha);
}

static void resultado(Local onde, const Produto *p, long pos)
{
    if (onde == NAO_ENCONTRADO) printf("Produto NAO encontrado.\n");
    else produto_imprimir(p, pos, onde == NO_PRINCIPAL ? "principal" : "overflow");
}

static void buscar(TabelaProdutos *t, Metodo metodo)
{
    Produto p;
    long pos;
    int64_t chave = ler_numero("id_produto: ", -1);
    produto_leituras = indice_leituras = 0;
    Local onde = tabela_buscar(t, chave, metodo, &p, &pos);
    resultado(onde, &p, pos);
    printf("(leituras no arquivo de dados: %ld | no arquivo de indice: %ld)\n",
           produto_leituras, indice_leituras);
}

static void comparar(TabelaProdutos *t)
{
    Produto p;
    long pos;
    int64_t chave = ler_numero("id_produto: ", -1);
    const Metodo metodos[2] = { METODO_BINARIA, METODO_INDICE };
    const char *nomes[2] = { "busca binaria direta", "indice parcial     " };
    const int repeticoes = 1000;

    for (int m = 0; m < 2; m++) {
        produto_leituras = indice_leituras = 0;
        Local onde = tabela_buscar(t, chave, metodos[m], &p, &pos);
        long ld = produto_leituras, li = indice_leituras;
        clock_t ini = clock();
        for (int r = 0; r < repeticoes; r++) tabela_buscar(t, chave, metodos[m], &p, &pos);
        double us = 1e6 * (double)(clock() - ini) / CLOCKS_PER_SEC / repeticoes;
        printf("%s: %-14s leituras dados=%ld indice=%ld  (~%.1f us/busca)\n", nomes[m],
               onde == NAO_ENCONTRADO ? "nao encontrado" : onde == NO_PRINCIPAL ? "principal" : "overflow",
               ld, li, us);
    }
}

static void inserir(TabelaProdutos *t)
{
    Produto p;
    char linha[128];
    memset(&p, 0, sizeof p);
    p.id = ler_numero("id_produto: ", -1);
    ler_linha("marca: ", linha, sizeof linha);
    snprintf(p.marca, sizeof p.marca, "%.*s", TAM_MARCA, linha);
    ler_linha("categoria: ", linha, sizeof linha);
    snprintf(p.categoria, sizeof p.categoria, "%.*s", TAM_CATEGORIA, linha);
    ler_linha("preco: ", linha, sizeof linha);
    p.preco = atof(linha);

    switch (tabela_inserir(t, &p)) {
        case 0:  printf("Inserido no overflow (%ld/%d).\n", t->n_ovf, LIMITE_OVERFLOW); break;
        case 1:  printf("Inserido. Overflow cheio: arquivo reorganizado e indice reconstruido.\n"); break;
        case -1: printf("Ja existe um produto ativo com esse id.\n"); break;
        default: printf("Dados invalidos (id 0..%lld, preco 0..%.2f).\n", ID_MAXIMO, PRECO_MAXIMO);
    }
}

static void informacoes(TabelaProdutos *t)
{
    const CabecalhoIndice *c = &t->indice.cab;
    printf("registro: %d bytes | principal: %ld registros | overflow: %ld/%d\n",
           TAM_REGISTRO, t->n_dados, t->n_ovf, LIMITE_OVERFLOW);
    printf("indice: K=%lld (nivel 1: %lld entradas) | M=%lld (nivel 2: %lld entradas)\n",
           (long long)c->k, (long long)c->n_nivel1, (long long)c->m, (long long)c->n_nivel2);
    printf("indice %s\n", t->indice_veio_do_arquivo ? "carregado de produtos.idx"
                                                   : "reconstruido ao abrir");
}

int main(int argc, char *argv[])
{
    const char *dir = argc > 1 ? argv[1] : "data";
    TabelaProdutos t;
    if (tabela_abrir(&t, dir) != 0) {
        fprintf(stderr, "Nao foi possivel abrir %s/produtos.bin (gere com ./gerar_produtos).\n", dir);
        return 1;
    }
    informacoes(&t);

    for (;;) {
        printf("\n1) Mostrar registros  2) Busca binaria  3) Busca por indice  4) Comparar metodos\n"
               "5) Inserir  6) Remover  7) Reorganizar+reconstruir indice  8) Mostrar overflow\n"
               "9) Informacoes  0) Sair\n> ");
        fflush(stdout);
        char op[16];
        if (!fgets(op, sizeof op, stdin)) break;
        switch (atoi(op)) {
            case 1: { long ini = ler_numero("a partir do registro [0]: ", 0);
                      long qtd = ler_numero("quantidade [10, 0 = todos]: ", 10);
                      tabela_mostrar(&t, ini, qtd); break; }
            case 2: buscar(&t, METODO_BINARIA); break;
            case 3: buscar(&t, METODO_INDICE); break;
            case 4: comparar(&t); break;
            case 5: inserir(&t); break;
            case 6: { int64_t id = ler_numero("id_produto: ", -1);
                      printf(tabela_remover(&t, id) ? "Removido (logicamente).\n" : "Produto nao encontrado.\n"); break; }
            case 7: printf(tabela_reorganizar(&t) == 0 ? "Reorganizado; indice reconstruido.\n" : "Erro na reorganizacao.\n"); break;
            case 8: tabela_mostrar_overflow(&t); break;
            case 9: informacoes(&t); break;
            case 0: tabela_fechar(&t); printf("Indice gravado em %s. Ate logo.\n", t.caminho_idx); return 0;
            default: printf("Opcao invalida.\n");
        }
    }
    tabela_fechar(&t);
    return 0;
}
