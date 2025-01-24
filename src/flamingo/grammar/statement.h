// This Source Form is subject to the terms of the AQUA Software License,
// v. 1.0. Copyright (c) 2024-2025 Aymeric Wibo

#pragma once

#include "assert.h"
#include "assignment.h"
#include "block.h"
#include "function_declaration.h"
#include "if_chain.h"
#include "import.h"
#include "print.h"
#include "return.h"
#include "var_decl.h"

#include "../common.h"

static int parse_statement(flamingo_t* flamingo, TSNode node) {
	// Line insensitive statements, which are only wrapped by a hidden node.

	char const* const type = ts_node_type(node);

	if (strcmp(type, "\n") == 0 || strcmp(type, ";") == 0) {
		return 0;
	}

	if (strcmp(type, "comment") == 0 || strcmp(type, "doc_comment") == 0) {
		return 0;
	}

	if (strcmp(type, "block") == 0) {
		return parse_block(flamingo, node, NULL);
	}

	if (strcmp(type, "function_declaration") == 0) {
		return parse_function_declaration(flamingo, node, FLAMINGO_FN_KIND_FUNCTION);
	}

	if (strcmp(type, "class_declaration") == 0) {
		return parse_function_declaration(flamingo, node, FLAMINGO_FN_KIND_CLASS);
	}

	if (strcmp(type, "if_chain") == 0) {
		return parse_if_chain(flamingo, node);
	}

	// All the line sensitive statements (which are wrapped by an explicit 'statement' node).

	assert(strcmp(type, "statement") == 0);
	assert(ts_node_child_count(node) == 1);

	if (
		flamingo->cur_fn_body != NULL &&                       // In function?
		flamingo->cur_fn_rv != NULL &&                         // Returning?
		memcmp(flamingo->cur_fn_body, &node, sizeof node) != 0 // Not the body node?
	) {
		return 0;
	}

	TSNode const child = ts_node_child(node, 0);
	char const* const child_type = ts_node_type(child);

	if (strcmp(child_type, "print") == 0) {
		return parse_print(flamingo, child);
	}

	if (strcmp(child_type, "return") == 0) {
		return parse_return(flamingo, child);
	}

	if (strcmp(child_type, "assert") == 0) {
		return parse_assert(flamingo, child);
	}

	if (strcmp(child_type, "var_decl") == 0) {
		return parse_var_decl(flamingo, child);
	}

	if (strcmp(child_type, "assignment") == 0) {
		return parse_assignment(flamingo, child);
	}

	if (strcmp(child_type, "proto") == 0) {
		return parse_function_declaration(flamingo, child, FLAMINGO_FN_KIND_EXTERN);
	}

	if (strcmp(child_type, "expression") == 0) {
		return parse_expr(flamingo, child, NULL, NULL);
	}

	if (strcmp(child_type, "import") == 0) {
		return parse_import(flamingo, child);
	}

	return error(flamingo, "unknown statement type: %s", child_type);
}
