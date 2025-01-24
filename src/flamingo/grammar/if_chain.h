// This Source Form is subject to the terms of the AQUA Software License,
// v. 1.0. Copyright (c) 2025 Aymeric Wibo

#pragma once

#include "../common.h"
#include "../grammar/expr.h"
#include "../val.h"

static int parse_if_chain(flamingo_t* flamingo, TSNode node) {
	// Get if condition.

	TSNode const if_condition_node = ts_node_child_by_field_name(node, "condition", 9);
	char const* const if_condition_type = ts_node_type(if_condition_node);

	if (strcmp(if_condition_type, "expression") != 0) {
		return error(flamingo, "expected expression for if condition, got %s", if_condition_type);
	}

	// Get if body.

	TSNode const if_body_node = ts_node_child_by_field_name(node, "body", 4);
	char const* const if_body_type = ts_node_type(if_body_node);

	if (strcmp(if_body_type, "block") != 0) {
		return error(flamingo, "expected block for if body, got %s", if_body_type);
	}

	// Evaluate condition.
	// TODO Should this be pulled out with the assert condition test?

	flamingo_val_t* val = NULL;

	if (parse_expr(flamingo, if_condition_node, &val, NULL) < 0) {
		return -1;
	}

	if (val->kind != FLAMINGO_VAL_KIND_BOOL) {
		return error(flamingo, "expected boolean value for if condition, got %s", val_type_str(val));
	}

	bool const pass = val->boolean.boolean;
	val_decref(val);

	// If the condition passed, we can just execute the body and return.

	if (pass) {
		return parse_block(flamingo, if_body_node, NULL);
	}

	// Get elif conditions.
	// The first named child we should encounter (other than the if) is the first elif condition.
	// Right after it, we should encounter it's respective body.

	for (size_t i = 0; i < ts_node_child_count(node); i++) {
		TSNode const child = ts_node_child(node, i);

		if (!ts_node_is_named(child)) {
			continue;
		}

		char const* const name = ts_node_field_name_for_child(node, i);

		// If we see any one of the other accepted nodes, skip it.

		if (
			strcmp(name, "condition") == 0 ||
			strcmp(name, "body") == 0 ||
			strcmp(name, "else_body") == 0
		) {
			continue;
		}

		if (strcmp(name, "elif_condition") != 0) {
			return error(flamingo, "expected elif_condition, got %s", name);
		}

		// We now know that child was an elif condition.

		TSNode const elif_condition_node = child;
		char const* const elif_condition_type = ts_node_type(elif_condition_node);

		if (strcmp(elif_condition_type, "expression") != 0) {
			return error(flamingo, "expected expression for elif condition, got %s", elif_condition_type);
		}

		// Go to the next named node, which should be the body of this elif statement.

		TSNode const elif_body_node = ts_node_next_sibling(elif_condition_node);
		char const* const elif_body_type = ts_node_type(elif_body_node);

		if (strcmp(elif_body_type, "block") != 0) {
			return error(flamingo, "expected block for elif body, got %s", elif_body_type);
		}

		// Evaluate condition.

		flamingo_val_t* val = NULL;

		if (parse_expr(flamingo, elif_condition_node, &val, NULL) < 0) {
			return -1;
		}

		if (val->kind != FLAMINGO_VAL_KIND_BOOL) {
			return error(flamingo, "expected boolean value for elif condition, got %s", val_type_str(val));
		}

		bool const pass = val->boolean.boolean;
		val_decref(val);

		// If the condition passed, we can execute the body and return.

		if (pass) {
			return parse_block(flamingo, elif_body_node, NULL);
		}

		// Jump forward so we don't hit the elif body again.

		i++;
	}

	// Get else body.

	TSNode const else_body_node = ts_node_child_by_field_name(node, "else_body", 9);
	bool const has_else_body = !ts_node_is_null(else_body_node);

	if (has_else_body) {
		char const* const else_body_type = ts_node_type(else_body_node);

		if (strcmp(else_body_type, "block") != 0) {
			return error(flamingo, "expected block for else body, got %s", else_body_type);
		}
	}

	// Execute else body if there is one.

	if (has_else_body) {
		return parse_block(flamingo, else_body_node, NULL);
	}

	// In this path, nothing happened.

	return 0;
}
