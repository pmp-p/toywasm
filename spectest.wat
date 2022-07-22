(; this aims to mimic:
https://github.com/WebAssembly/spec/blob/d48af683f5e6d00c13f775ab07d29a15daf92203/test/harness/sync_index.js#L98 ;)
(module
    (func (export "print_i32") (param i32))
    (func (export "print_i32_f32") (param i32 f32))
    (func (export "print_f64_f64") (param f64 f64))
    (func (export "print_f32") (param f32))
    (func (export "print_f64") (param f64))
    (global (export "global_i32") i32 (i32.const 666))
    (global (export "global_f32") f32 (f32.const 666))
    (global (export "global_f64") f64 (f64.const 666))
    (table (export "table") 10 20 funcref)
    (memory (export "memory") 1 2)
)
