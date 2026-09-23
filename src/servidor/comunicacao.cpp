/* servidor/comunicacao.cpp - lado IPC do servidor.
 * Cria a memoria compartilhada e os semaforos do Windows, retira
 * requisicoes da fila circular, devolve respostas nos slots e mantem o
 * pool de threads (pthreads). */

#include "servidor.h"

#include <windows.h>
#include <pthread.h>
#include <cstdio>
#include <cstring>

static HANDLE mapeamento = nullptr;
static AreaCompartilhada* area = nullptr;
static int total_threads = 0;

static HANDLE sem_vazios = nullptr;   /* espacos livres na fila   */
static HANDLE sem_cheios = nullptr;   /* requisicoes aguardando   */
static HANDLE sem_fila   = nullptr;   /* binario: cabeca/cauda    */
static HANDLE sem_slots  = nullptr;   /* slots de resposta livres */
static HANDLE sem_pilha  = nullptr;   /* binario: pilha de slots  */
static HANDLE sem_pronta[MAX_SLOTS] = {nullptr};

/* esperar = decrementa o semaforo, bloqueando se estiver em zero
 * liberar = incrementa e acorda quem estiver esperando */
static void esperar(HANDLE s) { WaitForSingleObject(s, INFINITE); }
static void liberar(HANDLE s) { ReleaseSemaphore(s, 1, nullptr); }

static HANDLE criar_semaforo(const char* nome, LONG inicial, LONG maximo)
{
    HANDLE h = CreateSemaphoreA(nullptr, inicial, maximo, nome);
    if (!h) fprintf(stderr, "nao criei o semaforo %s\n", nome);
    return h;
}

bool ipc_criar(int num_threads)
{
    mapeamento = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                    0, sizeof(AreaCompartilhada), NOME_SHM);
    if (!mapeamento) {
        fprintf(stderr, "nao criei a memoria compartilhada\n");
        return false;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        fprintf(stderr, "ja existe um servidor rodando\n");
        CloseHandle(mapeamento);
        mapeamento = nullptr;
        return false;
    }

    area = (AreaCompartilhada*)MapViewOfFile(mapeamento, FILE_MAP_ALL_ACCESS,
                                             0, 0, sizeof(AreaCompartilhada));
    if (!area) {
        fprintf(stderr, "nao mapeei a memoria compartilhada\n");
        CloseHandle(mapeamento);
        mapeamento = nullptr;
        return false;
    }

    memset(area, 0, sizeof(AreaCompartilhada));

    sem_vazios = criar_semaforo(NOME_VAZIOS, TAM_FILA, TAM_FILA);
    sem_cheios = criar_semaforo(NOME_CHEIOS, 0, TAM_FILA);
    sem_fila   = criar_semaforo(NOME_FILA,   1, 1);
    sem_slots  = criar_semaforo(NOME_SLOTS,  MAX_SLOTS, MAX_SLOTS);
    sem_pilha  = criar_semaforo(NOME_PILHA,  1, 1);

    for (int i = 0; i < MAX_SLOTS; i++) {
        char nome[64];
        snprintf(nome, sizeof nome, "%s%d", NOME_PRONTA, i);
        sem_pronta[i] = criar_semaforo(nome, 0, 1);
        area->pilha_livres[i] = i;
    }

    area->topo_livres    = MAX_SLOTS;
    area->servidor_ativo = 1;
    area->num_threads    = num_threads;
    total_threads        = num_threads;
    return true;
}

/* O Windows apaga os objetos sozinho quando o ultimo handle fecha, entao
 * nao sobra segmento orfao mesmo se o servidor for morto a forca. */
void ipc_destruir(void)
{
    for (int i = 0; i < MAX_SLOTS; i++)
        if (sem_pronta[i]) CloseHandle(sem_pronta[i]);

    if (sem_vazios) CloseHandle(sem_vazios);
    if (sem_cheios) CloseHandle(sem_cheios);
    if (sem_fila)   CloseHandle(sem_fila);
    if (sem_slots)  CloseHandle(sem_slots);
    if (sem_pilha)  CloseHandle(sem_pilha);

    if (area)       UnmapViewOfFile(area);
    if (mapeamento) CloseHandle(mapeamento);

    area = nullptr;
    mapeamento = nullptr;
}

/* Retira uma requisicao da fila. Devolve false quando o servidor esta
 * desligando: a parada libera sem_cheios sem enfileirar nada. */
static bool retirar(Requisicao* saida)
{
    esperar(sem_cheios);
    if (!area->servidor_ativo) return false;

    esperar(sem_fila);
    *saida = area->fila[area->cabeca];
    area->cabeca = (area->cabeca + 1) % TAM_FILA;
    liberar(sem_fila);

    liberar(sem_vazios);
    return true;
}

static void responder(int slot, const Resposta* res)
{
    if (slot < 0 || slot >= MAX_SLOTS) return;
    area->resposta[slot] = *res;
    liberar(sem_pronta[slot]);
}

void ipc_pedir_parada(void)
{
    if (!area) return;
    area->servidor_ativo = 0;
    for (int i = 0; i < total_threads; i++) liberar(sem_cheios);
}

/* Uma thread do pool. O processamento acontece fora da secao critica da
 * fila: enquanto esta thread trabalha na tabela, as outras ja retiram as
 * proximas requisicoes. */
static void* trabalhador(void* arg)
{
    int id = (int)(intptr_t)arg;
    log_escrever("POOL  thread %d pronta", id);

    Requisicao req;
    while (retirar(&req)) {
        log_escrever("RECV  thread %d <- pid %d op %d id %d",
                     id, req.pid_cliente, (int)req.op, req.id);

        Resposta res = banco_processar(req, id);
        responder(req.slot, &res);

        log_escrever("SEND  thread %d -> %s | %s",
                     id, res.ok ? "OK " : "ERR", res.texto);

        if (req.op == OP_ENCERRAR) {
            ipc_pedir_parada();
            break;
        }
    }

    log_escrever("POOL  thread %d encerrada", id);
    return nullptr;
}

void ipc_rodar_pool(void)
{
    pthread_t th[TAM_FILA];

    for (int i = 0; i < total_threads; i++)
        pthread_create(&th[i], nullptr, trabalhador, (void*)(intptr_t)i);

    for (int i = 0; i < total_threads; i++)
        pthread_join(th[i], nullptr);
}
