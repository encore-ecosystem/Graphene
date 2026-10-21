/* Graphene frame history and profiling policy. Fixed storage, no per-frame
 * allocation or logging. GPU timestamps remain owned by Vulkan. */
extern uint32_t encore_vk_timing_support(size_t);
extern bool encore_vk_timing_begin(size_t, uint64_t);
extern void encore_vk_timing_finish(size_t, bool);
extern uint64_t encore_vk_timing_wait_ns(size_t);
extern uint64_t encore_vk_timing_poll(size_t);
extern uint64_t encore_vk_timing_value(size_t, uint32_t, uint32_t);
extern void encore_vk_timing_release(size_t);
extern void encore_vk_parallel_frame_begin(size_t, uint64_t);
extern uint64_t encore_vk_parallel_frame_value(size_t, uint64_t, uint32_t, uint32_t);
enum { GRAPHENE_PROFILE_HISTORY = 600, GRAPHENE_PROFILE_EVENTS = 128 };
typedef struct { uint32_t label, depth; uint64_t start, duration, wait; } GrapheneProfileEvent;
typedef struct {
    uint64_t id, started, frame_ns, cpu_ns, wait_ns, gpu_ns;
    uint32_t gpu_state, cpu_count, gpu_count;
    uint64_t ui_acquire, ui_present, ui_prepare, ui_bytes;
    bool overflow;
    GrapheneProfileEvent cpu[GRAPHENE_PROFILE_EVENTS], gpu[GRAPHENE_PROFILE_EVENTS];
} GrapheneFrameProfile;
typedef struct {
    bool enabled, active, gpu_supported, gpu_started, live;
    size_t device;
    uint64_t next_id, last_present, selected, phase_start, phase_wait;
    uint64_t deferred_cpu, deferred_wait, event_wait;
    uint32_t count, head, phase;
    uint64_t previous_ui[4];
    GrapheneFrameProfile current, pinned, history[GRAPHENE_PROFILE_HISTORY];
} GrapheneProfiler;
static GrapheneProfiler graphene_profiler;
static void graphene_profiler_collect(void) {
    GrapheneProfiler *p = &graphene_profiler;
    uint64_t id;
    while ((id = encore_vk_timing_poll(p->device)) != 0) {
        GrapheneFrameProfile *frame = NULL;
        for (uint32_t n = 0; n < p->count; ++n) {
            GrapheneFrameProfile *f = &p->history[(p->head + GRAPHENE_PROFILE_HISTORY - p->count + n) % GRAPHENE_PROFILE_HISTORY];
            if (f->id == id) { frame = f; break; }
        }
        if (frame) {
            frame->gpu_state = (uint32_t)encore_vk_timing_value(p->device, 0, 0);
            frame->overflow |= encore_vk_timing_value(p->device, 0, 6) != 0;
            frame->gpu_count = (uint32_t)encore_vk_timing_value(p->device, 0, 1);
            if (frame->gpu_count > GRAPHENE_PROFILE_EVENTS) frame->gpu_count = GRAPHENE_PROFILE_EVENTS;
            for (uint32_t n = 0; n < frame->gpu_count; ++n) {
                GrapheneProfileEvent *e = &frame->gpu[n];
                e->label = (uint32_t)encore_vk_timing_value(p->device, n, 2);
                e->start = encore_vk_timing_value(p->device, n, 3);
                e->duration = encore_vk_timing_value(p->device, n, 4);
                e->depth = (uint32_t)encore_vk_timing_value(p->device, n, 5);
                if (!e->depth) frame->gpu_ns += e->duration;
            }
            for(uint32_t thread=0;thread<5 && frame->cpu_count<GRAPHENE_PROFILE_EVENTS;++thread) {
                uint64_t start=encore_vk_parallel_frame_value(p->device,id,thread,0);
                uint64_t end=encore_vk_parallel_frame_value(p->device,id,thread,1);
                uint64_t cpu=encore_vk_parallel_frame_value(p->device,id,thread,2);
                uint64_t elapsed=encore_vk_parallel_frame_value(p->device,id,thread,3);
                if(start && end>=start) {
                    if(cpu>frame->cpu_ns)frame->cpu_ns=cpu;
                    if(thread==4 && elapsed>cpu && elapsed-cpu>frame->wait_ns)frame->wait_ns=elapsed-cpu;
                    GrapheneProfileEvent *e=&frame->cpu[frame->cpu_count++];
                    *e=(GrapheneProfileEvent){20+thread,0,start>frame->started?start-frame->started:0,elapsed,elapsed>cpu?elapsed-cpu:0};
                }
            }
            if (!p->live && p->selected == id) p->pinned = *frame;
        }
        encore_vk_timing_release(p->device);
    }
}
void graphene_profiler_enable(size_t device, bool enabled) {
    GrapheneProfiler *p = &graphene_profiler;
    if (p->enabled == enabled && p->device == device) return;
    if (p->active) { encore_vk_timing_finish(p->device, false); p->active = false; }
    p->device = device; p->enabled = enabled; p->last_present = 0;
    p->deferred_cpu = p->deferred_wait = p->event_wait = 0;
    p->gpu_supported = enabled && encore_vk_timing_support(device) != 0;
    if (enabled) p->live = true;
    graphene_profiler_collect();
}
void graphene_profiler_clear(void) {
    GrapheneProfiler *p = &graphene_profiler;
    p->count = p->head = 0; p->selected = 0; p->live = true; p->last_present = 0;
    p->deferred_cpu = p->deferred_wait = p->event_wait = 0;
    memset(&p->pinned, 0, sizeof p->pinned);
}
void graphene_profiler_event_wait(uint64_t ns) {
    if (graphene_profiler.enabled) graphene_profiler.event_wait += ns;
}
void graphene_profiler_ui(uint64_t acquire, uint64_t present, uint64_t prepare, uint64_t bytes) {
    GrapheneProfiler *p = &graphene_profiler;
    uint64_t current[4] = {acquire,present,prepare,bytes}, delta[4];
    for (unsigned n=0;n<4;++n) { delta[n] = current[n] >= p->previous_ui[n] ? current[n]-p->previous_ui[n] : 0; p->previous_ui[n] = current[n]; }
    if (p->active) { p->current.ui_acquire += delta[0]; p->current.ui_present += delta[1]; p->current.ui_prepare += delta[2]; p->current.ui_bytes += delta[3]; }
}
void graphene_profiler_begin(void) {
    GrapheneProfiler *p = &graphene_profiler;
    if (!p->enabled) return;
    graphene_profiler_collect();
    memset(&p->current, 0, sizeof p->current);
    p->current.id = ++p->next_id; p->current.started = graphene_perf_counter_ns();
    encore_vk_parallel_frame_begin(p->device,p->current.id);
    p->gpu_started = p->gpu_supported && encore_vk_timing_begin(p->device, p->current.id);
    p->current.gpu_state = p->gpu_started ? 0 : 3;
    p->phase = 0; p->phase_start = p->current.started; p->phase_wait = 0; p->active = true;
}
void graphene_profiler_phase(uint32_t phase) {
    GrapheneProfiler *p = &graphene_profiler;
    if (!p->active) return;
    uint64_t now = graphene_perf_counter_ns(), wait = encore_vk_timing_wait_ns(p->device);
    if (p->current.cpu_count < GRAPHENE_PROFILE_EVENTS) {
        GrapheneProfileEvent *e = &p->current.cpu[p->current.cpu_count++];
        e->label = p->phase; e->depth = 0; e->start = p->phase_start - p->current.started;
        e->duration = now - p->phase_start; e->wait = wait >= p->phase_wait ? wait - p->phase_wait : 0;
    } else p->current.overflow = true;
    p->phase_start = now; p->phase_wait = wait; p->phase = phase;
}
void graphene_profiler_finish(bool presented) {
    GrapheneProfiler *p = &graphene_profiler;
    if (!p->active) return;
    graphene_profiler_phase(p->phase);
    uint64_t now = graphene_perf_counter_ns();
    p->current.wait_ns = encore_vk_timing_wait_ns(p->device);
    uint64_t elapsed = now - p->current.started;
    p->current.cpu_ns = elapsed > p->current.wait_ns ? elapsed - p->current.wait_ns : 0;
    if (p->gpu_started) encore_vk_timing_finish(p->device, presented);
    p->active = false;
    if (!presented) {
        p->deferred_cpu += p->current.cpu_ns;
        p->deferred_wait += p->current.wait_ns;
        return;
    }
    uint64_t deferred = p->deferred_cpu + p->deferred_wait + p->event_wait;
    if (deferred) {
        for (uint32_t n = 0; n < p->current.cpu_count; ++n) p->current.cpu[n].start += deferred;
        if (p->current.cpu_count < GRAPHENE_PROFILE_EVENTS) {
            GrapheneProfileEvent *e = &p->current.cpu[p->current.cpu_count++];
            *e = (GrapheneProfileEvent){6,0,0,deferred,p->deferred_wait+p->event_wait};
        } else p->current.overflow = true;
    }
    if(p->current.started>=deferred)p->current.started-=deferred;
    p->current.cpu_ns += p->deferred_cpu;
    p->current.wait_ns += p->deferred_wait + p->event_wait;
    p->deferred_cpu = p->deferred_wait = p->event_wait = 0;
    p->current.frame_ns = p->last_present ? now - p->last_present : elapsed + deferred;
    p->last_present = now;
    p->history[p->head] = p->current; p->head = (p->head + 1) % GRAPHENE_PROFILE_HISTORY;
    if (p->count < GRAPHENE_PROFILE_HISTORY) ++p->count;
}
static GrapheneFrameProfile *graphene_profile_at(uint32_t index) {
    GrapheneProfiler *p = &graphene_profiler;
    if (index >= p->count) return NULL;
    return &p->history[(p->head + GRAPHENE_PROFILE_HISTORY - p->count + index) % GRAPHENE_PROFILE_HISTORY];
}
static GrapheneFrameProfile *graphene_profile_selected(void) {
    GrapheneProfiler *p = &graphene_profiler;
    if (!p->live) return p->pinned.id ? &p->pinned : NULL;
    for (uint32_t n = p->count; n > 0; --n) {
        GrapheneFrameProfile *f = graphene_profile_at(n - 1);
        if (f->gpu_state == 1) return f;
    }
    return p->count ? graphene_profile_at(p->count - 1) : NULL;
}
void graphene_profiler_select(uint32_t index) {
    GrapheneFrameProfile *f = graphene_profile_at(index);
    if (f) { graphene_profiler.live = false; graphene_profiler.pinned = *f; graphene_profiler.selected = f->id; }
}
void graphene_profiler_live(void) { graphene_profiler.live = true; }
void graphene_profiler_step(int32_t step) {
    GrapheneProfiler *p = &graphene_profiler;
    if (!p->count) return;
    uint32_t index = p->count - 1;
    GrapheneFrameProfile *selected = graphene_profile_selected();
    for (uint32_t n = 0; n < p->count; ++n) if (graphene_profile_at(n)->id == (selected ? selected->id : 0)) { index = n; break; }
    int64_t next = (int64_t)index + step;
    if (next < 0) next = 0;
    if (next >= p->count) next = p->count - 1;
    graphene_profiler_select((uint32_t)next);
}
uint64_t graphene_profiler_value(uint32_t index, uint32_t field) {
    GrapheneProfiler *p = &graphene_profiler;
    if (field == 0) return p->count;
    if (field == 1) return p->live;
    if (field == 10) return p->gpu_supported;
    GrapheneFrameProfile *f = index == UINT32_MAX ? graphene_profile_selected() : graphene_profile_at(index);
    if (!f) return 0;
    switch(field) {
    case 2:return f->id; case 3:return f->frame_ns; case 4:return f->cpu_ns;
    case 5:return f->gpu_ns; case 6:return f->wait_ns; case 7:return f->gpu_state;
    case 12:return f->ui_acquire; case 13:return f->ui_present; case 14:return f->ui_prepare; case 15:return f->ui_bytes;
    case 8:return f->cpu_count; case 9:return f->gpu_count; case 11:return f->overflow;
    default:return 0;
    }
}
uint64_t graphene_profiler_event(uint32_t gpu, uint32_t index, uint32_t field) {
    GrapheneFrameProfile *f = graphene_profile_selected(); if (!f) return 0;
    if (index >= (gpu ? f->gpu_count : f->cpu_count)) return 0;
    GrapheneProfileEvent *e = gpu ? &f->gpu[index] : &f->cpu[index];
    switch(field) {case 0:return e->label;case 1:return e->start;case 2:return e->duration;case 3:return e->depth;case 4:return e->wait;default:return 0;}
}
static int graphene_profile_compare(const void *a, const void *b) {
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b; return (x > y) - (x < y);
}
uint64_t graphene_profiler_stat(uint32_t field) {
    GrapheneProfiler *p = &graphene_profiler;
    if (!p->count) return 0;
    uint64_t samples[GRAPHENE_PROFILE_HISTORY], total = 0;
    for (uint32_t n = 0; n < p->count; ++n) { samples[n] = graphene_profile_at(n)->frame_ns; total += samples[n]; }
    if (field == 0) return total / p->count;
    qsort(samples, p->count, sizeof samples[0], graphene_profile_compare);
    return samples[((p->count - 1) * 95) / 100];
}
uint32_t graphene_profiler_bottleneck(void) {
    GrapheneProfiler *p = &graphene_profiler;
    uint64_t cpu[60], gpu[60], wait[60]; uint32_t count = 0;
    for (uint32_t n = p->count; n > 0 && count < 60; --n) {
        GrapheneFrameProfile *f = graphene_profile_at(n - 1);
        if (f->gpu_state != 1) continue;
        cpu[count] = f->cpu_ns; gpu[count] = f->gpu_ns; wait[count++] = f->wait_ns;
    }
    if (count < 60) return 0;
    qsort(cpu,count,sizeof cpu[0],graphene_profile_compare);qsort(gpu,count,sizeof gpu[0],graphene_profile_compare);qsort(wait,count,sizeof wait[0],graphene_profile_compare);
    uint64_t c=cpu[29],g=gpu[29],w=wait[29];
    if (w > c && w > g) return 3;
    if (c > g && c - g >= 500000 && (double)c >= (double)g * 1.2) return 1;
    if (g > c && g - c >= 500000 && (double)g >= (double)c * 1.2) return 2;
    return 4;
}
