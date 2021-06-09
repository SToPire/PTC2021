#ifndef SEMANTIC_H
#define SEMANTIC_H
#include "syntax.h"

extern int isError;

enum Types { T_INT, T_FLOAT, T_FUNC, T_ARRAY, T_STRUCT };
enum STTypes { T_GLOBAL, T_STRUCTBODY, T_BLOCK, T_STRUCTONLY };

typedef struct typeMeta {
  enum Types type;
  union {
    struct {
      int defined;
      struct typeMeta *retType;
      struct fieldMeta *params;
      int line;
    } function;
    struct {
      int size;      //当前维度的数组大小
      int totalSize; //数组总大小
      struct typeMeta *type;
    } array;
    struct {
      int size;
      struct fieldMeta *fields;
    } stru;
  };
} typeMeta;

typedef struct fieldMeta {
  char id[40];
  typeMeta *type;
  struct fieldMeta *next;
  int num;    //对应的变量序号
  int offset; //只在结构体域中有意义
  int isParam; // 是一个函数参数，如果是数组/结构体就不分配内存了
} fieldMeta;

typedef struct symbolTable {
  fieldMeta *head;
  enum STTypes kind;
} symbolTable;

symbolTable symbolTables[100];
symbolTable structTable;
int STStackPtr;
void semanticInitial();
void semanticFinish();
void semanticAnalysis();
void pushSTable(int STTypes);
void popSTable();
void addSymbol(fieldMeta *symbol, int global);
void addStructSymbol(fieldMeta *symbol);
fieldMeta *searchSymbol(const char *name, int needRecursive, int globalOnly);
fieldMeta *searchStructSymbol(const char *name);

void parseExtDefList(syntaxMeta *node);
void parseExtDef(syntaxMeta *node);
void parseExtDecList(syntaxMeta *node, typeMeta *type);
typeMeta *parseSpecifier(syntaxMeta *node);
typeMeta *parseStructSpecifier(syntaxMeta *node);
void parseFunDec(syntaxMeta *node, typeMeta *type);
fieldMeta *parseVarList(syntaxMeta *node);
fieldMeta *parseParamDec(syntaxMeta *node);
void parseCompSt(syntaxMeta *node, typeMeta *type);
fieldMeta *parseDefList(syntaxMeta *node, int assignable);
fieldMeta *parseDef(syntaxMeta *node, int assignable);
fieldMeta *parseDecList(syntaxMeta *node, typeMeta *type, int assignable);
fieldMeta *parseDec(syntaxMeta *node, typeMeta *type, int assignable);
fieldMeta *parseVarDec(syntaxMeta *node, typeMeta *type);
void parseStmtList(syntaxMeta *node, typeMeta *type);
void parseStmt(syntaxMeta *node, typeMeta *type);
typeMeta *parseExp(syntaxMeta *node);
fieldMeta *parseArgs(syntaxMeta *node);

fieldMeta *DeepCopyOffieldMeta(fieldMeta *i);
typeMeta *DeepCopyOftypeMeta(typeMeta *i);
int Compare2typeMeta(typeMeta *a, typeMeta *b);

int anonymous;
#endif