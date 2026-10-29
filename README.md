
# Trabalho 1 - Arquivos de produtos e acessos (sequencial-indexado)

## Compilar e executar

```bash
make                                            # gera os programas de produtos e acessos
make teste                                      # testes automatizados

./gerar_produtos data 100000 2019-Nov.csv       # gera produtos.bin e produtos.idx
./gerar_acessos data 100000 2019-Nov.csv        # gera acessos.bin e acessos.idx

./produtos data                                 # menu de produtos
./acessos data                                  # menu de acessos
```

O valor `0` nos geradores indica que não há limite de linhas. Os arquivos CSV foram obtidos do dataset disponibilizado no Kaggle.

Dataset: https://www.kaggle.com/datasets/mkechinov/ecommerce-behavior-data-from-multi-category-store

## produtos.bin (registro fixo de 93 bytes, terminado em '\n')

| Campo | Bytes | Observacao |
|-------|-------|------------|
| id_produto | 10 | chave unica, zeros a esquerda |
| marca | 30 | preenchido com espacos |
| categoria | 40 | preenchido com espacos |
| preco | 11 | `%011.2f` (ex.: 00001299.90) |
| status | 1 | `A` ativo / `R` removido (remocao logica) |
| fim | 1 | `\n` |

Posicao do registro n = n * 93. Ordenado por id_produto.

## produtos.idx (indice parcial de 2 niveis)

Cabecalho (48 bytes: magic, K, M, n_registros, n_nivel1, n_nivel2), depois o nivel 2 e o nivel 1, com entradas de 16 bytes `(chave int64, posicao int64)`.

- Nivel 1: 1 entrada a cada K = 100 registros do arquivo de dados.
- Nivel 2: 1 entrada a cada M = 100 entradas do nivel 1.
- Consulta: busca binaria no nivel 2 -> busca binaria no bloco do nivel 1 -> seek no arquivo de dados e varredura de ate K registros. Tudo com fseek/fread no arquivo.
- Ao abrir, o indice e validado (magic, n_registros, tamanho); se invalido, e reconstruido.

## acessos.bin (registro fixo de 113 bytes, terminado em '\n')

| Campo | Bytes | Observacao |
|-------|-------|------------|
| id_evento | 12 | chave unica sequencial, zeros a esquerda |
| data_hora | 19 | data e hora do evento |
| tipo | 16 | tipo de evento, preenchido com espacos |
| id_produto | 12 | identificador do produto |
| id_usuario | 12 | identificador do usuario |
| sessao | 40 | identificador da sessao, preenchido com espacos |
| status | 1 | `A` ativo |
| fim | 1 | `\n` |

Posicao do registro n = n * 113. Ordenado por id_evento.

Os identificadores dos eventos sao gerados sequencialmente a partir dos registros validos do dataset, permitindo que cada acesso possua uma chave unica.

## acessos.idx (indice parcial de 2 niveis)

Cabecalho (48 bytes: magic, K, M, n_registros, n_nivel1, n_nivel2), seguido pelo nivel 2 e pelo nivel 1, com entradas de 16 bytes `(chave int64, posicao int64)`.

- Nivel 1: 1 entrada a cada K = 100 registros do arquivo de dados.
- Nivel 2: 1 entrada a cada M = 100 entradas do nivel 1.
- Consulta: busca binaria no nivel 2 -> busca binaria no bloco do nivel 1 -> seek no arquivo de dados e varredura de ate K registros.
- Ao abrir, o indice e validado. Se estiver ausente ou invalido, e reconstruido.

## Insercao, remocao e reconstrucao

### Produtos

- Remocao: logica (status = `R`), no principal ou no overflow.
- Insercao: anexada a `produtos.ovf` (nao ordenado, ate 100 registros).
- Reorganizacao: com overflow cheio, por comando, ou ao sair com overflow nao vazio -> merge em streaming (principal sem removidos + overflow ordenado) em arquivo temporario, troca do arquivo e reconstrucao/gravacao do indice.

### Acessos

- Insercao: realizada em ordem crescente de id_evento, por meio da reescrita sequencial do arquivo de dados.
- Remocao: fisica, com reescrita do arquivo sem o registro removido.
- Reconstrucao: o indice parcial e reconstruido apos cada insercao ou remocao.
- As operacoes utilizam leitura e escrita sequencial, sem carregar todos os registros na memoria RAM.

## Perguntas de pesquisa

Para o desenvolvimento do trabalho, foram definidas tres perguntas a serem respondidas utilizando os arquivos de dados do e-commerce:

**1. Qual e a marca, categoria e preco de um produto a partir de seu identificador?**

A consulta utiliza o arquivo `produtos.bin` para localizar o produto e apresentar suas informacoes. Pode ser executada pelo programa `produtos`, utilizando a pesquisa binaria ou o indice parcial.

**2. Quais sao os dados de um evento especifico realizado no e-commerce?**

A consulta utiliza o arquivo `acessos.bin` para localizar um evento pelo seu identificador e apresentar sua data, tipo, produto, usuario e sessao. O programa permite realizar essa pesquisa diretamente no arquivo ou utilizando o indice parcial.

**3. Como verificar se um produto está cadastrado e ativo utilizando seu identificador e o índice parcial?**

A consulta utiliza o índice parcial de produtos para localizar a região do arquivo onde o registro deve estar armazenado. Em seguida, realiza a pesquisa no bloco correspondente e verifica a existência e a situação do produto, sem precisar percorrer todo o arquivo de dados.

## Validacao com dados reais

Para validar a implementacao, foi utilizada uma amostra de 100.000 eventos do dataset *eCommerce behavior data from multi category store*, disponibilizado no Kaggle.

A partir dessa amostra, foram gerados:

- 20.675 registros distintos de produtos;
- 100.000 registros de acessos;
- Um indice parcial para produtos;
- Um indice parcial para acessos.

Os arquivos de dados e indices gerados estao disponiveis no diretorio `data/`.

Para executar os testes automatizados dos dois modulos:

```bash
make teste
```

As operacoes de consulta, insercao e remocao podem ser verificadas diretamente pelos menus dos programas `produtos` e `acessos`.
