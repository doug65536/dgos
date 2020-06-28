
%include {
#include "parser.h"
}

%token_type parser_token_t
%token_prefix TOKEN_
%token_destructor { parser_token_destructor($$); }

%type script parser_node_ptr_t
%type commands parser_node_ptr_t
%type command parser_node_ptr_t
%type optional_assignments parser_node_ptr_t
%type words parser_node_ptr_t
%type word parser_node_ptr_t
%type assignments parser_node_ptr_t
%type assignment parser_node_ptr_t

//%type words parser_node_ptr_t
//%type words parser_node_ptr_t
//%type words parser_node_ptr_t
//%type words parser_node_ptr_t
//%type words parser_node_ptr_t
//%type words parser_node_ptr_t
//%type words parser_node_ptr_t
//%type words parser_node_ptr_t

script(A) ::= commands(B) . {
    A = B;
}

commands(A) ::= commands(B) command(C) . {
    A = parser_node(2, Op_Commands, B, C);
}

commands(A) ::= command(C) . {
    A = C;
}

command(A) ::= assignments(B) words(C) . {
    A = parser_node(2, Op_Command, B, C);
}

command(A) ::= words(C) . {
    A = C;
}

assignments(A) ::= assignments(B) assignment(C) . {
    A = parser_node(2, Op_Assignments, B, C);
}

assignments(A) ::= assignment(C) . {
    A = C;
}

assignment(A) ::= assignment_name(B) word(C) . {
    A = parser_node(2, Op_Assign, B, C);
}

words(A) ::= words(B) word(C) . {
    A = parser_node(2, Op_Words, B, C);
}

word(A) ::= WORD(B) . {
    A = parser_node(1, Op_Word, B);
}

assignment_name(A) ::= ASSIGNMENT_NAME(B) . {
    A = B;
}
