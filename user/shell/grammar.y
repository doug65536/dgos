
%include {
#include "parser.h"
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
%type func_decl parser_node_ptr_t
%type func_call parser_node_ptr_t
%type value parser_node_ptr_t
%type comma_list parser_node_ptr_t

script(A) ::= declarations(B) . { A = B; }
declarations(A) ::= declarations(B) declaration(C) . {
        A = parser_node(2, Op_Declarations, B, C);
}
declaration(A) ::= statement(B) . { A = B; }
declaration(A) ::= func_decl(B) . { A = B; }
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

expression(A) ::= func_call(B) . {
    A = B;
}

comma_list(A) ::= comma_list(B) COMMA expression(C) . {
    A = parser_node(2, Op_Comma, B, C);
}

comma_list(A) ::= expression(B) . {
    A = B;
}

func_call(A) ::= name(B) OPENPAREN comma_list(C) CLOSEPAREN . {
        A = parser_node(2, Op_CallWithArgs, B, C);
}

func_call(A) ::= name(B) OPENPAREN CLOSEPAREN . {
        A = parser_node(1, Op_Call, B);
}

name(A) ::= QUOTED_STRING(B) . { A = parser_node(1, Op_String, B); }
name(A) ::= IDENTIFIER(B) . { A = parser_node(1, Op_String, B); }

func_decl(A) ::= FUNC name(B) OPENBRACE declarations(C) CLOSEBRACE . {
    A = parser_node(2, Op_Function, B, C);
}

number(A) ::= NUMBER(B) . { A = B; }
