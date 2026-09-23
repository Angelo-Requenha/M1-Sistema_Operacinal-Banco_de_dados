/* cliente/terminal.cpp - front do cliente.
 * Le argumentos e teclado, chama requisicao.cpp para montar o pedido,
 * comunicacao.cpp para enviar, e imprime o resultado. */

#include "cliente.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

static void mostrar_uso(const char* prog)
{
    printf("uso:\n");
    printf("  %s INSERT <id> <nome>\n", prog);
    printf("  %s SELECT <id>\n", prog);
    printf("  %s UPDATE <id> <nome>\n", prog);
    printf("  %s DELETE <id>\n", prog);
    printf("  %s LISTAR\n", prog);
    printf("  %s ENCERRAR\n", prog);
    printf("  %s -i     modo interativo\n", prog);
}

static void mostrar_resposta(const Resposta& res)
{
    printf("[%s] (thread %d) %s\n",
           res.ok ? "OK " : "ERR",
           res.thread_atendeu,
           res.texto);
    fflush(stdout);
}

static void modo_interativo(void)
{
    printf("modo interativo. uma requisicao por linha, 'sair' para terminar.\n");
    printf("  ex.: INSERT 7 Joao | SELECT 7 | UPDATE 7 Maria | DELETE 7 | LISTAR\n\n");

    char linha[256];
    while (true) {
        printf("> ");
        fflush(stdout);
        if (!fgets(linha, sizeof linha, stdin)) break;

        linha[strcspn(linha, "\r\n")] = '\0';
        if (linha[0] == '\0') continue;
        if (!strcasecmp(linha, "sair") || !strcasecmp(linha, "exit")) break;

        char* campo[16];
        int   n   = 0;
        char* tok = strtok(linha, " \t");
        while (tok && n < 16) {
            campo[n++] = tok;
            tok = strtok(nullptr, " \t");
        }

        Requisicao r;
        if (!montar_requisicao(&r, n, campo)) continue;

        mostrar_resposta(ipc_pedir(r));
        if (r.op == OP_ENCERRAR) break;
    }
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        mostrar_uso(argv[0]);
        return 1;
    }

    if (!ipc_abrir()) return 1;

    int rc = 0;

    if (!strcmp(argv[1], "-i") || !strcmp(argv[1], "--interativo")) {
        modo_interativo();
    } else {
        Requisicao r;
        if (!montar_requisicao(&r, argc - 1, argv + 1)) {
            mostrar_uso(argv[0]);
            rc = 1;
        } else {
            mostrar_resposta(ipc_pedir(r));
        }
    }

    ipc_fechar();
    return rc;
}
