// This Source Form is subject to the terms of the AQUA Software License,
// v. 1.0. Copyright (c) 2024 Aymeric Wibo

#pragma once

#include "access.h"
#include "binary_expr.h"
#include "call.h"
#include "identifier.h"
#include "index.h"
#include "lambda.h"
#include "literal.h"
#include "map.h"
#include "unary_expr.h"
#include "vec.h"

static int parse_expr(flamingo_t* flamingo, TSNode node, flamingo_val_t** val, flamingo_val_t** accessed_val_ref) {
	assert(strcmp(ts_node_type(node), "expression") == 0);
	assert(ts_node_child_count(node) == 1);

	TSNode const child = ts_node_child(node, 0);
	char const* const type = ts_node_type(child);

	// 'val == NULL' means that we don't care about the result of the expression and can discard it.
	// These types of expressions are dead-ends if we're discarding the value and they can't have side-effect either, so just don't parse them.

	if (val != NULL && strcmp(type, "literal") == 0) {
		return parse_literal(flamingo, child, val);
	}

	if (val != NULL && strcmp(type, "identifier") == 0) {
		return parse_identifier(flamingo, child, val);
	}

	if (val != NULL && strcmp(type, "lambda") == 0) {
		return parse_lambda(flamingo, child, val);
	}

	// These expressions could have side-effects, so we need to parse them anyway, even if 'val != NULL'.

	if (strcmp(type, "vec") == 0) {
		return parse_vec(flamingo, child, val);
	}

	if (strcmp(type, "map") == 0) {
		return parse_map(flamingo, child, val);
	}

	if (strcmp(type, "call") == 0) {
		return parse_call(flamingo, child, val);
	}

	if (strcmp(type, "parenthesized_expression") == 0) {
		TSNode const grandchild = ts_node_child_by_field_name(child, "expression", 10);
		return parse_expr(flamingo, grandchild, val, accessed_val_ref);
	}

	if (strcmp(type, "unary_expression") == 0) {
		return parse_unary_expr(flamingo, child, val);
	}

	if (strcmp(type, "binary_expression") == 0) {
		return parse_binary_expr(flamingo, child, val);
	}

	if (strcmp(type, "access") == 0) {
		return parse_access(flamingo, child, val, accessed_val_ref);
	}

	if (strcmp(type, "index") == 0) {
		return parse_index(flamingo, child, val, false);
	}

	return error(flamingo, "unknown expression type: %s", type);
}
