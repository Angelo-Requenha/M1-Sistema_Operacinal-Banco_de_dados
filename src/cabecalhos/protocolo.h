/* protocolo.h - contrato que cliente e servidor enxergam da mesma forma.
 * Os dois binarios abrem os mesmos objetos do Windows pelo nome e leem a
 * memoria compartilhada como AreaCompartilhada, entao este arquivo define
 * o layout exato dessa memoria e os nomes usados pelos dois lados. */

#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include <cstdint>

/* Nomes dos objetos do Windows. "Local\" = visivel na sessao do usuario.
 * O servidor cria, o cliente abre pelo mesmo nome. */
#define NOME_SHM        "Local\\so_m1_banco"
#define NOME_VAZIOS     "Local\\so_m1_vazios"
#define NOME_CHEIOS     "Local\\so_m1_cheios"
#define NOME_FILA       "Local\\so_m1_fila"
#define NOME_SLOTS      "Local\\so_m1_slots"
#define NOME_PILHA      "Local\\so_m1_pilha"
#define NOME_PRONTA     "Local\\so_m1_pronta_"   /* + numero do slot */

#define ARQUIVO_BANCO   "banco.txt"
#define ARQUIVO_LOG     "servidor.log"

#define MAX_REGISTROS   1024
#define TAM_NOME        50
#define TAM_FILA        16
#define MAX_SLOTS       8      /* respostas em voo ao mesmo tempo (ex.: 2-3 clientes simultaneos) */
#define TAM_TEXTO       160
#define TAM_LINHA_LOG   256
#define PADRAO_THREADS  4

/* int32_t: a struct atravessa a fronteira entre dois processos, o tamanho
 * do campo nao pode depender do compilador. */
enum Operacao : int32_t {
    OP_NENHUMA  = 0,
    OP_INSERT   = 1,
    OP_SELECT   = 2,
    OP_UPDATE   = 3,
    OP_DELETE   = 4,
    OP_LISTAR   = 5,
    OP_ENCERRAR = 6
};

typedef struct {
    int  id;
    char nome[TAM_NOME];
} Registro;

typedef struct {
    Operacao op;
    int      id;
    char     nome[TAM_NOME];
    int      slot;         /* onde o servidor deposita a resposta */
    int      pid_cliente;
} Requisicao;

typedef struct {
    int  ok;
    int  id;
    char nome[TAM_NOME];
    char texto[TAM_TEXTO];
    int  thread_atendeu;
} Resposta;

/* Só dados moram aqui. Os semaforos do Windows sao objetos do sistema,
 * abertos pelo nome em cada processo, e nao cabem dentro da struct. */
typedef struct {
    /* canal 1 - fila circular de requisicoes (produtor/consumidor) */
    Requisicao fila[TAM_FILA];
    int        cabeca;
    int        cauda;

    /* canal 2 - slots de resposta (poucos: so_m1 suporta alguns
     * clientes concorrentes, o suficiente para demonstrar o pool) */
    Resposta resposta[MAX_SLOTS];
    int      pilha_livres[MAX_SLOTS];
    int      topo_livres;

    int servidor_ativo;
    int num_threads;
} AreaCompartilhada;

#endif
