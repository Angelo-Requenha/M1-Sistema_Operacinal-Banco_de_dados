# Sistema de Processamento Paralelo de Requisições a um Banco de Dados

Avaliação M1 — Sistemas Operacionais — Univali
Tema: IPC, Threads e Exclusão Mútua

Dois processos independentes conversam por **memória compartilhada**. O
servidor atende as requisições com um **pool de threads** e protege a
tabela compartilhada com **mutex**.

Roda em **Windows nativo**: não precisa de WSL, Linux nem instalar nada. Os
binários já vêm prontos em `bin/`.

---

## Como rodar

1. Abra `executar.bat`
2. Opção `1` para subir o servidor, depois `3` ou `4` para o cliente

Para ver o pool de threads trabalhando em paralelo de verdade, abra **2 ou 3
janelas de cliente ao mesmo tempo** (opção 3, com requisições diferentes) e
depois confira o `servidor.log` (opção 5): dá pra ver requisições de
processos diferentes sendo atendidas por threads diferentes.

Pela linha de comando:

```
bin\servidor.exe --threads 4 --custo 500     (em uma janela)
bin\cliente.exe INSERT 7 Joao                (em outra)
bin\cliente.exe SELECT 7
bin\cliente.exe ENCERRAR
```

---

## Estrutura

Cada programa está dividido em três arquivos, um por responsabilidade:
**front** (tela), **back** (regra) e **comunicação** (IPC).

```
trabalho-m1/
├── src/
│   ├── cabecalhos/
│   │   ├── protocolo.h       # contrato dos DOIS binários: structs e layout da memória
│   │   ├── servidor.h        # o que os 3 arquivos do servidor expõem entre si
│   │   └── cliente.h         # o que os 3 arquivos do cliente expõem entre si
│   ├── servidor/
│   │   ├── comunicacao.cpp   # IPC:  memória compartilhada, fila, slots, pool de threads
│   │   ├── banco.cpp         # back: tabela, mutex, operações, log
│   │   └── terminal.cpp      # front: main e tudo que aparece na tela
│   └── cliente/
│       ├── comunicacao.cpp   # IPC:  abre a memória, envia e espera a resposta
│       ├── requisicao.cpp    # back: traduz texto digitado em Requisicao
│       └── terminal.cpp      # front: main, modo interativo e impressão
├── bin/                      # servidor.exe e cliente.exe (versionados)
├── banco.txt                 # a "tabela" persistida em disco
├── servidor.log              # log em arquivo (gerado em execução)
├── compilar.bat
└── executar.bat
```

---

## Como o sistema funciona

### Dois processos separados

`servidor.exe` e `cliente.exe` são executáveis distintos, rodando como
processos de sistema operacional independentes. Eles não compartilham
variável nem chamam função um do outro: conversam **só** por objetos do
sistema, criados pelo servidor e abertos pelo cliente através de um nome
combinado (`Local\so_m1_*`, declarados em `protocolo.h`).

| o que é | objeto do Windows |
|---|---|
| a área de dados compartilhada | `CreateFileMapping` + `MapViewOfFile` |
| os semáforos entre processos | `CreateSemaphore` (nomeado) |
| as threads do pool | **Pthreads** (winpthreads) |
| mutex sobre a tabela | **Pthreads** |

As threads e o mutex usam Pthreads dos dois lados — o `g++` do MinGW-w64 na
variante POSIX traz o winpthreads, a mesma API do Linux compilada para
Windows. Só a camada entre processos é específica do Windows.

### Os dois canais

Tudo vive numa única área compartilhada, descrita pela struct
`AreaCompartilhada` em `protocolo.h`. Dentro dela há dois canais:

**Canal 1 — fila de requisições.** Buffer circular clássico de
produtor/consumidor. O cliente é o produtor, as threads do pool são as
consumidoras. Três semáforos:

| semáforo | papel |
|---|---|
| `so_m1_vazios` | quantos espaços livres restam na fila |
| `so_m1_cheios` | quantas requisições aguardam atendimento |
| `so_m1_fila` | binário (mutex); exclusão mútua sobre `cabeca`/`cauda` |

**Canal 2 — slots de resposta.** Um vetor de respostas com um único
semáforo não serviria: dois clientes simultâneos roubariam a resposta um do
outro. Cada requisição em voo reserva um slot exclusivo (de um total de
`MAX_SLOTS = 8`, o suficiente para demonstrar alguns clientes concorrentes)
e dorme no semáforo `so_m1_pronta_<slot>` até o servidor depositar lá.

### Exclusão mútua sobre a tabela

A tabela compartilhada é protegida por um único `pthread_mutex_t`: qualquer
operação (`INSERT`, `SELECT`, `UPDATE`, `DELETE`, `LISTAR`) precisa travar
o mutex antes de mexer na tabela e destravar depois. Isso garante que duas
threads nunca leem/escrevem ao mesmo tempo, evitando condição de corrida.

O parâmetro `--custo MS` do servidor (padrão sugerido: 500ms) segura o
mutex por um tempo perceptível durante cada operação — é isso que permite
ver, no `servidor.log`, requisições de clientes diferentes esperando a
liberação do mutex antes de serem atendidas, provando a exclusão mútua na
prática.

### Log em arquivo

Cada linha do `servidor.log` mostra qual thread do pool atendeu qual
requisição, com timestamp. É a evidência de que o pool tem várias threads
ativas, mesmo que a tabela seja acessada uma de cada vez.

---

## Requisições suportadas

| comando | efeito |
|---|---|
| `INSERT <id> <nome>` | adiciona registro |
| `SELECT <id>` | busca por id |
| `UPDATE <id> <nome>` | altera o nome |
| `DELETE <id>` | remove o registro |
| `LISTAR` | total de registros |
| `ENCERRAR` | desliga o servidor |

---

## Parâmetros

**Servidor**

```
bin\servidor.exe [--threads N] [--custo MS]
  --threads N   tamanho do pool (padrao 4)
  --custo MS    atraso artificial por requisição, simula I/O do banco
```

**Cliente**

```
bin\cliente.exe INSERT <id> <nome>
bin\cliente.exe SELECT <id>
bin\cliente.exe UPDATE <id> <nome>
bin\cliente.exe DELETE <id>
bin\cliente.exe LISTAR
bin\cliente.exe ENCERRAR
bin\cliente.exe -i     modo interativo
```

---

## Problemas comuns

**"nao encontrei a memoria compartilhada"** — o servidor não está no ar.
Suba primeiro (opção 1 do `executar.bat`).

**"ja existe um servidor rodando"** — há outra janela do servidor aberta.
Feche-a, ou encerre pela opção 2.

Ao contrário do Linux, aqui **não sobra lixo**: o Windows destrói a memória
compartilhada e os semáforos sozinho quando o último processo fecha, mesmo
se o servidor for morto à força pelo Gerenciador de Tarefas.

---

## Limites conhecidos

- `salvar_banco()` reescreve `banco.txt` inteiro dentro da seção crítica.
  Correto, mas segura o mutex por mais tempo que o necessário.
- A busca por id é linear (`O(n)`). Com `MAX_REGISTROS = 1024` não pesa.
- Toda operação (mesmo `SELECT`) é serializada pelo mesmo mutex: não há
  leitura simultânea. É a opção mais simples e correta para o escopo deste
  trabalho — o enunciado cita semáforo "para controle mais geral, como
  múltiplas leituras simultâneas" como possibilidade extra, não obrigação.
