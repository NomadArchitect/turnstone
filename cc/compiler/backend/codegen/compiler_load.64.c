/**
 * @file compiler_load.64.c
 * @brief
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <compiler/compiler.h>
#include <logging.h>
#include <strings.h>
#include <utils.h>

MODULE("turnstone.compiler.codegen");

int8_t compiler_execute_load_int(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result);
int8_t compiler_execute_load_var(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result);


int8_t compiler_execute_load_int(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result) {
    *result = node->token->value;

    buffer_printf(compiler->text_buffer, "# begin load const %lli\n", *result);

    compiler->is_at_reg = false;
    compiler->is_const = true;

    compiler->computed_size = node->token->size;

    buffer_printf(compiler->text_buffer, "# const size %lli\n", compiler->computed_size);

    compiler->computed_type = COMPILER_SYMBOL_TYPE_INTEGER;

    buffer_printf(compiler->text_buffer, "# end load const %lli\n", *result);

    return 0;
}

int8_t compiler_execute_load_var(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result) {

    compiler->is_at_reg = false;
    compiler->is_const = false;

    const compiler_symbol_t * symbol = compiler_find_symbol(compiler, node->token->text);

    if(symbol == NULL) {
        PRINTLOG(COMPILER, LOG_ERROR, "symbol %s not found", node->token->text);
        return -1;
    }


    int64_t src_size = symbol->size;
    compiler_symbol_type_t src_type = symbol->type;
    compiler_symbol_type_t src_hidden_type = symbol->hidden_type;

    int64_t extra_offset = 0;

    if(symbol->type == COMPILER_SYMBOL_TYPE_CUSTOM) {
        if(node->next == NULL) {
            PRINTLOG(COMPILER, LOG_ERROR, "struct field not found");
            return -1;
        }

        const compiler_type_t* type = hashmap_get(compiler->types_by_id, (void*)symbol->custom_type_id);

        if(type == NULL) {
            PRINTLOG(COMPILER, LOG_ERROR, "type by id %lli not found", symbol->custom_type_id);
            return -1;
        }

        const compiler_type_field_t* field = hashmap_get(type->field_map, node->next->token->text);

        if(field == NULL) {
            PRINTLOG(COMPILER, LOG_ERROR, "field %s not found in type %s", node->left->token->text, type->name);
            return -1;
        }

        extra_offset = field->offset;
        src_size = field->symbol_size;
        src_type = field->symbol_type;
        src_hidden_type = field->symbol_hidden_type;
    }

    compiler->computed_size = src_size;
    compiler->computed_type = src_type;

    buffer_printf(compiler->text_buffer, "# begin load var %s\n", symbol->name);

    int64_t array_index = 0;
    int64_t array_index_reg = -1;

    if(node->is_array_subscript) {

        if(compiler_execute_ast_node(compiler, node->array_subscript, &array_index) != 0) {
            PRINTLOG(COMPILER, LOG_ERROR, "cannot execute array index");
            return -1;
        }

        if(compiler->is_at_reg) {
            array_index_reg = node->array_subscript->used_register;
            buffer_printf(compiler->text_buffer, "\tmovsx %%%s, %%%s\n",
                          compiler_cast_reg_to_size(compiler_regs[array_index_reg], compiler->computed_size),
                          compiler_regs[array_index_reg]);
        }
    }

    int16_t reg = compiler_find_free_reg(compiler);

    if(reg == -1) {
        PRINTLOG(COMPILER, LOG_ERROR, "no free register");
        return -1;
    }

    compiler->busy_regs[reg] = true;
    compiler->is_at_reg = true;
    node->used_register = reg;

    *result = symbol->int_value;

    if(!symbol->is_local) {
        buffer_printf(compiler->text_buffer, "\tmov $%s@GOT, %%%s\n", symbol->name, compiler_regs[reg]);
        buffer_printf(compiler->text_buffer, "\tmov (%%r15, %%%s), %%%s\n", compiler_regs[reg], compiler_regs[reg]);
    } else {
        if(src_hidden_type == COMPILER_SYMBOL_TYPE_STRING) {
            buffer_printf(compiler->text_buffer, "\tmov -%d(%%rbp), %%%s\n", symbol->stack_offset, compiler_regs[reg]);
        } else {
            buffer_printf(compiler->text_buffer, "\tlea -%d(%%rbp), %%%s\n", symbol->stack_offset, compiler_regs[reg]);
        }
    }

    if(extra_offset != 0) {
        buffer_printf(compiler->text_buffer, "\tadd $%lli, %%%s\n", extra_offset, compiler_regs[reg]);
    }

    const char_t* src = NULL;

    boolean_t deref = true;

    if(node->is_array_subscript) {
        if(array_index_reg != -1) {
            int8_t scale = 1;

            if(src_type == COMPILER_SYMBOL_TYPE_INTEGER) {
                scale = symbol->size / 8;
            } else if(src_type == COMPILER_SYMBOL_TYPE_STRING) {
                scale = 1;
            }

            src = sprintf("%lli(%%%s, %%%s, %d)", array_index, compiler_regs[reg], compiler_regs[array_index_reg], scale);
        } else {
            src = sprintf("%lli(%%%s)", array_index, compiler_regs[reg]);
        }
    } else {
        src = sprintf("(%%%s)", compiler_regs[reg]);

        if(symbol->is_array) {
            deref = false;
        }
    }

    if(deref) {
        buffer_printf(compiler->text_buffer, "\tmov%c %s, %%%s\n",
                      compiler_get_reg_suffix(src_size),
                      src,
                      compiler_cast_reg_to_size(compiler_regs[reg], src_size));
    }

    memory_free((void*)src);

    buffer_printf(compiler->text_buffer, "# end load var %s\n", symbol->name);

    if(node->is_array_subscript) {
        if(array_index_reg != -1) {
            compiler->busy_regs[array_index_reg] = false; // free register
        }
    }

    return 0;
}

int8_t compiler_execute_load(compiler_t* compiler, compiler_ast_node_t* node, int64_t* result) {
    if(node->type == COMPILER_AST_NODE_TYPE_INTEGER_CONST) {
        return compiler_execute_load_int(compiler, node, result);
    }

    if(node->type == COMPILER_AST_NODE_TYPE_STRING_CONST) {
        return compiler_execute_string_const(compiler, node, result);
    }

    if(node->type == COMPILER_AST_NODE_TYPE_VAR) {
        return compiler_execute_load_var(compiler, node, result);
    }

    PRINTLOG(COMPILER, LOG_ERROR, "Invalid node type for load");
    return -1;
}
