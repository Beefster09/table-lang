#pragma once

// ... Assumed to be coming from ast.h

typedef struct NODE_QUALNAME {
    AST_NODE_COMMON_FIELDS
    const char** parts;
} AST_Qualname;

typedef void AST_Param;
typedef void AST_KeywordParam;

typedef struct NODE_PARAMS {
    AST_NODE_COMMON_FIELDS
    AST_Param** pos_params;
    AST_KeywordParam** kw_params;
} AST_Params;

typedef union {
    NodeType _tag;
} AST_Type;

typedef struct NODE_IMPORT {
    AST_NODE_COMMON_FIELDS
    const char* local_name;
    union {
        AST_Qualname* qualified_name;
        const char* imported_file;
    };
    void* module_handle;
} AST_Import;

typedef struct NODE_FUNC {
    AST_NODE_COMMON_FIELDS
    const char* name;
    AST_Params* params;
    AST_Type* ret_type;
    AST_Node* body;
    bool pub;
} AST_Function;

typedef struct NODE_MODULE {
    AST_NODE_COMMON_FIELDS
    AST_Import** imports;
    AST_Node** declarations;
} AST_Module;
