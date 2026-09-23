/* cliente/comunicacao.cpp - lado IPC do cliente.
 * Abre pelo nome a memoria compartilhada e os semaforos que o servidor
 * criou, reserva um slot de resposta, enfileira a requisicao e dorme no
 * semaforo daquele slot ate a resposta aparecer. O cliente nao enxerga a
 * tabela nem chama funcao do servidor. */

#include "cliente.h"

#include <windows.h>
#include <cstdio>

static HANDLE mapeamento = nullptr;
static AreaCompartilhada* area = nullptr;

static HANDLE sem_vazios = nullptr;
static HANDLE sem_cheios = nullptr;
static HANDLE sem_fila   = nullptr;
static HANDLE sem_slots  = nullptr;
static HANDLE sem_pilha  = nullptr;
static HANDLE sem_pronta[MAX_SLOTS] = {nullptr};

static void esperar(HANDLE s) { WaitForSingleObject(s, INFINITE); }
static void liberar(HANDLE s) { ReleaseSemaphore(s, 1, nullptr); }

static HANDLE abrir_semaforo(const char* nome)
{
    HANDLE h = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, nome);
    if (!h) fprintf(stderr, "nao achei o semaforo %s\n", nome);
    return h;
}

bool ipc_abrir(void)
{
    mapeamento = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, NOME_SHM);
    if (!mapeamento) {
        fprintf(stderr, "nao encontrei a memoria compartilhada.\n"
                        "o servidor esta rodando?\n");
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

    sem_vazios = abrir_semaforo(NOME_VAZIOS);
    sem_cheios = abrir_semaforo(NOME_CHEIOS);
    sem_fila   = abrir_semaforo(NOME_FILA);
    sem_slots  = abrir_semaforo(NOME_SLOTS);
    sem_pilha  = abrir_semaforo(NOME_PILHA);

    for (int i = 0; i < MAX_SLOTS; i++) {
        char nome[64];
        snprintf(nome, sizeof nome, "%s%d", NOME_PRONTA, i);
        sem_pronta[i] = abrir_semaforo(nome);
        if (!sem_pronta[i]) return false;
    }

    return sem_vazios && sem_cheios && sem_fila && sem_slots && sem_pilha;
}

/* Fecha so os proprios handles: os objetos continuam vivos no servidor. */
void ipc_fechar(void)
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

/* Cada requisicao em voo precisa de um slot exclusivo. Com um unico slot
 * global, dois clientes simultaneos roubariam a resposta um do outro. */
static int reservar_slot(void)
{
    esperar(sem_slots);
    esperar(sem_pilha);
    int s = area->pilha_livres[--area->topo_livres];
    liberar(sem_pilha);
    return s;
}

static void liberar_slot(int slot)
{
    esperar(sem_pilha);
    area->pilha_livres[area->topo_livres++] = slot;
    liberar(sem_pilha);
    liberar(sem_slots);
}

/* Produtor da fila circular. Bloqueia enquanto a fila estiver cheia. */
static void enviar(Requisicao* r)
{
    r->pid_cliente = (int)GetCurrentProcessId();

    esperar(sem_vazios);
    esperar(sem_fila);

    area->fila[area->cauda] = *r;
    area->cauda = (area->cauda + 1) % TAM_FILA;

    liberar(sem_fila);
    liberar(sem_cheios);
}

static void receber(int slot, Resposta* res)
{
    esperar(sem_pronta[slot]);
    *res = area->resposta[slot];
}

/* Envia uma requisicao e espera a resposta correspondente. */
Resposta ipc_pedir(Requisicao r)
{
    int slot = reservar_slot();
    r.slot = slot;

    enviar(&r);

    Resposta res;
    receber(slot, &res);
    liberar_slot(slot);
    return res;
}
