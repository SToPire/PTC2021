#include "ir.h"
#include "asm.h"
#include "semantic.h"
#include "syntax.h"
#include "syntax.tab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void transFundec(syntaxMeta *node) {
  IRCode *code = NewIRCode(IR_FUNDEC);
  code->fundec.name = NewFuncNameOperand(node->lchild->vs);

  VarListType *dummyVarList = (VarListType *)malloc(sizeof(VarListType));
  dummyVarList->nxt = NULL;
  dummyVarList->op = NewConstantOperand(2333); // dummy的链表头

  code->fundec.localVarList = (void *)dummyVarList;
  code->fundec.size = 0;
  AllCode = IRCodeConcat(AllCode, code);

  fieldMeta *funcType = searchSymbol(node->lchild->vs, 0, 1);
  for (fieldMeta *p = funcType->type->function.params; p; p = p->next) {
    fieldMeta *tp = searchSymbol(p->id, 1, 0); //填坑，之前做了深拷贝
    code = NewIRCode(IR_PARAM);
    code->param.var = NewVariableOperand(tp);
    AllCode = IRCodeConcat(AllCode, code);
  }

  AllCode = IRCodeConcat(AllCode, tmpCompStCode);
}

IRCode *transCompst(syntaxMeta *node) {
  if (!node || node->isEmpty) return NULL;
  IRCode *c1 = transDefList(node->lchild->rcousin);
  IRCode *c2 = transStmtList(node->lchild->rcousin->rcousin);
  IRCode *res = IRCodeConcat(c1, c2);
  node->ptr = (void *)res;
  return res;
}

IRCode *transDefList(syntaxMeta *node) {
  if (!node || node->isEmpty) return NULL;
  return IRCodeConcat(transDef(node->lchild),
                      transDefList(node->lchild->rcousin));
}

IRCode *transDef(syntaxMeta *node) {
  if (!node || node->isEmpty) return NULL;
  return transDecList(node->lchild->rcousin);
}

IRCode *transDecList(syntaxMeta *node) {
  if (!node || node->isEmpty) return NULL;
  IRCode *c1 = transDec(node->lchild);
  IRCode *c2 = NULL;
  if (node->lchild->rcousin) {
    c2 = transDecList(node->lchild->rcousin->rcousin);
  }
  return IRCodeConcat(c1, c2);
}

IRCode *transDec(syntaxMeta *node) {
  syntaxMeta *id = node->lchild;
  while (id && id->type != ID) {
    id = id->lchild;
  }
  if (!id) {
    fprintf(stderr, "Invalid syntax\n");
    return NULL;
  }

  fieldMeta *sym = searchSymbol(id->vs, 1, 0);
  if (sym == NULL) {
    fprintf(stderr, "unknown symbol:%s\n", id->vs);
    return NULL;
  } else if (sym->type->type == T_ARRAY) { // ARRAY
    int objSize = sym->type->array.totalSize;
    Operand *var = NewVariableOperand(sym);

    IRCode *code = NewIRCode(IR_DEC);
    code->dec.size = objSize;
    code->dec.var = var;

    return code;

  } else if (sym->type->type == T_INT) { // INT
    Operand *op = NewVariableOperand(sym);
    if (node->lchild->rcousin) {
      return transExp(node->lchild->rcousin->rcousin, op, 1);
    }
  } else { // STRUCT
    int objSize = sym->type->stru.size;
    Operand *var = NewVariableOperand(sym);

    IRCode *code = NewIRCode(IR_DEC);
    code->dec.size = objSize;
    code->dec.var = var;

    return code;
  }
}

IRCode *transExp(syntaxMeta *node, Operand *target, int needDeref) {
  syntaxMeta *t1, *t2 = NULL, *t3 = NULL;
  t1 = node->lchild;
  if (t1) t2 = t1->rcousin;
  if (t2) t3 = t2->rcousin;
  switch (t1->type) {
  case LP: return transExp(t2, target, needDeref);
  case FLOAT: fprintf(stderr, "No float\n"); return NULL;
  case INT: {
    if (target == NULL) return NULL;
    IRCode *code = NewIRCode(IR_ASSIGN);
    code->assign.src = NewConstantOperand(t1->vi);
    code->assign.des = target;
    return code;
  }
  case MINUS: {
    if (t2->lchild->type == INT) { // fastpath:负号后为常数
      if (target == NULL) return NULL;
      IRCode *code = NewIRCode(IR_ASSIGN);
      code->assign.src = NewConstantOperand(-(t2->lchild->vi));
      code->assign.des = target;
      return code;
    }
    Operand *t = NewTmpOperand();
    IRCode *exp = transExp(t2, t, 1);
    if (target == NULL) { //不能省掉，可能有副作用
      return exp;
    }
    IRCode *code = NewIRCode(IR_SUB);
    code->binary.res = target;
    code->binary.op1 = NewConstantOperand(0);
    code->binary.op2 = t;
    return IRCodeConcat(exp, code);
  }
  case ID: {
    if (t2 == NULL) { // ID
      if (target == NULL) return NULL;
      fieldMeta *entry = searchSymbol(t1->vs, 1, 0);
      IRCode *code = NewIRCode(IR_ASSIGN);
      code->assign.src = NewVariableOperand(entry);
      code->assign.des = target;
      code->extra = malloc(sizeof(ExtraData));
      code->extra->type = entry->type;
      return code;
    } else if (t3->type == RP) {                    // function call, no arg
      if (target == NULL) target = NewTmpOperand(); //没有返回值的函数也要调用
      if (strcmp(t1->vs, "read") == 0) {
        IRCode *code = NewIRCode(IR_READ);
        code->read.var = target;
        return code;
      }
      IRCode *code = NewIRCode(IR_CALL);
      code->call.funcName = NewFuncNameOperand(t1->vs);
      code->call.ret = target;
      return code;
    } else { // function call with args
      IRCodeListPair lists = transArgs(t3);
      if (strcmp(t1->vs, "write") == 0) {
        IRCode *code = NewIRCode(IR_WRITE);
        code->write.var = lists.l2->arg.var; //一个特殊逻辑，找到write的参数名
        return IRCodeConcat(lists.l1, code);
      } else {
        if (target == NULL) target = NewTmpOperand();
        IRCode *call = NewIRCode(IR_CALL);
        call->call.funcName = NewFuncNameOperand(t1->vs);
        call->call.ret = target;
        return IRCodeConcat(IRCodeConcat(lists.l1, lists.l2), call);
      }
    }
  }
  case NOT: {
    goto label;
  }
  default: { // lchild = exp
    switch (t2->type) {
    case AND:
    case OR:
    case RELOP:
    label : {
      Operand *l1 = NewLabelOperand();
      Operand *l2 = NewLabelOperand();

      IRCode *code = transCondExp(node, l1, l2);
      if (target != NULL) {
        IRCode *c0 = NewIRCode(IR_ASSIGN);
        c0->assign.des = target;
        c0->assign.src = NewConstantOperand(0);
        code = IRCodeConcat(c0, code);

        IRCode *lc1 = NewIRCode(IR_LABEL);
        lc1->label.name = l1;
        code = IRCodeConcat(code, lc1);

        IRCode *c1 = NewIRCode(IR_ASSIGN);
        c1->assign.des = target;
        c1->assign.src = NewConstantOperand(1);
        code = IRCodeConcat(code, c1);

        IRCode *lc2 = NewIRCode(IR_LABEL);
        lc2->label.name = l2;
        code = IRCodeConcat(code, lc2);
      } else {
        IRCode *lc1 = NewIRCode(IR_LABEL);
        lc1->label.name = l1;
        IRCode *lc2 = NewIRCode(IR_LABEL);
        lc2->label.name = l2;
        return IRCodeConcat(IRCodeConcat(code, lc1), lc2);
      }
      break;
    }
    case LB: {
      Operand *tmp1 = NewTmpOperand();
      IRCode *arrayExp = transExp(t1, tmp1, 0);

      typeMeta *type = arrayExp->extra->type; // type一定是数组类型

      Operand *tmp2 = NewTmpOperand();
      IRCode *indexExp = transExp(t3, tmp2, 1);
      IRCode *code = IRCodeConcat(arrayExp, indexExp);

      if (target) {
        IRCode *offsetCode = NewIRCode(IR_MUL);
        offsetCode->binary.res = tmp2;
        offsetCode->binary.op1 = tmp2;
        offsetCode->binary.op2 =
            NewConstantOperand(getSizeFromType(type->array.type));
        code = IRCodeConcat(code, offsetCode);

        IRCode *plusCode = NewIRCode(IR_ADD);
        plusCode->binary.res = target;
        plusCode->binary.op1 = tmp1;
        plusCode->binary.op2 = tmp2;
        code = IRCodeConcat(code, plusCode);

        if (needDeref && type->array.type->type == T_INT) {
          IRCode *derefCode = NewIRCode(IR_LOAD);
          derefCode->load.des = target;
          derefCode->load.src = target;
          code = IRCodeConcat(code, derefCode);
        }
      }

      code->extra = malloc(sizeof(ExtraData));
      code->extra->type = type->array.type;
      return code;
      break;
    }
    case DOT: {
      Operand *tmp1 = NewTmpOperand();
      IRCode *code = transExp(t1, tmp1, 0);
      if (target == NULL) return code;

      typeMeta *type = code->extra->type; // type一定是结构体类型
      fieldMeta *fields = type->stru.fields;
      fieldMeta *fp;
      for (fp = fields; fp; fp = fp->next) {
        if (strcmp(fp->id, t3->vs) == 0) { break; }
      }

      IRCode *c1 = NewIRCode(IR_ADD);
      c1->binary.res = target;
      c1->binary.op1 = tmp1;
      c1->binary.op2 = NewConstantOperand(fp->offset);
      code = IRCodeConcat(code, c1);

      if (needDeref && fp->type->type == T_INT) {
        IRCode *c2 = NewIRCode(IR_LOAD);
        c2->load.des = c2->load.src = target;
        code = IRCodeConcat(code, c2);
      }

      code->extra->type = fp->type;
      return code;
    }
    case ASSIGNOP: {
      int assignToArray = 0;
      if (t1->lchild->type == ID) {
        fieldMeta *entry = searchSymbol(t1->lchild->vs, 1, 0);
        if (entry != NULL) {
          if (entry->type->type == T_INT) { // 向非结构体的变量赋值
            Operand *tmp = NewTmpOperand();
            IRCode *expr = transExp(t3, tmp, 1);

            Operand *var = NewVariableOperand(entry);
            IRCode *code = NewIRCode(IR_ASSIGN);
            code->assign.des = var;
            code->assign.src = tmp;

            code = IRCodeConcat(expr, code);

            if (target != NULL) {
              IRCode *assign = NewIRCode(IR_ASSIGN);
              assign->assign.des = target;
              assign->assign.src = var;

              code = IRCodeConcat(code, assign);
            }
            return code;
          } else if (entry->type->type == T_ARRAY) { //向数组赋值
            assignToArray = 1;
          }
        }
      } else {
        assignToArray = 1;
      }
      if (assignToArray) { return assignToArrayHelper(t1, t3, target); }
    }
    default: { // +-*/
      Operand *tmp1 = NewTmpOperand();
      Operand *tmp2 = NewTmpOperand();
      IRCode *code = IRCodeConcat(transExp(t1, tmp1, 1), transExp(t3, tmp2, 1));

      if (target != NULL) {
        IRCode *binOp;
        switch (t2->type) {
        case PLUS: binOp = NewIRCode(IR_ADD); break;
        case MINUS: binOp = NewIRCode(IR_SUB); break;
        case STAR: binOp = NewIRCode(IR_MUL); break;
        case DIV: binOp = NewIRCode(IR_DIV); break;
        }
        binOp->binary.res = target;
        binOp->binary.op1 = tmp1;
        binOp->binary.op2 = tmp2;
        return IRCodeConcat(code, binOp);
      }

      return code;
    }
    }
  }
  }
}

//  其实也是结构体的helper
IRCode *assignToArrayHelper(syntaxMeta *t1, syntaxMeta *t3, Operand *target) {
  Operand *tmp1 = NewTmpOperand();
  IRCode *laddr = transExp(t1, tmp1, 0);

  typeMeta *type = laddr->extra->type;
  if (type->type == T_INT) { //左边已经是基本类型
    Operand *tmp2 = NewTmpOperand();
    IRCode *rexp = transExp(t3, tmp2, 1);
    IRCode *storeCode = NewIRCode(IR_STORE);
    storeCode->store.des = tmp1;
    storeCode->store.src = tmp2;

    IRCode *code = IRCodeConcat(laddr, rexp);
    code = IRCodeConcat(code, storeCode);

    if (target) {
      IRCode *c1 = NewIRCode(IR_ASSIGN);
      c1->assign.des = target;
      c1->assign.src = tmp2;
      code = IRCodeConcat(code, c1);
    }

    return code;
  } else if (type->type == T_ARRAY) {
    /*
      tmp1, tmp2分别是des和src地址
      tep3是临时变量, cnt计数

      cnt = 0
      LABEL l:
      tmp3 := *tmp2
      *tmp1 := tmp3
      tmp1 = tmp1 + #4
      tmp2 = tmp2 + #4
      cnt = cnt + #4
      if cnt < totalsize goto l

    */
    Operand *tmp2 = NewTmpOperand();
    IRCode *rexp = transExp(t3, tmp2, 0);
    int rExpSz = 0;
    if (rexp->extra->type) { rExpSz = getSizeFromType(rexp->extra->type); }
    IRCode *code = IRCodeConcat(laddr, rexp);

    Operand *cnt = NewTmpOperand();
    IRCode *c1 = NewIRCode(IR_ASSIGN);
    c1->assign.des = cnt;
    c1->assign.src = NewConstantOperand(0);
    code = IRCodeConcat(code, c1);

    Operand *label = NewLabelOperand();
    IRCode *c2 = NewIRCode(IR_LABEL);
    c2->label.name = label;
    code = IRCodeConcat(code, c2);

    Operand *tmp3 = NewTmpOperand();
    IRCode *c3 = NewIRCode(IR_LOAD);
    c3->load.des = tmp3;
    c3->load.src = tmp2;
    code = IRCodeConcat(code, c3);

    IRCode *c4 = NewIRCode(IR_STORE);
    c4->store.des = tmp1;
    c4->store.src = tmp3;
    code = IRCodeConcat(code, c4);

    IRCode *c5 = NewIRCode(IR_ADD);
    IRCode *c6 = NewIRCode(IR_ADD);
    IRCode *c7 = NewIRCode(IR_ADD);
    c5->binary.op2 = c6->binary.op2 = c7->binary.op2 = NewConstantOperand(4);
    c5->binary.res = c5->binary.op1 = tmp1;
    c6->binary.res = c6->binary.op1 = tmp2;
    c7->binary.res = c7->binary.op1 = cnt;
    code = IRCodeConcat(code, c5);
    code = IRCodeConcat(code, c6);
    code = IRCodeConcat(code, c7);

    IRCode *c8 = NewIRCode(IR_CONDJUMP);
    c8->condJump.relop = NewRelOperand(RELOPS_L);
    c8->condJump.target = label;
    c8->condJump.op1 = cnt;
    c8->condJump.op2 = NewConstantOperand(
        getSizeFromType(type) < rExpSz ? getSizeFromType(type) : rExpSz);
    code = IRCodeConcat(code, c8);

    // if (target) {
    //   IRCode* c9 = NewIRCode(IR_ASSIGN);
    //   c9->assign.des = target;
    //   c9->assign.src =
    // }
    // TODO 连等

    return code;
  }
}

IRCode *transCondExp(syntaxMeta *node, Operand * true, Operand * false) {
  syntaxMeta *t1, *t2 = NULL, *t3 = NULL;
  t1 = node->lchild;
  if (t1) t2 = t1->rcousin;
  if (t2) t3 = t2->rcousin;

  if (t1->type == NOT) { return transCondExp(t2, false, true); }

  if (t2) {
    switch (t2->type) {
    case AND: {
      Operand *l1 = NewLabelOperand();
      IRCode *code = transCondExp(t1, l1, false); // 短路
      IRCode *labelCode = NewIRCode(IR_LABEL);
      labelCode->label.name = l1;
      code = IRCodeConcat(code, labelCode);
      code = IRCodeConcat(code, transCondExp(t3, true, false));
      return code;
    }
    case OR: {
      Operand *l1 = NewLabelOperand();
      IRCode *code = transCondExp(t1, true, l1);
      IRCode *labelCode = NewIRCode(IR_LABEL);
      labelCode->label.name = l1;
      code = IRCodeConcat(code, labelCode);
      code = IRCodeConcat(code, transCondExp(t3, true, false));
      return code;
    }
    case RELOP: {
      Operand *tmp1 = NewTmpOperand();
      Operand *tmp2 = NewTmpOperand();

      IRCode *code = IRCodeConcat(transExp(t1, tmp1, 1), transExp(t3, tmp2, 1));

      IRCode *condJump = NewIRCode(IR_CONDJUMP);
      condJump->condJump.op1 = tmp1;
      condJump->condJump.op2 = tmp2;
      condJump->condJump.relop = NewRelOperand(t2->vr);
      condJump->condJump.target = true;
      code = IRCodeConcat(code, condJump);

      IRCode *jump = NewIRCode(IR_JUMP);
      jump->jump.target = false;
      return IRCodeConcat(code, jump);
    }
    default: break;
    }
  }

  Operand *tmp1 = NewTmpOperand();
  IRCode *code = transExp(node, tmp1, 1);

  IRCode *condJump = NewIRCode(IR_CONDJUMP);
  condJump->condJump.op1 = NewConstantOperand(0);
  condJump->condJump.op2 = tmp1;
  condJump->condJump.relop = NewRelOperand(RELOPS_EQ);
  condJump->condJump.target = false;
  code = IRCodeConcat(code, condJump);

  IRCode *jump = NewIRCode(IR_JUMP);
  jump->jump.target = true;
  return IRCodeConcat(code, jump);
}

// 计算参数表达式 和 传参 是分开的代码
IRCodeListPair transArgs(syntaxMeta *node) {
  Operand *t = NewTmpOperand();
  IRCode *code = transExp(node->lchild, t, 1);
  IRCode *argCode = NewIRCode(IR_ARG);
  argCode->arg.var = t;
  if (node->lchild->rcousin) {
    IRCodeListPair r = transArgs(node->lchild->rcousin->rcousin);
    r.l1 = IRCodeConcat(code, r.l1);
    r.l2 = IRCodeConcat(r.l2, argCode);
    return r;
  } else {
    IRCodeListPair ret;
    ret.l1 = code;
    ret.l2 = argCode;
    return ret;
  }
}

IRCode *transStmtList(syntaxMeta *node) {
  if (!node || node->isEmpty) return NULL;
  return IRCodeConcat(transStmt(node->lchild),
                      transStmtList(node->lchild->rcousin));
}

IRCode *transStmt(syntaxMeta *node) {
  if (strcmp(node->lchild->name, "Exp") == 0) {
    return transExp(node->lchild, NULL, 1);
  } else if (strcmp(node->lchild->name, "CompSt") == 0) {
    return (IRCode *)node->lchild->ptr;
  }
  syntaxMeta *t1 = node->lchild;
  switch (t1->type) {
  case RETURN: {
    Operand *tmp = NewTmpOperand();
    IRCode *code = transExp(t1->rcousin, tmp, 1);
    IRCode *ret = NewIRCode(IR_RETURN);
    ret->ret.var = tmp;
    return IRCodeConcat(code, ret);
  }
  case IF: {
    syntaxMeta *condExp = t1->rcousin->rcousin;
    syntaxMeta *stmt1 = condExp->rcousin->rcousin;
    syntaxMeta *stmt2 = NULL;
    if (stmt1->rcousin) stmt2 = stmt1->rcousin->rcousin;

    Operand *l1 = NewLabelOperand();
    Operand *l2 = NewLabelOperand();
    IRCode *l1Code = NewIRCode(IR_LABEL);
    IRCode *l2Code = NewIRCode(IR_LABEL);
    l1Code->label.name = l1;
    l2Code->label.name = l2;

    IRCode *expCode = transCondExp(condExp, l1, l2);
    IRCode *stmt1Code = transStmt(stmt1);
    if (stmt2) { // exp l1 stmt1 GOTO_l3 L2 stmt2 L3
      IRCode *stmt2Code = transStmt(stmt2);

      Operand *l3 = NewLabelOperand();
      IRCode *l3Code = NewIRCode(IR_LABEL);
      l3Code->label.name = l3;

      IRCode *jumpCode = NewIRCode(IR_JUMP);
      jumpCode->jump.target = l3;

      IRCode *ret = IRCodeConcat(expCode, l1Code);
      ret = IRCodeConcat(ret, stmt1Code);
      ret = IRCodeConcat(ret, jumpCode);
      ret = IRCodeConcat(ret, l2Code);
      ret = IRCodeConcat(ret, stmt2Code);
      ret = IRCodeConcat(ret, l3Code);
      return ret;
    } else { // exp l1 stmt1 l2
      IRCode *ret = IRCodeConcat(expCode, l1Code);
      ret = IRCodeConcat(ret, stmt1Code);
      ret = IRCodeConcat(ret, l2Code);
      return ret;
    }
  }
  case WHILE: {
    syntaxMeta *condExp = node->lchild->rcousin->rcousin;
    syntaxMeta *stmt = condExp->rcousin->rcousin;
    // l3 exp l1 stmt GOTO_l3 l2
    Operand *l1 = NewLabelOperand();
    Operand *l2 = NewLabelOperand();
    Operand *l3 = NewLabelOperand();
    IRCode *l1Code = NewIRCode(IR_LABEL);
    IRCode *l2Code = NewIRCode(IR_LABEL);
    IRCode *l3Code = NewIRCode(IR_LABEL);
    l1Code->label.name = l1;
    l2Code->label.name = l2;
    l3Code->label.name = l3;

    IRCode *expCode = transCondExp(condExp, l1, l2);
    IRCode *stmtCode = transStmt(stmt);

    IRCode *jumpCode = NewIRCode(IR_JUMP);
    jumpCode->jump.target = l3;

    IRCode *ret = IRCodeConcat(l3Code, expCode);
    ret = IRCodeConcat(ret, l1Code);
    ret = IRCodeConcat(ret, stmtCode);
    ret = IRCodeConcat(ret, jumpCode);
    ret = IRCodeConcat(ret, l2Code);
    return ret;
  }
  }
}

/* -------------------------------------- */
IRCode *NewIRCode(enum IR_CODE_TYPE kind) {
  IRCode *code = malloc(sizeof(IRCode));
  code->kind = kind;
  code->pre = code->nxt = NULL;
  code->extra = NULL;
  return code;
}

static int VAR_CNT = 0;
Operand *NewVariableOperand(fieldMeta *v) {
  if (v->num == 0) { v->num = ++VAR_CNT; }

  Operand *op = malloc(sizeof(Operand));
  op->kind = T_VARIABLE;
  op->vnum = v->num;

  if (!v->isParam && v->type->type != T_INT) op->kind = T_ADDRESS;
  op->size = getSizeFromType(v->type);
  return op;
}

static int TMP_CNT = 0;
Operand *NewTmpOperand() {
  Operand *op = malloc(sizeof(Operand));
  op->kind = T_TMP;
  op->vnum = ++TMP_CNT;
  op->size = 4;
  return op;
}

static int LABEL_CNT = 0;
Operand *NewLabelOperand() {
  Operand *op = malloc(sizeof(Operand));
  op->kind = T_LABEL;
  op->vnum = ++LABEL_CNT;
  op->size = 0;
  return op;
}

Operand *NewConstantOperand(int v) {
  Operand *op = malloc(sizeof(Operand));
  op->kind = T_CONSTANT;
  op->vi = v;
  op->size = 4;
  return op;
}

Operand *NewFuncNameOperand(const char *name) {
  Operand *op = malloc(sizeof(Operand));
  op->kind = T_FUNCNAME;
  op->name = name;
  op->size = 0;
  return op;
}

Operand *NewRelOperand(enum RELOPS rel) {
  Operand *op = malloc(sizeof(Operand));
  op->kind = T_RELOP;
  op->vr = rel;
  op->size = 0;
  return op;
}
/* -------------------------------------- */

IRCode *IRCodeConcat(IRCode *pre, IRCode *nxt) {
  if (!pre) return nxt;
  if (!nxt) return pre;

  if (nxt->extra) {
    pre->extra = nxt->extra;
    nxt->extra = NULL;
  }

  IRCode *t = pre;
  while (t->nxt)
    t = t->nxt;
  t->nxt = nxt;
  nxt->pre = pre;
  return pre;
}

char *printOperand(Operand *op) {
  if (!op) return "";
  char *ret = malloc(32);
  switch (op->kind) {
  case T_CONSTANT: sprintf(ret, "#%d", op->vi); break;
  case T_VARIABLE: sprintf(ret, "v%d", op->vnum); break;
  case T_FUNCNAME: // intentional blank
  case T_PARAMNAME: sprintf(ret, "%s", op->name); break;
  case T_TMP: sprintf(ret, "t%d", op->vnum); break;
  case T_LABEL: sprintf(ret, "L%d", op->vnum); break;
  case T_RELOP: sprintf(ret, "%s", relop2Str(op->vr)); break;
  case T_ADDRESS: sprintf(ret, "&v%d", op->vnum); break;
  }
  return ret;
}

char *relop2Str(enum RELOPS relop) {
  switch (relop) {
  case RELOPS_EQ: return "==";
  case RELOPS_UNEQ: return "!=";
  case RELOPS_GE: return ">=";
  case RELOPS_LE: return "<=";
  case RELOPS_G: return ">";
  case RELOPS_L: return "<";
  }
}

void printCode(IRCode *codes, FILE *of) {
  IRCode *p = codes;
  while (p) {
    switch (p->kind) {
    case IR_ASSIGN:
      fprintf(of, "%s := %s\n", printOperand(p->assign.des),
              printOperand(p->assign.src));
      break;
    case IR_FUNDEC:
      fprintf(of, "FUNCTION %s :\n", printOperand(p->fundec.name));
      break;
    case IR_PARAM: fprintf(of, "PARAM %s\n", printOperand(p->param.var)); break;
    case IR_READ: fprintf(of, "READ %s\n", printOperand(p->read.var)); break;
    case IR_WRITE: fprintf(of, "WRITE %s\n", printOperand(p->write.var)); break;
    case IR_CALL:
      fprintf(of, "%s := CALL %s\n", printOperand(p->call.ret),
              printOperand(p->call.funcName));
      break;
    case IR_ARG: fprintf(of, "ARG %s\n", printOperand(p->arg.var)); break;
    case IR_ADD:
      fprintf(of, "%s := %s + %s\n", printOperand(p->binary.res),
              printOperand(p->binary.op1), printOperand(p->binary.op2));
      break;
    case IR_SUB:
      fprintf(of, "%s := %s - %s\n", printOperand(p->binary.res),
              printOperand(p->binary.op1), printOperand(p->binary.op2));
      break;
    case IR_MUL:
      fprintf(of, "%s := %s * %s\n", printOperand(p->binary.res),
              printOperand(p->binary.op1), printOperand(p->binary.op2));
      break;
    case IR_DIV:
      fprintf(of, "%s := %s / %s\n", printOperand(p->binary.res),
              printOperand(p->binary.op1), printOperand(p->binary.op2));
      break;
    case IR_RETURN: fprintf(of, "RETURN %s\n", printOperand(p->ret.var)); break;
    case IR_LABEL:
      fprintf(of, "LABEL %s :\n", printOperand(p->label.name));
      break;
    case IR_JUMP: fprintf(of, "GOTO %s\n", printOperand(p->jump.target)); break;
    case IR_CONDJUMP:
      fprintf(of, "IF %s %s %s GOTO %s\n", printOperand(p->condJump.op1),
              printOperand(p->condJump.relop), printOperand(p->condJump.op2),
              printOperand(p->condJump.target));
      break;
    case IR_DEC:
      /* 需要特殊处理保证不打出& */
      p->dec.var->kind = T_VARIABLE;
      fprintf(of, "DEC %s %d\n", printOperand(p->dec.var), p->dec.size);
      p->dec.var->kind = T_ADDRESS;
      break;
    case IR_LOAD:
      fprintf(of, "%s := *%s\n", printOperand(p->load.des),
              printOperand(p->load.src));
      break;
    case IR_STORE:
      fprintf(of, "*%s := %s\n", printOperand(p->store.des),
              printOperand(p->store.src));
      break;
    }
    p = p->nxt;
  }
}

int getSizeFromType(typeMeta *type) {
  switch (type->type) {
  case T_INT:
  case T_FLOAT: return 4;
  case T_ARRAY: return type->array.totalSize;
  case T_STRUCT: return type->stru.size;
  }
}