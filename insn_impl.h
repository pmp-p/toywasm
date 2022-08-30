
INSN_IMPL(unreachable)
{
        if (EXECUTING) {
                return trap_with_id(ECTX, TRAP_UNREACHABLE, "unreachable");
        } else if (VALIDATING) {
                mark_unreachable(VCTX);
        }
        return 0;
}

INSN_IMPL(nop) { INSN_SUCCESS; }

INSN_IMPL(block)
{
        int ret;
        struct resulttype *rt_parameter = NULL;
        struct resulttype *rt_result = NULL;

        LOAD_PC;
        READ_LEB_S33(blocktype);
        if (EXECUTING) {
                push_label(ORIG_PC, STACK, ECTX);
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                struct module *m = vctx->module;
                ret = get_functype_for_blocktype(m, blocktype, &rt_parameter,
                                                 &rt_result);
                if (ret != 0) {
                        goto fail;
                }
                ret = pop_valtypes(rt_parameter, vctx);
                if (ret != 0) {
                        goto fail;
                }
                uint32_t pc = ptr2pc(m, ORIG_PC - 1);
                ret = push_ctrlframe(pc, FRAME_OP_BLOCK, 0, rt_parameter,
                                     rt_result, vctx);
                if (ret != 0) {
                        goto fail;
                }
                rt_parameter = NULL;
                rt_result = NULL;
        }
        SAVE_PC;
        INSN_SUCCESS_RETURN;
fail:
        resulttype_free(rt_parameter);
        resulttype_free(rt_result);
        return ret;
}

INSN_IMPL(loop)
{
        struct resulttype *rt_parameter = NULL;
        struct resulttype *rt_result = NULL;
        int ret;

        LOAD_PC;
        READ_LEB_S33(blocktype);
        if (EXECUTING) {
                push_label(ORIG_PC, STACK, ECTX);
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                struct module *m = vctx->module;
                ret = get_functype_for_blocktype(m, blocktype, &rt_parameter,
                                                 &rt_result);
                if (ret != 0) {
                        goto fail;
                }
                ret = pop_valtypes(rt_parameter, vctx);
                if (ret != 0) {
                        goto fail;
                }
                uint32_t pc = ptr2pc(m, ORIG_PC - 1);
                ret = push_ctrlframe(pc, FRAME_OP_LOOP, 0, rt_parameter,
                                     rt_result, vctx);
                if (ret != 0) {
                        goto fail;
                }
                rt_parameter = NULL;
                rt_result = NULL;
        }
        SAVE_PC;
        INSN_SUCCESS_RETURN;
fail:
        resulttype_free(rt_parameter);
        resulttype_free(rt_result);
        return ret;
}

INSN_IMPL(if)
{
        struct resulttype *rt_parameter = NULL;
        struct resulttype *rt_result = NULL;
        int ret;

        LOAD_PC;
        READ_LEB_S33(blocktype);
        POP_VAL(TYPE_i32, c);
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                push_label(ORIG_PC, STACK, ECTX);
                if (val_c.u.i32 == 0) {
                        ectx->event_u.branch.index = 0;
                        ectx->event_u.branch.goto_else = true;
                        ectx->event = EXEC_EVENT_BRANCH;
                }
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                struct module *m = vctx->module;
                ret = get_functype_for_blocktype(m, blocktype, &rt_parameter,
                                                 &rt_result);
                if (ret != 0) {
                        goto fail;
                }
                ret = pop_valtypes(rt_parameter, vctx);
                if (ret != 0) {
                        goto fail;
                }
                uint32_t pc = ptr2pc(m, ORIG_PC - 1);
                ret = push_ctrlframe(pc, FRAME_OP_IF, 0, rt_parameter,
                                     rt_result, vctx);
                if (ret != 0) {
                        goto fail;
                }
                rt_parameter = NULL;
                rt_result = NULL;
        }
        SAVE_PC;
        INSN_SUCCESS_RETURN;
fail:
        resulttype_free(rt_parameter);
        resulttype_free(rt_result);
        return ret;
}

INSN_IMPL(else)
{
        if (EXECUTING) {
                /* equivalent of "br 0" */
                struct exec_context *ectx = ECTX;
                ectx->event_u.branch.index = 0;
                ectx->event_u.branch.goto_else = false;
                ectx->event = EXEC_EVENT_BRANCH;
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                struct ctrlframe cframe;
                int ret;

                LOAD_PC;
                uint32_t pc = ptr2pc(vctx->module, p);
                ret = pop_ctrlframe(pc, true, &cframe, vctx);
                if (ret != 0) {
                        return ret;
                }
                ret = push_ctrlframe(pc - 1, FRAME_OP_ELSE, cframe.jumpslot,
                                     cframe.start_types, cframe.end_types,
                                     vctx);
                cframe.start_types = NULL;
                cframe.end_types = NULL;
                ctrlframe_clear(&cframe);
                if (ret != 0) {
                        return ret;
                }
        }
        INSN_SUCCESS_RETURN;
}

INSN_IMPL(end)
{
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                assert(ectx->frames.lsize > 0);
                struct funcframe *frame = &VEC_LASTELEM(ectx->frames);
                xlog_trace("end: nlabels %" PRIu32 " labelidx %" PRIu32,
                           ectx->labels.lsize, frame->labelidx);
                if (ectx->labels.lsize > frame->labelidx) {
                        VEC_POP_DROP(ectx->labels);
                } else {
                        frame_exit(ectx);
                        SAVE_STACK_PTR;
                        rewind_stack(ectx, frame->height, frame->nresults);
                        LOAD_STACK_PTR;
                        RELOAD_PC;
                }
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                struct ctrlframe cframe;
                int ret;

                LOAD_PC;
                uint32_t pc = ptr2pc(vctx->module, p);
                ret = pop_ctrlframe(pc, false, &cframe, vctx);
                if (ret != 0) {
                        return ret;
                }
                if (cframe.op == FRAME_OP_IF) {
                        /* no "else" is same as an empty "else" */
                        xlog_trace("emulating an empty else");
                        ret = push_ctrlframe(pc, FRAME_OP_EMPTY_ELSE, 0,
                                             cframe.start_types,
                                             cframe.end_types, vctx);
                        if (ret == 0) {
                                cframe.start_types = NULL;
                                cframe.end_types = NULL;
                        }
                        ctrlframe_clear(&cframe);
                        if (ret != 0) {
                                return ret;
                        }
                        ret = pop_ctrlframe(pc, false, &cframe, vctx);
                        if (ret != 0) {
                                return ret;
                        }
                }
                assert((cframe.op == FRAME_OP_INVOKE) ==
                       (vctx->ncframes == 0));
                if (cframe.op == FRAME_OP_INVOKE) {
                        ctrlframe_clear(&cframe);
                } else {
                        ret = push_valtypes(cframe.end_types, vctx);
                        ctrlframe_clear(&cframe);
                        if (ret != 0) {
                                return ret;
                        }
                }
        }
        INSN_SUCCESS_RETURN;
}

INSN_IMPL(br)
{
        int ret;

        LOAD_PC;
        READ_LEB_U32(labelidx);
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                ectx->event_u.branch.index = labelidx;
                ectx->event_u.branch.goto_else = false;
                ectx->event = EXEC_EVENT_BRANCH;
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                const struct resulttype *rt;
                ret = target_label_types(vctx, labelidx, &rt);
                if (ret != 0) {
                        goto fail;
                }
                ret = pop_valtypes(rt, vctx);
                if (ret != 0) {
                        goto fail;
                }
                mark_unreachable(vctx);
        }
        SAVE_PC;
        INSN_SUCCESS_RETURN;
fail:
        return ret;
}

INSN_IMPL(br_if)
{
        int ret;

        LOAD_PC;
        READ_LEB_U32(labelidx);
        POP_VAL(TYPE_i32, l);
        if (EXECUTING) {
                if (val_l.u.i32 != 0) {
                        struct exec_context *ectx = ECTX;
                        ectx->event_u.branch.index = labelidx;
                        ectx->event_u.branch.goto_else = false;
                        ectx->event = EXEC_EVENT_BRANCH;
                }
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                const struct resulttype *rt;
                ret = target_label_types(vctx, labelidx, &rt);
                if (ret != 0) {
                        goto fail;
                }
                ret = pop_valtypes(rt, vctx);
                if (ret != 0) {
                        goto fail;
                }
                ret = push_valtypes(rt, vctx);
                if (ret != 0) {
                        goto fail;
                }
        }
        SAVE_PC;
        INSN_SUCCESS_RETURN;
fail:
        return ret;
}

INSN_IMPL(br_table)
{
        uint32_t *table = NULL;
        uint32_t vec_count = 0;
        int ret;
#if defined(__GNUC__) && !defined(__clang__)
        /*
         * GCC w/o optimization produces a "maybe-uninitialized"
         * false-positive here.
         */
        ret = 0;
#endif

        LOAD_PC;
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                vec_count = read_leb_u32_nocheck(&p);
                POP_VAL(TYPE_i32, l);
                /*
                 * Note: as we will jump anyway, we don't bother to
                 * update the instruction pointer (p) precisely here.
                 */
                uint32_t l = val_l.u.i32;
                if (l >= vec_count) {
                        l = vec_count;
                }
                uint32_t idx;
                do {
                        idx = read_leb_u32_nocheck(&p);
                } while (l-- > 0);
                ectx->event_u.branch.index = idx;
                ectx->event_u.branch.goto_else = false;
                ectx->event = EXEC_EVENT_BRANCH;
                SAVE_PC;
                INSN_SUCCESS_RETURN;
        }
        ret = read_vec_u32(&p, ep, &vec_count, &table);
        CHECK_RET(ret);
        READ_LEB_U32(defaultidx);
        POP_VAL(TYPE_i32, l);
        if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                const struct resulttype *rt_default;
                ret = target_label_types(vctx, defaultidx, &rt_default);
                if (ret != 0) {
                        goto fail;
                }
                uint32_t arity = rt_default->ntypes;
                uint32_t i;
                for (i = 0; i < vec_count; i++) {
                        uint32_t idx = table[i];
                        const struct resulttype *rt;
                        ret = target_label_types(vctx, idx, &rt);
                        if (ret != 0) {
                                goto fail;
                        }
                        if (rt->ntypes != arity) {
                                ret = EINVAL;
                                goto fail;
                        }
                        ret = peek_valtypes(rt, vctx);
                        if (ret != 0) {
                                goto fail;
                        }
                }
                ret = pop_valtypes(rt_default, vctx);
                if (ret != 0) {
                        goto fail;
                }
                mark_unreachable(vctx);
        }
        free(table);
        SAVE_PC;
        INSN_SUCCESS_RETURN;
fail:
        free(table);
        return ret;
}

INSN_IMPL(return )
{
        int ret;
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                assert(ectx->frames.lsize > 0);
                const struct funcframe *frame = &VEC_LASTELEM(ectx->frames);
                uint32_t nlabels = ectx->labels.lsize - frame->labelidx;
                xlog_trace("return as tr %" PRIu32, nlabels);
                ectx->event_u.branch.index = nlabels;
                ectx->event_u.branch.goto_else = false;
                ectx->event = EXEC_EVENT_BRANCH;
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                ret = pop_valtypes(returntype(vctx), vctx);
                if (ret != 0) {
                        goto fail;
                }
                mark_unreachable(vctx);
        }
        INSN_SUCCESS_RETURN;
fail:
        return ret;
}

INSN_IMPL(call)
{
        struct module *m;
        int ret;

        LOAD_PC;
        READ_LEB_U32(funcidx);
        m = MODULE;
        CHECK(funcidx < m->nimportedfuncs + m->nfuncs);
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                ectx->event_u.call.func =
                        VEC_ELEM(ectx->instance->funcs, funcidx);
                ectx->event = EXEC_EVENT_CALL;
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                const struct functype *ft = module_functype(m, funcidx);
                ret = pop_valtypes(&ft->parameter, vctx);
                if (ret != 0) {
                        goto fail;
                }
                ret = push_valtypes(&ft->result, vctx);
                if (ret != 0) {
                        goto fail;
                }
        }
        SAVE_PC;
        INSN_SUCCESS_RETURN;
fail:
        return ret;
}

INSN_IMPL(call_indirect)
{
        struct module *m = MODULE;
        int ret;

        LOAD_PC;
        READ_LEB_U32(typeidx);
        READ_LEB_U32(tableidx);
        CHECK(tableidx < m->nimportedtables + m->ntables);
        CHECK(typeidx < m->ntypes);
        const struct tabletype *tab;
        const struct functype *ft;
        if (EXECUTING || VALIDATING) {
                tab = module_tabletype(m, tableidx);
                ft = &m->types[typeidx];
        }
        CHECK(tab->et == TYPE_FUNCREF);
        POP_VAL(TYPE_i32, a);
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                struct instance *inst = ectx->instance;
                const struct tableinst *t = VEC_ELEM(inst->tables, tableidx);
                uint32_t i = val_a.u.i32;
                if (i >= t->size) {
                        ret = trap_with_id(
                                ectx,
                                TRAP_CALL_INDIRECT_OUT_OF_BOUNDS_TABLE_ACCESS,
                                "call_indirect (table idx out of range) "
                                "%" PRIu32 " %" PRIu32 " %" PRIu32,
                                typeidx, tableidx, i);
                        goto fail;
                }
                const struct funcref *ref = &t->vals[i].u.funcref;
                const struct funcinst *func = ref->func;
                if (func == NULL) {
                        ret = trap_with_id(
                                ectx, TRAP_CALL_INDIRECT_NULL_FUNCREF,
                                "call_indirect (null funcref) %" PRIu32
                                " %" PRIu32 " %" PRIu32,
                                typeidx, tableidx, i);
                        goto fail;
                }
                const struct functype *actual_ft = funcinst_functype(func);
                if (compare_functype(ft, actual_ft)) {
                        ret = trap_with_id(
                                ectx, TRAP_CALL_INDIRECT_FUNCTYPE_MISMATCH,
                                "call_indirect (functype mismatch) %" PRIu32
                                " %" PRIu32 " %" PRIu32,
                                typeidx, tableidx, i);
                        goto fail;
                }
                ectx->event_u.call.func = ref->func;
                ectx->event = EXEC_EVENT_CALL;
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                xlog_trace("call_indirect (table %u type %u) %u %u", tableidx,
                           typeidx, ft->parameter.ntypes, ft->result.ntypes);
                ret = pop_valtypes(&ft->parameter, vctx);
                if (ret != 0) {
                        goto fail;
                }
                ret = push_valtypes(&ft->result, vctx);
                if (ret != 0) {
                        goto fail;
                }
        }
        SAVE_PC;
        INSN_SUCCESS_RETURN;
fail:
        return ret;
}

INSN_IMPL(drop)
{
        LOAD_PC;
        int ret;
        if (EXECUTING) {
                uint32_t csz = find_type_annotation(ECTX, p);
                STACK_ADJ(-(int32_t)csz);
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                enum valtype type_a;
                ret = pop_valtype(TYPE_UNKNOWN, &type_a, vctx);
                if (ret != 0) {
                        goto fail;
                }
                ret = record_type_annotation(vctx, p, type_a);
                if (ret != 0) {
                        goto fail;
                }
        }
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(select)
{
        LOAD_PC;
        int ret;
        POP_VAL(TYPE_i32, cond);
        struct val val_c;
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                uint32_t csz = find_type_annotation(ectx, p);
                struct val val_v2;
                pop_val(&val_v2, csz, ectx);
                struct val val_v1;
                pop_val(&val_v1, csz, ectx);
                val_c = val_cond.u.i32 != 0 ? val_v1 : val_v2;
                push_val(&val_c, csz, ectx);
        } else if (VALIDATING) {
                POP_VAL(TYPE_UNKNOWN, v2);
                POP_VAL(type_v2, v1);
                ret = record_type_annotation(VCTX, p, type_v2);
                if (ret != 0) {
                        goto fail;
                }
                CHECK(is_numtype(type_v2) || is_vectype(type_v2) ||
                      type_v2 == TYPE_UNKNOWN);
                PUSH_VAL(type_v2, c);
        }
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(select_t)
{
        int ret;
        LOAD_PC;
        READ_LEB_U32(vec_count);
        CHECK(vec_count == 1);
        uint8_t u8;
        ret = read_u8(&p, ep, &u8);
        CHECK_RET(ret);
        enum valtype t = u8;
        CHECK(is_valtype(t));
        POP_VAL(TYPE_i32, cond);
        POP_VAL(t, v2);
        POP_VAL(t, v1);
        struct val val_c;
        if (EXECUTING) {
                val_c = val_cond.u.i32 != 0 ? val_v1 : val_v2;
        }
        PUSH_VAL(t, c);
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(local_get)
{
        int ret;

        LOAD_PC;
        READ_LEB_U32(localidx);
        struct val val_c;
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                uint32_t csz;
                local_get(ectx, localidx, STACK, &csz);
                STACK_ADJ(csz);
        } else if (VALIDATING) {
                CHECK(localidx < VCTX->nlocals);
                PUSH_VAL(VCTX->locals[localidx], c);
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(local_set)
{
        int ret;

        LOAD_PC;
        READ_LEB_U32(localidx);
        if (VALIDATING) {
                CHECK(localidx < VCTX->nlocals);
        }
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                uint32_t csz;
                local_set(ectx, localidx, STACK, &csz);
                STACK_ADJ(-(int32_t)csz);
        } else {
                POP_VAL(VCTX->locals[localidx], a);
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(local_tee)
{
        int ret;

        LOAD_PC;
        READ_LEB_U32(localidx);
        if (VALIDATING) {
                CHECK(localidx < VCTX->nlocals);
        }
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                uint32_t csz;
                local_set(ectx, localidx, STACK, &csz);
        } else {
                POP_VAL(VCTX->locals[localidx], a);
                PUSH_VAL(VCTX->locals[localidx], a);
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(global_get)
{
        struct module *m = MODULE;
        int ret;

        LOAD_PC;
        READ_LEB_U32(globalidx);
        CHECK(globalidx < m->nimportedglobals + m->nglobals);
        struct val val_c;
        if (EXECUTING) {
                val_c = VEC_ELEM(ECTX->instance->globals, globalidx)->val;
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                if (vctx->const_expr) {
                        const struct globaltype *t =
                                module_globaltype(m, globalidx);
                        if (t->mut != GLOBAL_CONST) {
                                ret = EINVAL;
                                goto fail;
                        }
                }
        }
        PUSH_VAL(module_globaltype(m, globalidx)->t, c);
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(global_set)
{
        struct module *m = MODULE;
        int ret;

        LOAD_PC;
        READ_LEB_U32(globalidx);
        CHECK(globalidx < m->nimportedglobals + m->nglobals);
        const struct globaltype *gt;
        if (EXECUTING || VALIDATING) {
                gt = module_globaltype(m, globalidx);
                CHECK(gt->mut != GLOBAL_CONST);
        }
        POP_VAL(gt->t, a);
        if (EXECUTING) {
                VEC_ELEM(ECTX->instance->globals, globalidx)->val = val_a;
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(table_get)
{
        struct module *m = MODULE;
        int ret;

        LOAD_PC;
        READ_LEB_U32(tableidx);
        CHECK(tableidx < m->nimportedtables + m->ntables);
        POP_VAL(TYPE_i32, offset);
        struct val val_c;
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                uint32_t offset = val_offset.u.i32;
                ret = table_access(ectx, tableidx, offset, 1);
                if (ret != 0) {
                        goto fail;
                }
                struct instance *inst = ectx->instance;
                struct tableinst *t = VEC_ELEM(inst->tables, tableidx);
                val_c = t->vals[offset];
        }
        PUSH_VAL(module_tabletype(m, tableidx)->et, c);
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(table_set)
{
        struct module *m = MODULE;
        int ret;

        LOAD_PC;
        READ_LEB_U32(tableidx);
        CHECK(tableidx < m->nimportedtables + m->ntables);
        POP_VAL(module_tabletype(m, tableidx)->et, a);
        POP_VAL(TYPE_i32, offset);
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                uint32_t offset = val_offset.u.i32;
                ret = table_access(ectx, tableidx, offset, 1);
                if (ret != 0) {
                        goto fail;
                }
                struct instance *inst = ectx->instance;
                struct tableinst *t = VEC_ELEM(inst->tables, tableidx);
                t->vals[offset] = val_a;
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

STOREOP(i32_store, 32, 32, )
STOREOP(i64_store, 64, 64, )
STOREOP_F(f32_store, 32, 32, )
STOREOP_F(f64_store, 64, 64, )
STOREOP(i32_store8, 8, 32, )
STOREOP(i32_store16, 16, 32, )
STOREOP(i64_store8, 8, 64, )
STOREOP(i64_store16, 16, 64, )
STOREOP(i64_store32, 32, 64, )

INSN_IMPL(memory_size)
{
        uint8_t zero;
        int ret;
        LOAD_PC;
        ret = read_u8(&p, ep, &zero);
        CHECK_RET(ret);
        CHECK(zero == 0);
        uint32_t memidx = 0;
        struct module *m = MODULE;
        CHECK(memidx < m->nimportedmems + m->nmems);
        struct val val_sz;
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                struct instance *inst = ectx->instance;
                struct meminst *minst = VEC_ELEM(inst->mems, memidx);
                val_sz.u.i32 = minst->size_in_pages;
        }
        PUSH_VAL(TYPE_i32, sz);
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(memory_grow)
{
        uint8_t zero;
        int ret;
        LOAD_PC;
        ret = read_u8(&p, ep, &zero);
        CHECK_RET(ret);
        CHECK(zero == 0);
        uint32_t memidx = 0;
        struct module *m = MODULE;
        CHECK(memidx < m->nimportedmems + m->nmems);
        POP_VAL(TYPE_i32, n);
        struct val val_error;
        if (EXECUTING) {
                val_error.u.i32 = memory_grow(ECTX, memidx, val_n.u.i32);
        }
        PUSH_VAL(TYPE_i32, error);
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

LOADOP(i32_load, 32, 32, )
LOADOP(i64_load, 64, 64, )
LOADOP_F(f32_load, 32, 32, )
LOADOP_F(f64_load, 64, 64, )

LOADOP(i32_load8_s, 8, 32, (int32_t)(int8_t))
LOADOP(i32_load8_u, 8, 32, (uint32_t)(uint8_t))
LOADOP(i32_load16_s, 16, 32, (int32_t)(int16_t))
LOADOP(i32_load16_u, 16, 32, (uint32_t)(uint16_t))

LOADOP(i64_load8_s, 8, 64, (int64_t)(int8_t))
LOADOP(i64_load8_u, 8, 64, (uint64_t)(uint8_t))
LOADOP(i64_load16_s, 16, 64, (int64_t)(int16_t))
LOADOP(i64_load16_u, 16, 64, (uint64_t)(uint16_t))
LOADOP(i64_load32_s, 32, 64, (int64_t)(int32_t))
LOADOP(i64_load32_u, 32, 64, (uint64_t)(uint32_t))

CONSTOP(i32_const, 32, i32)
CONSTOP(i64_const, 64, i64)
CONSTOP_F(f32_const, 32, f32)
CONSTOP_F(f64_const, 64, f64)

TESTOP(i32_eqz, i, 32, EQZ)
CMPOP(i32_eq, 32, uint32_t, ==)
CMPOP(i32_ne, 32, uint32_t, !=)
CMPOP(i32_lt_s, 32, int32_t, <)
CMPOP(i32_lt_u, 32, uint32_t, <)
CMPOP(i32_gt_s, 32, int32_t, >)
CMPOP(i32_gt_u, 32, uint32_t, >)
CMPOP(i32_le_s, 32, int32_t, <=)
CMPOP(i32_le_u, 32, uint32_t, <=)
CMPOP(i32_ge_s, 32, int32_t, >=)
CMPOP(i32_ge_u, 32, uint32_t, >=)

CMPOP_F(f32_eq, 32, ==)
CMPOP_F(f32_ne, 32, !=)
CMPOP_F(f32_lt, 32, <)
CMPOP_F(f32_gt, 32, >)
CMPOP_F(f32_le, 32, <=)
CMPOP_F(f32_ge, 32, >=)

CMPOP_F(f64_eq, 64, ==)
CMPOP_F(f64_ne, 64, !=)
CMPOP_F(f64_lt, 64, <)
CMPOP_F(f64_gt, 64, >)
CMPOP_F(f64_le, 64, <=)
CMPOP_F(f64_ge, 64, >=)

TESTOP(i64_eqz, i, 64, EQZ)
CMPOP(i64_eq, 64, uint64_t, ==)
CMPOP(i64_ne, 64, uint64_t, !=)
CMPOP(i64_lt_s, 64, int64_t, <)
CMPOP(i64_lt_u, 64, uint64_t, <)
CMPOP(i64_gt_s, 64, int64_t, >)
CMPOP(i64_gt_u, 64, uint64_t, >)
CMPOP(i64_le_s, 64, int64_t, <=)
CMPOP(i64_le_u, 64, uint64_t, <=)
CMPOP(i64_ge_s, 64, int64_t, >=)
CMPOP(i64_ge_u, 64, uint64_t, >=)

BITCOUNTOP(i32_clz, i32, clz)
BITCOUNTOP(i32_ctz, i32, ctz)
BITCOUNTOP(i32_popcnt, i32, popcount)

BINOP(i32_add, i, 32, ADD)
BINOP(i32_sub, i, 32, SUB)
BINOP(i32_mul, i, 32, MUL)

BINOP2(i32_div_u, i, 32, DIV_U, true, false, true)
BINOP2(i32_div_s, i, 32, DIV_S, true, true, true)
BINOP2(i32_rem_u, i, 32, REM_U, true, false, false)
BINOP2(i32_rem_s, i, 32, REM_S, true, true, false)

BINOP(i32_and, i, 32, AND)
BINOP(i32_or, i, 32, OR)
BINOP(i32_xor, i, 32, XOR)

BINOP(i32_shl, i, 32, SHL)
BINOP(i32_shr_s, i, 32, SHR_S)
BINOP(i32_shr_u, i, 32, SHR_U)
BINOP(i32_rotl, i, 32, ROTL)
BINOP(i32_rotr, i, 32, ROTR)

BITCOUNTOP(i64_clz, i64, clz64)
BITCOUNTOP(i64_ctz, i64, ctz64)
BITCOUNTOP(i64_popcnt, i64, popcount64)

BINOP(i64_add, i, 64, ADD)
BINOP(i64_sub, i, 64, SUB)
BINOP(i64_mul, i, 64, MUL)

BINOP2(i64_div_u, i, 64, DIV_U, true, false, true)
BINOP2(i64_div_s, i, 64, DIV_S, true, true, true)
BINOP2(i64_rem_u, i, 64, REM_U, true, false, false)
BINOP2(i64_rem_s, i, 64, REM_S, true, true, false)

BINOP(i64_and, i, 64, AND)
BINOP(i64_or, i, 64, OR)
BINOP(i64_xor, i, 64, XOR)

BINOP(i64_shl, i, 64, SHL)
BINOP(i64_shr_s, i, 64, SHR_S)
BINOP(i64_shr_u, i, 64, SHR_U)
BINOP(i64_rotl, i, 64, ROTL)
BINOP(i64_rotr, i, 64, ROTR)

UNOP(f32_abs, f, 32, fabsf)
UNOP(f32_neg, f, 32, -)
UNOP(f32_ceil, f, 32, ceilf)
UNOP(f32_floor, f, 32, floorf)
UNOP(f32_trunc, f, 32, truncf)
UNOP(f32_nearest, f, 32, rintf)
UNOP(f32_sqrt, f, 32, sqrtf)

BINOP(f32_add, f, 32, ADD);
BINOP(f32_sub, f, 32, SUB);
BINOP(f32_mul, f, 32, MUL);
BINOP(f32_div, f, 32, FDIV);
BINOP(f32_max, f, 32, FMAX32);
BINOP(f32_min, f, 32, FMIN32);
BINOP(f32_copysign, f, 32, FCOPYSIGN32);

UNOP(f64_abs, f, 64, fabs)
UNOP(f64_neg, f, 64, -)
UNOP(f64_ceil, f, 64, ceil)
UNOP(f64_floor, f, 64, floor)
UNOP(f64_trunc, f, 64, trunc)
UNOP(f64_nearest, f, 64, rint)
UNOP(f64_sqrt, f, 64, sqrt)

BINOP(f64_add, f, 64, ADD);
BINOP(f64_sub, f, 64, SUB);
BINOP(f64_mul, f, 64, MUL);
BINOP(f64_div, f, 64, FDIV);
BINOP(f64_max, f, 64, FMAX64);
BINOP(f64_min, f, 64, FMIN64);
BINOP(f64_copysign, f, 64, FCOPYSIGN64);

EXTENDOP(i32_wrap_i64, i64, i32, (uint32_t))

CVTOP(i32_trunc_f32_s, f32, i32, TRUNC_S_32_32)
CVTOP(i32_trunc_f32_u, f32, i32, TRUNC_U_32_32)
CVTOP(i32_trunc_f64_s, f64, i32, TRUNC_S_64_32)
CVTOP(i32_trunc_f64_u, f64, i32, TRUNC_U_64_32)

EXTENDOP(i64_extend_i32_s, i32, i64, (int64_t)(int32_t))
EXTENDOP(i64_extend_i32_u, i32, i64, (uint64_t)(uint32_t))

CVTOP(i64_trunc_f32_s, f32, i64, TRUNC_S_32_64)
CVTOP(i64_trunc_f32_u, f32, i64, TRUNC_U_32_64)
CVTOP(i64_trunc_f64_s, f64, i64, TRUNC_S_64_64)
CVTOP(i64_trunc_f64_u, f64, i64, TRUNC_U_64_64)

EXTENDOP(f32_convert_i32_s, i32, f32, (float)(int32_t))
EXTENDOP(f32_convert_i32_u, i32, f32, (float)(uint32_t))
EXTENDOP(f32_convert_i64_u, i64, f32, (float)(uint64_t))
EXTENDOP(f32_convert_i64_s, i64, f32, (float)(int64_t))
EXTENDOP(f32_demote_f64, f64, f32, (float))

EXTENDOP(f64_convert_i32_s, i32, f64, (double)(int32_t))
EXTENDOP(f64_convert_i32_u, i32, f64, (double)(uint32_t))
EXTENDOP(f64_convert_i64_u, i64, f64, (double)(uint64_t))
EXTENDOP(f64_convert_i64_s, i64, f64, (double)(int64_t))
EXTENDOP(f64_promote_f32, f32, f64, (double))

REINTERPRETOP(i32_reinterpret_f32, f32, i32)
REINTERPRETOP(i64_reinterpret_f64, f64, i64)
REINTERPRETOP(f32_reinterpret_i32, i32, f32)
REINTERPRETOP(f64_reinterpret_i64, i64, f64)

EXTENDOP(i32_extend8_s, i32, i32, (int32_t)(int8_t))
EXTENDOP(i32_extend16_s, i32, i32, (int32_t)(int16_t))
EXTENDOP(i64_extend8_s, i64, i64, (int64_t)(int8_t))
EXTENDOP(i64_extend16_s, i64, i64, (int64_t)(int16_t))
EXTENDOP(i64_extend32_s, i64, i64, (int64_t)(int32_t))

CVTOP(i32_trunc_sat_f32_s, f32, i32, TRUNC_SAT_S_32_32)
CVTOP(i32_trunc_sat_f32_u, f32, i32, TRUNC_SAT_U_32_32)
CVTOP(i32_trunc_sat_f64_s, f64, i32, TRUNC_SAT_S_64_32)
CVTOP(i32_trunc_sat_f64_u, f64, i32, TRUNC_SAT_U_64_32)

CVTOP(i64_trunc_sat_f32_s, f32, i64, TRUNC_SAT_S_32_64)
CVTOP(i64_trunc_sat_f32_u, f32, i64, TRUNC_SAT_U_32_64)
CVTOP(i64_trunc_sat_f64_s, f64, i64, TRUNC_SAT_S_64_64)
CVTOP(i64_trunc_sat_f64_u, f64, i64, TRUNC_SAT_U_64_64)

INSN_IMPL(ref_null)
{
        int ret;
        LOAD_PC;
        uint8_t u8;
        ret = read_u8(&p, ep, &u8);
        CHECK_RET(ret);
        enum valtype type = u8;
        CHECK(is_reftype(type));
        struct val val_null;
        if (EXECUTING) {
                memset(&val_null, 0, sizeof(val_null));
        }
        PUSH_VAL(type, null);
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(ref_is_null)
{
        /*
         * REVISIT: is this a correct interpretation of the spec
         * to treat ref.is_null as a value-polymorphic instruction?
         *
         * Subtyping and "anyref" has some back-and-forth history:
         * https://github.com/WebAssembly/reference-types/pull/43
         * https://github.com/WebAssembly/reference-types/pull/87
         * https://github.com/WebAssembly/reference-types/pull/100
         * https://github.com/WebAssembly/reference-types/pull/116
         */
        int ret;
        LOAD_PC;
        struct val val_result;
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                uint32_t csz = find_type_annotation(ectx, p);
                struct val val_n;
                pop_val(&val_n, csz, ectx);
                val_result.u.i32 = (int)(val_n.u.funcref.func == NULL);
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                enum valtype type_n;
                ret = pop_valtype(TYPE_ANYREF, &type_n, vctx);
                if (ret != 0) {
                        goto fail;
                }
                ret = record_type_annotation(VCTX, p, type_n);
                if (ret != 0) {
                        goto fail;
                }
        }
        PUSH_VAL(TYPE_i32, result);
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(ref_func)
{
        int ret;
        LOAD_PC;
        READ_LEB_U32(funcidx);
        struct module *m = MODULE;
        CHECK(funcidx < m->nimportedfuncs + m->nfuncs);
        struct val val_result;
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                val_result.u.funcref.func =
                        VEC_ELEM(ectx->instance->funcs, funcidx);
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                /*
                 * REVISIT: a bit abuse of const_expr.
                 * what we want to check here is if it's in the code
                 * section or not.
                 */
                if (vctx->const_expr) {
                        bitmap_set(vctx->refs, funcidx);
                } else {
                        if (!bitmap_test(vctx->refs, funcidx)) {
                                ret = validation_failure(
                                        vctx, "funcref %" PRIu32, funcidx);
                                goto fail;
                        }
                }
        }
        PUSH_VAL(TYPE_FUNCREF, result);
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(memory_init)
{
        int ret;
        LOAD_PC;
        uint32_t memidx = 0;
        READ_LEB_U32(dataidx);
        uint8_t zero;
        ret = read_u8(&p, ep, &zero);
        CHECK_RET(ret);
        CHECK(zero == 0);
        struct module *m = MODULE;
        CHECK(memidx < m->nimportedmems + m->nmems);
        POP_VAL(TYPE_i32, n);
        POP_VAL(TYPE_i32, s);
        POP_VAL(TYPE_i32, d);
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                uint32_t d = val_d.u.i32;
                uint32_t s = val_s.u.i32;
                uint32_t n = val_n.u.i32;
                ret = memory_init(ectx, memidx, dataidx, d, s, n);
                if (ret != 0) {
                        goto fail;
                }
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                CHECK(vctx->has_datacount);
                CHECK(dataidx < vctx->ndatas_in_datacount);
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(data_drop)
{
        int ret;
        LOAD_PC;
        READ_LEB_U32(dataidx);
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                data_drop(ectx, dataidx);
        } else if (VALIDATING) {
                struct validation_context *vctx = VCTX;
                CHECK(vctx->has_datacount);
                CHECK(dataidx < vctx->ndatas_in_datacount);
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(memory_copy)
{
        int ret;
        LOAD_PC;
        uint32_t memidx = 0;
        uint8_t zero;
        ret = read_u8(&p, ep, &zero);
        CHECK_RET(ret);
        CHECK(zero == 0);
        ret = read_u8(&p, ep, &zero);
        CHECK_RET(ret);
        CHECK(zero == 0);
        struct module *m = MODULE;
        CHECK(memidx < m->nimportedmems + m->nmems);
        POP_VAL(TYPE_i32, n);
        POP_VAL(TYPE_i32, s);
        POP_VAL(TYPE_i32, d);
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                uint32_t n = val_n.u.i32;
                void *src_p;
                void *dst_p;
                bool moved;
retry:
                ret = memory_getptr(ectx, memidx, val_s.u.i32, 0, n, &src_p);
                if (ret != 0) {
                        goto fail;
                }
                moved = false;
                ret = memory_getptr2(ectx, memidx, val_d.u.i32, 0, n, &dst_p,
                                     &moved);
                if (ret != 0) {
                        goto fail;
                }
                if (moved) {
                        goto retry;
                }
                memmove(dst_p, src_p, n);
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(memory_fill)
{
        int ret;
        LOAD_PC;
        uint32_t memidx = 0;
        uint8_t zero;
        ret = read_u8(&p, ep, &zero);
        CHECK_RET(ret);
        CHECK(zero == 0);
        struct module *m = MODULE;
        CHECK(memidx < m->nimportedmems + m->nmems);
        POP_VAL(TYPE_i32, n);
        POP_VAL(TYPE_i32, val);
        POP_VAL(TYPE_i32, d);
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                void *p;
                uint32_t n = val_n.u.i32;
                ret = memory_getptr(ectx, memidx, val_d.u.i32, 0, n, &p);
                if (ret != 0) {
                        goto fail;
                }
                memset(p, (uint8_t)val_val.u.i32, n);
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(table_init)
{
        int ret;
        LOAD_PC;
        READ_LEB_U32(elemidx);
        READ_LEB_U32(tableidx);
        struct module *m = MODULE;
        CHECK(elemidx < m->nelems);
        CHECK(tableidx < m->nimportedtables + m->ntables);
        CHECK(m->tables[tableidx].et == m->elems[elemidx].type);
        POP_VAL(TYPE_i32, n);
        POP_VAL(TYPE_i32, s);
        POP_VAL(TYPE_i32, d);
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                uint32_t d = val_d.u.i32;
                uint32_t s = val_s.u.i32;
                uint32_t n = val_n.u.i32;
                ret = table_init(ectx, tableidx, elemidx, d, s, n);
                if (ret != 0) {
                        goto fail;
                }
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(elem_drop)
{
        int ret;
        LOAD_PC;
        READ_LEB_U32(elemidx);
        struct module *m = MODULE;
        CHECK(elemidx < m->nelems);
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                elem_drop(ectx, elemidx);
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(table_copy)
{
        int ret;
        LOAD_PC;
        READ_LEB_U32(tableidx_dst);
        READ_LEB_U32(tableidx_src);
        struct module *m = MODULE;
        CHECK(tableidx_dst < m->nimportedtables + m->ntables);
        CHECK(tableidx_src < m->nimportedtables + m->ntables);
        CHECK(m->tables[tableidx_dst].et == m->tables[tableidx_src].et);
        POP_VAL(TYPE_i32, n);
        POP_VAL(TYPE_i32, s);
        POP_VAL(TYPE_i32, d);
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                struct instance *inst = ectx->instance;
                uint32_t d = val_d.u.i32;
                uint32_t s = val_s.u.i32;
                uint32_t n = val_n.u.i32;
                ret = table_access(ectx, tableidx_dst, d, n);
                if (ret != 0) {
                        goto fail;
                }
                ret = table_access(ectx, tableidx_src, s, n);
                if (ret != 0) {
                        goto fail;
                }
                memmove(&VEC_ELEM(inst->tables, tableidx_dst)->vals[d],
                        &VEC_ELEM(inst->tables, tableidx_src)->vals[s],
                        n * sizeof(struct val));
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(table_grow)
{
        int ret;
        LOAD_PC;
        READ_LEB_U32(tableidx);
        struct module *m = MODULE;
        CHECK(tableidx < m->nimportedtables + m->ntables);
        POP_VAL(TYPE_i32, n);
        POP_VAL(module_tabletype(m, tableidx)->et, val);
        struct val val_result;
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                struct instance *inst = ectx->instance;
                struct tableinst *t = VEC_ELEM(inst->tables, tableidx);
                uint32_t n = val_n.u.i32;
                if (UINT32_MAX - t->size < n ||
                    t->size + n > t->type->lim.max) {
                        val_result.u.i32 = (uint32_t)-1;
                } else {
                        uint32_t newsize = t->size + n;
                        ret = ARRAY_RESIZE(t->vals, newsize);
                        if (ret != 0) {
                                val_result.u.i32 = (uint32_t)-1;
                        } else {
                                uint32_t i;
                                for (i = t->size; i < newsize; i++) {
                                        t->vals[i] = val_val;
                                }
                                val_result.u.i32 = t->size;
                                t->size = newsize;
                        }
                }
        }
        PUSH_VAL(TYPE_i32, result);
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(table_size)
{
        int ret;
        LOAD_PC;
        READ_LEB_U32(tableidx);
        struct module *m = MODULE;
        CHECK(tableidx < m->nimportedtables + m->ntables);
        struct val val_n;
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                struct instance *inst = ectx->instance;
                struct tableinst *t = VEC_ELEM(inst->tables, tableidx);
                val_n.u.i32 = t->size;
        }
        PUSH_VAL(TYPE_i32, n);
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}

INSN_IMPL(table_fill)
{
        int ret;
        LOAD_PC;
        READ_LEB_U32(tableidx);
        struct module *m = MODULE;
        CHECK(tableidx < m->nimportedtables + m->ntables);
        POP_VAL(TYPE_i32, n);
        POP_VAL(module_tabletype(m, tableidx)->et, val);
        POP_VAL(TYPE_i32, i);
        if (EXECUTING) {
                struct exec_context *ectx = ECTX;
                uint32_t start = val_i.u.i32;
                uint32_t n = val_n.u.i32;
                ret = table_access(ectx, tableidx, start, n);
                if (ret != 0) {
                        goto fail;
                }
                struct instance *inst = ectx->instance;
                struct tableinst *t = VEC_ELEM(inst->tables, tableidx);
                uint32_t end = start + n;
                uint32_t i;
                for (i = start; i < end; i++) {
                        t->vals[i] = val_val;
                }
        }
        SAVE_PC;
        INSN_SUCCESS;
fail:
        return ret;
}
