#ifndef ASM_H
#define ASM_H
#include "ir.h"

extern IRCode *AllCode;

typedef struct VarListType {
  Operand *op;
  struct VarListType *nxt;
} VarListType;

void toAsm(FILE *file);
void code2Asm(IRCode *code, FILE *file);
void load(char *reg, Operand *var, FILE *file);
void store(char *reg, Operand *var, FILE *file);
int preProcessFunc(IRCode *func);
int assignOffset(VarListType *varList, Operand *op, int curOffset);
VarListType *searchVarList(VarListType *varList, Operand *op);
void addVarList(VarListType *varList, Operand *op);


#endif