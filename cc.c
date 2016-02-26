#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>

#define BUFSIZE 1000

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

typedef enum NodeType NodeType;
enum NodeType
{
    Nprog,
    Nstmt,
    Ndecl,
    Ntype,
    Nsym,
};

typedef enum Type Type;
enum Type {
    Tint,
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
    } args[3];
};

void
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

    if (p == NULL)
        panic("emalloc");

    return p;
}

int
peek()
{
    int c = fgetc(infile);

    if (c != EOF)
        ungetc(c, infile);

    return c;
}

int
shift()
{
    return fgetc(infile);
}

void
unshift(int c)
{
    if (c != EOF)
        ungetc(c, infile);
}

char *
shiftname()
{
    int c;
    char *buf = emalloc(BUFSIZE);
    int i = 0;

    c = shift();

    if (!isalpha(c) && c != '_')
        panic("expected identifier, got `%c'", c);

    buf[i++] = c;

    for (;;) {
        c = peek();

        if (!isalpha(c) && !isdigit(c) && c != '_')
            break;

        if (i == BUFSIZE - 1)
            panic("shiftname - identifier too long");

        buf[i++] = c;
        shift();
    }

    buf[i++] = '\0';

    return buf;
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

void
expectc(char expected)
{
    int c = shift();

    if (c == EOF)
        panic("expected '%c', but got EOF", expected);

    if (c != expected)
        panic("expected '%c', but got '%c'", expected);
}
Node *
mknode(NodeType type)
{
    Node *n = emalloc(sizeof(Node));
    n->type = type;
    n->next = NULL;
    return n;
}

void
eachnode(Node *n, void (*f)(Node *))
{
    while (n != NULL) {
        f(n);
        n = n->next;
    }
}

void
append(Node **list, Node *n)
{

    // empty list
    if (*list == NULL) {
        *list = n;
    } else {
        Node *l = *list;

        while (l->next != NULL) {
            l = l->next;
        }

        l->next = n;
    }
}

Node *
parse_type()
{
    Node *n;
    char *s = shiftname();

    n = mknode(Ntype);

    if (strcmp(s, "int") == 0) {
        n->args[0].type = Tint;
    } else {
        panic("unknown type `%s'", s);
    }

    return n;
}

Node *
parse_name()
{
    Node *n;
    char *s = shiftname();

    n = mknode(Nsym);
    n->args[0].s = s;

    return n;
}

Node *
parse_decl(void)
{
    Node *type, *name, *decl;

    type = parse_type();
    trim();
    name = parse_name();

    decl = mknode(Ndecl);
    decl->args[0].n = type;
    decl->args[1].n = name;

    return decl;
}

Node *
parse_stmt(void)
{
    Node *n = mknode(Nstmt);
    n->args[0].n = parse_decl();
    trim();
    expectc(';');

    return n;
}

Node *
parse_prog(void)
{
    Node *prog = mknode(Nprog);

    while (peek() != EOF) {
        trim();
        append(&prog->args[0].n, parse_stmt());
        trim();
    }

    return prog;
}

Node *
parse()
{
    Node *n;
    char buf[BUFSIZE];
    char *ret;

    if ((infile = fopen(filename, "r")) == NULL) {
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
    if (n == NULL)
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
