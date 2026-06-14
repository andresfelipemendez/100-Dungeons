#include "mutate.h"

/* append one mutation site (silently dropped past `max`) */
static void emit(mutant *out, int *count, int max,
                 int off, int len, const char *repl, int line) {
    int i = 0;
    if (*count >= max) {
        return;
    }
    out[*count].offset = off;
    out[*count].len = len;
    while (repl[i] && i < (int)sizeof(out[*count].repl) - 1) {
        out[*count].repl[i] = repl[i];
        i++;
    }
    out[*count].repl[i] = 0;
    out[*count].line = line;
    (*count)++;
}

int mutate_scan(const char *src, int len, mutant *out, int max) {
    int i = 0, count = 0, line = 1;
    while (i < len) {
        char c = src[i];

        if (c == '\n') { line++; i++; continue; }

        /* line comment */
        if (c == '/' && i + 1 < len && src[i + 1] == '/') {
            i += 2;
            while (i < len && src[i] != '\n') i++;
            continue;
        }
        /* block comment */
        if (c == '/' && i + 1 < len && src[i + 1] == '*') {
            i += 2;
            while (i + 1 < len && !(src[i] == '*' && src[i + 1] == '/')) {
                if (src[i] == '\n') line++;
                i++;
            }
            i += 2;
            continue;
        }
        /* string literal */
        if (c == '"') {
            i++;
            while (i < len && src[i] != '"') {
                if (src[i] == '\\' && i + 1 < len) i++;
                if (src[i] == '\n') line++;
                i++;
            }
            i++;
            continue;
        }
        /* char literal */
        if (c == '\'') {
            i++;
            while (i < len && src[i] != '\'') {
                if (src[i] == '\\' && i + 1 < len) i++;
                i++;
            }
            i++;
            continue;
        }

        /* two-char operators */
        if (i + 1 < len) {
            char d = src[i + 1];
            if (c == '<' && d == '<') { i += 2; continue; } /* shift, skip */
            if (c == '>' && d == '>') { i += 2; continue; }
            if (c == '<' && d == '=') { emit(out,&count,max,i,2,"<",line);  i += 2; continue; }
            if (c == '>' && d == '=') { emit(out,&count,max,i,2,">",line);  i += 2; continue; }
            if (c == '=' && d == '=') { emit(out,&count,max,i,2,"!=",line); i += 2; continue; }
            if (c == '!' && d == '=') { emit(out,&count,max,i,2,"==",line); i += 2; continue; }
            if (c == '&' && d == '&') { emit(out,&count,max,i,2,"||",line); i += 2; continue; }
            if (c == '|' && d == '|') { emit(out,&count,max,i,2,"&&",line); i += 2; continue; }
        }
        /* single-char relational (not part of a two-char op above) */
        if (c == '<') { emit(out,&count,max,i,1,"<=",line); i++; continue; }
        if (c == '>') { emit(out,&count,max,i,1,">=",line); i++; continue; }

        i++;
    }
    return count;
}
