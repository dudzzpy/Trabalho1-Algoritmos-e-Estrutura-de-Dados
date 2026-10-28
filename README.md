# Trabalho 1 - Arquivo de produtos (sequencial-indexado)

## Compilar e executar
    make                                  # gera ./produtos e ./gerar_produtos
    make teste                            # testes automatizados
    ./gerar_produtos data 2000000 2019-Nov.csv   # 0 = sem limite de linhas
    ./produtos data                       # menu

## produtos.bin (registro fixo de 93 bytes, texto, terminado em '\n')
| Campo        | Bytes | Observacao                              |
|--------------|-------|-----------------------------------------|
| id_produto   | 10    | chave unica, zeros a esquerda           |
| marca        | 30    | preenchido com espacos                  |
| categoria    | 40    | preenchido com espacos                  |
| preco        | 11    | `%011.2f` (ex.: 00001299.90)            |
| status       | 1     | `A` ativo / `R` removido (remocao logica)|
| fim          | 1     | `\n`                                    |

Posicao do registro n = n * 93. Ordenado por id_produto.

## produtos.idx (indice parcial de 2 niveis)
Cabecalho (48 bytes: magic, K, M, n_registros, n_nivel1, n_nivel2), depois o nivel 2 e o nivel 1,
com entradas de 16 bytes `(chave int64, posicao int64)`.
- Nivel 1: 1 entrada a cada K = 100 registros do arquivo de dados.
- Nivel 2: 1 entrada a cada M = 100 entradas do nivel 1.
- Consulta: busca binaria no nivel 2 -> busca binaria no bloco do nivel 1 -> seek no
  arquivo de dados e varredura de ate K registros. Tudo com fseek/fread no arquivo.
- Ao abrir, o indice e validado (magic, n_registros, tamanho); se invalido, e reconstruido.

## Insercao, remocao e reconstrucao
- Remocao: logica (status = `R`), no principal ou no overflow.
- Insercao: anexada a `produtos.ovf` (nao ordenado, ate 100 registros).
- Reorganizacao: com overflow cheio, por comando, ou ao sair com overflow nao vazio -> merge
  em streaming (principal sem removidos + overflow ordenado) em arquivo temporario, troca do
  arquivo e reconstrucao/gravacao do indice.
