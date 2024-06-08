#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <toywasm/expr_parser.h>
#include <toywasm/leb128.h>
#include <toywasm/name.h>
#include <toywasm/type.h>

static void
fatal(int error)
{
        fprintf(stderr, "fatal error %d: %s", error, strerror(error));
        exit(1);
}

void
dump_table_type(uint32_t tableidx, const char *typestr)
{
        printf("subgraph cluster_table_%" PRIu32 " { \"table%" PRIu32
               "_%s\" [label=\"%s\",shape=box]; label=table_%" PRIu32
               "; color=purple }\n",
               tableidx, tableidx, typestr, typestr, tableidx);
}

static void
dump_calls(const struct module *m, uint32_t i, struct nametable *table)
{
        uint32_t funcidx = m->nimportedfuncs + i;
        const struct func *func = &m->funcs[i];
        const uint8_t *insn = func->e.start;
        struct parse_expr_context ctx;
        parse_expr_context_init(&ctx);
        do {
                const uint8_t *imm;
                struct name callee_func_name;
                uint32_t callee;
                uint32_t tableidx;
                uint32_t typeidx;
                const struct functype *ft;
                char *typestr;
                int ret;
                switch (insn[0]) {
                case 0x10: /* call */
                        imm = &insn[1];
                        callee = read_leb_u32_nocheck(&imm);
                        nametable_lookup_func(table, m, callee,
                                              &callee_func_name);
                        printf("f%" PRIu32 " -> f%" PRIu32 "\n", funcidx,
                               callee);
                        break;
                case 0x11: /* call_indirect */
                        imm = &insn[1];
                        typeidx = read_leb_u32_nocheck(&imm);
                        tableidx = read_leb_u32_nocheck(&imm);
                        ft = &m->types[typeidx];
                        ret = functype_to_string(&typestr, ft);
                        if (ret != 0) {
                                fatal(ret);
                        }
                        dump_table_type(tableidx, typestr);
                        printf("f%" PRIu32 " -> \"table%" PRIu32
                               "_%s\" [color=purple]\n",
                               funcidx, tableidx, typestr);
                        functype_string_free(typestr);
                        break;
#if defined(TOYWASM_ENABLE_WASM_TAILCALL)
                        /* XXX implement tail-call instructions */
#endif
                default:
                        break;
                }
                parse_expr(&insn, &ctx);
        } while (insn != NULL);
        parse_expr_context_clear(&ctx);
}

/*
 * print the function callgraph of the given module in the graphviz format.
 *
 * XXX should escape the names
 *
 * references:
 * https://graphviz.org/doc/info/lang.html
 * https://graphviz.org/doc/info/attrs.html
 */
void
callgraph(const struct module *m)
{
        int ret;
        uint32_t i;
        struct nametable table;
        nametable_init(&table);
        printf("strict digraph {\n");
        for (i = 0; i < m->nimportedfuncs + m->nfuncs; i++) {
                const char *color;
                if (i < m->nimportedfuncs) {
                        color = "blue";
                } else {
                        color = "black"; /* default */
                }
                struct name func_name;
                nametable_lookup_func(&table, m, i, &func_name);
                printf("f%" PRIu32 " [label=\"%.*s\",color=%s]\n", i,
                       CSTR(&func_name), color);
        }
        /*
         * here we only implement active elements with funcidxes.
         * ie. the case where we don't need to execute exprs.
         * although it's limited, it should be enough to
         * cover the most of common cases.
         */
        for (i = 0; i < m->nelems; i++) {
                const struct element *e = &m->elems[i];
                if (e->mode != ELEM_MODE_ACTIVE) {
                        continue;
                }
                if (e->funcs == NULL) {
                        continue;
                }
                uint32_t j;
                for (j = 0; j < e->init_size; j++) {
                        uint32_t funcidx = e->funcs[j];
                        uint32_t tableidx = e->table;
                        const struct functype *ft =
                                module_functype(m, funcidx);
                        char *typestr;
                        ret = functype_to_string(&typestr, ft);
                        if (ret != 0) {
                                fatal(ret);
                        }
                        dump_table_type(tableidx, typestr);
                        printf("\"table%" PRIu32 "_%s\" -> f%" PRIu32
                               " [color=purple]\n",
                               tableidx, typestr, funcidx);
                        functype_string_free(typestr);
                }
        }
        for (i = 0; i < m->nimports; i++) {
                const struct import *im = &m->imports[i];
                printf("subgraph \"cluster_import_%.*s\" { import%" PRIu32
                       " [label=\"%.*s\",shape=rectangle]; label=\"%.*s\"; "
                       "color=blue;rank=sink }\n",
                       CSTR(&im->module_name), i, CSTR(&im->name),
                       CSTR(&im->module_name));
                printf("f%" PRIu32 " -> import%" PRIu32 " [color=blue]\n", i,
                       i);
        }
        for (i = 0; i < m->nexports; i++) {
                const struct wasm_export *ex = &m->exports[i];
                if (ex->desc.type != EXTERNTYPE_FUNC) {
                        continue;
                }
                printf("subgraph cluster_export { export%" PRIu32
                       " [label=\"%.*s\",shape=rectangle]; label=exports; "
                       "color=red;rank=source }\n",
                       i, CSTR(&ex->name));
                printf("export%" PRIu32 " -> f%" PRIu32 " [color=red]\n", i,
                       ex->desc.idx);
        }
        if (m->has_start) {
                /*
                 * group the start function together with exports as
                 * their functionalites are similar for our purpose.
                 */
                printf("subgraph cluster_export { start [shape=plaintext]; "
                       "label=exports; "
                       "color=red;rank=source }\n");
                printf("start -> f%" PRIu32 " [color=red]\n", m->start);
        }
        for (i = 0; i < m->nfuncs; i++) {
                dump_calls(m, i, &table);
        }
        printf("}\n");
        nametable_clear(&table);
}
