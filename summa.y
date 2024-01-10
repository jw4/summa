%{
#include <stdio.h>
#include <stdlib.h>

extern int yylex(void);
extern int yyparse(void);
void yyerror(const char * s);
%}

%require "3.8"

%token character dash hash hour minute nl percent punctuation word ws

%%

input:
     | input logline nl { fprintf(stderr, "logline\n"); }
     | input ignore nl { fprintf(stderr, "ignore\n"); }
     | input nl { fprintf(stderr, "newline\n"); }
     ;

logline:
       timestamp ws description ws tags { fprintf(stderr, "timestamp description tags\n"); }
       | timestamp ws description { fprintf(stderr, "timestamp description\n"); }
       | timestamp ws tags { fprintf(stderr, "timestamp tags\n"); }
       | timespan ws percent ws description ws tags { fprintf(stderr, "timespan percent description tags\n"); }
       | timespan ws percent ws description { fprintf(stderr, "timespan percent description\n"); }
       | timespan ws percent ws tags { fprintf(stderr, "timespan percent tags\n"); }
       | timespan ws description ws tags { fprintf(stderr, "timespan description tags\n"); }
       | timespan ws description { fprintf(stderr, "timespan description\n"); }
       | timespan ws tags { fprintf(stderr, "timespan tags\n"); }
       ;

timespan:
        timestamp dash timestamp { fprintf(stderr, "timespan\n"); }
        ;

timestamp:
         hour minute { fprintf(stderr, "timestamp\n"); }
         ;

description:
           character
           | dash
           | punctuation
           | word
           | ws
           | description character
           | description dash
           | description punctuation
           | description word
           | description ws
           ;

tags:
    tag
    | tags ws tag
    ;

tag:
   hash word { fprintf(stderr, "tag\n"); }
   ;

ignore:
      character
      | dash
      | hash
      | percent
      | punctuation
      | ws
      | word
      | ignore character
      | ignore dash
      | ignore hash
      | ignore percent
      | ignore punctuation
      | ignore timestamp
      | ignore word
      | ignore ws
      ;
%%

int main(int argc, char ** argv) {
  while(yyparse()) {};
}

void yyerror(const char * s) {
  fprintf(stderr, "error: %s\n", s);
  exit(1);
}
