/*
 *  charset_discovery.cc
 *
 *  Created by Masatoshi Teruya on 13/05/01.
 *  Copyright 2013 Masatoshi Teruya. All rights reserved.
 *
 */
#include <node.h>
#include <node_buffer.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

using namespace v8;
using namespace node;


#define ObjUnwrap(tmpl,obj) ObjectWrap::Unwrap<tmpl>(obj)
#define Arg2Str(v)          *(String::Utf8Value( (v)->ToString()))
#define Arg2Int(v)          ((v)->IntegerValue())
#define Arg2Uint32(v)       ((v)->Uint32Value())
#define Throw(v)            ThrowException(v)
#define Str2Err(v)          Exception::Error(String::New(v))
#define Str2TypeErr(v)      Exception::TypeError(String::New(v))

// interface
class CharsetDiscovery : public ObjectWrap 
{
    public:
        CharsetDiscovery();
        ~CharsetDiscovery();
        static void Initialize( Handle<Object> target );
    
    private:
        static Handle<Value> New( const Arguments& argv );
};

CharsetDiscovery::CharsetDiscovery()
{
};

CharsetDiscovery::~CharsetDiscovery()
{
}

Handle<Value> CharsetDiscovery::New( const Arguments& argv )
{
    HandleScope scope;
    CharsetDiscovery *cd = new CharsetDiscovery();
    
    cd->Wrap( argv.This() );
    
    return scope.Close( argv.This() );
}


void CharsetDiscovery::Initialize( Handle<Object> target )
{
    HandleScope scope;
    Local<FunctionTemplate> t = FunctionTemplate::New( New );
    
    t->InstanceTemplate()->SetInternalFieldCount(1);
    t->SetClassName( String::NewSymbol("charset_discovery") );
    
    target->Set( String::NewSymbol("charset_discovery"), t->GetFunction() );
}

static void init( Handle<Object> target ){
    HandleScope scope;
    CharsetDiscovery::Initialize( target );
}

NODE_MODULE( charset_discovery, init );

