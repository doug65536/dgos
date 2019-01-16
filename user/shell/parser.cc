#include "parser.h"
#include "grammar.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

parser_node_ptr_t parser_node(size_t count, parser_op_t op, ...)
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
    static constexpr size_t const sz = 4 << 10;
    char buf[sz];
    fgets(buf, sz, stdin);
    printf("You said %s\n", buf);
    return 0;
}

void parser_token_destructor(parser_token_t token)
{

}
