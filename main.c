#define RUBY_EXPORT 1
#include "ruby/defines.h"
#include "ruby.h"
#include "internal.h"
#include "eval_intern.h"
#include "iseq.h"
#include "gc.h"
#include "ruby/vm.h"
#include "vm_core.h"
#include "probes_helper.h"
#include <locale.h>
#include "ruby/util.h"

static void ruby_init_prelude(void) {
    Init_prelude();
}

static const struct rb_block* toplevel_context(rb_binding_t *bind) {
    return &bind->block;
}

typedef struct ruby_cmdline_options ruby_cmdline_options_t;
struct ruby_cmdline_options {
    const char *script;
    VALUE script_name;
    VALUE e_script;
    struct {
        struct {
            VALUE name;
            int index;
        } enc;
    } src, ext, intern;
    VALUE req_list;
    unsigned int features;
    unsigned int dump;
    int safe_level;
    int sflag, xflag;
    unsigned int warning: 1;
    unsigned int verbose: 1;
    unsigned int do_loop: 1;
    unsigned int do_print: 1;
    unsigned int do_line: 1;
    unsigned int do_split: 1;
    unsigned int do_search: 1;
    unsigned int setids: 2;
};
static void error_print(rb_execution_context_t *ec) {
    rb_ec_error_print(ec, ec->errinfo);
}

#define CALL(n) {void Init_##n(void); Init_##n();}

void
ohno_rb_call_inits(void)
{
    CALL(Method);
    CALL(RandomSeedCore);
    CALL(sym);
    CALL(var_tables);
    CALL(Object);
    CALL(top_self);
    CALL(Encoding);
    CALL(Comparable);
    CALL(Enumerable);
    CALL(String);
    CALL(Exception);
    CALL(safe);
    CALL(jump);
    CALL(Numeric);
    CALL(Bignum);
    CALL(syserr);
    CALL(Array);
    CALL(Hash);
    CALL(Struct);
    CALL(Regexp);
    CALL(Range);
    CALL(IO);
    CALL(Dir);
    CALL(Time);
    CALL(Random);
    CALL(signal);
    CALL(load);
    CALL(Proc);
    CALL(Binding);
    CALL(Math);
    CALL(GC);
    CALL(VM);
}

int ohno_ruby_setup(void) {
    enum ruby_tag_type state;

    if (GET_VM()) {
        return 0;
    }

    ruby_init_stack((void *)&state);
    Init_BareVM();
    Init_heap();
    Init_vm_objects();

    EC_PUSH_TAG(GET_EC());
    if ((state = EC_EXEC_TAG()) == TAG_NONE) {
        ohno_rb_call_inits();
        GET_VM()->running = 1;
    }
    EC_POP_TAG();

    return state;
}


void ohno_ruby_init(void) {
    int state = ohno_ruby_setup();
    if (state) {
        if (RTEST(ruby_debug))
            error_print(GET_EC());
        exit(EXIT_FAILURE);
    }
}

static rb_ast_t* ohno_run_parser(const char* program) {
    rb_ast_t *ast = NULL;
    VALUE parser = rb_parser_new();

    ruby_cmdline_options_t* opt = malloc(sizeof(ruby_cmdline_options_t));
    memset(opt, 0, sizeof(ruby_cmdline_options_t));

    opt->warning = 1;
    opt->verbose = 1;
    opt->do_loop = 0;
    opt->do_print = 0;
    opt->do_line = 0;
    opt->do_split = 0;
    opt->do_search = 0;
    opt->script = "-e";
    opt->script_name = rb_str_new_cstr(opt->script);
    opt->script = RSTRING_PTR(opt->script_name);
    opt->e_script = rb_str_new(0,0);
    rb_str_cat2(opt->e_script, program);
    rb_str_cat2(opt->e_script, "\n");

    rb_binding_t *toplevel_binding;
    GetBindingPtr(rb_const_get(rb_cObject, rb_intern("TOPLEVEL_BINDING")),
            toplevel_binding);
    const struct rb_block *base_block = toplevel_context(toplevel_binding);
    rb_parser_set_context(parser, base_block, TRUE);
    rb_parser_set_options(parser, opt->do_print, opt->do_loop,
            opt->do_line, opt->do_split);
    ast = rb_parser_compile_string(parser, opt->script, opt->e_script, 1);
    return ast;
}

static VALUE ohno_process_options()
{
    VALUE prog_name = rb_str_new_cstr("-e");
    const char* prog = "puts 'hi'";
    rb_ast_t* ast = ohno_run_parser(prog);

    Init_enc();
    rb_encoding *lenc = rb_locale_encoding();
    const rb_iseq_t *iseq;
    rb_encoding* enc = lenc;
    rb_enc_set_default_external(rb_enc_from_encoding(enc));
    ruby_set_script_name(prog_name);
    ruby_gc_set_params(0);
    ruby_init_prelude();
    const char* argv[4] = { "", "--disable=gems", "-e", prog};
    ruby_set_argv(4, (char**)argv);

    if (!ast->root) {
        rb_ast_dispose(ast);
        printf("no ast root\n");
        return Qfalse;
    }
    ruby_set_script_name(prog_name);

    {
        VALUE path = Qnil;
        rb_binding_t *toplevel_binding;
        GetBindingPtr(rb_const_get(rb_cObject, rb_intern("TOPLEVEL_BINDING")),
                toplevel_binding);
        const struct rb_block *base_block = toplevel_context(toplevel_binding);
        iseq = rb_iseq_new_main(ast->root, prog_name, path, vm_block_iseq(base_block));
        rb_io_write(rb_stdout, rb_iseq_disasm((const rb_iseq_t *)iseq));
        rb_io_flush(rb_stdout);
        rb_ast_dispose(ast);
    }

    rb_set_safe_level(0);

    return (VALUE)iseq;
}
void* ohno_ruby_process_options() {
    VALUE iseq;
    iseq = ohno_process_options();

    return (void*)(struct RData*)iseq;
}

static int error_handle(int ex) {
    int status = EXIT_FAILURE;
    printf("%d\n", ex);
    rb_execution_context_t *ec = GET_EC();
    VALUE errinfo = ec->errinfo;
    if (rb_obj_is_kind_of(errinfo, rb_eSystemExit)) {
        status = 17;
    }
    else {
        rb_ec_error_print(ec, errinfo);
    }
}
void* ohno_ruby_options() {
    enum ruby_tag_type state;
    void *volatile iseq = 0;

    ruby_init_stack((void *)&iseq);
    EC_PUSH_TAG(GET_EC());
    if ((state = EC_EXEC_TAG()) == TAG_NONE) {
        SAVE_ROOT_JMPBUF(GET_THREAD(), iseq = ohno_ruby_process_options());
    } else {
        rb_clear_trace_func();
        state = error_handle(state);
        iseq = (void *)INT2FIX(state);
    }
    EC_POP_TAG();
    return iseq;
}

void ohno_init() {
    setlocale(LC_CTYPE, "");

    {
        ohno_ruby_init();
    }
}

int main(int _argc, char **_argv) {
    ohno_init();
    rb_ast_t* ast = ohno_run_parser("puts 'hi'");
    printf("%p\n", ast->root);

    //rb_io_write(rb_stdout, rb_parser_dump_tree(ast->root, 1));
    //rb_io_flush(rb_stdout);
    //int argc = 0;
    //char **argv = NULL;

    //ruby_sysinit(&argc, &argv);
    //{
    //    RUBY_INIT_STACK;
    //    ohno_ruby_init();
    //    return ruby_run_node(ohno_ruby_options());
    //}
}
