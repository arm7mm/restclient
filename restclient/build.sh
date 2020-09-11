#!/bin/sh
exec clang -std=c99 src/strcmpr.c src/stack.c src/url.c src/jsonutf.c src/restclient.c -o restclient
