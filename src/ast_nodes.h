#pragma once

// ... Assumed to be coming from ast.h

typedef struct NODE_QUALNAME {
	AST_NODE_COMMON_FIELDS
	const char* ARRAY parts;
} AST_Qualname;

typedef struct NODE_NAME {
	AST_NODE_COMMON_FIELDS
	const char* name;
} AST_Name;

typedef struct NODE_IMPORT {
	AST_NODE_COMMON_FIELDS
	AST_Name* local_name;
	AST_Qualname* qualified_name;
	const char* imported_file;
	void* module_handle;
} AST_Import;

typedef struct NODE_PARAM {
	AST_NODE_COMMON_FIELDS
	AST_Name* name;
	AST_Node* type;
	AST_Node* default_value;
	bool is_vararg;
	bool is_kw_only;
} AST_Param;

typedef struct NODE_FUNC_DEF {
	AST_NODE_COMMON_FIELDS
	AST_Name* name;
	struct { const char* key; AST_Param* value; } MAP params;
	AST_Node* ret_type;
	AST_Node* ARRAY body;
	bool pub;
} AST_FuncDef;

typedef struct NODE_FUNC_OVERLOAD {
	AST_NODE_COMMON_FIELDS
	const char* name;
	AST_FuncDef* ARRAY overloads;
} AST_FuncOverload;

typedef struct NODE_CONST {
	AST_NODE_COMMON_FIELDS
	AST_Name* name;
	AST_Node* type;
	AST_Node* value;
	bool pub;
} AST_Const;

typedef struct NODE_MODULE {
	AST_NODE_COMMON_FIELDS
	struct { const char* key; AST_Node* value; } MAP scope;
	// const char* ARRAY exports;
} AST_Module;

typedef struct NODE_INT {
	AST_NODE_COMMON_FIELDS
	intmax_t value;
} AST_Int;

typedef struct NODE_FLOAT {
	AST_NODE_COMMON_FIELDS
	long double value;
} AST_Float;

typedef struct NODE_BOOL {
	AST_NODE_COMMON_FIELDS
	bool value;
} AST_Bool;

typedef struct NODE_STRING {
	AST_NODE_COMMON_FIELDS
	const char* value;
} AST_String;

typedef struct NODE_CHAR {
	AST_NODE_COMMON_FIELDS
	Rune value;
} AST_Char;

typedef struct NODE_BINOP {
	AST_NODE_COMMON_FIELDS
	const char* op;
	AST_Node* lhs;
	AST_Node* rhs;
} AST_Binop;

typedef struct NODE_COMPARISON {
	AST_NODE_COMMON_FIELDS
	const char* ARRAY comparisons;
	AST_Node* ARRAY operands;
} AST_ComparisonChain;

typedef struct NODE_UNARY {
	AST_NODE_COMMON_FIELDS
	const char* op;
	AST_Node* expr;
} AST_Unary;

typedef struct NODE_NOT {
	AST_NODE_COMMON_FIELDS
	AST_Node* expr;
} AST_Not;

typedef struct NODE_AND {
	AST_NODE_COMMON_FIELDS
	AST_Node* lhs;
	AST_Node* rhs;
} AST_And;

typedef struct NODE_OR {
	AST_NODE_COMMON_FIELDS
	AST_Node* lhs;
	AST_Node* rhs;
} AST_Or;

typedef struct NODE_TERNARY {
	AST_NODE_COMMON_FIELDS
	AST_Node* condition;
	AST_Node* true_expr;
	AST_Node* false_expr;
} AST_Ternary;

typedef struct NODE_REREFERENCE {
	AST_NODE_COMMON_FIELDS
	AST_Node* target;
	int levels;
} AST_Reref;

typedef struct NODE_ARRAY {
	AST_NODE_COMMON_FIELDS
	AST_Node* ARRAY elements;
} AST_Array;

typedef struct NODE_FUNC_CALL {
	AST_NODE_COMMON_FIELDS
	AST_Node* func;
	AST_Node* ARRAY pos_args;
	struct { const char* key; AST_Node* value; } MAP kw_args;
	bool is_word_op;
} AST_FuncCall;

typedef struct NODE_SUBSCRIPT {
	AST_NODE_COMMON_FIELDS
	AST_Node* array;
	AST_Node* ARRAY subscripts;
} AST_Subscript;

typedef struct NODE_FIELD_ACCESS {
	AST_NODE_COMMON_FIELDS
	AST_Node* base;
	AST_Qualname* field;
} AST_FieldAccess;

typedef struct NODE_SIMPLE_TYPE {
	AST_NODE_COMMON_FIELDS
	AST_Qualname* base;
	bool is_nullable;
	bool is_mutable;
} AST_SimpleType;

typedef struct NODE_POINTER_TYPE {
	AST_NODE_COMMON_FIELDS
	AST_Node* base;
	bool is_nullable;
	bool is_mutable;
} AST_PointerType;

typedef struct NODE_ARRAY_TYPE {
	AST_NODE_COMMON_FIELDS
	AST_Node* element_type;
	size_t ARRAY dimensions;
	// empty/null = any dimensionality
	// negative value in slot = fixed runtime size
	bool is_dynamic;
	bool is_mutable;
	bool is_nullable;
} AST_ArrayType;

typedef struct NODE_FUNC_TYPE {
	AST_NODE_COMMON_FIELDS
	AST_Node* ARRAY param_types;
	AST_Node* return_type;
} AST_FuncType;

typedef struct NODE_TEMPLATE_TYPE {
	AST_NODE_COMMON_FIELDS
	AST_Node* ARRAY arguments;
} AST_TemplateType;

typedef struct NODE_UNION {
	AST_NODE_COMMON_FIELDS
	AST_Node* ARRAY variants;
} AST_UnionType;
