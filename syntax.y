%locations
%token-table
%{
  #include "lex.yy.c"
  void myerror(char* str);
  void yyerror(char* str);
  #define YYLLOC_DEFAULT(Cur, Rhs, N) \
  { \
    if (N) { \
      (Cur).first_line = YYRHSLOC(Rhs, 1).first_line; \
      (Cur).first_column = YYRHSLOC(Rhs, 1).first_column; \
      (Cur).last_line = YYRHSLOC(Rhs, N).last_line; \
      (Cur).last_column = YYRHSLOC(Rhs, N).last_column; \
    } \
    else { \
      (Cur).first_line = (Cur).last_line = YYRHSLOC(Rhs, 0).last_line; \
      (Cur).first_column = (Cur).last_column = YYRHSLOC(Rhs, 0).last_column; \
    } \
    syntaxMeta *meta = (syntaxMeta*)malloc(sizeof(syntaxMeta)); \
    meta->isEmpty = (N==0); \
    meta->type = 0; \
    meta->isTerm = 0; \
    meta->ptr = NULL; \
    meta->line = (Cur).first_line; \
    meta->column = (Cur).first_column; \
    strcpy(meta->name, yytname[yyr1[yyn]]); \
    meta->rcousin = NULL; \
    if(!N) { \
      meta->lchild = NULL; \
    } else { \
      meta->lchild = YYRHSLOC(Rhs, 1).meta; \
      for(int i=1;i<=N-1;i++) \
        (YYRHSLOC(Rhs, i).meta)->rcousin = YYRHSLOC(Rhs, i+1).meta; \
      (YYRHSLOC(Rhs, N).meta)->rcousin = NULL; \
    } \
    (Cur).meta = meta; \
  } 
%}

%code requires {
  #include "syntax.h"

  #define YYLTYPE YYLTYPE
  #define YYLTYPE_IS_DECLARED 1
  #define YYLTYPE_IS_TRIVIAL 0
  typedef struct YYLTYPE {
    int first_line;
    int first_column;
    int last_line;
    int last_column;
    
    syntaxMeta *meta;
  } YYLTYPE;
}

%token TYPE ID
%token INT
%token FLOAT
%token SEMI COMMA
%token LC RC
%token STRUCT RETURN IF WHILE
%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

%right ASSIGNOP

%left OR

%left AND 

%left RELOP

%left PLUS MINUS 

%left STAR DIV

%right NOT NEG

%left DOT LB RB LP RP 

%%
Program: ExtDefList { syntaxTreeRoot = @$.meta; }
  ;
ExtDefList: ExtDef ExtDefList
  | 
  ;
ExtDef: Specifier ExtDecList SEMI
  | Specifier SEMI
  | Specifier FunDec SEMI
  | Specifier FunDec CompSt
  | error SEMI {myerror("mysterious content before ';'");}
  ;
ExtDecList: VarDec
  | VarDec COMMA ExtDecList
  ;

Specifier: TYPE 
  | StructSpecifier
  ;
StructSpecifier: STRUCT OptTag LC DefList RC
  | STRUCT OptTag LC error RC {myerror("mysterious content in struct definition before '}'");}
  | STRUCT Tag
  ;
OptTag: ID
  |
  ;
Tag: ID
  ;

VarDec: ID
  | VarDec LB INT RB
  | VarDec LB error RB {myerror("mysterious content in index before ']'");}
  ;
FunDec: ID LP VarList RP
  | ID LP RP
  | ID LP error RP {myerror("mysterious arguments in function declaration before ')'");}
  ;
VarList: ParamDec COMMA VarList
  | ParamDec
  ;
ParamDec: Specifier VarDec
  ;

CompSt: LC DefList StmtList RC
  ;
StmtList: Stmt StmtList
  | 
  ;
Stmt: Exp SEMI
  | error SEMI {myerror("mysterious content before ';'");}
  | CompSt
  | RETURN Exp SEMI
  | RETURN error SEMI {myerror("mysterious content before ';'");}
  | IF LP Exp RP Stmt %prec LOWER_THAN_ELSE 
  | IF LP error RP Stmt %prec LOWER_THAN_ELSE {myerror("mysterious content between '(' and ')'");}
  | IF LP Exp RP Stmt ELSE Stmt
  | IF LP error RP Stmt ELSE Stmt {myerror("mysterious content between '(' and ')'");}
  | WHILE LP Exp RP Stmt
  | WHILE LP error RP Stmt {myerror("mysterious content between '(' and ')'");}
  ;

DefList : Def DefList
  |
  ;
Def: Specifier DecList SEMI
  | Specifier error SEMI {myerror("mysterious variable definition");}
  ;
DecList: Dec
  | Dec COMMA DecList
  ;
Dec: VarDec
  | VarDec ASSIGNOP Exp
  ;

Exp: Exp ASSIGNOP Exp
  | Exp ASSIGNOP error SEMI {myerror("mysterious content after '=='");}
  | Exp AND Exp
  | Exp AND error SEMI {myerror("mysterious content after '&&'");}
  | Exp OR Exp
  | Exp OR error SEMI {myerror("mysterious content after '||'");}
  | Exp RELOP Exp
  | Exp RELOP error SEMI {myerror("mysterious content after relation operator");}
  | Exp PLUS Exp
  | Exp PLUS error SEMI {myerror("mysterious content after '+'");}
  | Exp MINUS Exp
  | Exp MINUS error SEMI {myerror("mysterious content after '-'");}
  | Exp STAR Exp
  | Exp STAR error SEMI {myerror("mysterious content after '*'");}
  | Exp DIV Exp
  | Exp DIV error SEMI {myerror("mysterious content after '/'");}
  | LP Exp RP
  | LP error RP {myerror("mysterious content between '(' and ')'");}
  | LP error SEMI {myerror("missing ')'");}
  | MINUS Exp %prec NEG
  | NOT Exp
  | ID LP Args RP 
  | ID LP RP
  | ID LP error RP {myerror("invalid arguments");}
  | ID LP error SEMI {myerror("missing ')'");}
  | Exp LB Exp RB
  | Exp LB error RB {myerror("invalid index");}
  | Exp LB error SEMI {myerror("missing ']'");} 
  | Exp DOT ID
  | ID
  | INT
  | FLOAT
  ;
Args: Exp COMMA Args
  | Exp
  ;
%%

extern int isError;
void yyerror(char* str)
{
  isError = 1;
}
void myerror(char* str)
{
  isError = 1;
  printf("Error type B at Line %d: %s near \"%s\".\n", yylineno, str, yytext);
}