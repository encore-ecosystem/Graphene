#define _GNU_SOURCE
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sched.h>
typedef struct {size_t ref_count,len;char data[];} test_string;
typedef struct {test_string *object;} encore_str;
void *encore_str_from_cstr(const char *s) {size_t n=strlen(s);test_string *p=malloc(sizeof *p+n+1);p->len=n;p->ref_count=1;memcpy(p->data,s,n+1);return p;}
encore_str encore_str_from_buffer(const void *s,size_t n) {test_string *p=malloc(sizeof *p+n+1);p->len=n;p->ref_count=1;memcpy(p->data,s,n);p->data[n]=0;return (encore_str){p};}
static encore_str graphene_string(const char *s) {return (encore_str){encore_str_from_cstr(s?s:"")};}
static char *graphene_c_string(encore_str s) {return strndup(s.object->data,s.object->len);}
#include "../native/mcp/mailbox.inc"
static void expect(encore_str value,const char *text) {assert(value.object);assert(value.object->len==strlen(text));assert(!memcmp(value.object->data,text,value.object->len));free(value.object);}
static uintptr_t shared;
static void *client(void *arg) {
    char payload[40];snprintf(payload,sizeof payload,"worker-%zu",(size_t)arg);
    encore_str request=graphene_string(payload);
    for(unsigned i=0;i<60;++i) {
        uint64_t ticket;
        while(!(ticket=graphene_mcp_mailbox_submit(shared,request)))sched_yield();
        for(;;) {
            encore_str reply=graphene_mcp_mailbox_take(shared,ticket);
            if(reply.object->len){expect(reply,payload);break;}
            free(reply.object);sched_yield();
        }
    }
    free(request.object);return NULL;
}
int main(void) {
    shared=graphene_mcp_mailbox_create();assert(shared);
    encore_str request=graphene_string("request"),response=graphene_string("response");
    uint64_t tickets[GRAPHENE_MCP_SLOTS];
    for(unsigned i=0;i<GRAPHENE_MCP_SLOTS;++i){tickets[i]=graphene_mcp_mailbox_submit(shared,request);assert(tickets[i]);}
    assert(!graphene_mcp_mailbox_submit(shared,request));
    for(unsigned i=0;i<GRAPHENE_MCP_SLOTS;++i){assert(graphene_mcp_mailbox_next(shared)==tickets[i]);expect(graphene_mcp_mailbox_request(shared,tickets[i]),"request");assert(graphene_mcp_mailbox_respond(shared,tickets[i],response));expect(graphene_mcp_mailbox_take(shared,tickets[i]),"response");}
    assert(!graphene_mcp_mailbox_next(shared));
    uint64_t expired=graphene_mcp_mailbox_submit(shared,request);
    graphene_mcp_mailbox *box=(void *)shared;
    for(unsigned i=0;i<GRAPHENE_MCP_SLOTS;++i)if(box->slots[i].ticket==expired)box->slots[i].deadline=0;
    assert(!graphene_mcp_mailbox_next(shared));
    uint64_t abandoned=graphene_mcp_mailbox_submit(shared,request);
    assert(graphene_mcp_mailbox_next(shared)==abandoned);graphene_mcp_mailbox_abandon(shared,abandoned);
    assert(!graphene_mcp_mailbox_respond(shared,abandoned,response));
    expect(graphene_mcp_mailbox_take(shared,abandoned),"");
    pthread_t workers[8];for(size_t i=0;i<8;++i)assert(!pthread_create(&workers[i],NULL,client,(void *)i));
    unsigned completed=0;
    while(completed<8*60){uint64_t ticket=graphene_mcp_mailbox_next(shared);if(!ticket){sched_yield();continue;}encore_str message=graphene_mcp_mailbox_request(shared,ticket);assert(graphene_mcp_mailbox_respond(shared,ticket,message));free(message.object);++completed;}
    for(unsigned i=0;i<8;++i)assert(!pthread_join(workers[i],NULL));
    encore_str image=graphene_string("abcde");expect(graphene_mcp_base64(image),"YWJjZGU=");free(image.object);
    graphene_mcp_mailbox_close(shared);assert(!graphene_mcp_mailbox_submit(shared,request));
    graphene_mcp_mailbox_destroy(shared);free(request.object);free(response.object);
    puts("PASS: bounded FIFO, overflow, expiry, abandoned replies, 480 concurrent commands, base64, shutdown");
}
