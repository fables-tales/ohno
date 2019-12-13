LDSHARED = -dynamic -bundle
CFLAGS = -O3 -ggdb3 -Wall -Wextra -Wno-unused-parameter -Wno-parentheses -Wno-long-long -Wno-missing-field-initializers -Wno-tautological-compare -Wno-parentheses-equality -Wno-constant-logical-operand -Wno-self-assign -Wunused-variable -Wimplicit-int -Wpointer-arith -Wwrite-strings -Wdeclaration-after-statement -Wshorten-64-to-32 -Wimplicit-function-declaration -Wdivision-by-zero -Wdeprecated-declarations -Wextra-tokens   -pipe
XCFLAGS = -D_FORTIFY_SOURCE=2 -fstack-protector -fno-strict-overflow -fvisibility=hidden -DRUBY_EXPORT -fPIE
CPPFLAGS = -D_XOPEN_SOURCE -D_DARWIN_C_SOURCE -D_DARWIN_UNLIMITED_SELECT -D_REENTRANT   -I./rubies/ruby-2.5.7 -I./rubies/ruby-2.5.7/.ext/include/x86_64-darwin18 -I./rubies/ruby-2.5.7/include -I./rubies/ruby-2.5.7/enc/unicode/10.0.0
DLDFLAGS = -Wl,-undefined,dynamic_lookup -Wl,-multiply_defined,suppress -fstack-protector -Wl,-pie -framework Foundation
LANG = en_US.UTF-8

all: main

main: main.c
	clang $(CFLAGS) $(XCFLAGS) $(CPPFLAGS) $(DLDFLAGS) ./libruby.2.5.7-static.a ./rubies/ruby-2.5.7/ruby.c ./main.c -o main

ruby.o: ./rubies/ruby-2.5.7/ruby.c
	clang $(CFLAGS) $(XCFLAGS) $(CPPFLAGS) $(DLDFLAGS) -c ./rubies/ruby-2.5.7/ruby.c -o ruby.o
