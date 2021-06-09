%option yylineno
%{
  #include "syntax.tab.h"
  #include "syntax.h"

  int yycolumn = 1;
  #define YY_USER_ACTION \
    yylloc.first_line = yylloc.last_line = yylineno; \
    yylloc.first_column = yycolumn; \
    yylloc.last_column = yycolumn + yyleng - 1; \
    yycolumn += yyleng; 

  enum lexicalErrorType {
    LEXICALERR_UNKNOWN_CHARACTER,
    LEXICALERR_WRONG_ID,
    LEXICALERR_COMMENT_EOF,
  };
  int handleToken(const char* name, int arg);
  void handleLexicalError(int type, int arg);
%}
BLANK " "
TAB "\t"
NEWLINE "\n"
WS ({BLANK}|{TAB}|{NEWLINE})*

letter_ [a-zA-Z_]
ID {letter_}({letter_}|[0-9])*
WRONGID [0-9]({letter_}|[0-9])*

DEC 0|[1-9][0-9]*
OCT 0[0-7]+ 
HEX 0(x|X)[0-9A-Fa-f]+
INT {DEC}|{OCT}|{HEX}

NORMALFLOAT ([0-9]+\.[0-9]+)
EXPFLOAT (([0-9]*\.[0-9]+)|([0-9]+\.))(E|e)[+-]?[0-9]+
FLOAT {NORMALFLOAT}|{EXPFLOAT}

TYPE int|float
RELOP ">"|"<"|">="|"<="|"=="|"!="
%%
"//" {
  while(1){
    char ch = input();
    ++yycolumn;
    if(ch == '\n'){
      yycolumn = 1;
      break;
    }
  }
}
"/*" {
  int curline = yylineno;
  char pre = input();
  char ch = input();
  yycolumn += 2;
  while(1){
    if(pre == '*' && ch == '/') break;
    if(ch == '\n') yycolumn = 1;
    if(!ch){ // eof
      handleLexicalError(LEXICALERR_COMMENT_EOF,curline);
      break; 
    }
    pre = ch; 
    ch = input();
    ++yycolumn;
  }
}
";" {return handleToken("SEMI",SEMI);}
"," {return handleToken("COMMA",COMMA);}
"=" {return handleToken("ASSIGNOP",ASSIGNOP);}
"+" {return handleToken("PLUS",PLUS);}
"-" {return handleToken("MINUS",MINUS);}
"*" {return handleToken("STAR",STAR);}
"/" {return handleToken("DIV",DIV);}
"&&" {return handleToken("AND",AND);}
"||" {return handleToken("OR",OR);}
"." {return handleToken("DOT",DOT);}
"!" {return handleToken("NOT",NOT);}
"(" {return handleToken("LP",LP);}
")" {return handleToken("RP",RP);}
"[" {return handleToken("LB",LB);}
"]" {return handleToken("RB",RB);}
"{" {return handleToken("LC",LC);}
"}" {return handleToken("RC",RC);}
"struct" {return handleToken("STRUCT",STRUCT);}
"return" {return handleToken("RETURN",RETURN);}
"if" {return handleToken("IF",IF);}
"else" {return handleToken("ELSE",ELSE);}
"while" {return handleToken("WHILE",WHILE);}

{NEWLINE} {yycolumn = 1;}
{WS} {}
{RELOP} {return handleToken("RELOP", RELOP);}
{TYPE} {return handleToken("TYPE",TYPE);}
{FLOAT} {return handleToken("FLOAT",FLOAT);}
{INT} {return handleToken("INT",INT);}
{ID} {return handleToken("ID",ID);}
{WRONGID} {handleLexicalError(LEXICALERR_WRONG_ID,0);return handleToken("ID",ID);}

. {handleLexicalError(LEXICALERR_UNKNOWN_CHARACTER,0);}
%%
int getRelop(char* text){
  if (strcmp(text,"==") == 0) return RELOPS_EQ;
  if (strcmp(text,"!=") == 0) return RELOPS_UNEQ;
  if (strcmp(text,">=") == 0) return RELOPS_GE;
  if (strcmp(text,"<=") == 0) return RELOPS_LE;
  if (strcmp(text,">") == 0) return RELOPS_G;
  if (strcmp(text,"<") == 0) return RELOPS_L;
  return 0;
}

int handleToken(const char* name, int arg){
  syntaxMeta *meta = (syntaxMeta*)malloc(sizeof(syntaxMeta));
  switch (arg) { 
      case TYPE:
      case ID:
        strcpy(meta->vs, yytext); 
        break; 
      case INT:
        meta->vi = (unsigned)strtol(yytext, NULL, 0); 
        break; 
      case RELOP:
        meta->vr = getRelop(yytext);
        break;
      case FLOAT:
        meta->vf = strtof(yytext, NULL);        
        break; 
      default:break;
  } 
  meta->type = arg;
  meta->isEmpty = 0;
  meta->isTerm = 1;
  meta->line = yylineno; 
  meta->column = yycolumn; 
  meta->lchild = meta->rcousin = NULL;
  strcpy(meta->name,name); 
  
  yylloc.meta = meta; 

  return arg; 
}

extern int isError;

void handleLexicalError(int type, int arg){
  isError = 1;
  switch(type){
  case LEXICALERR_UNKNOWN_CHARACTER:
    printf("Error type A at Line %d: Mysterious character \"%s\".\n",yylineno, yytext);
    break;
  case LEXICALERR_WRONG_ID:
    printf("Error type A at Line %d: Illegal identifier \"%s\".\n",yylineno, yytext);
    break;
  case LEXICALERR_COMMENT_EOF:
    printf("Error type A at Line %d: Block comment without termination.\n", arg);
  default:break;
  }
}