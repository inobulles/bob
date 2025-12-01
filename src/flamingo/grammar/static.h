// This Source Form is subject to the terms of the AQUA Software License, v. 1.0.
// Copyright (c) 2024 Aymeric Wibo

#pragma once

#include "../common.h"
#include "../env.h"
#include "var_decl.h"

static inline bool check_is_static(flamingo_t* flamingo, TSNode node) {
	TSNode const qualifiers = ts_node_child_by_field_name(node, "qualifiers", 10);
	bool const has_qualifiers = !ts_node_is_null(qualifiers);

	if (!has_qualifiers) {
		return false;
	}

	assert(strcmp(ts_node_type(qualifiers), "qualifier_list") == 0);

	size_t const qualifier_count = ts_node_child_count(qualifiers);

	for (size_t i = 0; i < qualifier_count; i++) {
		TSNode const qualifier = ts_node_child(qualifiers, i);

		size_t const start = ts_node_start_byte(qualifier);
		size_t const end = ts_node_end_byte(qualifier);

		char const* const qualifier_name = flamingo->src + start;
		size_t const size = end - start;

		if (strncmp(qualifier_name, "static", size) == 0) {
			return true;
		}
	}

	return false;
}

static inline int find_static_members_in_class(flamingo_t* flamingo, flamingo_scope_t* scope, TSNode body) {
	assert(strcmp(ts_node_type(body), "block") == 0);

	size_t const n = ts_node_named_child_count(body);
	env_gently_attach_scope(flamingo->env, scope);

	for (size_t i = 0; i < n; i++) {
		TSNode node = ts_node_named_child(body, i);
		char const* type = ts_node_type(node);

		// If node is wrapped in a statement, unwrap it.
		// This is the case for line-sensitive nodes, like 'var_decl'.

		if (strcmp(type, "statement") == 0) {
			node = ts_node_child(node, 0);
			type = ts_node_type(node);
		}

		// Check if static.

		bool const is_static = check_is_static(flamingo, node);

		if (!is_static) {
			continue;
		}

		// Parse static member.

		if (strcmp(type, "var_decl") == 0) {
			if (parse_var_decl(flamingo, node) < 0) {
				return -1;
			}

			continue;
		}

		if (strcmp(type, "function_declaration") == 0) {
			if (parse_function_declaration(flamingo, node, FLAMINGO_FN_KIND_FUNCTION) < 0) {
				return -1;
			}

			continue;
		}

		if (strcmp(type, "class_declaration") == 0) {
			if (parse_function_declaration(flamingo, node, FLAMINGO_FN_KIND_CLASS) < 0) {
				return -1;
			}

			continue;
		}

		if (strcmp(type, "proto") == 0) {
			if (parse_function_declaration(flamingo, node, FLAMINGO_FN_KIND_EXTERN) < 0) {
				return -1;
			}

			continue;
		}

		return error(flamingo, "static qualifier applied to %s, which can't have the static qualifier applied to it", type);
	}

	env_gently_detach_scope(flamingo->env);

	return 0;
}
