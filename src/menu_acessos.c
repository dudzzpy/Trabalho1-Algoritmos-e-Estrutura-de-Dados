/* Programa independente: ./acessos <dir_dados> */
#include "acessos.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ler_texto(const char *titulo, char *dest, size_t cap) {
    printf("%s", titulo);
    if (!fgets(dest, (int)cap, stdin)) { dest[0] = '\0'; return; }
    dest[strcspn(dest, "\r\n")] = '\0';
}
static int64_t pedir_id(const char *prompt) {
    char s[64], *fim;
    ler_texto(prompt, s, sizeof s);
    long long n = strtoll(s, &fim, 10);
    if (!*s || *fim || n <= 0 || n > AC_ID_MAX) return -1;
    return n;
}
/* Consulta 2: contagem de compras do produto informado (varredura com 1 registro em RAM). */
static void contar_compras(TabelaAcessos *t) {
    int64_t id = pedir_id("ID do produto: ");
    if (id < 0) { puts("ID invalido."); return; }
    int64_t compras = 0;
    for (int64_t i = 0; i < t->quantidade; ++i) {
        Acesso a;
        if (acesso_ler(t->arquivo_dados, i, &a)) { puts("Falha de leitura."); return; }
        if (a.id_produto == id && strcmp(a.tipo, "purchase") == 0) ++compras;
    }
    printf("Total de compras do produto %" PRId64 ": %" PRId64 "\n", id, compras);
}
/* Consulta 3: evento mais frequente, sem carregar todos os acessos. */
static void evento_mais_frequente(TabelaAcessos *t) {
    const char *tipos[] = {"view", "cart", "purchase", "remove_from_cart", "outros"};
    int64_t cont[5] = {0};
    for (int64_t i = 0; i < t->quantidade; ++i) {
        Acesso a;
        if (acesso_ler(t->arquivo_dados, i, &a)) { puts("Falha de leitura."); return; }
        int j = 0;
        while (j < 4 && strcmp(tipos[j], a.tipo) != 0) ++j;
        ++cont[j];
    }
    int maior = 0;
    for (int i = 0; i < 5; ++i) {
        printf("%s: %" PRId64 "\n", tipos[i], cont[i]);
        if (cont[i] > cont[maior]) maior = i;
    }
    if (t->quantidade) printf("Mais frequente: %s (%" PRId64 " registros)\n", tipos[maior], cont[maior]);
}
static void inserir(TabelaAcessos *t) {
    Acesso a = {0};
    a.id_evento = pedir_id("ID novo do evento: ");
    a.id_produto = pedir_id("ID do produto: ");
    a.id_usuario = pedir_id("ID do usuario: ");
    if (a.id_evento <= 0 || a.id_produto <= 0 || a.id_usuario <= 0) { puts("IDs invalidos."); return; }
    ler_texto("Data (AAAA-MM-DD HH:MM:SS): ", a.data_hora, sizeof a.data_hora);
    ler_texto("Tipo (view/cart/purchase/remove_from_cart): ", a.tipo, sizeof a.tipo);
    ler_texto("Sessao: ", a.sessao, sizeof a.sessao);
    a.status = 'A';
    int r = acesso_inserir(t, &a);
    puts(r == 0 ? "Inserido. Dados ordenados e indice reconstruido." :
         r == 1 ? "Ja existe evento com este ID." : "Falha ao inserir.");
}
int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "data";
    TabelaAcessos t;
    if (acesso_abrir(&t, dir)) {
        fprintf(stderr, "Nao foi possivel abrir %s/acessos.bin. Execute gerar_acessos primeiro.\n", dir);
        return 1;
    }
    printf("Registros: %" PRId64 " | Indice: %s\n", t.quantidade,
           t.indice_carregado ? "carregado do arquivo" : "reconstruido");
    for (;;) {
        puts("\n--- ACESSOS AO E-COMMERCE ---\n"
             "1. Mostrar primeiros registros\n"
             "2. Buscar evento por ID (binaria direta)\n"
             "3. Buscar evento por ID (indice parcial)\n"
             "4. Inserir evento\n"
             "5. Remover evento\n"
             "6. Contar compras de um produto\n"
             "7. Tipo de evento mais frequente\n"
             "8. Informacoes dos arquivos\n"
             "0. Sair");
        char opcao[32];
        ler_texto("Opcao: ", opcao, sizeof opcao);
        if (strcmp(opcao, "0") == 0) break;
        if (strcmp(opcao, "1") == 0) {
            int64_t limite = pedir_id("Quantidade para mostrar: ");
            if (limite <= 0) puts("Quantidade invalida.");
            else if (acesso_listar(&t, limite)) puts("Erro ao listar.");
        } else if (strcmp(opcao, "2") == 0 || strcmp(opcao, "3") == 0) {
            int64_t id = pedir_id("ID do evento: ");
            Acesso a;
            int64_t pos = strcmp(opcao, "2") == 0 ? acesso_buscar_binaria(&t, id, &a)
                                                   : acesso_buscar_indice(&t, id, &a);
            if (pos < 0) puts("Registro nao encontrado.");
            else { printf("Posicao: %" PRId64 "\n", pos); acesso_imprimir(&a); }
        } else if (strcmp(opcao, "4") == 0) inserir(&t);
        else if (strcmp(opcao, "5") == 0) {
            int64_t id = pedir_id("ID a remover: ");
            int r = acesso_remover(&t, id);
            puts(r == 0 ? "Removido. Dados ordenados e indice reconstruido." :
                 r == 1 ? "Registro nao encontrado." : "Falha ao remover.");
        } else if (strcmp(opcao, "6") == 0) contar_compras(&t);
        else if (strcmp(opcao, "7") == 0) evento_mais_frequente(&t);
        else if (strcmp(opcao, "8") == 0) {
            printf("%s: %" PRId64 " registros de %d bytes\n", t.dados, t.quantidade, AC_TAM_REGISTRO);
            printf("%s: K=%" PRId64 ", M=%" PRId64 ", nivel 1=%" PRId64 ", nivel 2=%" PRId64 "\n",
                   t.indice, t.cab.k, t.cab.m, t.cab.n_nivel1, t.cab.n_nivel2);
        } else puts("Opcao invalida.");
    }
    acesso_fechar(&t);
    return 0;
}
