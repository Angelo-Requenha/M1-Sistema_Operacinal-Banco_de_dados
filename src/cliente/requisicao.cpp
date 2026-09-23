/* cliente/requisicao.cpp - back do cliente.
 * Traduz o texto digitado (ex.: "INSERT 7 Joao") numa struct Requisicao.
 * So devolve dados: quem imprime e o terminal.cpp. */

#include "cliente.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>

Operacao operacao_de(const char* texto)
{
    if (!strcasecmp(texto, "INSERT"))   return OP_INSERT;
    if (!strcasecmp(texto, "SELECT"))   return OP_SELECT;
    if (!strcasecmp(texto, "UPDATE"))   return OP_UPDATE;
    if (!strcasecmp(texto, "DELETE"))   return OP_DELETE;
    if (!strcasecmp(texto, "LISTAR"))   return OP_LISTAR;
    if (!strcasecmp(texto, "ENCERRAR")) return OP_ENCERRAR;
    return OP_NENHUMA;
}

static bool precisa_id(Operacao op)
{
    return op == OP_INSERT || op == OP_SELECT ||
           op == OP_UPDATE || op == OP_DELETE;
}

static bool precisa_nome(Operacao op)
{
    return op == OP_INSERT || op == OP_UPDATE;
}

/* campo[0] e a operacao, campo[1] o id, o resto vira o nome. */
bool montar_requisicao(Requisicao* r, int n, char** campo)
{
    memset(r, 0, sizeof *r);
    if (n < 1) return false;

    r->op = operacao_de(campo[0]);
    if (r->op == OP_NENHUMA) {
        fprintf(stderr, "operacao %s nao existe\n", campo[0]);
        return false;
    }

    if (precisa_id(r->op)) {
        if (n < 2) {
            fprintf(stderr, "%s exige um id\n", campo[0]);
            return false;
        }
        r->id = atoi(campo[1]);
    }

    if (precisa_nome(r->op)) {
        if (n < 3) {
            fprintf(stderr, "%s exige um nome\n", campo[0]);
            return false;
        }
        r->nome[0] = '\0';
        for (int i = 2; i < n; i++) {
            if (i > 2) strncat(r->nome, " ", TAM_NOME - strlen(r->nome) - 1);
            strncat(r->nome, campo[i], TAM_NOME - strlen(r->nome) - 1);
        }
    }

    return true;
}
