#ifndef SYNTAX_H
#define SYNTAX_H

enum RELOPS {
  RELOPS_EQ,
  RELOPS_UNEQ,
  RELOPS_GE,
  RELOPS_LE,
  RELOPS_G,
  RELOPS_L
};

typedef struct syntaxMeta {
  int type; // invalid for non-term
  union {
    unsigned vi;
    float vf;
    enum RELOPS vr;
    char vs[40];
  };

  int isTerm;
  int isEmpty;
  int line, column;
  char name[40];

  struct syntaxMeta *lchild, *rcousin;
  void* ptr;
} syntaxMeta;

extern syntaxMeta *syntaxTreeRoot;
#endif