#include <stdarg.h>
#include "decls.h"
#include "../include/exe.h"


#define CODEBUF_ALLOC_SIZE (256)
#define LINENUM_BUF_ALLOC_SIZE (256)

extern OpcodeInfo opcode_info[];

static 
struct {
    int             size;
    int             cap;
    Byte            *codes;
} codebuf;

static
struct {
    LineNumber  *arr;
    int         size;
    int         cap;
} linenum_buf;

static int linenum;

void reset_code_buf() {
    codebuf.size = 0;
}

void reset_linenum_buf() {
    linenum_buf.size = 0;
}

/*
    start_i is the start index of backpacked codes.
    jump_i is the destination pc of 'start_i' codes.

    We regulate the operand size to fixed 2 byte for 'jump' family opcode.
*/
void backpack(int start_i, int jump_i) {
    codebuf.codes[start_i + 1] = (jump_i >> 8) & 0xff;
    codebuf.codes[start_i + 2] = jump_i & 0xff;
}

static
int emit(OpCodeTag opcode, ...) {
    char *para;
    OpcodeInfo opinfo;
    int operand_sz;
    int start_pc;

    opinfo = opcode_info[opcode];
    switch (opinfo.para[0])
    {
    case '\0':
        operand_sz = 0;
        break;
    case 'b':
        operand_sz = 1;
        break;
    case 'd':    
    case 's':
    case 'p':
        operand_sz = 2;
        break;
    default:
        break;
    }
    
    if(codebuf.size + operand_sz >= codebuf.cap) {
        codebuf.cap += CODEBUF_ALLOC_SIZE;
        codebuf.codes = (Byte*)MEM_realloc(codebuf.codes, sizeof(Byte) * codebuf.cap);
    }
    
    start_pc = codebuf.size;
    codebuf.codes[codebuf.size ++] = (Byte)opcode;
    va_list args;
    va_start(args, opcode);
    for(int i = 0; opinfo.para[i] != '\0'; i ++) {
        unsigned int operand = va_arg(args, int); /* ??? why cast int to unsigned int*/
        switch (opinfo.para[i])
        {
        case 'b':
            codebuf.codes[codebuf.size ++] = (Byte)operand;
            break;
        case 'd':   /* Fallthrough */ 
        case 's':   /* Fallthrough */
        case 'p':
            codebuf.codes[codebuf.size ++] = (Byte)(operand >> 8 & 0xff);
            codebuf.codes[codebuf.size ++] = (Byte)(operand & 0xff);
            break;
        default:
            break;
        }
    }
    
    // if(linenum_buf.arr == NULL ||
    //     linenum_buf.arr[linenum_buf.size - 1].num != linenum) 
    // {
    //     if(linenum_buf.size >= linenum_buf.cap) {
    //         linenum_buf.cap += LINENUM_BUF_ALLOC_SIZE;
    //         linenum_buf.arr = (LineNumber*)MEM_realloc(linenum_buf.arr, linenum_buf.cap * sizeof(LineNumber));
    //     }
    //     LineNumber *newl = &linenum_buf.arr[linenum_buf.size ++];
    //     newl->num = linenum;
    //     newl->start_pc = start_pc;
    //     newl->last_pc = codebuf.size - 1;
    // } else {
    //     LineNumber *curl = &linenum_buf.arr[linenum_buf.size - 1];
    //     curl->last_pc = codebuf.size - 1;
    // }

    va_end(args);

    return start_pc;
}

static
int add_constant_to_pool(Constant *cons, Executable *exe) {
    exe->constant_seg->size ++;
    exe->constant_seg->arr = (Constant*)MEM_realloc(exe->constant_seg->arr, exe->constant_seg->size * sizeof(Constant));
    exe->constant_seg->arr[exe->constant_seg->size - 1] = *cons;

    return exe->constant_seg->size - 1;
}

static
void gen_data_segment(Executable *exe) {

}

static int
type_offset(ValueType basic_type)
{
    switch (basic_type) {
    case BOOLEAN_TYPE:
        return 0;
        break;
    case INT_TYPE:
        return 0;
        break;
    case DOUBLE_TYPE:
        return 1;
        break;
    case STRING_TYPE:
        return 2;
        break;
    case UNDETERMIEND:
    default:
        break;
    }

    return 0;
}

static 
void pop_to_identifier(Expression *ident_expr) {
    int is_local;
    ValueType basic_type;
    int index;
    is_local = ident_expr->ident->decl->u.declaration_stat.is_local;
    basic_type = ident_expr->type->basic_type;
    index = ident_expr->ident->decl->u.declaration_stat.index;
    if(is_local) {
        emit(POP_STACK_INT + type_offset(basic_type), index);
    } else {
        emit(POP_STATIC_INT + type_offset(basic_type), index);
    }
}

void gen_code_of_identifier_expression(Expression *expr) {
    Statement *stat = expr->ident->decl;

    if(expr->ident->is_func) {
        emit(PUSH_FUNCTION, expr->ident->func_def->index);
        return;
    }

    if(stat->u.declaration_stat.is_local) {
        emit(PUSH_STACK_INT + type_offset(expr->type->basic_type), stat->u.declaration_stat.index);
    } else {
        emit(PUSH_STACK_INT + type_offset(expr->type->basic_type), stat->u.declaration_stat.index);
    }
}

static
void gen_code_of_expression(Expression *expr, Executable *exe) {
    int backpack_i; /* the start index of codes which need to backpack. */

    linenum = expr->linenum;
    switch (expr->kind)
    {
    case NORMAL_ASSIGN_EXPRESSION:
        gen_code_of_expression(expr->binary_expr.right, exe);
        emit(DUPLICATE);
        pop_to_identifier(expr->binary_expr.left);
        break;
    case ADD_ASSIGN_EXPRESSION:
        gen_code_of_expression(expr->binary_expr.right, exe);
        gen_code_of_expression(expr->binary_expr.left, exe);
        emit(ADD_INT + type_offset(expr->type->basic_type));
        emit(DUPLICATE);
        pop_to_identifier(expr->binary_expr.left);
        break;
    case SUB_ASSIGN_EXPRESSION:
        gen_code_of_expression(expr->binary_expr.right, exe);
        gen_code_of_expression(expr->binary_expr.left, exe);
        emit(SUB_INT + type_offset(expr->type->basic_type));
        emit(DUPLICATE);
        pop_to_identifier(expr->binary_expr.left);
        break;
    case MUL_ASSIGN_EXPRESSION:
        gen_code_of_expression(expr->binary_expr.right, exe);
        gen_code_of_expression(expr->binary_expr.left, exe);
        emit(MUL_INT + type_offset(expr->type->basic_type));
        emit(DUPLICATE);
        pop_to_identifier(expr->binary_expr.left);
        break;
    case DIV_ASSIGN_EXPRESSION:
        gen_code_of_expression(expr->binary_expr.right, exe);
        gen_code_of_expression(expr->binary_expr.left, exe);
        emit(DIV_INT + type_offset(expr->type->basic_type));
        emit(DUPLICATE);
        pop_to_identifier(expr->binary_expr.left);
        break;
    case MOD_ASSIGN_EXPRESSION:     
        gen_code_of_expression(expr->binary_expr.right, exe);
        gen_code_of_expression(expr->binary_expr.left, exe);
        emit(MOD_INT + type_offset(expr->type->basic_type));
        emit(DUPLICATE);
        pop_to_identifier(expr->binary_expr.left);
        break;        
    case ARITH_ADDITIVE_EXPRESSION:
        gen_code_of_expression(expr->binary_expr.left, exe);
        gen_code_of_expression(expr->binary_expr.right, exe);
        emit(ADD_INT + type_offset(expr->type->basic_type));
        break;
    case ARITH_SUBSTRACTION_EXPRESSION:
        gen_code_of_expression(expr->binary_expr.left, exe);
        gen_code_of_expression(expr->binary_expr.right, exe);
        emit(SUB_INT + type_offset(expr->type->basic_type));
        break;
    case ARITH_MULTIPLICATION_EXPRESSION:
        gen_code_of_expression(expr->binary_expr.left, exe);
        gen_code_of_expression(expr->binary_expr.right, exe);
        emit(MUL_INT + type_offset(expr->type->basic_type));
        break;
    case ARITH_DIVISION_EXPRESSION:
        gen_code_of_expression(expr->binary_expr.left, exe);
        gen_code_of_expression(expr->binary_expr.right, exe);
        emit(DIV_INT + type_offset(expr->type->basic_type));
        break;
    case ARITH_MODULO_EXPRESSION:
        gen_code_of_expression(expr->binary_expr.left, exe);
        gen_code_of_expression(expr->binary_expr.right, exe);
        emit(MOD_INT + type_offset(expr->type->basic_type));
        break;    
    case RELATION_EQ_EXPRESSION: 
        gen_code_of_expression(expr->binary_expr.left, exe);
        gen_code_of_expression(expr->binary_expr.right, exe);
        emit(EQ_INT + type_offset(expr->type->basic_type));
        break;        
    case RELATION_NE_EXPRESSION:
        gen_code_of_expression(expr->binary_expr.left, exe);
        gen_code_of_expression(expr->binary_expr.right, exe);
        emit(NE_INT + type_offset(expr->type->basic_type));
        break;
    case RELATION_GT_EXPRESSION:
        gen_code_of_expression(expr->binary_expr.left, exe);
        gen_code_of_expression(expr->binary_expr.right, exe);
        emit(GT_INT + type_offset(expr->type->basic_type));
        break;
    case RELATION_LT_EXPRESSION:
        gen_code_of_expression(expr->binary_expr.left, exe);
        gen_code_of_expression(expr->binary_expr.right, exe);
        emit(LT_INT + type_offset(expr->type->basic_type));
        break;
    case RELATION_GE_EXPRESSION:
        gen_code_of_expression(expr->binary_expr.left, exe);
        gen_code_of_expression(expr->binary_expr.right, exe);
        emit(GE_INT + type_offset(expr->type->basic_type));
        break;
    case RELATION_LE_EXPRESSION:
        gen_code_of_expression(expr->binary_expr.left, exe);
        gen_code_of_expression(expr->binary_expr.right, exe);
        emit(LE_INT + type_offset(expr->type->basic_type));
        break;
    case LOGICAL_AND_EXPRESSION:
        gen_code_of_expression(expr->binary_expr.left, exe);
        emit(DUPLICATE);
        backpack_i = emit(JUMP_IF_FALSE);
        gen_code_of_expression(expr->binary_expr.right, exe);
        emit(LOGICAL_AND_OP);
        backpack(backpack_i, codebuf.size);
        break;
    case LOGICAL_OR_EXPRESSION:
        gen_code_of_expression(expr->binary_expr.left, exe);
        emit(DUPLICATE);
        backpack_i = emit(JUMP_IF_TRUE);
        gen_code_of_expression(expr->binary_expr.right, exe);
        emit(LOGICAL_OR_OP);
        backpack(backpack_i, codebuf.size);
        break;
    case LOGICAL_NOT_EXPRESSION:
        gen_code_of_expression(expr->unary_operand, exe);
        emit(LOGICAL_NOT_OP);
        break;
    case MINUS_EXPRESSION:
        gen_code_of_expression(expr->unary_operand, exe);
        emit(MINUS_INT  + type_offset(expr->type->basic_type));
        break;
    case FUNC_CALL_EXPRESSION:
        
        break;
    case IDENTIFIER_EXPRESSION:
        gen_code_of_identifier_expression(expr);
        break;
    case COMMA_EXPRESSION:
        break;
    case POST_INCREMENT_EXPRESSION:
    case POST_DECREMENT_EXPRESSION:
        break;
    case CAST_EXPRESSION:               
    case BOOLEAN_LITERAL_EXPRESSION:
        if(expr->boolean_v == 1) {
            emit(PUSH_INT_1BYTE, 1);
        } else {
            emit(PUSH_INT_1BYTE, 0);
        }
        break;
    case INT_LITERAL_EXPRESSION:
        if(expr->int_v >= 0 && expr->int_v <= 255) {
            emit(PUSH_INT_1BYTE, expr->int_v);
        } else if(expr->int_v >= 256 && expr->int_v <= 65535) {
            emit(PUSH_INT_2BYTE, expr->int_v);
        } else {
            Constant *cons = (Constant*)MEM_malloc(sizeof(Constant));
            cons->tag = INT_CONSTANT;
            cons->int_constant = expr->int_v;
            int idx = add_constant_to_pool(cons, exe);
            emit(PUSH_INT, idx);
            MEM_free(cons);
        }
        break;
    case DOUBLE_LITERAL_EXPRESSION:
        if(expr->double_v == 0) {
            emit(PUSH_DOUBLE_0);
        } else if(expr->double_v == 1) {
            emit(PUSH_DOUBLE_1);
        } else {
            Constant *cons = (Constant*)MEM_malloc(sizeof(Constant));
            cons->tag = DOUBLE_CONSTANT;
            cons->double_constant = expr->double_v;
            int idx = add_constant_to_pool(cons, exe);
            emit(PUSH_DOUBLE, idx);
            MEM_free(cons);
        }
        break;
    case STRING_LITERAL_EXPRESSION:
        Constant *cons = (Constant*)MEM_malloc(sizeof(Constant));
        cons->tag = STRING_CONSTANT;
        cons->string_constant = expr->string_v;
        int idx = add_constant_to_pool(cons, exe);
        emit(PUSH_STRING, idx);
        MEM_free(cons);
        break;
    default:
        break;
    }
}

static
void walk_expression_statement(Statement *stat, Executable *exe) {
    gen_code_of_expression(stat->u.expr, exe);
}

static
void walk_statement_list(StatementList *stat_list, Executable *exe) {
    Statement *stat;

    for(stat=stat_list->phead; stat; stat = stat->next) {
        switch (stat->kind)
        {
        case EXPRESSION_STATEMENT:
            walk_expression_statement(stat, exe);
            break;
        case FOR_STATEMENT:
            break;
        case WHILE_STATEMENT:
            break;
        case FOREACH_STATEMENT:
            break;
        case IF_STATEMENT:
            break;
        case RETURN_STATEMENT:
            break;
        case TRY_STATEMENT:
            break;
        case THROW_STATEMENT:
            break;
        case BREAK_STATEMENT:
            break;
        case CONTINUE_STATEMENT:
            break;
        case DECLARATION_STATEMENT:
            break;    
        default:
            break;
        }
    }
}

static 
void walk_block(Block *block, Executable *exe) {
    walk_statement_list(block->stat_list, exe);
}

/*
    todo 
*/
static 
void copy_function_definition(FunctionDefinition *fd, Function *f) {
    Statement *stat;

    f->type = fd->type;
    f->name = fd->ident->name;

    /* copy local variables info */
    f->local_vars = MEM_malloc(sizeof(Variable) * fd->local_variable_cnt);
    for(int i = 0; i < fd->local_variable_cnt; i ++) {
        stat = fd->local_variables[i];
        f->local_vars[i].name = stat->u.declaration_stat.ident->name;
        f->local_vars[i].type = stat->u.declaration_stat.type;
    }
}

static
void gen_code_segment(Executable *exe) {
    Compiler *comp = get_current_compiler();
    FunctionDefinition *fd_p;
    int i = 0;

    exe->code_seg->size = comp->function_list->len;
    exe->code_seg->arr = (Function*)MEM_malloc(exe->code_seg->size * sizeof(Function));

    for(fd_p=comp->function_list->phead; fd_p; fd_p=fd_p->next) {
        Function *f = &exe->code_seg->arr[i ++];

        copy_function_definition(fd_p, f);
        
        if(fd_p->block != NULL) {
            reset_code_buf();
            reset_linenum_buf();
            walk_block(fd_p->block, exe);

            f->codes = (Byte*)MEM_malloc(sizeof(Byte) * codebuf.size);
            memcpy(f->codes, codebuf.codes, codebuf.size);
            f->code_size = codebuf.size;
            // f->line_numbers = linenum_buf.arr;
            // f->line_number_size = linenum_buf.size;
        } else {

        }
    }
}

void gen_top_codes(Executable *exe) {

}

Executable* alloc_excutable() {
    Executable *exe;

    exe = (Executable*)MEM_malloc(sizeof(Executable));

    exe->constant_seg = (ConstantSegment*)MEM_malloc(sizeof(ConstantSegment));
    exe->constant_seg->arr = NULL, exe->constant_seg->size = 0;

    exe->data_seg = (DataSegment*)MEM_malloc(sizeof(DataSegment));
    exe->data_seg->arr = NULL, exe->data_seg->size = 0;
    
    exe->code_seg = (CodeSegment*)MEM_malloc(sizeof(CodeSegment));
    exe->code_seg->arr = NULL, exe->code_seg->size = 0;

    exe->top_code_size = 0;
    exe->top_codes = NULL;

    exe->line_numbers = NULL;
    exe->line_number_size = 0;

    exe->stk_sz_needed = 0;

    return exe;
}

/*
    After semantic analysis, all statement'semantic is right, and information in symbol table is completed.
    So in code generation step, just utilizing symbol table, walk ast to generate code.
*/
Executable* walk_ast_for_gen_exe() {
    Compiler *comp;
    Executable *exe;

    comp = get_current_compiler();
    exe = alloc_excutable();
    // gen_constant_segment(exe);
    // gen_data_segment(exe);
    // gen_top_codes(exe);
    gen_code_segment(exe);
    
    return exe;
}