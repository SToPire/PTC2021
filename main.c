#include "ir.h"
#include "semantic.h"
#include "syntax.h"
#include "syntax.tab.h"
#include "asm.h"
#include <stdio.h>

extern void yyrestart(FILE *);
extern int yyparse(void);

syntaxMeta *syntaxTreeRoot;
IRCode *AllCode;
IRCode *tmpCompStCode;

void printSyntaxTree(syntaxMeta *node, int level) {
  if (!node || node->isEmpty) return;
  for (int i = 1; i <= level; i++)
    printf("  "); // 2 spaces for each level
  if (node->type == 0) {
    printf("%s (%d)\n", node->name, node->line);
  } else {
    printf("%s", node->name);
    switch (node->type) {
    case ID:
    case TYPE: printf(": %s\n", node->vs); break;
    case INT: printf(": %u\n", node->vi); break;
    case FLOAT: printf(": %f\n", node->vf); break;
    default: printf("\n"); break;
    }
  }
  syntaxMeta *p = node->lchild;
  while (p != NULL) {
    printSyntaxTree(p, level + 1);
    p = p->rcousin;
  }
}

int isError = 0;
int main(int argc, char **argv) {
  if (argc <= 1) return 1;
  FILE *f = fopen(argv[1], "r");
  if (!f) {
    perror(argv[1]);
    return 1;
  }
  yyrestart(f);

  yyparse();
  if (isError) {
    // printSyntaxTree(syntaxTreeRoot, 0);
    printf("syntax error\n");
    return 0;
  }

  semanticAnalysis();
  if (isError) {
    printf("semantic error\n");
    return 0;
  }

  FILE *of = fopen(argv[2], "w");
  if(!of){
    perror(argv[2]);
    return 1;
  }
  
  //printCode(AllCode, of);
  toAsm(of);

  return 0;
}