#ifndef _CTF_PARSER_H
#define _CTF_PARSER_H

#include <helpers/list.h>
#include <stdio.h>
#include <glib.h>

// the parameter name (of the reentrant 'yyparse' function)
// data is a pointer to a 'SParserParam' structure
//#define YYPARSE_PARAM	scanner

// the argument for the 'yylex' function
#define YYLEX_PARAM	((struct ctf_scanner *) scanner)->scanner

struct ctf_node;
struct ctf_parser;

enum node_type {
	NODE_UNKNOWN,
	NODE_ROOT,
	NODE_EVENT,
	NODE_STREAM,
	NODE_TYPE,
	NODE_TRACE,

	NR_NODE_TYPES,
};

struct ctf_node {
	enum node_type type;
	char *str;
	long long ll;
	struct ctf_node *parent;
	char *ident;
	struct cds_list_head siblings;
	struct cds_list_head children;
	struct cds_list_head gc;
};

struct scope;
struct scope {
	struct scope *parent;
	GHashTable *types;
};

struct ctf_ast {
	struct ctf_node root;
};

#endif /* _CTF_PARSER_H */
