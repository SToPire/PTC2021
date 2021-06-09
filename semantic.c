#include "semantic.h"
#include "ir.h"
#include "syntax.h"
#include "syntax.tab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *semanticErrors[] = {
    "null",
    "Using undefined variable",
    "Using undefined function",
    "Variable redefinition",
    "Function redefinition",
    "Type mismatched for assignment",
    "The left-hand side of an assignment must be a variable",
    "Type mismatched for operands",
    "Type mismatched for return",
    "Mismatch arguments",
    "Using [] for non-array value",
    "Using () for non-function value",
    "Non-integer in array index",
    "Illegal use of '.'",
    "Accessing undefined field",
    "Field redefinition or assignment",
    "Duplicated struct name",
    "Undefined structure",
    "Undefined function",
    "Inconsistent function declaration"};

void printError(int index, int line) {
  isError = 1;
  printf("Error type %d at Line %d: %s.\n", index, line, semanticErrors[index]);
}

void semanticAnalysis() {
  semanticInitial();
  parseExtDefList(syntaxTreeRoot->lchild);
  semanticFinish();
}

void semanticInitial() {
  STStackPtr = 0;
  symbolTables[0].head = (fieldMeta *)malloc(sizeof(fieldMeta));
  symbolTables[0].head->next = NULL;
  symbolTables[0].kind = T_GLOBAL;
  strcpy(symbolTables[0].head->id, "LISTHEAD");

  // 预定义read
  fieldMeta *readSymbol = (fieldMeta *)malloc(sizeof(fieldMeta));
  strcpy(readSymbol->id, "read");
  typeMeta *t = (typeMeta *)malloc(sizeof(typeMeta));
  readSymbol->type = t;
  readSymbol->num = 0;
  readSymbol->isParam = 0;
  t->type = T_FUNC;
  t->function.retType = (typeMeta *)malloc(sizeof(typeMeta));
  t->function.retType->type = T_INT;
  t->function.defined = 1;
  t->function.line = 0;
  t->function.params = NULL;
  addSymbol(readSymbol, 1);

  // 预定义write
  fieldMeta *writeSymbol = (fieldMeta *)malloc(sizeof(fieldMeta));
  strcpy(writeSymbol->id, "write");
  t = (typeMeta *)malloc(sizeof(typeMeta));
  writeSymbol->type = t;
  writeSymbol->num = 0;
  writeSymbol->isParam = 0;
  t->type = T_FUNC;
  t->function.retType = (typeMeta *)malloc(sizeof(typeMeta));
  t->function.retType->type = T_INT;
  t->function.defined = 1;
  t->function.line = 0;

  fieldMeta *param = (fieldMeta *)malloc(sizeof(fieldMeta));
  param->next = NULL;
  param->type = (typeMeta *)malloc(sizeof(typeMeta));
  param->type->type = T_INT;

  t->function.params = param;
  addSymbol(writeSymbol, 1);

  structTable.head = (fieldMeta *)malloc(sizeof(fieldMeta));
  structTable.head->next = NULL;
  structTable.kind = T_STRUCTONLY;
  strcpy(structTable.head->id, "LISTHEAD");

  anonymous = 0;
}

void semanticFinish() {
  fieldMeta *t = symbolTables[0].head->next; // skip LISTHEAD
  while (t) {
    if (t->type->type == T_FUNC && !t->type->function.defined)
      printError(18, t->type->function.line);
    t = t->next;
  }
}

void pushSTable(int STTypes) {
  ++STStackPtr;
  symbolTables[STStackPtr].head = (fieldMeta *)malloc(sizeof(fieldMeta));
  symbolTables[STStackPtr].head->next = NULL;
  symbolTables[STStackPtr].kind = STTypes;
  strcpy(symbolTables[STStackPtr].head->id, "LISTHEAD");
}

void popSTable() {
  memset(&symbolTables[STStackPtr], 0, sizeof(symbolTable));
  --STStackPtr;
}

void addSymbol(fieldMeta *symbol, int global) {
  int i = STStackPtr;
  if (global) { i = 0; }
  symbol->next = symbolTables[i].head->next;
  symbolTables[i].head->next = symbol;
}

void addStructSymbol(fieldMeta *symbol) {
  symbol->next = structTable.head->next;
  structTable.head->next = symbol;
}

fieldMeta *searchSymbol(const char *name, int needRecursive, int globalOnly) {
  if (globalOnly) {
    fieldMeta *p = symbolTables[0].head;
    while (p) {
      if (strcmp(p->id, name) == 0) { return p; }
      p = p->next;
    }
    return NULL;
  } else if (!needRecursive) {
    fieldMeta *p = symbolTables[STStackPtr].head;
    while (p) {
      if (strcmp(p->id, name) == 0) { return p; }
      p = p->next;
    }
    return NULL;
  } else {
    for (int tmp = STStackPtr; tmp >= 0; --tmp) {
      fieldMeta *p = symbolTables[tmp].head;
      while (p) {
        if (strcmp(p->id, name) == 0) { return p; }
        p = p->next;
      }
    }
    return NULL;
  }
}

fieldMeta *searchStructSymbol(const char *name) {
  fieldMeta *p = structTable.head;
  while (p) {
    if (strcmp(p->id, name) == 0) { return p; }
    p = p->next;
  }
  return NULL;
}
/*------------------------------------------*/

void parseExtDefList(syntaxMeta *node) {
  if (!node || node->isEmpty) return;
  parseExtDef(node->lchild);
  parseExtDefList(node->lchild->rcousin);
}

void parseExtDef(syntaxMeta *node) {
  if (!node || node->isEmpty) return;
  typeMeta *type = parseSpecifier(node->lchild);
  syntaxMeta *def = node->lchild->rcousin;
  if (strcmp(def->name, "SEMI") == 0) return;
  else if (strcmp(def->name, "FunDec") == 0) {
    parseFunDec(def, type);
  } else if (strcmp(def->name, "ExtDecList") == 0) {
    parseExtDecList(def, type);
  }
}

void parseExtDecList(syntaxMeta *node, typeMeta *type) {
  if (!node || node->isEmpty) return;
  parseVarDec(node->lchild, type);
  if (node->lchild->rcousin)
    parseExtDecList(node->lchild->rcousin->rcousin, type);
}

typeMeta *parseSpecifier(syntaxMeta *node) {
  typeMeta *ret = (typeMeta *)malloc(sizeof(typeMeta));
  ret->type = T_INT;

  if (!node || node->isEmpty) return ret;
  if (strcmp(node->lchild->name, "TYPE") == 0) {
    if (strcmp(node->lchild->vs, "float") == 0) {
      ret->type = T_FLOAT;
    } else if (strcmp(node->lchild->vs, "int") == 0) {
      ret->type = T_INT;
    }
  } else if (strcmp(node->lchild->name, "StructSpecifier") == 0) {
    return parseStructSpecifier(node->lchild);
  }
  return ret;
}

typeMeta *parseStructSpecifier(syntaxMeta *node) {
  typeMeta *ret = (typeMeta *)malloc(sizeof(typeMeta));
  ret->type = T_INT;

  char name[40];
  if (node->lchild->rcousin->lchild)
    strcpy(name, node->lchild->rcousin->lchild->vs);
  else
    sprintf(name, "ANONYMOUS_%d", anonymous++);

  if (node->lchild->rcousin->rcousin) { // STRUCT OptTag LC DefList RC
    if (searchSymbol(name, 1, 0) || searchStructSymbol(name)) {
      printError(16, node->lchild->rcousin->line);
      return ret;
    }

    typeMeta *structType = (typeMeta *)malloc(sizeof(typeMeta));
    structType->type = T_STRUCT;
    pushSTable(T_STRUCTBODY);
    structType->stru.fields =
        parseDefList(node->lchild->rcousin->rcousin->rcousin, 0);

    /* 设置结构体中域的偏移量 */
    fieldMeta *fp = structType->stru.fields;
    int offset = 0;
    while (fp) {
      fp->offset = offset;
      switch (fp->type->type) {
      case T_INT:
      case T_FLOAT: offset += 4; break;
      case T_STRUCT: offset += fp->type->stru.size; break;
      case T_ARRAY: offset += fp->type->array.totalSize; break;
      }
      fp = fp->next;
    }
    structType->stru.size = offset;

    popSTable();

    fieldMeta *t = (fieldMeta *)malloc(sizeof(fieldMeta));
    strcpy(t->id, name);
    t->type = structType;

    if (searchSymbol(name, 1, 0) || searchStructSymbol(name)) {
      printError(16, node->lchild->rcousin->line);
      return ret;
    } else
      addStructSymbol(t);

    return structType;
  } else { // STRUCT Tag
    fieldMeta *cur = searchStructSymbol(name);
    if (!cur) {
      printError(17, node->lchild->rcousin->line);
      return ret;
    } else {
      return cur->type;
    }
  }
}

void parseFunDec(syntaxMeta *node, typeMeta *type) {
  if (!node || node->isEmpty) return;
  syntaxMeta *id = node->lchild;
  syntaxMeta *varlist = id->rcousin->rcousin;
  if (strcmp(varlist->name, "VarList") != 0) varlist = NULL;

  fieldMeta *cur = searchSymbol(id->vs, 0, 1);
  if (cur && cur->type->function.defined) { //已经定义过
    printError(4, id->line);
    return;
  }

  pushSTable(T_BLOCK);

  fieldMeta *newEntry = (fieldMeta *)malloc(sizeof(fieldMeta));
  strcpy(newEntry->id, id->vs);
  typeMeta *funcType = (typeMeta *)malloc(sizeof(typeMeta));
  newEntry->type = funcType;
  newEntry->num = 0;
  newEntry->isParam = 0;

  funcType->type = T_FUNC;
  funcType->function.retType = DeepCopyOftypeMeta(type);
  funcType->function.defined = 0;
  funcType->function.line = node->line;
  funcType->function.params = NULL;
  if (varlist) {
    funcType->function.params = parseVarList(varlist);
    /* 函数参数需要标记 */
    fieldMeta *fp = funcType->function.params;
    while (fp) {
      fieldMeta *entry = searchSymbol(fp->id, 1, 0);
      entry->isParam = 1;
      fp->isParam = 1;
      fp = fp->next;
    }
  }
  if (strcmp(node->rcousin->name, "CompSt") == 0) {
    funcType->function.defined = 1;
    if (!cur) addSymbol(newEntry, 1); // 第一次定义，无声明
    else if (!Compare2typeMeta(funcType, cur->type)) {
      cur->type->function.defined = 1; // 错误定义也是定义
      printError(19, id->line);
    } else // 第一次定义，有声明
      cur->type = funcType;
    parseCompSt(node->rcousin, type);
    transFundec(node);
  } else {
    if (!cur) addSymbol(newEntry, 1); // 第一次声明
    else if (!Compare2typeMeta(funcType, cur->type))
      printError(19, id->line);
  }

  popSTable();
}

fieldMeta *parseVarList(syntaxMeta *node) {
  fieldMeta *cur = parseParamDec(node->lchild);
  if (cur) cur->next = NULL;
  if (node->lchild->rcousin) {
    fieldMeta *t = parseVarList(node->lchild->rcousin->rcousin);
    if (cur) cur->next = t;
    else
      cur = t;
  }
  return cur;
}

fieldMeta *parseParamDec(syntaxMeta *node) {
  typeMeta *type = parseSpecifier(node->lchild);
  return parseVarDec(node->lchild->rcousin, type);
}

void parseCompSt(syntaxMeta *node, typeMeta *type) {
  if (!node || node->isEmpty) return;
  parseDefList(node->lchild->rcousin, 1);
  parseStmtList(node->lchild->rcousin->rcousin, type);

  if (!isError) { tmpCompStCode = transCompst(node); }
}

fieldMeta *parseDefList(syntaxMeta *node, int assignable) {
  if (!node || node->isEmpty) return NULL;
  fieldMeta *cur = parseDef(node->lchild, assignable);
  if (cur) {
    fieldMeta *p = cur;
    while (p->next) {
      p = p->next;
    }
    p->next = parseDefList(node->lchild->rcousin, assignable);
    return cur;
  } else {
    return parseDefList(node->lchild->rcousin, assignable);
  }
}

fieldMeta *parseDef(syntaxMeta *node, int assignable) {
  typeMeta *type = parseSpecifier(node->lchild);
  return parseDecList(node->lchild->rcousin, type, assignable);
}

fieldMeta *parseDecList(syntaxMeta *node, typeMeta *type, int assignable) {
  fieldMeta *cur = parseDec(node->lchild, type, assignable);
  if (cur) cur->next = NULL;
  if (node->lchild->rcousin) {
    fieldMeta *t =
        parseDecList(node->lchild->rcousin->rcousin, type, assignable);
    if (cur) cur->next = t;
    else
      cur = t;
  }
  return cur;
}

fieldMeta *parseDec(syntaxMeta *node, typeMeta *type, int assignable) {
  fieldMeta *ret = parseVarDec(node->lchild, type);

  if (node->lchild->rcousin) {
    if (!assignable) {
      printError(15, node->lchild->rcousin->line);
      return ret;
    }
    typeMeta *assignType = parseExp(node->lchild->rcousin->rcousin);
    if (!Compare2typeMeta(ret->type, assignType)) {
      printError(5, node->lchild->line);
    }
  }
  return ret;
}

fieldMeta *parseVarDec(syntaxMeta *node, typeMeta *type) {
  if (!node || node->isEmpty) return NULL;
  if (strcmp(node->lchild->name, "ID") == 0) {
    syntaxMeta *id = node->lchild;

    if (searchSymbol(id->vs, 0, 0)) {
      if (symbolTables[STStackPtr].kind == T_STRUCTBODY)
        printError(15, id->line);
      else
        printError(3, id->line);
      return NULL;
    } else if (searchStructSymbol(id->vs)) {
      printError(3, id->line);
    }

    fieldMeta *newEntry = (fieldMeta *)malloc(sizeof(fieldMeta));
    strcpy(newEntry->id, id->vs);
    newEntry->type = DeepCopyOftypeMeta(type);
    newEntry->num = 0;
    newEntry->isParam = 0;
    addSymbol(newEntry, 0);

    return DeepCopyOffieldMeta(newEntry);

  } else {
    typeMeta *arrayType = (typeMeta *)malloc(sizeof(typeMeta));
    arrayType->type = T_ARRAY;
    arrayType->array.type = type;
    arrayType->array.size = node->lchild->rcousin->rcousin->vi;
    switch (type->type) {
    case T_INT:
    case T_FLOAT: arrayType->array.totalSize = 4 * arrayType->array.size; break;
    case T_ARRAY:
      arrayType->array.totalSize =
          type->array.totalSize * arrayType->array.size;
      break;
    case T_STRUCT:
      arrayType->array.totalSize = type->stru.size * arrayType->array.size;
      break;
    }
    return parseVarDec(node->lchild, arrayType);
  }
  return NULL;
}

typeMeta *parseExp(syntaxMeta *node) {
  // ret是一个类型为INT的dummy返回值
  typeMeta *ret = (typeMeta *)malloc(sizeof(typeMeta));
  ret->type = T_INT;
  if (!node || node->isEmpty) return ret;

  syntaxMeta *t1, *t2 = NULL, *t3 = NULL;
  t1 = node->lchild;
  if (t1) t2 = t1->rcousin;
  if (t2) t3 = t2->rcousin;
  switch (t1->type) {
  case INT:
  case FLOAT: ret->type = (t1->type == INT) ? T_INT : T_FLOAT; return ret;
  case LP: return parseExp(t2);
  case NOT:
    ret = parseExp(t2);
    if (ret->type != T_INT) { printError(7, t1->line); }
    return ret;
  case MINUS:
    ret = parseExp(t2);
    if (ret->type != T_INT && ret->type != T_FLOAT) { printError(7, t1->line); }
    return ret;
  case ID:
    if (!t2) {
      fieldMeta *cur = searchSymbol(t1->vs, 1, 0);
      if (cur == NULL) {
        printError(1, t1->line);
        return ret;
      }
      return cur->type;
    } else { // function call
      fieldMeta *cur = searchSymbol(t1->vs, 1, 0);
      if (cur == NULL) {
        printError(2, t1->line);
        return ret;
      } else if (cur->type->type != T_FUNC) {
        printError(11, t1->line);
        return ret;
      }
      if (t3->type != RP) {
        fieldMeta *args = parseArgs(t3);
        fieldMeta *params = cur->type->function.params;

        fieldMeta *p1 = args, *p2 = params;
        int flag = 1;
        while (p1 && p2) {
          if (Compare2typeMeta(p1->type, p2->type)) {
            p1 = p1->next;
            p2 = p2->next;
          } else {
            printError(9, t1->line);
            flag = 0;
            break;
          }
        }
        if (flag && (p1 || p2)) printError(9, t1->line);
      } else if (cur->type->function.params != NULL) //有形参无实参
        printError(9, t1->line);

      // 即使报错误9，也返回符号表中函数的返回类型
      return cur->type->function.retType;
    }
  default: { // Exp打头的所有情况
    typeMeta *left = parseExp(t1);
    typeMeta *right;
    switch (t2->type) {
    case LB: {
      if (left->type != T_ARRAY) {
        printError(10, t1->line);
        return ret;
      }
      if (parseExp(t3)->type != T_INT) { printError(12, t3->line); }
      return left->array.type;
    }
    case DOT: {
      if (left->type != T_STRUCT) {
        printError(13, t1->line);
        return left;
      }

      fieldMeta *p = left->stru.fields;
      while (p) {
        if (strcmp(p->id, t3->vs) == 0) { return DeepCopyOftypeMeta(p->type); }
        p = p->next;
      }

      printError(14, t3->line);
      return ret;
    }

    case AND:
    case OR:
      right = parseExp(t3);
      if (left->type != T_INT) {
        printError(7, t1->line);
      } else if (right->type != T_INT) {
        printError(7, t3->line);
      }
      return ret;

    case ASSIGNOP: {
      int isLvalue = 0;
      right = parseExp(t3);
      if (!Compare2typeMeta(left, right)) {
        printError(5, t1->line);
        return ret;
      }
      if (strcmp(t1->lchild->name, "ID") == 0 && !t1->lchild->rcousin)
        isLvalue = 1;
      if (t1->lchild->rcousin &&
          (t1->lchild->rcousin->type == LB || t1->lchild->rcousin->type == DOT))
        isLvalue = 1;
      if (!isLvalue) {
        printError(6, t1->line);
        return ret;
      }
      return left;
    }

    default: { // relop and +-*/
      right = parseExp(t3);
      if (left->type != T_INT && left->type != T_FLOAT) {
        printError(7, t1->line);
      } else if (right->type != T_INT && right->type != T_FLOAT) {
        printError(7, t3->line);
      } else if (left->type != right->type) {
        printError(7, t1->line);
      }
      if (t2->type != RELOP) { // 比较运算永远返回int
        ret->type = left->type;
      }
      return ret;
    }
    }
  }
  }
}

fieldMeta *parseArgs(syntaxMeta *node) {
  fieldMeta *cur = (fieldMeta *)malloc(sizeof(fieldMeta));
  strcpy(cur->id, "");
  cur->type = DeepCopyOftypeMeta(parseExp(node->lchild));
  cur->next = NULL;
  if (node->lchild->rcousin) {
    cur->next = parseArgs(node->lchild->rcousin->rcousin);
  }
  return cur;
}

void parseStmtList(syntaxMeta *node, typeMeta *type) {
  if (!node || node->isEmpty) return;
  parseStmt(node->lchild, type);
  parseStmtList(node->lchild->rcousin, type);
}

void parseStmt(syntaxMeta *node, typeMeta *type) {
  if (strcmp(node->lchild->name, "CompSt") == 0) {
    pushSTable(T_BLOCK);
    parseCompSt(node->lchild, type);
    popSTable();
    return;
  }
  if (strcmp(node->lchild->name, "Exp") == 0) {
    parseExp(node->lchild);
    return;
  }
  syntaxMeta *t1 = node->lchild;
  switch (t1->type) {
  case RETURN: {
    syntaxMeta *t2 = node->lchild->rcousin;
    typeMeta *retType = parseExp(t2);
    if (!Compare2typeMeta(retType, type)) { printError(8, t1->line); }
    return;
  }
  case IF: {
    syntaxMeta *t2 = node->lchild->rcousin->rcousin; // EXP
    typeMeta *condType = parseExp(t2);
    if (condType->type != T_INT) { printError(7, t2->line); }
    syntaxMeta *t3 = t2->rcousin->rcousin; // Stmt
    parseStmt(t3, type);
    if (t3->rcousin && t3->rcousin->rcousin) {
      parseStmt(t3->rcousin->rcousin, type);
    }
    return;
  }
  case WHILE: {
    syntaxMeta *t2 = node->lchild->rcousin->rcousin;
    typeMeta *condType = parseExp(t2);
    if (condType->type != T_INT) { printError(7, t2->line); }
    parseStmt(t2->rcousin->rcousin, type);
    return;
  }
  }
}

/*--------------------utils--------------------*/
fieldMeta *DeepCopyOffieldMeta(fieldMeta *i) {
  fieldMeta *ret = (fieldMeta *)malloc(sizeof(fieldMeta));
  memcpy(ret, i, sizeof(fieldMeta));

  ret->type = (typeMeta *)malloc(sizeof(typeMeta));
  memcpy(ret->type, i->type, sizeof(typeMeta));

  ret->next = NULL;
  return ret;
}

typeMeta *DeepCopyOftypeMeta(typeMeta *i) {
  typeMeta *ret = (typeMeta *)malloc(sizeof(typeMeta));
  memcpy(ret, i, sizeof(typeMeta));
  return ret;
}

int Compare2typeMeta(typeMeta *a, typeMeta *b) {
  if (a->type != b->type) return 0;
  if (a->type == T_ARRAY) {
    int a_dim = 1, b_dim = 1;
    typeMeta *t1 = a->array.type;
    while (t1->type == T_ARRAY) {
      ++a_dim;
      t1 = t1->array.type;
    }
    typeMeta *t2 = b->array.type;
    while (t2->type == T_ARRAY) {
      ++b_dim;
      t2 = t2->array.type;
    }
    if (a_dim == b_dim && Compare2typeMeta(t1, t2)) return 1;
    else
      return 0;
  } else if (a->type == T_STRUCT) {
    fieldMeta *p1 = a->stru.fields, *p2 = b->stru.fields;
    while (p1 && p2) {
      if (!Compare2typeMeta(p1->type, p2->type)) return 0;
      p1 = p1->next;
      p2 = p2->next;
    }
    if (p1 || p2) return 0;
    return 1;
  } else if (a->type == T_FUNC) {
    if (!Compare2typeMeta(a->function.retType, b->function.retType)) return 0;
    fieldMeta *p1 = a->function.params, *p2 = b->function.params;
    while (p1 && p2) {
      if (!Compare2typeMeta(p1->type, p2->type)) return 0;
      p1 = p1->next;
      p2 = p2->next;
    }
    if (p1 || p2) return 0;
    return 1;
  } else {
    return 1;
  }
}