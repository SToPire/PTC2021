#include "asm.h"
#include <stdlib.h>
const char *header = ".data\n"
                     "_prompt: .asciiz \"Enter an integer:\"\n"
                     "_ret: .asciiz \"\\n\"\n"
                     ".globl main\n"
                     ".text\n"
                     "read:\n"
                     "   li $v0, 4\n"
                     "   la $a0, _prompt\n"
                     "   syscall\n"
                     "   li $v0, 5\n"
                     "   syscall\n"
                     "   jr $ra\n\n"
                     "write:\n"
                     "   li $v0, 1\n"
                     "   syscall\n"
                     "   li $v0, 4\n"
                     "   la $a0, _ret\n"
                     "   syscall\n"
                     "   move $v0, $0\n"
                     "   jr $ra\n\n";
char *relationInstructions[] = {"beq", "bne", "bge", "ble", "bgt", "blt"};

int argcnt = 0;

void toAsm(FILE *file) {
  fprintf(file, "%s", header);
  for (IRCode *p = AllCode; p; p = p->nxt) {
    if (p->kind == IR_FUNDEC) { p->fundec.size = preProcessFunc(p); }
  }
  for (IRCode *p = AllCode; p; p = p->nxt) {
    code2Asm(p, file);
  }
}

void code2Asm(IRCode *code, FILE *file) {
  switch (code->kind) {
  case IR_FUNDEC:
    fprintf(file, "%s:\n", code->fundec.name->name);
    fprintf(file, "sw $ra, -4($sp)\n");
    fprintf(file, "sw $fp, -8($sp)\n");
    fprintf(file, "move $fp, $sp\n");
    fprintf(file, "sub $sp, $sp, %u\n", code->fundec.size);
    break;
  case IR_ASSIGN:
    load("t0", code->assign.src, file);
    store("t0", code->assign.des, file);
    break;
  case IR_ADD:
    load("t0", code->binary.op1, file);
    load("t1", code->binary.op2, file);
    fprintf(file, "add $%s,$%s,$%s\n", "t0", "t0", "t1");
    store("t0", code->binary.res, file);
    break;
  case IR_SUB:
    load("t0", code->binary.op1, file);
    load("t1", code->binary.op2, file);
    fprintf(file, "sub $%s,$%s,$%s\n", "t0", "t0", "t1");
    store("t0", code->binary.res, file);
    break;
  case IR_MUL:
    load("t0", code->binary.op1, file);
    load("t1", code->binary.op2, file);
    fprintf(file, "mul $%s,$%s,$%s\n", "t0", "t0", "t1");
    store("t0", code->binary.res, file);
    break;
  case IR_DIV:
    load("t0", code->binary.op1, file);
    load("t1", code->binary.op2, file);
    fprintf(file, "div $%s,$%s,$%s\n", "t0", "t0", "t1");
    store("t0", code->binary.res, file);
    break;
  case IR_RETURN:
    load("v0", code->ret.var, file);
    fprintf(file, "lw $ra, -4($fp)\n");
    fprintf(file, "lw $fp, -8($fp)\n");
    fprintf(file, "add $sp, $sp, %u\n", code->ret.size);
    fprintf(file, "jr $ra\n");
    break;
  case IR_LABEL: fprintf(file, "l%d:\n", code->label.name->vnum); break;
  case IR_LOAD:
    load("t0", code->load.src, file);
    fprintf(file, "lw $t1, 0($t0)\n");
    store("t1", code->load.des, file);
    break;
  case IR_STORE:
    load("t0", code->store.src, file);
    load("t1", code->store.des, file);
    fprintf(file, "sw $t0, 0($t1)\n");
    break;
  case IR_READ:
    fprintf(file, "jal read\n");
    store("v0", code->read.var, file);
    break;
  case IR_WRITE:
    load("a0", code->write.var, file);
    fprintf(file, "jal write\n");
    break;
  case IR_JUMP: fprintf(file, "j l%d\n", code->jump.target->vnum); break;
  case IR_CONDJUMP:
    load("t0", code->condJump.op1, file);
    load("t1", code->condJump.op2, file);
    fprintf(file, "%s $t0,$t1, l%d\n",
            relationInstructions[code->condJump.relop->vr],
            code->condJump.target->vnum);
    break;
  case IR_ARG:
    load("t0", code->arg.var, file);
    fprintf(file, "sw $t0,0($sp)\n");
    fprintf(file, "sub $sp, $sp, 4\n");
    ++argcnt;
    break;
  case IR_CALL:
    fprintf(file, "jal %s\n", code->fundec.name->name);
    fprintf(file, "add $sp, $sp, %d\n", argcnt * 4);
    store("v0", code->call.ret, file);
    argcnt = 0;
    break;

  case IR_PARAM:
  case IR_DEC: // intentionally left blank
  default: break;
  }
}

void load(char *reg, Operand *var, FILE *file) {
  switch (var->kind) {
  case T_CONSTANT: fprintf(file, "li $%s, %d\n", reg, var->vi); break;
  case T_ADDRESS: fprintf(file, "la $%s, %d($fp)\n", reg, -var->offset); break;
  default: fprintf(file, "lw $%s, %d($fp)\n", reg, -var->offset);
  }
}

void store(char *reg, Operand *var, FILE *file) {
  fprintf(file, "sw $%s, %d($fp)\n", reg, -var->offset);
}

int preProcessFunc(IRCode *func) {
  int size = 8;
  int reverse = 0; //实参在fp的上面
  VarListType *varList = (VarListType *)func->fundec.localVarList;
  for (IRCode *p = func->nxt; p && p->kind != IR_FUNDEC; p = p->nxt) {
    switch (p->kind) {
    case IR_ASSIGN:
      size = assignOffset(varList, p->assign.des, size);
      size = assignOffset(varList, p->assign.src, size);
      break;
    case IR_LOAD:
      size = assignOffset(varList, p->load.des, size);
      size = assignOffset(varList, p->load.src, size);
      break;
    case IR_STORE:
      size = assignOffset(varList, p->store.des, size);
      size = assignOffset(varList, p->store.src, size);
      break;
    case IR_ADD:
    case IR_SUB:
    case IR_MUL:
    case IR_DIV:
      size = assignOffset(varList, p->binary.res, size);
      size = assignOffset(varList, p->binary.op1, size);
      size = assignOffset(varList, p->binary.op2, size);
      break;
    case IR_CALL: size = assignOffset(varList, p->call.ret, size); break;
    case IR_RETURN:
      size = assignOffset(varList, p->ret.var, size);
      p->ret.size = size;
      break;
    case IR_ARG: size = assignOffset(varList, p->arg.var, size); break;
    case IR_READ: size = assignOffset(varList, p->read.var, size); break;
    case IR_WRITE: size = assignOffset(varList, p->write.var, size); break;
    case IR_DEC: size = assignOffset(varList, p->dec.var, size); break;
    case IR_PARAM:
      reverse = assignOffset(varList, p->param.var, reverse);
      p->param.var->offset = -p->param.var->offset;
      break;
    default: break;
    }
  }
  return size;
}

int assignOffset(VarListType *varList, Operand *op, int curOffset) {
  switch (op->kind) {
  case T_TMP:
  case T_VARIABLE:
  case T_ADDRESS: {
    VarListType *cur = searchVarList(varList, op);
    if (cur) { //这个局部变量已经分配过偏移量
      op->offset = cur->op->offset;
      return curOffset; //没有在栈帧里为新的局部变量分配空间
    } else {
      op->offset = curOffset + op->size;
      addVarList(varList, op);
      return op->offset;
    }
  }

  default: return curOffset;
  }
}

VarListType *searchVarList(VarListType *varList, Operand *op) {
  if (!varList) return NULL;
  VarListType *p = varList;
  while (p) {
    if (p->op->kind == op->kind && p->op->vnum == op->vnum) return p;
    p = p->nxt;
  }
  return NULL;
}

void addVarList(VarListType *varList, Operand *op) {
  VarListType *p = varList;
  while (p->nxt)
    p = p->nxt;

  VarListType *newNode = (VarListType *)malloc(sizeof(VarListType));
  newNode->op = op;
  newNode->nxt = NULL;

  p->nxt = newNode;
}