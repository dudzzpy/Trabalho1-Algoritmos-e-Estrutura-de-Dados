# Modulo de acessos. Este arquivo e incluido automaticamente pelo Makefile principal.
ALVOS += acessos gerar_acessos
TESTES += teste_acessos

acessos: src/menu_acessos.c src/acessos.c src/acessos.h
	$(CC) $(CFLAGS) -o $@ src/menu_acessos.c src/acessos.c

gerar_acessos: src/gerar_acessos.c src/acessos.c src/acessos.h
	$(CC) $(CFLAGS) -o $@ src/gerar_acessos.c src/acessos.c

teste_acessos: tests/teste_acessos.c src/acessos.c src/acessos.h
	$(CC) $(CFLAGS) -DACESSOS_K=3 -DACESSOS_M=2 -Isrc -o $@ tests/teste_acessos.c src/acessos.c
