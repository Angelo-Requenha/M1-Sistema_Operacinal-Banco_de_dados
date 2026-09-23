/* servidor/terminal.cpp - front do servidor.
 * Le os argumentos, monta a ordem de inicializacao e imprime tudo que
 * aparece na tela. Nao toca na tabela nem na memoria compartilhada
 * diretamente: chama as funcoes de banco.cpp e comunicacao.cpp. */

#include "servidor.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static int num_threads = PADRAO_THREADS;
static int custo_ms    = 0;

static void mostrar_uso(const char* prog)
{
    printf("uso: %s [--threads N] [--custo MS]\n", prog);
    printf("  --threads N   tamanho do pool (padrao %d)\n", PADRAO_THREADS);
    printf("  --custo MS    atraso artificial por requisicao, em ms\n");
}

static bool ler_argumentos(int argc, char** argv)
{
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--threads") && i + 1 < argc) {
            num_threads = atoi(argv[++i]);
            if (num_threads < 1)        num_threads = 1;
            if (num_threads > TAM_FILA) num_threads = TAM_FILA;
        } else if (!strcmp(argv[i], "--custo") && i + 1 < argc) {
            custo_ms = atoi(argv[++i]);
            if (custo_ms < 0) custo_ms = 0;
        } else {
            return false;
        }
    }
    return true;
}

static void mostrar_abertura(void)
{
    printf("servidor no ar - pool de %d thread(s), memoria compartilhada %s\n",
           num_threads, NOME_SHM);
    printf("custo artificial por requisicao: %d ms\n", custo_ms);
    printf("log sendo gravado em %s\n", ARQUIVO_LOG);
    printf("Ctrl+C ou 'cliente ENCERRAR' para desligar.\n\n");
    fflush(stdout);
}

static void mostrar_estatisticas(void)
{
    printf("\n--- estatisticas ---\n");
    printf("  INSERT %ld | SELECT %ld | UPDATE %ld | DELETE %ld | LISTAR %ld\n",
           banco_contagem(OP_INSERT), banco_contagem(OP_SELECT),
           banco_contagem(OP_UPDATE), banco_contagem(OP_DELETE),
           banco_contagem(OP_LISTAR));
    printf("  registros na tabela: %d\n", banco_total());
    printf("  veja %s para o historico completo, com a thread que atendeu cada\n"
           "  requisicao (prova de que o pool trabalhou em paralelo).\n", ARQUIVO_LOG);
}

static void ao_interromper(int)
{
    ipc_pedir_parada();
}

int main(int argc, char** argv)
{
    if (!ler_argumentos(argc, argv)) {
        mostrar_uso(argv[0]);
        return 1;
    }

    log_abrir();

    if (!ipc_criar(num_threads)) return 1;

    signal(SIGINT,  ao_interromper);
    signal(SIGTERM, ao_interromper);

    banco_definir_custo(custo_ms);
    banco_carregar();

    log_escrever("BOOT  servidor no ar | pool=%d | custo=%dms | shm=%s",
                 num_threads, custo_ms, NOME_SHM);
    mostrar_abertura();

    ipc_rodar_pool();

    mostrar_estatisticas();

    log_escrever("EXIT  servidor encerrado");
    log_fechar();
    ipc_destruir();
    return 0;
}
