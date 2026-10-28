CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c11
COMUM   = src/produto.c src/indice.c src/tabela.c

all: produtos gerar_produtos

produtos: src/main.c $(COMUM) src/*.h
	$(CC) $(CFLAGS) -o $@ src/main.c $(COMUM)

gerar_produtos: src/gerar_produtos.c src/produto.c src/*.h
	$(CC) $(CFLAGS) -o $@ src/gerar_produtos.c src/produto.c

# Teste automatizado; intervalos pequenos exercitam os 2 niveis do indice.
teste: tests/teste_produtos.c $(COMUM) src/*.h
	$(CC) $(CFLAGS) -DINTERVALO_NIVEL1=7 -DINTERVALO_NIVEL2=5 -DLIMITE_OVERFLOW=20 \
	    -Isrc -o teste_produtos tests/teste_produtos.c $(COMUM)
	./teste_produtos

clean:
	rm -f produtos gerar_produtos teste_produtos
