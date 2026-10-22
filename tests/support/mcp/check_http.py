#!/usr/bin/env python3
"""Exercise the production Blaze endpoint and protocol without linking a renderer."""
from pathlib import Path
import argparse, json, shutil, subprocess, socket, time, urllib.request, urllib.error, concurrent.futures
root=Path(__file__).resolve().parents[3]
p=argparse.ArgumentParser();p.add_argument('--compiler',default='encore');args=p.parse_args()
work=root/'target/mcp/http-fixture';src=work/'src/mcp';src.mkdir(parents=True,exist_ok=True)
for module in ['protocol','http']:shutil.copy2(root/f'src/mcp/{module}.enq',src/f'{module}.enq')
(work/'src/main.enq').write_text('''import core::fmt::Debug
import core::option::Option
import json::{Json, field}
import mod::mcp::http::McpHttpServer
import mod::mcp::protocol::{mcp_arguments,mcp_string,mcp_tool_result,mcp_unsigned}
import std::os::argv
import std::time::sleep_ms
extern fn fixture_payload(large: bool) -> str
fn main() -> u32 {
    let port=mcp_unsigned(Json::Number(argv(1).unwrap_or("18765"))).unwrap_or(18765) as usize
    let mut server=McpHttpServer::new()
    if !server.start(port,"fixture-secret",0) {ret 2}
    let mailbox=server.mailbox()
    let mut running=true
    while running {
        let ticket=mailbox.next()
        if ticket>0 {
            let source=mailbox.request(ticket)
            let query=mcp_string(mcp_arguments(source),"query")
            let payload=unsafe {fixture_payload(query == "large")}
            let _sent=mailbox.respond(ticket,mcp_tool_result(source,Json::Object(vec![field("ok",Json::Bool(true)),field("payload",Json::String(payload))])))
            if query == "stop" {running=false}
        }
        sleep_ms(2)
    }
    sleep_ms(50) server.stop() ret 0
}
''')
(work/'runtime.c').write_text('''#define _GNU_SOURCE
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
typedef struct {size_t ref_count,len;char data[];} object;
typedef struct {object *object;} encore_str;
extern void *encore_str_from_cstr(const char *);
extern encore_str encore_str_from_buffer(const void *,size_t);
static encore_str graphene_string(const char *s) {return (encore_str){encore_str_from_cstr(s?s:"")};}
static char *graphene_c_string(encore_str s) {size_t n=s.object?s.object->len:0;char *p=malloc(n+1);if(p){if(n)memcpy(p,s.object->data,n);p[n]=0;}return p;}
bool luma_waker_wake(size_t h){return false;}
encore_str fixture_payload(bool large){if(!large)return graphene_string("fixture");char *p=malloc(300000);memset(p,120,300000);encore_str s=encore_str_from_buffer(p,300000);free(p);return s;}
#include "mailbox.inc"
''')
shutil.copy2(root/'native/mcp/mailbox.inc',work/'mailbox.inc');shutil.copy2(root/'build.enq',work/'build.enq')
(work/'encore.toml').write_text('[project]\nname="mcp-http-fixture"\nversion="0.0.0"\nbuild="build.enq"\ndependencies=["index@std@0.0.5","index@json@0.0.3","path@../../../../Blaze"]\n')
shutil.copy2(root/'encore.lock',work/'encore.lock')
subprocess.run([args.compiler,'build'],cwd=work,check=True)
with socket.socket() as s:s.bind(('127.0.0.1',0));port=s.getsockname()[1]
log=(work/'server.log').open('w')
proc=subprocess.Popen([str(work/'target/dev/mcp-http-fixture'),str(port)],cwd=work,stdout=log,stderr=subprocess.STDOUT)
url=f'http://127.0.0.1:{port}/mcp'
def request(payload=None,headers=None,method='POST'):
 h={'Authorization':'Bearer fixture-secret','Content-Type':'application/json','Accept':'application/json, text/event-stream','MCP-Protocol-Version':'2025-11-25'};h.update(headers or {})
 data=json.dumps(payload).encode() if payload is not None else None
 try:
  with urllib.request.urlopen(urllib.request.Request(url,data=data,headers=h,method=method),timeout=10) as r:return r.status,r.read(),r.headers
 except urllib.error.HTTPError as e:return e.code,e.read(),e.headers
seq=0
def rpc(method,params=None,**kwargs):
 global seq
 seq+=1
 return request(dict(jsonrpc='2.0',id=seq,method=method,params=params or {}),**kwargs)
def call(name,arguments={}):
 code,body,_=rpc('tools/call',dict(name=name,arguments=arguments));assert code==200,(code,body);return json.loads(body)
try:
 for _ in range(300):
  if proc.poll() is not None:raise RuntimeError((work/'server.log').read_text())
  try:
   if request({})[0] in (200,400):break
  except OSError:pass
  time.sleep(.05)
 else:raise RuntimeError('server startup timed out')
 assert rpc('initialize',dict(protocolVersion='2025-11-25',capabilities={},clientInfo=dict(name='fixture',version='1')))[0]==200
 registry=json.loads(rpc('tools/list')[1])['result']['tools'];assert len(registry)>=35
 assert not any(x['name']=='approval_status' for x in registry)
 assert request({},dict(Authorization='Bearer wrong'))[0]==401
 assert request({},dict(Origin='https://example.com'))[0]==403
 assert request({},dict(Origin=f'http://127.0.0.1:{port}'))[0]==200
 assert request({},dict(Accept='application/json'))[0]==406
 assert request({},dict(**{'MCP-Protocol-Version':'unsupported'}))[0]==400
 assert request(method='GET')[0]==405
 assert request(headers=dict(Origin='https://example.com'),method='GET')[0]==403
 assert request(dict(jsonrpc='2.0',method='notifications/initialized'))[:2]==(202,b'')
 assert json.loads(rpc('ping')[1])['result']=={}
 assert json.loads(rpc('unknown')[1])['error']['code']==-32601
 assert 'error' in call('set_actor_transform',dict(actor=1,position=[1,2],rotation=[0,0,0],scale=[1,1,1]))
 assert 'error' in call('delete_actor',dict(actor=-1))
 assert 'error' in call('delete_actor',dict(actor=1,unexpected=True))
 assert 'error' in call('list_actors',dict(limit=201))
 result=call('get_editor_state');assert result['result']['structuredContent']['payload']=='fixture'
 large=call('get_logs',dict(query='large'));assert len(large['result']['structuredContent']['payload'])==300000
 with concurrent.futures.ThreadPoolExecutor(max_workers=12) as pool:
  replies=list(pool.map(lambda _:call('get_editor_state'),range(24)))
 assert all(x['result']['structuredContent']['ok'] for x in replies)
 assert call('get_logs',dict(query='stop'))['result']['structuredContent']['ok']
 assert proc.wait(timeout=5)==0
 print(f'PASS: {len(registry)} tools; protocol, auth, Origin, notifications, schemas, 300 KB replies, concurrent requests, clean shutdown')
finally:
 if proc.poll() is None:proc.terminate();proc.wait(timeout=5)
 log.close()
