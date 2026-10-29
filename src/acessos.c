#define _POSIX_C_SOURCE 200809L
#include "acessos.h"
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif

/* Somente constantes e buffers pequenos permanecem na RAM. */
_Static_assert(sizeof(CabecalhoIndiceAcesso) == 48, "Cabecalho precisa ter 48 bytes");
_Static_assert(sizeof(EntradaIndiceAcesso) == 16, "Entrada precisa ter 16 bytes");
static const char AC_MAGIC[8] = {'A','C','I','D','X','0','1','\0'};

static int seek64(FILE *f, int64_t deslocamento) {
    if (deslocamento < 0) return -1;
#ifdef _WIN32
    return _fseeki64(f, deslocamento, SEEK_SET);
#else
    return fseeko(f, (off_t)deslocamento, SEEK_SET);
#endif
}
static int64_t tamanho_arquivo(FILE *f) {
#ifdef _WIN32
    __int64 atual = _ftelli64(f);
    if (_fseeki64(f, 0, SEEK_END) != 0) return -1;
    __int64 fim = _ftelli64(f);
    if (atual >= 0) _fseeki64(f, atual, SEEK_SET);
    return (int64_t)fim;
#else
    off_t atual = ftello(f);
    if (fseeko(f, 0, SEEK_END) != 0) return -1;
    off_t fim = ftello(f);
    if (atual >= 0) fseeko(f, atual, SEEK_SET);
    return (int64_t)fim;
#endif
}
/* POSIX substitui arquivo existente diretamente. No Windows, usa backup
   temporario para permitir a troca sem apagar o arquivo original antes. */
static int substituir_arquivo(const char *temporario, const char *destino) {
#ifdef _WIN32
    char backup[520];
    if (snprintf(backup, sizeof backup, "%s.bak", destino) >= (int)sizeof backup) return -1;
    remove(backup);
    if (rename(destino, backup) != 0 && errno != ENOENT) return -1;
    if (rename(temporario, destino) != 0) {
        rename(backup, destino);
        return -1;
    }
    remove(backup);
    return 0;
#else
    return rename(temporario, destino);
#endif
}
static int caminho(char *dest, size_t cap, const char *dir, const char *nome) {
    int n = snprintf(dest, cap, "%s/%s", dir, nome);
    return n >= 0 && (size_t)n < cap ? 0 : -1;
}
int acesso_criar_diretorio(const char *dir) {
#ifdef _WIN32
    int r = _mkdir(dir);
#else
    int r = mkdir(dir, 0775);
#endif
    return r == 0 || errno == EEXIST ? 0 : -1;
}
static int numero_12(int64_t n, char *s) {
    if (n < 0 || n > AC_ID_MAX) return -1;
    char temp[32];
    if (snprintf(temp, sizeof temp, "%012" PRId64, n) != 12) return -1;
    memcpy(s, temp, 12);
    return 0;
}
static void preencher(char *dest, size_t largura, const char *origem) {
    size_t len = strlen(origem);
    if (len > largura) len = largura;
    memset(dest, ' ', largura);
    memcpy(dest, origem, len);
}
static void desempacotar(char *dest, size_t largura, const char *origem) {
    size_t n = largura;
    while (n && origem[n - 1] == ' ') --n;
    memcpy(dest, origem, n);
    dest[n] = '\0';
}
static int ler_12(const char *s, int64_t *out) {
    int64_t n = 0;
    for (int i = 0; i < 12; ++i) {
        if (s[i] < '0' || s[i] > '9') return -1;
        n = n * 10 + (s[i] - '0');
    }
    *out = n;
    return 0;
}
int acesso_valido(const Acesso *a) {
    return a && a->id_evento >= 1 && a->id_evento <= AC_ID_MAX &&
           a->id_produto >= 0 && a->id_produto <= AC_ID_MAX &&
           a->id_usuario >= 0 && a->id_usuario <= AC_ID_MAX &&
           a->data_hora[0] && a->tipo[0] &&
           strlen(a->data_hora) <= AC_TAM_DATA && strlen(a->tipo) <= AC_TAM_TIPO &&
           strlen(a->sessao) <= AC_TAM_SESSAO;
}
int acesso_gravar(FILE *f, const Acesso *a) {
    if (!f || !acesso_valido(a)) return -1;
    char buf[AC_TAM_REGISTRO];
    if (numero_12(a->id_evento, buf) || numero_12(a->id_produto, buf + 47) ||
        numero_12(a->id_usuario, buf + 59)) return -1;
    preencher(buf + 12, 19, a->data_hora);
    preencher(buf + 31, 16, a->tipo);
    preencher(buf + 71, 40, a->sessao);
    buf[111] = 'A';
    buf[112] = '\n';
    return fwrite(buf, 1, sizeof buf, f) == sizeof buf ? 0 : -1;
}
int acesso_ler(FILE *f, int64_t pos, Acesso *a) {
    if (!f || !a || pos < 0 || pos > INT64_MAX / AC_TAM_REGISTRO) return -1;
    char buf[AC_TAM_REGISTRO];
    if (seek64(f, pos * AC_TAM_REGISTRO) || fread(buf, 1, sizeof buf, f) != sizeof buf || buf[112] != '\n') return -1;
    if (ler_12(buf, &a->id_evento) || ler_12(buf + 47, &a->id_produto) ||
        ler_12(buf + 59, &a->id_usuario)) return -1;
    desempacotar(a->data_hora, 19, buf + 12);
    desempacotar(a->tipo, 16, buf + 31);
    desempacotar(a->sessao, 40, buf + 71);
    a->status = buf[111];
    return a->status == 'A' ? 0 : -1;
}
void acesso_imprimir(const Acesso *a) {
    printf("ID=%" PRId64 " | %s | %-16s | Produto=%" PRId64 " | Usuario=%" PRId64 " | Sessao=%s\n",
           a->id_evento, a->data_hora, a->tipo, a->id_produto, a->id_usuario, a->sessao);
}
static int escrever_entrada(FILE *f, int64_t chave, int64_t pos) {
    EntradaIndiceAcesso e = {chave, pos};
    return fwrite(&e, sizeof e, 1, f) == 1 ? 0 : -1;
}
static int ler_entrada(FILE *f, int64_t pos, EntradaIndiceAcesso *e) {
    if (pos < 0 || pos > (INT64_MAX - 48) / 16 ||
        seek64(f, 48 + pos * 16)) return -1;
    return fread(e, sizeof *e, 1, f) == 1 ? 0 : -1;
}
/* Constroi [cabecalho][nivel 2][nivel 1] sem usar vetor de indices em RAM. */
int acesso_reconstruir_indice(TabelaAcessos *t) {
    if (!t || !t->arquivo_dados) return -1;
    char temp[sizeof t->indice + 5];
    if (snprintf(temp, sizeof temp, "%s.tmp", t->indice) >= (int)sizeof temp) return -1;
    FILE *f = fopen(temp, "wb");
    if (!f) return -1;
    const int64_t k = ACESSOS_K, m = ACESSOS_M;
    const int64_t n1 = (t->quantidade + k - 1) / k;
    const int64_t n2 = (n1 + m - 1) / m;
    CabecalhoIndiceAcesso cab = {{0}, k, m, t->quantidade, n1, n2};
    memcpy(cab.magic, AC_MAGIC, 8);
    int erro = fwrite(&cab, sizeof cab, 1, f) != 1;
    Acesso a;
    /* Nivel 2: 1 entrada a cada m entradas do nivel 1. */
    for (int64_t i = 0; !erro && i < n2; ++i) {
        if (acesso_ler(t->arquivo_dados, i * m * k, &a) ||
            escrever_entrada(f, a.id_evento, i * m)) erro = 1;
    }
    /* Nivel 1: 1 entrada a cada k registros de dados. */
    for (int64_t i = 0; !erro && i < n1; ++i) {
        if (acesso_ler(t->arquivo_dados, i * k, &a) ||
            escrever_entrada(f, a.id_evento, i * k)) erro = 1;
    }
    if (fclose(f) != 0) erro = 1;
    if (erro) { remove(temp); return -1; }
    if (t->arquivo_indice) { fclose(t->arquivo_indice); t->arquivo_indice = NULL; }
    /* Arquivo antigo preservado se falhar a escrita do temporario. */
    if (substituir_arquivo(temp, t->indice) != 0) { remove(temp); return -1; }
    t->arquivo_indice = fopen(t->indice, "rb");
    if (!t->arquivo_indice) return -1;
    t->cab = cab;
    t->indice_carregado = 0;
    return 0;
}
static int indice_valido(TabelaAcessos *t) {
    if (!t->arquivo_indice) return 0;
    CabecalhoIndiceAcesso c;
    if (seek64(t->arquivo_indice, 0) || fread(&c, sizeof c, 1, t->arquivo_indice) != 1) return 0;
    int64_t n1 = (t->quantidade + ACESSOS_K - 1) / ACESSOS_K;
    int64_t n2 = (n1 + ACESSOS_M - 1) / ACESSOS_M;
    if (memcmp(c.magic, AC_MAGIC, 8) || c.k != ACESSOS_K || c.m != ACESSOS_M ||
        c.n_registros != t->quantidade || c.n_nivel1 != n1 || c.n_nivel2 != n2 ||
        tamanho_arquivo(t->arquivo_indice) != 48 + 16 * (n1 + n2)) return 0;
    t->cab = c;
    return 1;
}
int acesso_abrir(TabelaAcessos *t, const char *dir) {
    if (!t || !dir) return -1;
    memset(t, 0, sizeof *t);
    if (caminho(t->dados, sizeof t->dados, dir, "acessos.bin") ||
        caminho(t->indice, sizeof t->indice, dir, "acessos.idx")) return -1;
    t->arquivo_dados = fopen(t->dados, "rb");
    if (!t->arquivo_dados) return -1;
    int64_t tam = tamanho_arquivo(t->arquivo_dados);
    if (tam < 0 || tam % AC_TAM_REGISTRO) { acesso_fechar(t); return -1; }
    t->quantidade = tam / AC_TAM_REGISTRO;
    t->arquivo_indice = fopen(t->indice, "rb");
    if (indice_valido(t)) { t->indice_carregado = 1; return 0; }
    if (t->arquivo_indice) { fclose(t->arquivo_indice); t->arquivo_indice = NULL; }
    if (acesso_reconstruir_indice(t)) { acesso_fechar(t); return -1; }
    return 0;
}
void acesso_fechar(TabelaAcessos *t) {
    if (!t) return;
    if (t->arquivo_dados) { fclose(t->arquivo_dados); t->arquivo_dados = NULL; }
    if (t->arquivo_indice) { fclose(t->arquivo_indice); t->arquivo_indice = NULL; }
}
int64_t acesso_buscar_binaria(TabelaAcessos *t, int64_t id, Acesso *saida) {
    if (!t || !t->arquivo_dados) return -1;
    int64_t lo = 0, hi = t->quantidade;
    Acesso a;
    while (lo < hi) {
        int64_t meio = lo + (hi - lo) / 2;
        if (acesso_ler(t->arquivo_dados, meio, &a)) return -1;
        if (a.id_evento < id) lo = meio + 1;
        else hi = meio;
    }
    if (lo < t->quantidade && !acesso_ler(t->arquivo_dados, lo, &a) && a.id_evento == id) {
        if (saida) *saida = a;
        return lo;
    }
    return -1;
}
/* Ultima entrada com chave <= id, usando busca binaria diretamente no indice. */
static int64_t antecessor(FILE *f, int64_t base, int64_t n, int64_t chave) {
    int64_t lo = 0, hi = n;
    EntradaIndiceAcesso e;
    while (lo < hi) {
        int64_t meio = lo + (hi - lo) / 2;
        if (ler_entrada(f, base + meio, &e)) return -1;
        if (e.chave <= chave) lo = meio + 1;
        else hi = meio;
    }
    return lo ? lo - 1 : -1;
}
int64_t acesso_buscar_indice(TabelaAcessos *t, int64_t id, Acesso *saida) {
    if (!t || !t->arquivo_dados || !t->arquivo_indice || !t->cab.n_nivel2) return -1;
    const int64_t n1 = t->cab.n_nivel1, n2 = t->cab.n_nivel2;
    int64_t bloco2 = antecessor(t->arquivo_indice, 0, n2, id);
    if (bloco2 < 0) return -1;
    EntradaIndiceAcesso e2;
    if (ler_entrada(t->arquivo_indice, bloco2, &e2)) return -1;
    int64_t inicio1 = e2.pos;
    int64_t qtd1 = n1 - inicio1;
    if (qtd1 > t->cab.m) qtd1 = t->cab.m;
    int64_t bloco1 = antecessor(t->arquivo_indice, n2 + inicio1, qtd1, id);
    if (bloco1 < 0) return -1;
    EntradaIndiceAcesso e1;
    if (ler_entrada(t->arquivo_indice, n2 + inicio1 + bloco1, &e1)) return -1;
    int64_t fim = e1.pos + t->cab.k;
    if (fim > t->quantidade) fim = t->quantidade;
    for (int64_t i = e1.pos; i < fim; ++i) {
        Acesso a;
        if (acesso_ler(t->arquivo_dados, i, &a)) return -1;
        if (a.id_evento == id) { if (saida) *saida = a; return i; }
        if (a.id_evento > id) break;
    }
    return -1;
}
int acesso_listar(TabelaAcessos *t, int64_t limite) {
    if (!t || !t->arquivo_dados) return -1;
    if (limite <= 0 || limite > t->quantidade) limite = t->quantidade;
    for (int64_t i = 0; i < limite; ++i) {
        Acesso a;
        if (acesso_ler(t->arquivo_dados, i, &a)) return -1;
        acesso_imprimir(&a);
    }
    return 0;
}
/* Copia os registros 1 a 1 para um arquivo novo; nenhum vetor de registros. */
static int modificar(TabelaAcessos *t, int remover, int64_t id, const Acesso *novo) {
    if (!t || !t->arquivo_dados) return -1;
    if (!remover && !acesso_valido(novo)) return -1;
    if (acesso_buscar_binaria(t, id, NULL) >= 0) {
        if (!remover) return 1;
    } else if (remover) return 1;
    char temp[sizeof t->dados + 5];
    if (snprintf(temp, sizeof temp, "%s.tmp", t->dados) >= (int)sizeof temp) return -1;
    FILE *out = fopen(temp, "wb");
    if (!out) return -1;
    int incluido = 0, erro = 0;
    for (int64_t i = 0; !erro && i < t->quantidade; ++i) {
        Acesso atual;
        if (acesso_ler(t->arquivo_dados, i, &atual)) { erro = 1; break; }
        if (!remover && !incluido && atual.id_evento > id) {
            if (acesso_gravar(out, novo)) { erro = 1; break; }
            incluido = 1;
        }
        if (remover && atual.id_evento == id) continue;
        if (acesso_gravar(out, &atual)) erro = 1;
    }
    if (!erro && !remover && !incluido && acesso_gravar(out, novo)) erro = 1;
    if (fclose(out)) erro = 1;
    if (erro) { remove(temp); return -1; }
    /* Substituicao POSIX atomica. O arquivo de indice e refeito na sequencia. */
    fclose(t->arquivo_dados);
    t->arquivo_dados = NULL;
    if (substituir_arquivo(temp, t->dados)) {
        remove(temp);
        t->arquivo_dados = fopen(t->dados, "rb");
        return -1;
    }
    t->arquivo_dados = fopen(t->dados, "rb");
    if (!t->arquivo_dados) return -1;
    t->quantidade += remover ? -1 : 1;
    return acesso_reconstruir_indice(t);
}
int acesso_inserir(TabelaAcessos *t, const Acesso *novo) {
    if (!novo) return -1;
    return modificar(t, 0, novo->id_evento, novo);
}
int acesso_remover(TabelaAcessos *t, int64_t id) {
    return modificar(t, 1, id, NULL);
}
