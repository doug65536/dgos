#include "parser.h"
#include "grammar.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>

parser_node_ptr_t parser_node(int count, parser_op_t op, ...)
{
    va_list ap;
    va_start(ap, op);
    parser_node_ptr_t node = new parser_node_t;
    node->ops = count;
    for (size_t i = 0; i < count; ++i)
        node->op[i] = va_arg(ap, void*);
    va_end(ap);

    return node;
}

int main(int argc, char **argv)
{

}

void parser_token_destructor(parser_token_t token)
{

}

void *operator new(size_t n)
{
    return malloc(n);
}
