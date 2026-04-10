/**
 * nutjob_compositor.h — C wrapper for the Clojure compositor native library.
 *
 * Manages the GraalVM isolate lifecycle and provides a simple API
 * for WebKit to call into the compositor.
 *
 * Activated by NUTJOB_COMPOSITOR=1 environment variable.
 */

#ifndef NUTJOB_COMPOSITOR_H
#define NUTJOB_COMPOSITOR_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* GraalVM isolate types — forward declared to avoid header dependency */
typedef struct graal_isolate_t graal_isolate_t;
typedef struct graal_isolatethread_t graal_isolatethread_t;

#include <dlfcn.h>

/* Function pointer types */
typedef int (*fn_graal_create_isolate)(void*, graal_isolate_t**, graal_isolatethread_t**);
typedef void (*fn_njc_void_ii)(graal_isolatethread_t*, int, int);
typedef void (*fn_njc_void_iiii)(graal_isolatethread_t*, int, int, int, int);
typedef void (*fn_njc_void_li)(graal_isolatethread_t*, long, int);
typedef void (*fn_njc_void_l)(graal_isolatethread_t*, long);
typedef void (*fn_njc_void_lfff)(graal_isolatethread_t*, long, float, float, float);
typedef void (*fn_njc_void_lffff)(graal_isolatethread_t*, long, float, float, float, float);
typedef void (*fn_njc_void_lf)(graal_isolatethread_t*, long, float);
typedef void (*fn_njc_void_lipi)(graal_isolatethread_t*, long, int*, int);
typedef void (*fn_njc_void_lipii)(graal_isolatethread_t*, long, int*, int, int);
typedef void (*fn_njc_trace_contents)(graal_isolatethread_t*, long, int*, int, int, int, int, int, int, float, long, int);
typedef void (*fn_njc_void_t)(graal_isolatethread_t*);
typedef void (*fn_njc_void_tpii)(graal_isolatethread_t*, int*, int, int);
typedef int (*fn_njc_int_t)(graal_isolatethread_t*);

/* Singleton state — all in one struct, accessed via inline function */
struct NJCState {
    graal_isolate_t* isolate;
    graal_isolatethread_t* thread;
    bool active;
    bool checked;
    bool initialized;
    void* lib;

    fn_njc_void_ii init;
    fn_njc_void_iiii transaction_begin;
    fn_njc_void_t transaction_end;
    fn_njc_void_li layer_create;
    fn_njc_void_l layer_destroy;
    fn_njc_void_lfff set_position;
    fn_njc_void_lfff set_anchor_point;
    fn_njc_void_lffff set_bounds;
    fn_njc_void_lf set_opacity;
    fn_njc_void_li set_hidden;
    fn_njc_void_li set_masks_to_bounds;
    fn_njc_void_lipi set_children;
    fn_njc_void_lf set_contents_scale;
    fn_njc_void_li set_geometry_flipped;
    fn_njc_void_li set_background_color;
    fn_njc_void_li set_blend_mode;
    fn_njc_void_lipii set_contents;
    fn_njc_trace_contents set_contents_with_metadata;
    fn_njc_void_l set_root_layer;
    fn_njc_void_lipii async_layer_contents;
    fn_njc_void_t commit;
    fn_njc_void_tpii get_pixels;
    fn_njc_int_t layer_count;
    fn_njc_void_t dump_layers;
};

inline struct NJCState* njc_state(void) {
    static struct NJCState s = {0};
    return &s;
}

#define S njc_state()

static inline bool njc_is_active(void) {
    if (!S->checked) {
        S->checked = true;
        const char* val = getenv("NUTJOB_COMPOSITOR");
        if (!val || val[0] != '1')
            return false;

        const char* libPath = getenv("NUTJOB_COMPOSITOR_LIB");
        if (!libPath)
            libPath = "libnutjobcompositor.dylib";

        S->lib = dlopen(libPath, RTLD_NOW);
        if (!S->lib)
            return false;

        auto fn_create_isolate = (fn_graal_create_isolate)dlsym(S->lib, "graal_create_isolate");
        if (!fn_create_isolate || fn_create_isolate(NULL, &S->isolate, &S->thread) != 0) {
            dlclose(S->lib);
            S->lib = NULL;
            return false;
        }

        /* Resolve all function pointers */
        S->init = (fn_njc_void_ii)dlsym(S->lib, "nutjob_compositor_init");
        S->transaction_begin = (fn_njc_void_iiii)dlsym(S->lib, "nutjob_transaction_begin");
        S->transaction_end = (fn_njc_void_t)dlsym(S->lib, "nutjob_transaction_end");
        S->layer_create = (fn_njc_void_li)dlsym(S->lib, "nutjob_layer_create");
        S->layer_destroy = (fn_njc_void_l)dlsym(S->lib, "nutjob_layer_destroy");
        S->set_position = (fn_njc_void_lfff)dlsym(S->lib, "nutjob_layer_set_position");
        S->set_anchor_point = (fn_njc_void_lfff)dlsym(S->lib, "nutjob_layer_set_anchor_point");
        S->set_bounds = (fn_njc_void_lffff)dlsym(S->lib, "nutjob_layer_set_bounds");
        S->set_opacity = (fn_njc_void_lf)dlsym(S->lib, "nutjob_layer_set_opacity");
        S->set_hidden = (fn_njc_void_li)dlsym(S->lib, "nutjob_layer_set_hidden");
        S->set_masks_to_bounds = (fn_njc_void_li)dlsym(S->lib, "nutjob_layer_set_masks_to_bounds");
        S->set_children = (fn_njc_void_lipi)dlsym(S->lib, "nutjob_layer_set_children");
        S->set_contents_scale = (fn_njc_void_lf)dlsym(S->lib, "nutjob_layer_set_contents_scale");
        S->set_geometry_flipped = (fn_njc_void_li)dlsym(S->lib, "nutjob_layer_set_geometry_flipped");
        S->set_background_color = (fn_njc_void_li)dlsym(S->lib, "nutjob_layer_set_background_color");
        S->set_blend_mode = (fn_njc_void_li)dlsym(S->lib, "nutjob_layer_set_blend_mode");
        S->set_contents = (fn_njc_void_lipii)dlsym(S->lib, "nutjob_layer_set_contents");
        S->set_contents_with_metadata = (fn_njc_trace_contents)dlsym(S->lib, "nutjob_layer_set_contents_with_metadata");
        S->set_root_layer = (fn_njc_void_l)dlsym(S->lib, "nutjob_set_root_layer");
        S->async_layer_contents = (fn_njc_void_lipii)dlsym(S->lib, "nutjob_async_layer_contents");
        if (!S->async_layer_contents)
            S->async_layer_contents = S->set_contents; /* fallback */
        S->commit = (fn_njc_void_t)dlsym(S->lib, "nutjob_compositor_commit");
        S->get_pixels = (fn_njc_void_tpii)dlsym(S->lib, "nutjob_compositor_get_pixels");
        S->layer_count = (fn_njc_int_t)dlsym(S->lib, "nutjob_compositor_layer_count");
        S->dump_layers = (fn_njc_void_t)dlsym(S->lib, "nutjob_compositor_dump_layers");

        S->active = true;
    }
    return S->active;
}

static inline graal_isolatethread_t* njc_thread(void) {
    return S->thread;
}

static inline void nutjob_compositor_ensure_initialized(graal_isolatethread_t* t, int w, int h) {
    if (!njc_is_active() || S->initialized)
        return;
    if (S->init)
        S->init(t, w, h);
    S->initialized = true;
}

static inline long njc_layer_id(uint64_t webkitLayerId) {
    return (long)webkitLayerId;
}

/* --- Convenience wrappers that null-check function pointers --- */
#define NJC_CALL(fn, ...) do { if (S->fn) S->fn(__VA_ARGS__); } while(0)

static inline void nutjob_compositor_init(graal_isolatethread_t* t, int w, int h) { NJC_CALL(init, t, w, h); }
static inline void nutjob_compositor_transaction_begin(graal_isolatethread_t* t, int tc, int cr, int ch, int de) { NJC_CALL(transaction_begin, t, tc, cr, ch, de); }
static inline void nutjob_compositor_transaction_end(graal_isolatethread_t* t) {
    if (S->transaction_end) {
        S->transaction_end(t);
        return;
    }
    NJC_CALL(commit, t);
    NJC_CALL(dump_layers, t);
}
static inline void nutjob_layer_create(graal_isolatethread_t* t, long id, int type) { NJC_CALL(layer_create, t, id, type); }
static inline void nutjob_layer_destroy(graal_isolatethread_t* t, long id) { NJC_CALL(layer_destroy, t, id); }
static inline void nutjob_layer_set_position(graal_isolatethread_t* t, long id, float x, float y, float z) { NJC_CALL(set_position, t, id, x, y, z); }
static inline void nutjob_layer_set_anchor_point(graal_isolatethread_t* t, long id, float x, float y, float z) { NJC_CALL(set_anchor_point, t, id, x, y, z); }
static inline void nutjob_layer_set_bounds(graal_isolatethread_t* t, long id, float x, float y, float w, float h) { NJC_CALL(set_bounds, t, id, x, y, w, h); }
static inline void nutjob_layer_set_opacity(graal_isolatethread_t* t, long id, float o) { NJC_CALL(set_opacity, t, id, o); }
static inline void nutjob_layer_set_hidden(graal_isolatethread_t* t, long id, int h) { NJC_CALL(set_hidden, t, id, h); }
static inline void nutjob_layer_set_masks_to_bounds(graal_isolatethread_t* t, long id, int m) { NJC_CALL(set_masks_to_bounds, t, id, m); }
static inline void nutjob_layer_set_children(graal_isolatethread_t* t, long id, int* c, int n) { NJC_CALL(set_children, t, id, c, n); }
static inline void nutjob_layer_set_contents_scale(graal_isolatethread_t* t, long id, float s) { NJC_CALL(set_contents_scale, t, id, s); }
static inline void nutjob_layer_set_geometry_flipped(graal_isolatethread_t* t, long id, int f) { NJC_CALL(set_geometry_flipped, t, id, f); }
static inline void nutjob_layer_set_background_color(graal_isolatethread_t* t, long id, int c) { NJC_CALL(set_background_color, t, id, c); }
static inline void nutjob_layer_set_blend_mode(graal_isolatethread_t* t, long id, int m) { NJC_CALL(set_blend_mode, t, id, m); }
static inline void nutjob_layer_set_contents(graal_isolatethread_t* t, long id, int* p, int w, int h) { NJC_CALL(set_contents, t, id, p, w, h); }
static inline void nutjob_layer_set_contents_with_metadata(graal_isolatethread_t* t, long id, int* p, int w, int h, int path, int contentsAreFlipped, int geometryFlipped, int normalization, float contentsScale, long changedMask, int sourceHashBeforeNormalization)
{
    if (S->set_contents_with_metadata) {
        S->set_contents_with_metadata(t, id, p, w, h, path, contentsAreFlipped, geometryFlipped, normalization, contentsScale, changedMask, sourceHashBeforeNormalization);
        return;
    }

    if (path == 2 && S->async_layer_contents) {
        S->async_layer_contents(t, id, p, w, h);
        return;
    }

    NJC_CALL(set_contents, t, id, p, w, h);
}
static inline void nutjob_set_root_layer(graal_isolatethread_t* t, long id) { NJC_CALL(set_root_layer, t, id); }
static inline void nutjob_async_layer_contents(graal_isolatethread_t* t, long id, int* p, int w, int h) { NJC_CALL(async_layer_contents, t, id, p, w, h); }
static inline void nutjob_compositor_commit(graal_isolatethread_t* t) { NJC_CALL(commit, t); }
static inline void nutjob_compositor_get_pixels(graal_isolatethread_t* t, int* buf, int w, int h) { NJC_CALL(get_pixels, t, buf, w, h); }
static inline void nutjob_compositor_dump_layers(graal_isolatethread_t* t) { NJC_CALL(dump_layers, t); }
static inline int nutjob_compositor_layer_count(graal_isolatethread_t* t) { return S->layer_count ? S->layer_count(t) : 0; }

#undef S

#endif /* NUTJOB_COMPOSITOR_H */
