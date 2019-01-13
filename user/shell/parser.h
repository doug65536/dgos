#include <assert.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum parser_op_t {
    Op_Null,
    Op_Declarations,
    Op_Number,
    Op_Identifier,
    Op_String,
    Op_Assign,
    Op_Add,
    Op_Sub,
    Op_Mul,
    Op_Div
} parser_op_t;

typedef struct parser_token_t {
    char const *st;
    char const *en;
    char const *file;
    int line;
} parser_token_t;

void parser_token_destructor(parser_token_t token);

typedef struct parser_node_t parser_node_t;
struct parser_node_t {
    int ops;
    parser_op_t type;
    void *op[4];
};

typedef parser_node_t *parser_node_ptr_t;

parser_node_ptr_t parser_node(size_t count, parser_op_t op, ...);

#ifdef __cplusplus
}
#endif
