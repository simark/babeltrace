/*
 * Copyright (c) 2019 EfficiOS Inc. and Linux Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define BT_LOG_TAG "CLI-CFG-SRC-AUTO-DISC"
#include "logging.h"

#include "babeltrace2-cfg-src-auto-disc.h"
#include "babeltrace2-plugins.h"
#include "babeltrace2-query.h"
#include "common/common.h"

/* Finalize and free a `struct auto_source_discovery_result`. */

static
void auto_source_discovery_result_destroy(struct auto_source_discovery_result *res)
{
	if (res) {
		g_free(res->group);
		bt_value_put_ref(res->inputs);
		g_free(res);
	}
}

/* Allocate and initialize a `struct auto_source_discovery_result`. */

static
struct auto_source_discovery_result *auto_source_discovery_result_create(
		const char *plugin_name, const char *source_cc_name,
		const char *group)
{
	struct auto_source_discovery_result *res;

	res = g_new0(struct auto_source_discovery_result, 1);
	if (!res) {
		BT_CLI_LOGE_APPEND_CAUSE(
			"Failed to allocate a auto_source_discovery_result structure.");
		goto error;
	}

	res->plugin_name = plugin_name;
	res->source_cc_name = source_cc_name;
	res->group = g_strdup(group);
	if (group && !res->group) {
		BT_CLI_LOGE_APPEND_CAUSE("Failed to allocate a string.");
		goto error;
	}

	res->inputs = bt_value_array_create();
	if (!res->inputs) {
		BT_CLI_LOGE_APPEND_CAUSE("Failed to allocate an array value.");
		goto error;
	}

	res->original_input_indices = bt_value_array_create();
	if (!res->original_input_indices) {
		BT_CLI_LOGE_APPEND_CAUSE("Failed to allocate an array value.");
		goto error;
	}

	goto end;
error:
	auto_source_discovery_result_destroy(res);

end:
	return res;
}

/* Finalize a `struct auto_source_discovery`. */

void auto_source_discovery_fini(struct auto_source_discovery *auto_disc)
{
	if (auto_disc->results) {
		g_ptr_array_free(auto_disc->results, TRUE);
	}
}

/* Initialize an already allocated `struct auto_source_discovery`. */

int auto_source_discovery_init(struct auto_source_discovery *auto_disc)
{
	int status;

	auto_disc->results = g_ptr_array_new_with_free_func(
		(GDestroyNotify) auto_source_discovery_result_destroy);

	if (!auto_disc->results) {
		goto error;
	}

	status = 0;
	goto end;

error:
	auto_source_discovery_fini(auto_disc);
	status = -1;

end:

	return status;
}

/*
 * Assign `input` to source component class `source_cc_name` of plugin
 * `plugin_name`, in the group with key `group`.
 */

static
int auto_source_discovery_add(struct auto_source_discovery *auto_disc,
		const char *plugin_name,
		const char *source_cc_name,
		const char *group,
		const char *input,
		uint64_t original_input_index)
{
	int status;
	bt_value_array_append_element_status append_status;
	guint len;
	guint i;
	struct auto_source_discovery_result *res = NULL;

	len = auto_disc->results->len;
	i = len;

	if (group) {
		for (i = 0; i < len; i++) {
			res = g_ptr_array_index(auto_disc->results, i);

			if (strcmp(res->plugin_name, plugin_name) != 0) {
				continue;
			}

			if (strcmp(res->source_cc_name, source_cc_name) != 0) {
				continue;
			}

			if (g_strcmp0(res->group, group) != 0) {
				continue;
			}

			break;
		}
	}

	if (i == len) {
		/* Add a new result entry. */
		res = auto_source_discovery_result_create(plugin_name,
			source_cc_name, group);
		if (!res) {
			goto error;
		}

		g_ptr_array_add(auto_disc->results, res);
	}

	append_status = bt_value_array_append_string_element(res->inputs, input);
	if (append_status != BT_VALUE_ARRAY_APPEND_ELEMENT_STATUS_OK) {
		BT_CLI_LOGE_APPEND_CAUSE("Failed to append a string value.");
		goto error;
	}

	if (bt_value_array_get_size(res->original_input_indices) == 0 || bt_value_integer_unsigned_get(bt_value_array_borrow_element_by_index(res->original_input_indices, bt_value_array_get_size(res->original_input_indices) - 1)) != original_input_index) {
		bt_value_array_append_unsigned_integer_element(res->original_input_indices, original_input_index);
	}

	status = 0;
	goto end;

error:
	status = -1;

end:
	return status;
}

static
int convert_weight_value(const bt_value *weight_value, double *weight,
		const char *plugin_name, const char *source_cc_name,
		const char *input, const char *input_type)
{
	enum bt_value_type weight_value_type;
	int status;

	weight_value_type = bt_value_get_type(weight_value);

	if (weight_value_type == BT_VALUE_TYPE_REAL) {
		*weight = bt_value_real_get(weight_value);
	} else if (weight_value_type == BT_VALUE_TYPE_SIGNED_INTEGER) {
		/* Accept signed integer as a convenience for "return 0" or "return 1" in Python. */
		*weight = bt_value_integer_signed_get(weight_value);
	} else {
		BT_LOGW("babeltrace.support-info query: unexpected type for weight: "
			"component-class-name=source.%s.%s, input=%s, input-type=%s, "
			"expected-entry-type=%s, actual-entry-type=%s",
			plugin_name, source_cc_name, input, input_type,
			bt_common_value_type_string(BT_VALUE_TYPE_REAL),
			bt_common_value_type_string(bt_value_get_type(weight_value)));
		goto error;
	}

	if (*weight < 0.0 || *weight > 1.0) {
		BT_LOGW("babeltrace.support-info query: weight value is out of range [0.0, 1.0]: "
			"component-class-name=source.%s.%s, input=%s, input-type=%s, "
			"weight=%f",
			plugin_name, source_cc_name, input, input_type, *weight);
		goto error;
	}

	status = 0;
	goto end;

error:
	status = -1;

end:
	return status;
}

/*
 * Query all known source components to see if any of them can handle `input`
 * as the given `type`(arbitrary string, directory or file).
 *
 * If `plugin_restrict` is non-NULL, only query source component classes provided
 * by the plugin with that name.
 *
 * If `component_class_restrict` is non-NULL, only query source component classes
 * with that name.
 *
 * Return:
 *
 * - > 0 on success, if no source component class has reported that it handles `input`
 * -   0 on success, if a source component class has reported that it handles `input`
 * - < 0 on failure (e.g. memory error)
 */
static
int support_info_query_all_sources(const char *input,
		const char *input_type,
		uint64_t original_input_index,
		size_t plugin_count,
		const char *plugin_restrict,
		const char *component_class_restrict,
		enum bt_logging_level log_level,
		struct auto_source_discovery *auto_disc)
{
	bt_value_map_insert_entry_status insert_status;
	bt_value *query_params = NULL;
	int status;
	size_t i_plugins;
	const struct bt_value *query_result = NULL;
	struct {
		const bt_component_class_source *source;
		const bt_plugin *plugin;
		const bt_value *group;
		double weigth;
	} winner = { NULL, NULL, NULL, 0 };

	query_params = bt_value_map_create();
	if (!query_params) {
		BT_CLI_LOGE_APPEND_CAUSE("Failed to allocate a map value.");
		goto error;
	}

	insert_status = bt_value_map_insert_string_entry(query_params, "input", input);
	if (insert_status != BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK) {
		BT_CLI_LOGE_APPEND_CAUSE("Failed to insert a map entry.");
		goto error;
	}

	insert_status = bt_value_map_insert_string_entry(query_params, "type", input_type);
	if (insert_status != BT_VALUE_MAP_INSERT_ENTRY_STATUS_OK) {
		BT_CLI_LOGE_APPEND_CAUSE("Failed to insert a map entry.");
		goto error;
	}

	for (i_plugins = 0; i_plugins < plugin_count; i_plugins++) {
		const bt_plugin *plugin;
		const char *plugin_name;
		uint64_t source_count;
		uint64_t i_sources;

		plugin = borrow_loaded_plugin(i_plugins);
		plugin_name = bt_plugin_get_name(plugin);

		/*
		 * If the search is restricted to a specific plugin, only consider
		 * the plugin with that name.
		 */
		if (plugin_restrict && strcmp(plugin_restrict, plugin_name) != 0) {
			continue;
		}

		source_count = bt_plugin_get_source_component_class_count(plugin);

		for (i_sources = 0; i_sources < source_count; i_sources++) {
			const bt_component_class_source *source_cc;
			const bt_component_class *cc;
			const char *source_cc_name;
			bt_query_executor_query_status query_status;

			source_cc = bt_plugin_borrow_source_component_class_by_index_const(plugin, i_sources);
			cc = bt_component_class_source_as_component_class_const(source_cc);
			source_cc_name = bt_component_class_get_name(cc);

			/*
			 * If the search is restricted to a specific component class, only consider the
			 * component classes with that name.
			 */
			if (component_class_restrict && strcmp(component_class_restrict, source_cc_name) != 0) {
				continue;
			}

			BT_LOGD("babeltrace.support-info query: before: component-class-name=source.%s.%s, input=%s, "
				"type=%s", plugin_name, source_cc_name, input, input_type);

			BT_VALUE_PUT_REF_AND_RESET(query_result);
			query_status = cli_query(cc, "babeltrace.support-info",
				query_params, log_level, NULL, &query_result,
				NULL);

			if (query_status == BT_QUERY_EXECUTOR_QUERY_STATUS_OK) {
				double weight;
				const bt_value *group_value = NULL;
				enum bt_value_type query_result_type;

				BT_ASSERT(query_result);

				query_result_type = bt_value_get_type(query_result);

				if (query_result_type == BT_VALUE_TYPE_REAL || query_result_type == BT_VALUE_TYPE_SIGNED_INTEGER) {
					if (convert_weight_value(query_result, &weight, plugin_name, source_cc_name, input, input_type) != 0) {
						/* convert_weight_value has already warned. */
						continue;
					}
				} else if (query_result_type == BT_VALUE_TYPE_MAP) {
					const bt_value *weight_value;

					if (!bt_value_map_has_entry(query_result, "weight")) {
						BT_LOGW("babeltrace.support-info query: result is missing `weight` entry: "
							"component-class-name=source.%s.%s, input=%s, input-type=%s",
							bt_plugin_get_name(plugin),
							bt_component_class_get_name(cc), input,
							input_type);
						continue;
					}

					weight_value = bt_value_map_borrow_entry_value_const(query_result, "weight");
					BT_ASSERT(weight_value);

					if (convert_weight_value(weight_value, &weight, plugin_name, source_cc_name, input, input_type) != 0) {
						/* convert_weight_value has already warned. */
						continue;
					}

					if (bt_value_map_has_entry(query_result, "group")) {
						enum bt_value_type group_value_type;

						group_value = bt_value_map_borrow_entry_value_const(query_result, "group");
						BT_ASSERT(group_value);

						group_value_type = bt_value_get_type(group_value);

						if (group_value_type == BT_VALUE_TYPE_NULL) {
							/* Do as if no value was passed. */
							group_value = NULL;
						} else if (bt_value_get_type(group_value) != BT_VALUE_TYPE_STRING) {
							BT_LOGW("babeltrace.support-info query: unexpected type for entry `group`: "
								"component-class-name=source.%s.%s, input=%s, input-type=%s, "
								"expected-entry-type=%s,%s, actual-entry-type=%s",
								bt_plugin_get_name(plugin),
								bt_component_class_get_name(cc), input,
								input_type,
								bt_common_value_type_string(BT_VALUE_TYPE_NULL),
								bt_common_value_type_string(BT_VALUE_TYPE_STRING),
								bt_common_value_type_string(bt_value_get_type(group_value)));
							continue;
						}
					}
				} else {
					BT_LOGW("babeltrace.support-info query: unexpected result type: "
						"component-class-name=source.%s.%s, input=%s, input-type=%s, "
						"expected-types=%s,%s,%s, actual-type=%s",
						bt_plugin_get_name(plugin),
						bt_component_class_get_name(cc), input,
						input_type,
						bt_common_value_type_string(BT_VALUE_TYPE_REAL),
						bt_common_value_type_string(BT_VALUE_TYPE_MAP),
						bt_common_value_type_string(BT_VALUE_TYPE_SIGNED_INTEGER),
						bt_common_value_type_string(bt_value_get_type(query_result)));
					continue;
				}

				BT_LOGD("babeltrace.support-info query: success: component-class-name=source.%s.%s, input=%s, "
					"type=%s, weight=%f\n",
					bt_plugin_get_name(plugin), bt_component_class_get_name(cc), input,
					input_type, weight);

				if (weight > winner.weigth) {
					winner.source = source_cc;
					winner.plugin = plugin;

					bt_value_put_ref(winner.group);
					winner.group = group_value;
					bt_value_get_ref(winner.group);

					winner.weigth = weight;
				}
			} else if (query_status == BT_QUERY_EXECUTOR_QUERY_STATUS_ERROR) {
				BT_CLI_LOGE_APPEND_CAUSE("babeltrace.support-info query failed.");
				goto error;
			} else if (query_status == BT_QUERY_EXECUTOR_QUERY_STATUS_MEMORY_ERROR) {
				BT_CLI_LOGE_APPEND_CAUSE("Memory error.");
				goto error;
			} else {
				BT_LOGD("babeltrace.support-info query: failure: component-class-name=source.%s.%s, input=%s, "
					"type=%s, status=%s\n",
					bt_plugin_get_name(plugin), bt_component_class_get_name(cc), input,
					input_type,
					bt_common_func_status_string(query_status));
			}
		}
	}

	if (winner.source) {
		const char *source_name;
		const char *plugin_name;
		const char *group;

		source_name = bt_component_class_get_name(
			bt_component_class_source_as_component_class_const(winner.source));
		plugin_name = bt_plugin_get_name(winner.plugin);
		group = winner.group ? bt_value_string_get(winner.group) : NULL;

		BT_LOGI("Input %s is awarded to component class source.%s.%s with weight %f",
			input, plugin_name, source_name, winner.weigth);

		status = auto_source_discovery_add(auto_disc, plugin_name, source_name, group, input, original_input_index);
		if (status != 0) {
			goto error;
		}
	} else {
		BT_LOGI("Input %s (%s) was not recognized by any source component class.",
			input, input_type);
		status = 1;
	}

	goto end;

error:
	status = -1;

end:
	bt_value_put_ref(query_result);
	bt_value_put_ref(query_params);
	bt_value_put_ref(winner.group);

	return status;
}

/*
 * Look for a source component class that recognizes `input` as an arbitrary
 * string.
 *
 * Same return value semantic as `support_info_query_all_sources`.
 */

static
int auto_discover_source_for_input_as_string(const char *input,
		uint64_t original_input_index,
		size_t plugin_count, const char *plugin_restrict,
		const char *component_class_restrict,
		enum bt_logging_level log_level,
		struct auto_source_discovery *auto_disc)
{
	return support_info_query_all_sources(input, "string", original_input_index,
		plugin_count, plugin_restrict, component_class_restrict,
		log_level, auto_disc);
}

static
int auto_discover_source_for_input_as_dir_or_file_rec(GString *input,
		uint64_t original_input_index,
		size_t plugin_count, const char *plugin_restrict,
		const char *component_class_restrict,
		enum bt_logging_level log_level,
		struct auto_source_discovery *auto_disc)
{
	int status;
	GError *error = NULL;

	if (g_file_test(input->str, G_FILE_TEST_IS_REGULAR)) {
		/* It's a file. */
		status = support_info_query_all_sources(input->str,
			"file", original_input_index, plugin_count,
			plugin_restrict, component_class_restrict, log_level, auto_disc);
	} else if (g_file_test(input->str, G_FILE_TEST_IS_DIR)) {
		GDir *dir;
		const gchar *dirent;
		gsize saved_input_len;
		int dir_status = 1;

		/* It's a directory. */
		status = support_info_query_all_sources(input->str,
			"directory", original_input_index, plugin_count,
			plugin_restrict, component_class_restrict, log_level,
			auto_disc);

		if (status < 0) {
			/* Fatal error. */
			goto error;
		} else if (status == 0) {
			/*
			 * A component class claimed this input as a directory,
			 * don't recurse.
			 */
			goto end;
		}

		dir = g_dir_open(input->str, 0, &error);
		if (!dir) {
			const char *fmt = "Failed to open directory %s: %s";
			BT_LOGW(fmt, input->str, error->message);

			if (error->code == G_FILE_ERROR_ACCES) {
				/* This is not a fatal error, we just skip it. */
				status = 1;
				goto end;
			} else {
				BT_CLI_LOGE_APPEND_CAUSE(fmt, input->str,
					error->message);
				goto error;
			}
		}

		saved_input_len = input->len;

		do {
			errno = 0;
			dirent = g_dir_read_name(dir);
			if (dirent) {
				g_string_append_c_inline(input, G_DIR_SEPARATOR);
				g_string_append(input, dirent);

				status = auto_discover_source_for_input_as_dir_or_file_rec(
					input, original_input_index, plugin_count,
					plugin_restrict, component_class_restrict,
					log_level, auto_disc);

				g_string_truncate(input, saved_input_len);

				if (status < 0) {
					/* Fatal error. */
					goto error;
				} else if (status == 0) {
					dir_status = 0;
				}
			} else if (errno != 0) {
				BT_LOGW_ERRNO("Failed to read directory entry", ": dir=%s", input->str);
				goto error;
			}
		} while (dirent);

		status = dir_status;

		g_dir_close(dir);
	} else {
		BT_LOGD("Skipping %s, not a file or directory", input->str);
		status = 1;
	}

	goto end;

error:
	status = -1;

end:

	if (error) {
		g_error_free(error);
	}

	return status;
}

/*
 * Look for a source component class that recognizes `input` as a directory or
 * file.  If `input` is a directory and is not directly recognized, recurse and
 * apply the same logic to children nodes.
 *
 * Same return value semantic as `support_info_query_all_sources`.
 */

static
int auto_discover_source_for_input_as_dir_or_file(const char *input,
		uint64_t original_input_index,
		size_t plugin_count, const char *plugin_restrict,
		const char *component_class_restrict,
		enum bt_logging_level log_level,
		struct auto_source_discovery *auto_disc)
{
	GString *mutable_input;
	int status;

	mutable_input = g_string_new(input);
	if (!mutable_input) {
		status = -1;
		goto end;
	}

	status = auto_discover_source_for_input_as_dir_or_file_rec(
		mutable_input, original_input_index, plugin_count, plugin_restrict,
		component_class_restrict, log_level, auto_disc);

	g_string_free(mutable_input, TRUE);
end:
	return status;
}

int auto_discover_source_components(
		const bt_value *plugin_paths,
		const bt_value *inputs,
		const char *plugin_restrict,
		const char *component_class_restrict,
		enum bt_logging_level log_level,
		struct auto_source_discovery *auto_disc)
{
	uint64_t i_inputs, input_count;
	int status;
	size_t plugin_count;

	input_count = bt_value_array_get_size(inputs);

	status = require_loaded_plugins(plugin_paths);
	if (status != 0) {
		goto end;
	}

	plugin_count = get_loaded_plugins_count();

	for (i_inputs = 0; i_inputs < input_count; i_inputs++) {
		const bt_value *input_value;
		const char *input;

		input_value = bt_value_array_borrow_element_by_index_const(inputs, i_inputs);
		input = bt_value_string_get(input_value);
		status = auto_discover_source_for_input_as_string(input, i_inputs,
			plugin_count, plugin_restrict, component_class_restrict,
			log_level, auto_disc);
		if (status < 0) {
			/* Fatal error. */
			goto end;
		} else if (status == 0) {
			/* A component class has claimed this input as an arbitrary string. */
			continue;
		}

		status = auto_discover_source_for_input_as_dir_or_file(input, i_inputs,
			plugin_count, plugin_restrict, component_class_restrict,
			log_level, auto_disc);
		if (status < 0) {
			/* Fatal error. */
			goto end;
		} else if (status == 0) {
			/*
			 * This input (or something under it) was recognized.
			 */
			continue;
		}

		BT_LOGW("No trace was found based on input `%s`.", input);
	}

	status = 0;
end:
	return status;
}
