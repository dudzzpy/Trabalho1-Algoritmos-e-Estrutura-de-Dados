CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c11
COMUM   = src/produto.c src/indice.c src/tabela.c
HEADERS = $(wildcard src/*.h)

# Alvos de cada parte do projeto. A parte dos acessos (colega) NAO edita este
# arquivo: cria um acessos.mk que acrescenta seus alvos a ALVOS e TESTES.
ALVOS   = produtos gerar_produtos
TESTES  = teste_produtos

-include acessos.mk

.DEFAULT_GOAL := all
.PHONY: all teste clean

all: data $(ALVOS)

# O git nao versiona pastas vazias; garante que data/ exista.
data:
	mkdir -p data

produtos: src/main.c $(COMUM) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ src/main.c $(COMUM)

gerar_produtos: src/gerar_produtos.c src/produto.c $(HEADERS)
	$(CC) $(CFLAGS) -o $@ src/gerar_produtos.c src/produto.c

# Intervalos pequenos exercitam os 2 niveis do indice e a reorganizacao.
teste_produtos: tests/teste_produtos.c $(COMUM) $(HEADERS)
	$(CC) $(CFLAGS) -DINTERVALO_NIVEL1=7 -DINTERVALO_NIVEL2=5 -DLIMITE_OVERFLOW=20 \
	    -Isrc -o $@ tests/teste_produtos.c $(COMUM)

teste: $(TESTES)
	@for t in $(TESTES); do echo "== $$t"; ./$$t || exit 1; done

clean:
	rm -f $(ALVOS) $(TESTES)
