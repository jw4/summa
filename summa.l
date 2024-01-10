%{
#include <stdio.h>
#include "summa.h"
%}

%option header-file="summa.yy.h" outfile="summa.yy.c"

%s time

%%

([01][0-9])|(2[0-9])/[0-5][0-9]                 { fprintf(stderr, "\t\thour '%s'\n", yytext);   BEGIN(time);    return hour; }
<time>[0-5][0-9]/[^0-9]                         { fprintf(stderr, "\t\tminute '%s'\n", yytext); BEGIN(INITIAL); return minute; }

[./a-zA-Z][./a-zA-Z0-9_\-]*                     { fprintf(stderr, "\t\tword '%s'\n", yytext);         return word; }
[ \t\v\h]+                                      { fprintf(stderr, "\t\tws '%s'\n", yytext);           return ws; }
[.,?;:'"`~!@$^&*()+={}\\\[\]%]                  { fprintf(stderr, "\t\tpunctuation '%s'\n", yytext);  return punctuation; }
\n                                              { fprintf(stderr, "\t\tnewline\n");                   return nl; }
#                                               { fprintf(stderr, "\t\thash '%s'\n", yytext);         return hash; }
%([0-9][0-9]?)|100                              { fprintf(stderr, "\t\tpercent '%s'\n", yytext);      return percent; }
-                                               { fprintf(stderr, "\t\tdash '%s'\n", yytext);         return dash; }
.                                               { fprintf(stderr, "\t\tchar '%s'\n", yytext);         return character; }

%%
