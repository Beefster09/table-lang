#pragma once

// ... Assumed to be coming from ast.h


// atoms

typedef struct NODE_QUALNAME {
	AST_NODE_COMMON_FIELDS
	const char* ARRAY parts;
} AST_Qualname;

typedef struct NODE_NAME {
	AST_NODE_COMMON_FIELDS
	const char* name;
} AST_Name;

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

typedef struct NODE_NULL {
	AST_NODE_COMMON_FIELDS
} AST_Null;

// top-level stuff

typedef struct NODE_BLOCK {
	AST_NODE_COMMON_FIELDS
	AST_Node* ARRAY body;
} AST_Block;

typedef struct NODE_IMPORT {
	AST_NODE_COMMON_FIELDS
	AST_Name* local_name;
	AST_Qualname* qualified_name;
	const char* imported_file;
	void* module_handle;
	bool is_using;
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
	AST_Block* body;
	bool pub;
} AST_FuncDef;

typedef struct NODE_MACRO {
	AST_NODE_COMMON_FIELDS
	AST_Name* name;
	struct { const char* key; AST_Param* value; } MAP params;
	AST_Node* expansion;
	bool pub;
} AST_Macro;

typedef struct NODE_FUNC_OVERLOAD {
	AST_NODE_COMMON_FIELDS
	AST_Name* name;
	AST_Name* ARRAY overloads;
} AST_FuncOverload;

typedef struct NODE_CONST {
	AST_NODE_COMMON_FIELDS
	AST_Name* name;
	AST_Node* type;
	AST_Node* value;
	bool pub;
} AST_Const;

typedef struct NODE_FIELD {
	AST_NODE_COMMON_FIELDS
	AST_Name* name;
	AST_Node* type;
	AST_Node* default_value;
	bool is_using;
} AST_Field;

typedef struct NODE_STRUCT {
	AST_NODE_COMMON_FIELDS
	AST_Name* name;
	AST_Node* ARRAY constraints;
	struct { const char* key; AST_Field* value; } MAP fields;
	bool pub;
} AST_Struct;

typedef struct NODE_TEST {
	AST_NODE_COMMON_FIELDS
	AST_String* description;
	AST_Block* body;
} AST_Test;

typedef struct NODE_MODULE {
	AST_NODE_COMMON_FIELDS
	struct { const char* key; AST_Node* value; } MAP scope;
	AST_Test* ARRAY tests;
} AST_Module;

// Expressions

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

typedef struct NODE_BROADCAST {
	AST_NODE_COMMON_FIELDS
	AST_Node* target;
} AST_Broadcast;

typedef struct NODE_ASYNC {
	AST_NODE_COMMON_FIELDS
	AST_Node* target;
} AST_Async;

typedef struct NODE_AWAIT {
	AST_NODE_COMMON_FIELDS
	AST_Node* target;
} AST_Await;

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

typedef struct NODE_SLICE {
	AST_NODE_COMMON_FIELDS
	AST_Node* start;
	AST_Node* end;
	AST_Node* step;
	bool is_inclusive;
} AST_Slice;

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

// Types

typedef struct NODE_SIMPLE_TYPE {
	AST_NODE_COMMON_FIELDS
	AST_Qualname* base;
} AST_SimpleType;

typedef struct NODE_POINTER_TYPE {
	AST_NODE_COMMON_FIELDS
	AST_Node* base;
} AST_PointerType;

typedef struct NODE_MUTABLE_TYPE {
	AST_NODE_COMMON_FIELDS
	AST_Node* base;
} AST_MutableType;

typedef struct NODE_OPTIONAL_TYPE {
	AST_NODE_COMMON_FIELDS
	AST_Node* base;
} AST_OptionalType;

typedef struct NODE_ARRAY_TYPE {
	AST_NODE_COMMON_FIELDS
	AST_Node* element_type;
	AST_Node* ARRAY shape;
	// shape is empty or null & is_dynamic = dynamically resizable 1-dimensional array
	// shape is empty or null & !is_dynamic = fixed shape of any dimensionality determined at runtime
	// null value in slot = fixed size determined at runtime
	bool is_dynamic;
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

// Statements

typedef struct NODE_VAR_DECL {
	AST_NODE_COMMON_FIELDS
	AST_Name* name;
	AST_Node* type;
	AST_Node* value;
	bool is_uninitialized;
} AST_VarDecl;

typedef struct NODE_OP_ASSIGN {
	AST_NODE_COMMON_FIELDS
	const char* op;
	AST_Node* dest_expr;
	AST_Node* src_expr;
} AST_OpAssign;

typedef struct NODE_ASSIGN {
	AST_NODE_COMMON_FIELDS
	AST_Node* ARRAY dest_exprs;
	AST_Node* src_expr;
} AST_AssignChain;

typedef struct NODE_ASSIGN_MANY {
	AST_NODE_COMMON_FIELDS
	AST_Node* ARRAY dest_exprs;
	AST_Node* ARRAY src_exprs;
} AST_AssignParallel;

typedef union {
	NodeType node_type;
	AST_AssignChain as_chain;
	AST_AssignParallel as_parallel;
} AST_Assignment;

typedef struct NODE_IF_STMT {
	AST_NODE_COMMON_FIELDS
	AST_Node* condition;
	AST_Block* body;
	AST_Node* alternative;
} AST_IfStatement;

typedef struct NODE_WHILE_LOOP {
	AST_NODE_COMMON_FIELDS
	AST_Node* condition;
	AST_Block* body;
} AST_WhileLoop;

typedef struct NODE_FOR_SIMPLE {
	AST_NODE_COMMON_FIELDS
	AST_Name* name;
	AST_Node* iterable;
} AST_ForSimple;

typedef struct NODE_FOR_RANGE {
	AST_NODE_COMMON_FIELDS
	AST_Name* name;
	AST_Node* start;
	AST_Node* end;
	AST_Node* step;
	bool is_inclusive;
} AST_ForRange;

typedef struct NODE_FOR_PARALLEL {
	AST_NODE_COMMON_FIELDS
	AST_Name* ARRAY names;
	struct { const char* key; AST_Node* value; } MAP zips;
} AST_ForParallel;

typedef enum FOR_ {
	FOR_NORMAL   = 0,
	FOR_PARALLEL = 1,
	FOR_GPU      = 2,
} ForMode;

typedef struct NODE_FOR_LOOP {
	AST_NODE_COMMON_FIELDS
	AST_Node* ARRAY iterables;
	AST_Name* label;
	AST_Block* body;
	// ForMode mode;
	int mode;
} AST_ForLoop;

typedef struct NODE_MATCH_CASE {
	AST_NODE_COMMON_FIELDS
	AST_Node* ARRAY patterns;
	AST_Block* body;
} AST_MatchCase;

typedef struct NODE_MATCH {
	AST_NODE_COMMON_FIELDS
	AST_MatchCase* ARRAY patterns;
} AST_Match;

typedef struct NODE_CONTEXT {
	AST_NODE_COMMON_FIELDS
	AST_Name* name;
	AST_Node* value;
} AST_Context;

typedef struct NODE_WITH {
	AST_NODE_COMMON_FIELDS
	AST_Context* ARRAY contexts;
} AST_With;

typedef struct NODE_RETURN {
	AST_NODE_COMMON_FIELDS
	AST_Node* value;
} AST_Return;

typedef struct NODE_BREAK {
	AST_NODE_COMMON_FIELDS
	AST_Name* label;
} AST_Break;

typedef struct NODE_SKIP {
	AST_NODE_COMMON_FIELDS
	AST_Name* label;
} AST_Skip;

typedef struct NODE_FAIL {
	AST_NODE_COMMON_FIELDS
	AST_Node* value;
	AST_Node* message;
} AST_Fail;

typedef struct NODE_ASSERT {
	AST_NODE_COMMON_FIELDS
	AST_Node* value;
	AST_Node* message;
} AST_Assert;

typedef struct NODE_DEFER {
	AST_NODE_COMMON_FIELDS
	AST_Node* code;
} AST_Defer;

typedef struct NODE_CANCEL {
	AST_NODE_COMMON_FIELDS
	AST_Node* target;
} AST_Cancel;
