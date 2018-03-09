
%include {
    #include "assert.h"

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

    parser_node_ptr_t parser_node(int count, parser_op_t op, ...);
}

%token_type parser_token_t
%token_prefix TOKEN_
%token_destructor { parser_token_destructor($$); }
%type script parser_node_ptr_t
%type declarations parser_node_ptr_t
%type declaration parser_node_ptr_t
%type statements parser_node_ptr_t
%type statement parser_node_ptr_t
%type assignment parser_node_ptr_t
%type expression parser_node_ptr_t
%type name parser_node_ptr_t
%type value parser_node_ptr_t

script(A) ::= declarations(B) . { A = B; }
declarations(A) ::= declarations(B) declaration(C) . {
        A = parser_node(2, Op_Declarations, B, C);
}
declaration(A) ::= statement(B) . { A = B; }
statement(A) ::= assignment(B) . { A = B; }
assignment(A) ::= name(B) EQ expression(C) . {
        A = parser_node(2, Op_Assign, B, C);
}
value(A) ::= name(B) . { A = parser_node(1, Op_Identifier, B); }
value(A) ::= number(B) . { A = parser_node(1, Op_Number, B); }
expression(A) ::= value(B) . { A = B; }
expression(A) ::= expression(B) PLUS value(C) . {
        A = parser_node(2, Op_Add, B, C);
}
expression(A) ::= expression(B) MINUS value(C) . {
        A = parser_node(2, Op_Sub, B, C);
}
expression(A) ::= expression(B) TIMES value(C) . {
        A = parser_node(2, Op_Mul, B, C);
}
expression(A) ::= expression(B) DIVIDE value(C) . {
        A = parser_node(2, Op_Div, B, C);
}

name(A) ::= QUOTED_STRING(B) . { A = parser_node(1, Op_String, B); }
name(A) ::= IDENTIFIER(B) . { A = parser_node(1, Op_String, B); }

number(A) ::= NUMBER(B) . { A = B; }
