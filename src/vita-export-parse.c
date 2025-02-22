#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

#include "vita-export.h"
#include "utils/yamltree.h"
#include "utils/yamltreeutil.h"
#include "utils/sha256.h"
#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

static void print_module_tree(vita_export_t *export)
{
		printf(	"\nLOADED EXPORT CONFIGURATION.\n"
			"MODULE: \"%s\"\n"
			"ATTRIBUTES: 0x%04X\n"
			"NID: 0x%08X\n"
			"VERSION: %u.%u\n"
			"ENTRY: %s\n"
			"STOP: %s\n"
			"EXIT: %s\n"
			"MODULES: %zd\n"
			, export->name, export->attributes, export->nid, export->ver_major, export->ver_minor, export->start, export->stop, export->exit, export->lib_n);
			
	for (int i = 0; i < export->lib_n; ++i) {
		printf(	"\tLIBRARY: \"%s\"\n"
				"\tNID: 0x%08X\n"
				"\tSYSCALL: %s\n"
				"\tFUNCTIONS: %zd\n"
			, export->libs[i]->name, export->libs[i]->nid, export->libs[i]->syscall ? ("true") : ("false"), export->libs[i]->function_n);
			
		for (int j = 0; j < export->libs[i]->function_n; ++j) {
			printf(	"\t\tEXPORT SYMBOL: \"%s\"\n"
					"\t\tNID: 0x%08X\n"
					, export->libs[i]->functions[j]->name, export->libs[i]->functions[j]->nid);
		}
		
		printf("\tVARIABLES: %zd\n", export->libs[i]->variable_n);
		
		for (int j = 0; j < export->libs[i]->variable_n; ++j) {
			printf(	"\t\tEXPORT SYMBOL: \"%s\"\n"
					"\t\tNID: 0x%08X\n"
					, export->libs[i]->variables[j]->name, export->libs[i]->variables[j]->nid);
		}
	}
}

#define EXPORT_NID_DATA_NUMBER (3)

int process_functions(yaml_node *entry, vita_library_export *export) {
	if (is_scalar(entry)) {
		yaml_scalar *key = &entry->data.scalar;

		// create an export symbol for this function
		vita_export_symbol *symbol = malloc(sizeof(vita_export_symbol));
		symbol->name = strdup(key->value);

		if (export->version == 0 || export->version == 1 || export->syscall != 0) {
			symbol->nid = sha256_32_vector(1, (uint8_t **)&key->value, (size_t *)&key->len);
		}
		else {
			uint32_t ver;
			uint8_t *data_ptr[EXPORT_NID_DATA_NUMBER];
			size_t size_ptr[EXPORT_NID_DATA_NUMBER];

			ver = htonl(export->version);

			data_ptr[0] = (uint8_t *)&ver;
			size_ptr[0] = sizeof(ver);

			data_ptr[1] = (uint8_t *)export->name;
			size_ptr[1] = strlen(export->name);

			data_ptr[2] = (uint8_t *)key->value;
			size_ptr[2] = key->len;

			symbol->nid = sha256_32_vector(EXPORT_NID_DATA_NUMBER, (uint8_t **)data_ptr, size_ptr);
		}

		// append to list
		export->functions = realloc(export->functions, (export->function_n+1)*sizeof(const char*));
		export->functions[export->function_n++] = symbol;

		return 0;
	}

	if (is_mapping(entry)) {
		if ((entry->data.mapping.count - 1) != 0) {
			fprintf(stderr, "error: line: %zd, column: %zd, Invalid reference count : %lu\n"
				, entry->position.line
				, entry->position.column
				, entry->data.mapping.count);
			return -1;
		}

		if (is_scalar(entry->data.mapping.pairs[0]->lhs) == 0 || is_scalar(entry->data.mapping.pairs[0]->rhs) == 0) {

			fprintf(stderr, "error: line: %zd, column: %zd, expecting function name to be scalar, got '%s'.\n"
				, entry->position.line
				, entry->position.column
				, node_type_str(entry));
			return -1;
		}

		// create an export symbol for this function
		vita_export_symbol *symbol = malloc(sizeof(vita_export_symbol));
		symbol->name = strdup(entry->data.mapping.pairs[0]->lhs->data.scalar.value);

		if (process_32bit_integer(entry->data.mapping.pairs[0]->rhs, &symbol->nid) < 0) {
			fprintf(stderr, "error: line: %zd, column: %zd, could not convert function nid '%s' to 32 bit integer.\n"
				, entry->data.mapping.pairs[0]->rhs->position.line
				, entry->data.mapping.pairs[0]->rhs->position.column
				, entry->data.mapping.pairs[0]->rhs->data.scalar.value);

			free((void *)symbol->name);
			free(symbol);

			return -1;
		}

		// append to list
		export->functions = realloc(export->functions, (export->function_n+1)*sizeof(const char*));
		export->functions[export->function_n++] = symbol;

		return 0;
	}

	fprintf(stderr, "error: line: %zd, column: %zd, Unhandled type, got '%s'.\n"
		, entry->position.line
		, entry->position.column
		, node_type_str(entry));

	return -1;
}

int process_variables(yaml_node *entry, vita_library_export *export) {
	if (is_scalar(entry)) {
		yaml_scalar *key = &entry->data.scalar;

		// create an export symbol for this variable
		vita_export_symbol *symbol = malloc(sizeof(vita_export_symbol));
		symbol->name = strdup(key->value);

		if (export->version == 0 || export->version == 1 || export->syscall != 0) {
			symbol->nid = sha256_32_vector(1, (uint8_t **)&key->value, (size_t *)&key->len);
		}
		else {
			uint32_t ver;
			uint8_t *data_ptr[EXPORT_NID_DATA_NUMBER];
			size_t size_ptr[EXPORT_NID_DATA_NUMBER];

			ver = htonl(export->version);

			data_ptr[0] = (uint8_t *)&ver;
			size_ptr[0] = sizeof(ver);

			data_ptr[1] = (uint8_t *)export->name;
			size_ptr[1] = strlen(export->name);

			data_ptr[2] = (uint8_t *)key->value;
			size_ptr[2] = key->len;

			symbol->nid = sha256_32_vector(EXPORT_NID_DATA_NUMBER, (uint8_t **)data_ptr, size_ptr);
		}

		// append to list
		export->variables = realloc(export->variables, (export->variable_n+1)*sizeof(const char*));
		export->variables[export->variable_n++] = symbol;

		return 0;
	}

	if (is_mapping(entry)) {
		if ((entry->data.mapping.count - 1) != 0) {
			fprintf(stderr, "error: line: %zd, column: %zd, Invalid reference count : %lu\n"
				, entry->position.line
				, entry->position.column
				, entry->data.mapping.count);
			return -1;
		}

		if (is_scalar(entry->data.mapping.pairs[0]->lhs) == 0 || is_scalar(entry->data.mapping.pairs[0]->rhs) == 0) {

			fprintf(stderr, "error: line: %zd, column: %zd, expecting variable name to be scalar, got '%s'.\n"
				, entry->position.line
				, entry->position.column
				, node_type_str(entry));
			return -1;
		}

		// create an export symbol for this variable
		vita_export_symbol *symbol = malloc(sizeof(vita_export_symbol));
		symbol->name = strdup(entry->data.mapping.pairs[0]->lhs->data.scalar.value);

		if (process_32bit_integer(entry->data.mapping.pairs[0]->rhs, &symbol->nid) < 0) {
			fprintf(stderr, "error: line: %zd, column: %zd, could not convert variable nid '%s' to 32 bit integer.\n"
				, entry->data.mapping.pairs[0]->rhs->position.line
				, entry->data.mapping.pairs[0]->rhs->position.column
				, entry->data.mapping.pairs[0]->rhs->data.scalar.value);

			free((void *)symbol->name);
			free(symbol);

			return -1;
		}

		// append to list
		export->variables = realloc(export->variables, (export->variable_n+1)*sizeof(const char*));
		export->variables[export->variable_n++] = symbol;

		return 0;
	}

	fprintf(stderr, "error: line: %zd, column: %zd, Unhandled type, got '%s'.\n"
		, entry->position.line
		, entry->position.column
		, node_type_str(entry));

	return -1;
}

int process_module_version(yaml_node *parent, yaml_node *child, vita_export_t *info) {
	if (!is_scalar(parent)) {
		fprintf(stderr, "error: line: %zd, column: %zd, expecting module version key to be scalar, got '%s'.\n", parent->position.line, parent->position.column, node_type_str(parent));
		return -1;
	}
	
	yaml_scalar *key = &parent->data.scalar;
	
	if (strcmp(key->value, "major") == 0) {
		if (!is_scalar(child)) {
			fprintf(stderr, "error: line: %zd, column: %zd, expecting module major version to be scalar, got '%s'.\n", child->position.line, child->position.column, node_type_str(child));
			return -1;
		}
		
		uint32_t int32 = 0;
		
		if (process_32bit_integer(child, &int32) < 0) {
			// error code is a bit of a lie, but its more indicative of what is expected
			fprintf(stderr, "error: line: %zd, column: %zd, could not convert module major version '%s' to 8 bit integer.\n", child->position.line, child->position.column, child->data.scalar.value);
			return -1;
		}
		
		if (int32 > UCHAR_MAX) {
			fprintf(stderr, "error: line: %zd, column: %zd, module major version must be no more than 8 bits long.\n", child->position.line, child->position.column);
			return -1;
		}
		
		info->ver_major = (uint8_t)int32;
	}
	else if (strcmp(key->value, "minor") == 0) {
		if (!is_scalar(child)) {
			fprintf(stderr, "error: line: %zd, column: %zd, expecting module minor version to be scalar, got '%s'.\n", child->position.line, child->position.column, node_type_str(child));
			return -1;
		}
		
		uint32_t int32 = 0;
		
		if (process_32bit_integer(child, &int32) < 0) {
			// error code is a bit of a lie, but its more indicative of what is expected
			fprintf(stderr, "error: line: %zd, column: %zd, could not convert module minor version '%s' to 8 bit integer.\n", child->position.line, child->position.column, child->data.scalar.value);
			return -1;
		}
		
		if (int32 > UCHAR_MAX) {
			fprintf(stderr, "error: line: %zd, column: %zd, module minor version must be no more than 8 bits long.\n", child->position.line, child->position.column);
			return -1;
		}
		
		info->ver_minor = (uint8_t)int32;
	}
	else {
		fprintf(stderr, "error: line: %zd, column: %zd, unrecognised module version key '%s'.\n", child->position.line, child->position.column, key->value);
		return -1;
	}
	
	return 0;
}

int process_export(yaml_node *parent, yaml_node *child, vita_library_export *export) {
	if (!is_scalar(parent)) {
		fprintf(stderr, "error: line: %zd, column: %zd, expecting library key to be scalar, got '%s'.\n", parent->position.line, parent->position.column, node_type_str(parent));
		return -1;
	}
	
	yaml_scalar *key = &parent->data.scalar;
	
	if (strcmp(key->value, "syscall") == 0) {
		if (!is_scalar(child)) {
			fprintf(stderr, "error: line: %zd, column: %zd, expecting library syscall flag to be scalar, got '%s'.\n", child->position.line, child->position.column, node_type_str(child));
			return -1;
		}
		
		if (process_boolean(child, &export->syscall) < 0) {
			fprintf(stderr, "error: line: %zd, column: %zd, could not convert export library flag to boolean, got '%s'. expected 'true' or 'false'.\n", child->position.line, child->position.column, child->data.scalar.value);
			return -1;
		}
	}
	else if (strcmp(key->value, "functions") == 0) {
		if (yaml_iterate_sequence(child, (sequence_functor)process_functions, export) < 0)
			return -1;
	}
	else if (strcmp(key->value, "variables") == 0) {
		if (yaml_iterate_sequence(child, (sequence_functor)process_variables, export) < 0)
			return -1;
	}
	else if (strcmp(key->value, "nid") == 0) {
		if (!is_scalar(child)) {
			fprintf(stderr, "error: line: %zd, column: %zd, expecting library nid to be scalar, got '%s'.\n", child->position.line, child->position.column, node_type_str(child));
			return -1;
		}
		
		if (process_32bit_integer(child, &export->nid) < 0) {
			fprintf(stderr, "error: line: %zd, column: %zd, could not convert library nid '%s' to 32 bit integer.\n", child->position.line, child->position.column, child->data.scalar.value);
			return -1;
		}
	}
	else if (strcmp(key->value, "version") == 0) {
		if (!is_scalar(child)) {
			fprintf(stderr, "error: line: %zd, column: %zd, expecting library version to be scalar, got '%s'.\n", child->position.line, child->position.column, node_type_str(child));
			return -1;
		}
		
		if (process_32bit_integer(child, &export->version) < 0) {
			fprintf(stderr, "error: line: %zd, column: %zd, could not convert library version '%s' to 32 bit integer.\n", child->position.line, child->position.column, child->data.scalar.value);
			return -1;
		}

		if (export->version > 0xFFFF) {
			fprintf(stderr, "error: line: %zd, column: %zd, Library version must be 65535 or lower.\n", child->position.line, child->position.column);
			return -1;
		}

		uint32_t ver;
		uint8_t *data_ptr[2];
		size_t size_ptr[2];

		ver = htonl(export->version);

		data_ptr[0] = (uint8_t *)&ver;
		size_ptr[0] = sizeof(ver);

		data_ptr[1] = (uint8_t *)export->name;
		size_ptr[1] = strlen(export->name);

		export->nid = sha256_32_vector(2, (uint8_t **)data_ptr, size_ptr);
	}
	else {
		fprintf(stderr, "error: line: %zd, column: %zd, unrecognised library key '%s'.\n", child->position.line, child->position.column, key->value);
		return -1;
	}
	
	return 0;
}

int process_export_list(yaml_node *parent, yaml_node *child, vita_export_t *info) {
	if (!is_scalar(parent)) {
		fprintf(stderr, "error: line: %zd, column: %zd, expecting export list key to be scalar, got '%s'.\n", parent->position.line, parent->position.column, node_type_str(parent));
		return -1;
	}
	
	yaml_scalar *key = &parent->data.scalar;
	vita_library_export *export = malloc(sizeof(vita_library_export));
	memset(export, 0, sizeof(vita_library_export));
	
	// default values
	export->name = strdup(key->value);
	export->nid = sha256_32_vector(1, (uint8_t **)&key->value, &key->len);
	export->syscall = 0;
	export->version = 1;
	
	if (yaml_iterate_mapping(child, (mapping_functor)process_export, export) < 0)
		return -1;
	
	info->libs = realloc(info->libs, (info->lib_n+1)*sizeof(vita_library_export*));
	info->libs[info->lib_n++] = export;
	return 0;
}

int process_syslib_list(yaml_node *parent, yaml_node *child, vita_export_t *info) {
	if (!is_scalar(parent)) {
		fprintf(stderr, "error: line: %zd, column: %zd, expecting main entry key to be scalar, got '%s'.\n", parent->position.line, parent->position.column, node_type_str(parent));
		return -1;
	}
	
	yaml_scalar *key = &parent->data.scalar;
	
	if (strcmp(key->value, "start") == 0) {
		if (!is_scalar(child)) {
			fprintf(stderr, "error: line: %zd, column: %zd, expecting 'start' entry-point to be scalar, got '%s'.\n", child->position.line, child->position.column, node_type_str(child));
			return -1;
		}
		
		const char *str = NULL;
		if (process_string(child, &str) < 0) {
			fprintf(stderr, "error: line: %zd, column: %zd, could not convert 'start' entry-point to string, got '%s'.\n", child->position.line, child->position.column, child->data.scalar.value);
			return -1;
		}
		
		info->start = strdup(str);
	}
	else if (strcmp(key->value, "bootstart") == 0) {
		if (!is_scalar(child)) {
			fprintf(stderr, "error: line: %zd, column: %zd, expecting 'bootstart' entry-point to be scalar, got '%s'.\n", child->position.line, child->position.column, node_type_str(child));
			return -1;
		}
		const char *str = NULL;
		if (process_string(child, &str) < 0) {
			fprintf(stderr, "error: line: %zd, column: %zd, could not convert 'bootstart' entry-point to string, got '%s'.\n", child->position.line, child->position.column, child->data.scalar.value);
			return -1;
		}
		
		info->bootstart = strdup(str);
	}
	else if (strcmp(key->value, "stop") == 0) {
		if (!is_scalar(child)) {
			fprintf(stderr, "error: line: %zd, column: %zd, expecting 'stop' entry-point to be scalar, got '%s'.\n", child->position.line, child->position.column, node_type_str(child));
			return -1;
		}
		const char *str = NULL;
		if (process_string(child, &str) < 0) {
			fprintf(stderr, "error: line: %zd, column: %zd, could not convert 'stop' entry-point to string, got '%s'.\n", child->position.line, child->position.column, child->data.scalar.value);
			return -1;
		}
		
		info->stop = strdup(str);
	}
	else if (strcmp(key->value, "exit") == 0) {
		if (!is_scalar(child)) {
			fprintf(stderr, "error: line: %zd, column: %zd, expecting 'exit' entry-point to be scalar, got '%s'.\n", child->position.line, child->position.column, node_type_str(child));
			return -1;
		}
		
		const char *str = NULL;
		if (process_string(child, &str) < 0) {
			fprintf(stderr, "error: line: %zd, column: %zd, could not convert 'exit' entry-point to string, got '%s'.\n", child->position.line, child->position.column, child->data.scalar.value);
			return -1;
		}
		
		info->exit = strdup(str);
	}
	else {
		fprintf(stderr, "error: line: %zd, column: %zd, unrecognised entry-point '%s'.\n", child->position.line, child->position.column, key->value);
		return -1;
	}
	
	return 0;
}

int process_module_info(yaml_node *parent, yaml_node *child, vita_export_t *info) {
	if (!is_scalar(parent)) {
		fprintf(stderr, "error: line: %zd, column: %zd, expecting module info key to be scalar, got '%s'.\n", parent->position.line, parent->position.column, node_type_str(parent));
		return -1;
	}
	
	yaml_scalar *key = &parent->data.scalar;
	
	if (strcmp(key->value, "attributes") == 0) {
		// TODO: replace number with enum?
		if (!is_scalar(child)) {
			fprintf(stderr, "error: line: %zd, column: %zd, expecting module attribute to be scalar, got '%s'.\n", child->position.line, child->position.column, node_type_str(child));
			return -1;
		}
		
		uint32_t attrib32 = 0;
		if (process_32bit_integer(child, &attrib32) < 0) {
			// error code is a bit of a lie, but its more indicative of what is expected
			fprintf(stderr, "error: line: %zd, column: %zd, could not convert module attribute '%s' to 16 bit integer.\n", child->position.line, child->position.column, child->data.scalar.value);
			return -1;
		}
		
		if (attrib32 > USHRT_MAX) {
			fprintf(stderr, "error: line: %zd, column: %zd, module attribute must be no more than 16 bits long.\n", child->position.line, child->position.column);
			return -1;
		}
		
		// perform cast to 16 bit
		info->attributes = (uint16_t)attrib32;
	}

	else if (strcmp(key->value, "imagemodule") == 0) {

		if (!is_scalar(child)) {
			fprintf(stderr, "error: line: %zd, column: %zd, expecting imagemodule to be scalar, got '%s'.\n", child->position.line, child->position.column, node_type_str(child));
			return -1;
		}

		key = &child->data.scalar;

		if (strcmp(key->value, "true") == 0) {
			info->is_image_module = 1;
		} else if (strcmp(key->value, "false") == 0) {
			info->is_image_module = 0;
		} else {
			fprintf(stderr, "error: line: %zd, column: %zd, Received unexpected value in imagemodule, got '%s'. Vaild value is \"true\" or \"false\".\n", child->position.line, child->position.column, key->value);
			return -1;
		}
	}

	else if (strcmp(key->value, "version") == 0) {
		if (yaml_iterate_mapping(child, (mapping_functor)process_module_version, info) < 0)
			return -1;
	}
	
	else if (strcmp(key->value, "nid") == 0) {
		if (!is_scalar(child)) {
			fprintf(stderr, "error: line: %zd, column: %zd, expecting module nid to be scalar, got '%s'.\n", child->position.line, child->position.column, node_type_str(child));
			return -1;
		}
		
		if (process_32bit_integer(child, &info->nid) < 0) {
			fprintf(stderr, "error: line: %zd, column: %zd, could not convert module nid '%s' to 32 bit integer.\n", child->position.line, child->position.column, child->data.scalar.value);
			return -1;
		}
	}
	
	else if (strcmp(key->value, "main") == 0) {
		if (yaml_iterate_mapping(child, (mapping_functor)process_syslib_list, info) < 0)
			return -1;
	}
	
	else if (strcmp(key->value, "modules") == 0) {
		fprintf(stderr, "warning: line: %zd, column: %zd, use of 'modules' is deprecated, 'libraries' should be used instead.\n", child->position.line, child->position.column);
		if (yaml_iterate_mapping(child, (mapping_functor)process_export_list, info) < 0)
			return -1;
	}
	else if (strcmp(key->value, "libraries") == 0) {
		if (yaml_iterate_mapping(child, (mapping_functor)process_export_list, info) < 0)
			return -1;
	}
	else {
		fprintf(stderr, "error: line: %zd, column: %zd, module info key '%s'.\n", child->position.line, child->position.column, key->value);
		return -1;
	}
	
	return 0;
}

vita_export_t *read_module_exports(yaml_document *doc, uint32_t default_nid) {
	if (!is_mapping(doc)) {
		fprintf(stderr, "error: line: %zd, column: %zd, expecting root node to be a mapping, got '%s'.\n", doc->position.line, doc->position.column, node_type_str(doc));
		return NULL;
	}
	
	yaml_mapping *root = &doc->data.mapping;
	
	// check we only have one entry
	if (root->count != 1) {
		fprintf(stderr, "error: line: %zd, column: %zd, expecting a single entry within root mapping, got %zd.\n", doc->position.line, doc->position.column, root->count);
		return NULL;
	}
	
	vita_export_t *export = malloc(sizeof(vita_export_t));
	memset(export, 0, sizeof(vita_export_t));
	
	// check lhs is a scalar
	if (!is_scalar(root->pairs[0]->lhs)) {
		fprintf(stderr, "error: line: %zd, column: %zd, expecting a scalar for module name, got '%s'.\n", root->pairs[0]->lhs->position.line, root->pairs[0]->lhs->position.column, node_type_str(root->pairs[0]->lhs));
		return NULL;
	}
	
	if (strlen(root->pairs[0]->lhs->data.scalar.value) >= 27) {
		fprintf(stderr, "error: line: %zd, column: %zd, module name '%s' is too long for module info. use %d characters or less.\n", root->pairs[0]->lhs->position.line, root->pairs[0]->lhs->position.column, root->pairs[0]->lhs->data.scalar.value, 26);
		return NULL;
	}
	
	strncpy(export->name, root->pairs[0]->lhs->data.scalar.value, 27);
	export->nid = default_nid;
	
	if (yaml_iterate_mapping(root->pairs[0]->rhs, (mapping_functor)process_module_info, export) < 0)
		return NULL;

	return export;
}

vita_export_t *vita_exports_load(const char *filename, const char *elf, int verbose)
{
	FILE *fp = fopen(filename, "r");
	if (fp == NULL) {
		fprintf(stderr, "Error: could not open %s\n", filename);
		return NULL;
	}
	vita_export_t *imports = vita_exports_loads(fp, elf, verbose);

	fclose(fp);

	return imports;
}

vita_export_t *vita_exports_loads(FILE *text, const char *elf, int verbose)
{
	uint32_t nid = 0;
	yaml_error error = {0};
	
	yaml_tree *tree = parse_yaml_stream(text, &error);
	
	if (!tree)
	{
		fprintf(stderr, "error: %s\n", error.problem);
		free(error.problem);
		return NULL;
	}
	
	if (tree->count != 1)
	{
		fprintf(stderr, "error: expecting a single yaml document, got: %zd\n", tree->count);
		// TODO: cleanup tree
		return NULL;
	}
	
	if (sha256_32_file(elf, &nid) < 0)
	{
		// TODO: cleanup tree
		return NULL;
	}
	
	return read_module_exports(tree->docs[0], nid);
}

vita_export_t *vita_export_generate_default(const char *elf)
{
	vita_export_t *exports = calloc(1, sizeof(vita_export_t));
	
	// set module name to elf output name
	const char *fs = strrchr(elf, '/');
	const char *bs = strrchr(elf, '\\');
	const char *base = elf;
	
	if (fs && bs){
		base = (fs > bs) ? (&fs[1]) : (bs);
	}
	else if (fs) {
		base = &fs[1];
	}
	else if (bs) {
		base = bs;
	}
	
	// try to copy only the file name if a full path is provided
	strncpy(exports->name, base, sizeof(exports->name));
	
	// default version 1.1
	exports->ver_major = 1;
	exports->ver_minor = 1;
	
	// default attribute of 0
	exports->attributes = 0;

	// nid is SHA256-32 of ELF
	if (sha256_32_file(elf, &exports->nid) < 0)
	{
		free(exports);
		return NULL;
	}

	exports->is_image_module = 0;
	
	// we don't specify any specific symbols
	exports->bootstart = NULL;
	exports->start = NULL;
	exports->stop = NULL;
	exports->exit = NULL;
	
	// we have no libraries to export
	exports->lib_n = 0;
	exports->libs = NULL;
	return exports;
}

void vita_exports_free(vita_export_t *exp)
{
	vita_library_export *curlib;
	vita_export_symbol *cursym;
	int i, j;

	for (i = 0; i < exp->lib_n; i++) 
	{
		curlib = exp->libs[i];
		for (j = 0; j < curlib->function_n; j++)
		{
			cursym = curlib->functions[j];
			free((void *)cursym->name);
			free(cursym);
		}
		for (j = 0; j < curlib->variable_n; j++)
		{
			cursym = curlib->variables[j];
			free((void *)cursym->name);
			free(cursym);
		}

		free(curlib->functions);
		free(curlib->variables);
		free((void *)curlib->name);

		free(curlib);
	}

	free(exp->libs);
	free((void *)exp->exit);
	free((void *)exp->stop);
	free((void *)exp->bootstart);
	free((void *)exp->start);
	free(exp);
}