/* Owned byte messages bridge Blaze's worker and the editor thread. Never share
 * Encore graph objects or UI/GPU pointers across this boundary. */
#if !defined(_WIN32)
#include <pthread.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdatomic.h>
#define GRAPHENE_MCP_SLOTS 64
#define GRAPHENE_MCP_REQUEST_MAX (1024u * 1024u)
#define GRAPHENE_MCP_RESPONSE_MAX (32u * 1024u * 1024u)
typedef struct {
    uint64_t ticket;
    uint64_t deadline;
    unsigned state; /* 0 free, 1 queued, 2 executing, 3 ready, 4 abandoned */
    char *request, *response;
    size_t request_len, response_len;
} graphene_mcp_slot;
typedef struct {
    pthread_mutex_t lock;
    graphene_mcp_slot slots[GRAPHENE_MCP_SLOTS];
    uint64_t next_ticket;
    bool closing;
    char *error;
    _Atomic unsigned pending;
} graphene_mcp_mailbox;
static uint64_t graphene_mcp_now(void) {
    struct timespec now; clock_gettime(CLOCK_MONOTONIC,&now);
    return (uint64_t)now.tv_sec*1000u+(uint64_t)now.tv_nsec/1000000u;
}
static void graphene_mcp_slot_clear(graphene_mcp_slot *slot) {
    free(slot->request); free(slot->response); memset(slot, 0, sizeof *slot);
}
uintptr_t graphene_mcp_mailbox_create(void) {
    graphene_mcp_mailbox *box = calloc(1, sizeof *box);
    if (!box) return 0;
    if (pthread_mutex_init(&box->lock, NULL)) { free(box); return 0; }
    box->next_ticket = 1;
    return (uintptr_t)box;
}
uint64_t graphene_mcp_mailbox_submit(uintptr_t handle, encore_str request) {
    graphene_mcp_mailbox *box = (void *)handle;
    if (!box || !request.object || request.object->len > GRAPHENE_MCP_REQUEST_MAX) return 0;
    pthread_mutex_lock(&box->lock);
    uint64_t ticket = 0, now = graphene_mcp_now();
    /* Dropped HTTP futures cannot own native storage forever. Expired queued
     * commands never execute; executing commands retain their slot until reply. */
    for (unsigned i=0;i<GRAPHENE_MCP_SLOTS;++i) {
        graphene_mcp_slot *slot=&box->slots[i];
        if (!slot->state || now<slot->deadline) continue;
        if (slot->state==1) atomic_fetch_sub_explicit(&box->pending,1,memory_order_release);
        if (slot->state==2 || slot->state==4) slot->state=4;
        else graphene_mcp_slot_clear(slot);
    }
    for (unsigned i = 0; !box->closing && i < GRAPHENE_MCP_SLOTS; ++i) {
        graphene_mcp_slot *slot = &box->slots[i];
        if (slot->state) continue;
        slot->request = malloc(request.object->len + 1);
        if (!slot->request) break;
        memcpy(slot->request, request.object->data, request.object->len);
        slot->request[request.object->len] = 0;
        slot->request_len = request.object->len;
        slot->ticket = ticket = box->next_ticket++;
        slot->deadline=now+30000u;
        slot->state = 1;
        atomic_fetch_add_explicit(&box->pending, 1, memory_order_release);
        break;
    }
    pthread_mutex_unlock(&box->lock);
    return ticket;
}
uint64_t graphene_mcp_mailbox_next(uintptr_t handle) {
    graphene_mcp_mailbox *box = (void *)handle;
    if (!box || !atomic_load_explicit(&box->pending, memory_order_acquire)) return 0;
    pthread_mutex_lock(&box->lock);
    uint64_t ticket = 0, now = graphene_mcp_now();
    for (unsigned i = 0; i < GRAPHENE_MCP_SLOTS; ++i) {
        graphene_mcp_slot *slot = &box->slots[i];
        if (slot->state==1 && now>=slot->deadline) {
            atomic_fetch_sub_explicit(&box->pending,1,memory_order_release);graphene_mcp_slot_clear(slot);continue;
        }
        if (slot->state == 1 && (!ticket || slot->ticket < ticket)) ticket = slot->ticket;
    }
    for (unsigned i = 0; ticket && i < GRAPHENE_MCP_SLOTS; ++i) {
        if (box->slots[i].ticket != ticket) continue;
        box->slots[i].state = 2;
        atomic_fetch_sub_explicit(&box->pending, 1, memory_order_release);
        break;
    }
    pthread_mutex_unlock(&box->lock);
    return ticket;
}
encore_str graphene_mcp_mailbox_request(uintptr_t handle, uint64_t ticket) {
    graphene_mcp_mailbox *box = (void *)handle;
    if (!box) return graphene_string("");
    pthread_mutex_lock(&box->lock);
    encore_str result = {0};
    for (unsigned i = 0; i < GRAPHENE_MCP_SLOTS; ++i) {
        graphene_mcp_slot *slot = &box->slots[i];
        if (slot->ticket == ticket && slot->state == 2) {
            result = encore_str_from_buffer(slot->request, slot->request_len); break;
        }
    }
    pthread_mutex_unlock(&box->lock);
    return result.object ? result : graphene_string("");
}
bool graphene_mcp_mailbox_respond(uintptr_t handle, uint64_t ticket, encore_str response) {
    graphene_mcp_mailbox *box = (void *)handle;
    if (!box || !response.object || response.object->len > GRAPHENE_MCP_RESPONSE_MAX) return false;
    pthread_mutex_lock(&box->lock);
    bool result = false;
    for (unsigned i = 0; i < GRAPHENE_MCP_SLOTS; ++i) {
        graphene_mcp_slot *slot = &box->slots[i];
        if (slot->ticket != ticket) continue;
        if (slot->state == 4) { graphene_mcp_slot_clear(slot); break; }
        if (slot->state != 2) break;
        slot->response = malloc(response.object->len + 1);
        if (!slot->response) break;
        memcpy(slot->response, response.object->data, response.object->len);
        slot->response[response.object->len] = 0;
        slot->response_len = response.object->len;
        slot->state = 3; result = true; break;
    }
    pthread_mutex_unlock(&box->lock);
    return result;
}
encore_str graphene_mcp_mailbox_take(uintptr_t handle, uint64_t ticket) {
    graphene_mcp_mailbox *box = (void *)handle;
    if (!box) return graphene_string("");
    pthread_mutex_lock(&box->lock);
    encore_str result = {0};
    for (unsigned i = 0; i < GRAPHENE_MCP_SLOTS; ++i) {
        graphene_mcp_slot *slot = &box->slots[i];
        if (slot->ticket == ticket && slot->state == 3) {
            result = encore_str_from_buffer(slot->response, slot->response_len);
            graphene_mcp_slot_clear(slot); break;
        }
    }
    pthread_mutex_unlock(&box->lock);
    return result.object ? result : graphene_string("");
}
void graphene_mcp_mailbox_abandon(uintptr_t handle, uint64_t ticket) {
    graphene_mcp_mailbox *box = (void *)handle;
    if (!box) return;
    pthread_mutex_lock(&box->lock);
    for (unsigned i = 0; i < GRAPHENE_MCP_SLOTS; ++i) {
        graphene_mcp_slot *slot = &box->slots[i];
        if (slot->ticket != ticket) continue;
        if (slot->state == 1) atomic_fetch_sub_explicit(&box->pending, 1, memory_order_release);
        if (slot->state == 2) slot->state = 4;
        else graphene_mcp_slot_clear(slot);
        break;
    }
    pthread_mutex_unlock(&box->lock);
}
void graphene_mcp_mailbox_close(uintptr_t handle) {
    graphene_mcp_mailbox *box = (void *)handle;
    if (!box) return;
    pthread_mutex_lock(&box->lock); box->closing = true; pthread_mutex_unlock(&box->lock);
}
void graphene_mcp_mailbox_error(uintptr_t handle, encore_str error) {
    graphene_mcp_mailbox *box = (void *)handle;
    if (!box) return;
    pthread_mutex_lock(&box->lock); free(box->error); box->error = graphene_c_string(error); pthread_mutex_unlock(&box->lock);
}
encore_str graphene_mcp_mailbox_status(uintptr_t handle) {
    graphene_mcp_mailbox *box = (void *)handle;
    if (!box) return graphene_string("Unable to allocate MCP mailbox");
    pthread_mutex_lock(&box->lock); encore_str result = graphene_string(box->error); pthread_mutex_unlock(&box->lock);
    return result;
}
void graphene_mcp_mailbox_destroy(uintptr_t handle) {
    graphene_mcp_mailbox *box = (void *)handle;
    if (!box) return;
    /* Caller must join the HTTP worker before releasing the mailbox. */
    for (unsigned i = 0; i < GRAPHENE_MCP_SLOTS; ++i) graphene_mcp_slot_clear(&box->slots[i]);
    free(box->error); pthread_mutex_destroy(&box->lock); free(box);
}
#endif

encore_str graphene_mcp_base64(encore_str input) {
    static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t n=input.object?input.object->len:0;
    if (!n || n>24u*1024u*1024u) return graphene_string("");
    const unsigned char *data=(const void *)input.object->data;
    size_t length=((n+2)/3)*4;
    char *out=malloc(length);
    if (!out) return graphene_string("");
    size_t target=0;
    for (size_t i=0;i<n;i+=3) {
        uint32_t value=(uint32_t)data[i]<<16;
        if(i+1<n)value|=(uint32_t)data[i+1]<<8;
        if(i+2<n)value|=data[i+2];
        out[target++]=alphabet[(value>>18)&63];out[target++]=alphabet[(value>>12)&63];
        out[target++]=i+1<n?alphabet[(value>>6)&63]:'=';
        out[target++]=i+2<n?alphabet[value&63]:'=';
    }
    encore_str result=encore_str_from_buffer(out,length);free(out);return result;
}
#if !defined(_WIN32)
bool graphene_mcp_store_token(encore_str path,encore_str token) {
    char *name=graphene_c_string(path);
    if (!name || !token.object || token.object->len!=64) {free(name);return false;}
    int fd=open(name,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
    if(fd<0){free(name);return false;}
    size_t offset=0;
    while(offset<token.object->len) {
        ssize_t count=write(fd,token.object->data+offset,token.object->len-offset);
        if(count<0 && errno==EINTR)continue;
        if(count<=0)break;
        offset+=(size_t)count;
    }
    bool ok=offset==token.object->len;
    if(close(fd)!=0)ok=false;
    if(!ok)unlink(name);
    free(name);return ok;
}
#endif
