#ifndef IR_H
#define IR_H
#include "semantic.h"
#include "syntax.h"
#include <stdio.h>

enum OP_TYPE {
  T_TMP,
  T_LABEL,
  T_VARIABLE,
  T_CONSTANT,
  T_ADDRESS,
  T_FUNCNAME,
  T_PARAMNAME,
  T_RELOP,
};

enum IR_CODE_TYPE {
  IR_ASSIGN,
  IR_FUNDEC,
  IR_PARAM,
  IR_READ,
  IR_WRITE,
  IR_CALL,
  IR_ARG,
  IR_ADD,
  IR_SUB,
  IR_MUL,
  IR_DIV,
  IR_RETURN,
  IR_LABEL,
  IR_JUMP,
  IR_CONDJUMP,
  IR_DEC,
  IR_LOAD,
  IR_STORE,
  IR_DUMMY, //占位
};

typedef struct Operand {
  enum OP_TYPE kind;
  union {
    int vnum;
    int vi;
    float vf;
    const char *name;
    enum RELOPS vr;
  };
  int size;
  int offset; // 汇编中在函数栈帧中的偏移量
} Operand;

typedef struct ExtraData {
  typeMeta *type;
} ExtraData;

typedef struct IRCode {
  enum IR_CODE_TYPE kind;
  union {
    struct {
      Operand *src, *des;
    } assign, load, store;
    struct {
      Operand *name;
    } label;
    struct {
      Operand *var;
    } param, read, write, arg;
    struct {
      Operand *var;
      int size; //返回的时候要知道栈帧大小
    } ret;
    struct {
      Operand *funcName, *ret;
    } call;
    struct {
      Operand *op1, *op2, *res;
    } binary;
    struct {
      Operand *target;
    } jump;
    struct {
      Operand *op1, *op2, *relop, *target;
    } condJump;
    struct {
      Operand *var;
      int size;
    } dec;
    struct {
      Operand *name;
      void *localVarList; //类型为VarListType
      int size;
    } fundec;
  };
  struct IRCode *pre, *nxt;

  //丑陋的逻辑，作用是在transExp返回一个指针时指明其类型
  struct ExtraData *extra;
} IRCode;
extern IRCode *AllCode;
extern IRCode *tmpCompStCode;

// 用于transArgs返回两个codelist
typedef struct IRCodeListPair {
  IRCode *l1, *l2;
} IRCodeListPair;

void transExtDefList(syntaxMeta *node);
void transExtDef(syntaxMeta *node);
void transFundec(syntaxMeta *node);
IRCode *transCompst(syntaxMeta *node);
IRCode *transDefList(syntaxMeta *node);
IRCode *transDef(syntaxMeta *node);
IRCode *transDecList(syntaxMeta *node);
IRCode *transDec(syntaxMeta *node);
IRCode *transStmtList(syntaxMeta *node);
IRCode *transStmt(syntaxMeta *node);
IRCode *transExp(syntaxMeta *node, Operand *target, int needDeref);
IRCodeListPair transArgs(syntaxMeta *node);
IRCode *transCondExp(syntaxMeta *node, Operand * true, Operand * false);
IRCode *assignToArrayHelper(syntaxMeta *t1, syntaxMeta *t3, Operand *target);

IRCode *NewIRCode(enum IR_CODE_TYPE kind);
Operand *NewTmpOperand();
Operand *NewVariableOperand(fieldMeta *v);
Operand *NewLabelOperand();
Operand *NewConstantOperand(int v);
Operand *NewFuncNameOperand(const char *name);
Operand *NewRelOperand(enum RELOPS rel);

IRCode *IRCodeConcat(IRCode *pre, IRCode *nxt);
char *printOperand(Operand *op);
char *relop2Str(enum RELOPS relop);
void printCode(IRCode *codes, FILE *of);
int getSizeFromType(typeMeta *type);
#endif