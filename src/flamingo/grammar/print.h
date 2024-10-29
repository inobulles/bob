// This Source Form is subject to the terms of the AQUA Software License,
// v. 1.0. Copyright (c) 2024 Aymeric Wibo

#pragma once

#include "expr.h"

#include "../common.h"
#include "../val.h"

#include <inttypes.h>

static int repr(flamingo_t* flamingo, flamingo_val_t* val, char** res, bool inner) {
	char* buf;

	switch (val->kind) {
	case FLAMINGO_VAL_KIND_NONE:
		*res = strdup("<none>");
		break;
	case FLAMINGO_VAL_KIND_BOOL:
		*res = strdup(val->boolean.boolean ? "true" : "false");
		break;
	case FLAMINGO_VAL_KIND_INT:
		asprintf(res, "%" PRId64, val->integer.integer);
		break;
	case FLAMINGO_VAL_KIND_STR:
		if (inner) {
			asprintf(res, "\"%.*s\"", (int) val->str.size, val->str.str);
		}

		else {
			asprintf(res, "%.*s", (int) val->str.size, val->str.str);
		}

		break;
	case FLAMINGO_VAL_KIND_VEC:
		*res = strdup("[");
		assert(*res != NULL);

		for (size_t i = 0; i < val->vec.count; i++) {
			flamingo_val_t* const elem = val->vec.elems[i];
			char* elem_repr;

			if (repr(flamingo, elem, &elem_repr, true) < 0) {
				free(res);
				return -1;
			}

			buf = strdup(*res);
			assert(buf != NULL);
			asprintf(res, "%s%s%s", buf, i == 0 ? "" : ", ", elem_repr);
			assert(*res != NULL);

			free(buf);
			free(elem_repr);
		}

		buf = strdup(*res);
		assert(buf != NULL);
		asprintf(res, "%s]", buf);
		free(buf);

		break;
	case FLAMINGO_VAL_KIND_MAP:
		*res = strdup("{");
		assert(*res != NULL);

		for (size_t i = 0; i < val->map.count; i++) {
			flamingo_val_t* const k = val->map.keys[i];
			flamingo_val_t* const v = val->map.vals[i];

			char* key_repr;

			if (repr(flamingo, k, &key_repr, true) < 0) {
				free(res);
				return -1;
			}

			char* val_repr;

			if (repr(flamingo, v, &val_repr, true) < 0) {
				free(key_repr);
				free(res);
				return -1;
			}

			buf = strdup(*res);
			assert(buf != NULL);
			asprintf(res, "%s%s%s: %s", buf, i == 0 ? "" : ", ", key_repr, val_repr);
			assert(*res != NULL);
			free(key_repr);
			free(val_repr);
			free(buf);
		}

		buf = strdup(*res);
		assert(buf != NULL);
		asprintf(res, "%s}", buf);
		free(buf);

		break;
	case FLAMINGO_VAL_KIND_FN:
		if (val->name_size == 0) {
			*res = strdup("<anonymous function>");
			break;
		}

		asprintf(res, "<%s %.*s>", val_type_str(val), (int) val->name_size, val->name);
		break;
	case FLAMINGO_VAL_KIND_INST:;
		flamingo_val_t* const class = val->inst.class;
		assert(class != NULL);

		asprintf(res, "<instance of %.*s>", (int) class->name_size, class->name);
		break;
	default:
		return error(flamingo, "can't print expression kind: %s (%d)", val_type_str(val), val->kind);
	}

	assert(*res != NULL);
	return 0;
}

static int parse_print(flamingo_t* flamingo, TSNode node) {
	assert(ts_node_child_count(node) == 2);

	TSNode const msg_node = ts_node_child_by_field_name(node, "msg", 3);
	char const* const type = ts_node_type(msg_node);

	if (strcmp(ts_node_type(msg_node), "expression") != 0) {
		return error(flamingo, "expected expression for message, got %s", type);
	}

	flamingo_val_t* val = NULL;

	if (parse_expr(flamingo, msg_node, &val, NULL) < 0) {
		return -1;
	}

	// XXX Don't forget to decrement reference at the end!

	char* to_print = NULL;
	int rv = repr(flamingo, val, &to_print, false);

	if (rv == 0) {
		assert(to_print != NULL);
		printf("%s\n", to_print);
	}

	free(to_print);
	val_decref(val);

	return rv;
}
