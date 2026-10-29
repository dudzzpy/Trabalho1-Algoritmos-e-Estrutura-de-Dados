/* Geracao streaming, sem carregar o CSV inteiro na RAM.
   Uso: ./gerar_acessos <dir_saida> <max_linhas|0> <arquivo.csv> [outro.csv ...]
   CSV Kaggle: event_time,event_type,product_id,category_id,category_code,
               brand,price,user_id,user_session */
#include "acessos.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define TAM_LINHA_CSV 8192

/* CSV com campos vazios e aspas duplas, incluindo virgulas entre aspas. */
static int dividir_csv(char *s, char *campos[], int max) {
    int qtd = 0;
    char *p = s;
    while (*p && qtd < max) {
        campos[qtd++] = p;
        char *dest = p;
        int aspas = 0;
        while (*p && (*p != ',' || aspas)) {
            if (*p == '"') {
                if (aspas && p[1] == '"') { *dest++ = '"'; p += 2; }
                else { aspas = !aspas; ++p; }
            } else *dest++ = *p++;
        }
        int separador = (*p == ',');
        *dest = '\0';
        if (separador) { ++p; if (!*p && qtd < max) campos[qtd++] = p; }
    }
    return qtd;
}
static int inteiro(const char *s, int64_t *n) {
    if (!s || !*s) return -1;
    errno = 0;
    char *fim;
    long long v = strtoll(s, &fim, 10);
    if (errno || *fim || v < 0 || v > AC_ID_MAX) return -1;
    *n = v;
    return 0;
}
int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Uso: %s <dir_saida> <max_linhas|0> <arquivo.csv> [outro.csv...]\n", argv[0]);
        return 1;
    }
    errno = 0;
    char *fim;
    unsigned long long limite = strtoull(argv[2], &fim, 10);
    if (errno || *fim || argv[2][0] == '-') { fprintf(stderr, "Limite invalido.\n"); return 1; }
    if (acesso_criar_diretorio(argv[1])) { perror("Diretorio"); return 1; }
    char path[512];
    if (snprintf(path, sizeof path, "%s/acessos.bin", argv[1]) >= (int)sizeof path) return 1;
    FILE *dados = fopen(path, "wb");
    if (!dados) { perror(path); return 1; }
    char linha[TAM_LINHA_CSV];
    int64_t gerados = 0;
    unsigned long long ignorados = 0;
    int erro = 0;
    for (int arq = 3; !erro && arq < argc && (limite == 0 || (unsigned long long)gerados < limite); ++arq) {
        FILE *csv = fopen(argv[arq], "r");
        if (!csv) { perror(argv[arq]); erro = 1; break; }
        int primeira = 1;
        while (fgets(linha, sizeof linha, csv) && (limite == 0 || (unsigned long long)gerados < limite)) {
            if (!strchr(linha, '\n') && !feof(csv)) {
                int c;
                while ((c = fgetc(csv)) != '\n' && c != EOF) {}
                ++ignorados;
                continue;
            }
            linha[strcspn(linha, "\r\n")] = '\0';
            if (primeira) { primeira = 0; if (strncmp(linha, "event_time,", 11) == 0) continue; }
            char *campos[9] = {0};
            if (dividir_csv(linha, campos, 9) != 9) { ++ignorados; continue; }
            if (gerados >= AC_ID_MAX) { erro = 1; break; }
            Acesso a = {0};
            a.id_evento = gerados + 1;
            if (inteiro(campos[2], &a.id_produto) || inteiro(campos[7], &a.id_usuario) ||
                !campos[0][0] || !campos[1][0]) { ++ignorados; continue; }
            snprintf(a.data_hora, sizeof a.data_hora, "%.19s", campos[0]);
            snprintf(a.tipo, sizeof a.tipo, "%.16s", campos[1]);
            snprintf(a.sessao, sizeof a.sessao, "%.40s", campos[8]);
            a.status = 'A';
            if (acesso_gravar(dados, &a)) { erro = 1; break; }
            ++gerados;
        }
        if (ferror(csv)) erro = 1;
        fclose(csv);
    }
    if (fclose(dados)) erro = 1;
    if (erro) { fprintf(stderr, "Erro ao gerar registros.\n"); return 1; }
    TabelaAcessos t;
    if (acesso_abrir(&t, argv[1])) {
        fprintf(stderr, "Falha ao abrir e construir indice.\n"); return 1;
    }
    acesso_fechar(&t);
    printf("Gerados: %" PRId64 " registros (%d bytes cada); ignorados: %llu.\n",
           gerados, AC_TAM_REGISTRO, ignorados);
    printf("Arquivos: %s/acessos.bin e %s/acessos.idx\n", argv[1], argv[1]);
    return 0;
}
