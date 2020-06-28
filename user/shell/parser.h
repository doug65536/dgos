#include <assert.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum parser_op_t {
    Op_Null,

    Op_Empty,

    // Primitives
    Op_Number,
    Op_Identifier,
    Op_String,
    Op_Word,

    // Unary
    Op_Neg,
    Op_BitNot,
    Op_Call,

    // Binary
    Op_Assign,
    Op_Add,
    Op_Sub,
    Op_Mul,
    Op_Div,
    Op_And,
    Op_Or,
    Op_Xor,
    Op_Comma,

    // Complex
    Op_Command,
    Op_Words,
    Op_Function,
    Op_Arguments,
    Op_CallWithArgs,
    Op_Assignments,
    Op_Commands,
    Op_Declarations
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
