#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>

#define BUFSIZE 1024
#define nil NULL

/*
 * Grammar:
 *
 * prog:
 *      stmt+
 *
 * stmt:
 *      declaration ';'
 *      expr ';'
 *
 * expr:
 *      name
 *      num
 *
 * declaration:
 *      type name
 *
 * type:
 *      "int"
 *
 * name:
 *      [a-zA-Z_]\w*
 */

char *progname;
char *filename;
FILE *infile;

typedef enum TokenType TokenType;
enum TokenType
{
    TTtype,
    TTsym,
    TTnum,
    TTsemi,
};

char *
tok2str(TokenType type)
{
    switch (type) {
        case TTtype:
            return "type";
        case TTsym:
            return "symbol";
        case TTsemi:
            return "`;'";
        default:
            return "(unknown token type)";
    }
}

typedef enum Type Type;
enum Type {
    Tint,
};

char *
type2str(Type type)
{
    switch (type) {
        case Tint:
            return "int";
        default:
            return "(unknown type)";
    }
}

typedef struct Token Token;
struct Token
{
    TokenType type;
    union {
        Type type;
        char *s;
    } val;
};

typedef enum NodeType NodeType;
enum NodeType
{
    Nprog,
    Nstmt,
    Ndecl,
    Ntype,
    Nsym,
    Nnum,
};

typedef struct Node Node;
struct Node
{
    NodeType type;
    Node *next;    // for lists of nodes
    union {
        Node *n;
        Type type; // Ttype
        char *s;   // Nsym
        long num;  // Nnum
    } args[3];
};

__attribute__((noreturn)) void
panic(char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    fprintf(stderr, "%s: %s: ", progname, filename);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);

    exit(1);
}


void *
emalloc(size_t size)
{
    void *p = calloc(1, size);

    if (p == nil)
        panic("emalloc");

    return p;
}

char *
estrdup(char *s1)
{
    char *s2 = strdup(s1);

    if (s2 == nil)
        panic("estrdup");

    return s2;
}

int
shift()
{
    return fgetc(infile);
}

void
unshift(int c)
{
    if (c == EOF)
        return;

    if (ungetc(c, infile) == EOF)
        panic("ungetc('%c')", c);
}

int
peek()
{
    int c = fgetc(infile);

    if (c != EOF)
        unshift(c);

    return c;
}

void
trim(void)
{
    int c;

    int i = 0;

    for (;;) {
        c = peek();
        if (c == EOF)
            break;
        if (!isspace(c))
            break;

        shift();
    }
}

Token *
mktoken(TokenType type)
{
    Token *t = emalloc(sizeof(TokenType));
    t->type = type;
    return t;
}

void
freetok(Token *t)
{
    if (t == nil)
        return;

    if (t->type == TTsym)
        free(t->val.s);

    free(t);
}

char *
shiftwhile(char *chars)
{
    char *buf = emalloc(BUFSIZE);
    int c, i = 0;

    for (;;) {
        c = shift();

        if (strchr(chars, c) == nil)
            break;

        if (i == BUFSIZE - 1)
            panic("shiftwhile - token too long");

        buf[i++] = c;
    }

    unshift(c);
    buf[i++] = '\0';

    return buf;
}

Token *
lexnum()
{
    Token *t = mktoken(TTnum);
    t->val.s = shiftwhile("0123456789");

    return t;
}

Token *
lexname()
{
    Token *t;
    char *s = shiftwhile("abcdefghijklmnopqrstuvwxyz"
                         "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                         "0123456789_");

    if (strcmp(s, "int") == 0) {
        t = mktoken(TTtype);
        t->val.type = Tint;
    } else {
        t = mktoken(TTsym);
        t->val.s = s;
    }

    return t;
}

Token *
nexttok()
{
    Token *t;
    int c, c2;
    char *s;

    trim();

    c = peek();

    if (c == EOF)
        return nil;

    if (c == ';') {
        t = mktoken(TTsemi);
        shift();
    } else if (isalpha(c)) {
        t = lexname();
    } else if (isdigit(c)) {
        t = lexnum();
    } else {
        panic("unknown token starting with `%c'", c);
    }

    return t;
}

// token buffer of size one. allows for peeking
Token *tokbuf = nil;

Token *
peektok()
{
    if (tokbuf)
        return tokbuf;

    return tokbuf = nexttok();
}

Token *
shifttok()
{
    Token *t;

    if (tokbuf) {
        t = tokbuf;
        tokbuf = nil;
    } else {
        t = nexttok();
    }

    return t;
}

Token *
expect(TokenType type)
{
    Token *t = shifttok();

    if (t == nil)
        panic("expected %s, got EOF", tok2str(type));

    if (t->type != type)
        panic("expected %s, got %s", tok2str(type), tok2str(t->type));

    return t;
}

Node *
mknode(NodeType type)
{
    Node *n = emalloc(sizeof(Node));
    n->type = type;
    n->next = nil;
    return n;
}

void
eachnode(Node *n, void (*f)(Node *))
{
    while (n != nil) {
        f(n);
        n = n->next;
    }
}

void
append(Node **list, Node *n)
{

    // empty list
    if (*list == nil) {
        *list = n;
    } else {
        Node *l = *list;

        while (l->next != nil) {
            l = l->next;
        }

        l->next = n;
    }
}

Node *
parse_type()
{
    Token *t = expect(TTtype);
    Node *n;

    n = mknode(Ntype);
    n->args[0].type = t->val.type;

    freetok(t);

    return n;
}

Node *
parse_name()
{
    Token *t = expect(TTsym);
    Node *n;

    n = mknode(Nsym);
    n->args[0].s = estrdup(t->val.s);

    freetok(t);

    return n;
}

Node *
parse_decl(void)
{
    Node *type, *name, *decl;

    type = parse_type();
    name = parse_name();

    decl = mknode(Ndecl);
    decl->args[0].n = type;
    decl->args[1].n = name;

    return decl;
}

Node *
parse_num()
{
    Token *t = expect(TTnum);
    Node *n = mknode(Nnum);
    n->args[0].num = strtol(t->val.s, NULL, 0);

    freetok(t);

    return n;
}

Node *
parse_expr(void)
{
    Token *t = peektok();

    if (t->type == TTsym)
        return parse_name();
    else if (t->type == TTnum)
        return parse_num();
    else
        panic("parse_expr");
}

Node *
parse_stmt(void)
{
    Token *t = peektok();
    Node *n = mknode(Nstmt);

    if (t->type == TTtype) {
        n->args[0].n = parse_decl();
    } else {
        n->args[0].n = parse_expr();
    }

    freetok(expect(TTsemi));

    return n;
}

Node *
parse_prog(void)
{
    Node *prog = mknode(Nprog);

    while (peektok() != nil) {
        append(&prog->args[0].n, parse_stmt());
    }

    return prog;
}

Node *
parse()
{
    Node *n;
    char buf[BUFSIZE];
    char *ret;

    if ((infile = fopen(filename, "r")) == nil) {
        fprintf(stderr, "%s: ", progname);
        perror(filename);
        exit(1);
    }

    n = parse_prog();

    fclose(infile);

    return n;
}

void
printtype(Type t)
{
    switch (t) {
        case Tint:
            printf("int");
            break;
        default:
            panic("unknown type %d", t);
    }
}

void
printnode(Node *n)
{
    if (n == nil)
        panic("printnode");

    switch (n->type) {
        case Nprog:
            eachnode(n->args[0].n, printnode);
            break;
        case Nstmt:
            printnode(n->args[0].n);
            printf(";\n");
            break;
        case Ndecl:
            printnode(n->args[0].n);
            printf(" ");
            printnode(n->args[1].n);
            break;
        case Ntype:
            printtype(n->args[0].type);
            break;
        case Nsym:
            printf("%s", n->args[0].s);
            break;
        case Nnum:
            printf("%ld", n->args[0].num);
            break;
        default:
            panic("printnode - unknown node type %d", n->type);
    }
}

void
usage()
{
    fprintf(stderr, "usage: %s files\n", progname);
    exit(1);
}

int
main(int argc, char *argv[])
{
    FILE *in;
    Node *n;

    progname = argv[0];

    if (argc < 2) {
        usage();
        exit(1);
    }

    filename = argv[1];

    n = parse();

    printnode(n);

    return 0;
}
