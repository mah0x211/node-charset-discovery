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
#include <unicode/ucsdet.h>

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
        UCharsetDetector *csd;
        static Handle<Value> New( const Arguments& argv );
        static Handle<Value> Detect( const Arguments& argv );
};

CharsetDiscovery::CharsetDiscovery()
{
    csd = NULL;
};

CharsetDiscovery::~CharsetDiscovery()
{
    if( csd ){
        ucsdet_close( csd );
    }
}

Handle<Value> CharsetDiscovery::New( const Arguments& argv )
{
    HandleScope scope;
    Handle<Value> retval = Undefined();
    UErrorCode status = U_ZERO_ERROR;
    CharsetDiscovery *cd = new CharsetDiscovery();
    
    cd->csd = ucsdet_open( &status );
    if( U_FAILURE( status ) ){
        delete cd;
        retval = Throw( Str2Err( u_errorName( status ) ) );
    }
    else {
        cd->Wrap( argv.This() );
        retval = argv.This();
    }
    
    return scope.Close( retval );
}


Handle<Value> CharsetDiscovery::Detect( const Arguments& argv )
{
    HandleScope scope;
    CharsetDiscovery *cd = ObjUnwrap( CharsetDiscovery, argv.This() );
    int argc = argv.Length();
    UErrorCode status = U_ZERO_ERROR;
    Local<String> str;
    size_t len = 0;
    const UCharsetMatch *match = NULL;
    const char *enc = NULL;
    
    if( !argc ){
        return scope.Close( Throw( Str2Err( "undefined arguments" ) ) );
    }
    else if( !argv[0]->IsString() ){
        return scope.Close( Throw( Str2TypeErr( "invalid type of arguments" ) ) );
    }
    
    str = argv[0]->ToString();
    len = str->Utf8Length();
    if( !len ){
        return scope.Close( Undefined() );
    }
    
    ucsdet_setText( cd->csd, *String::Utf8Value( str ), len, &status );
    if( U_FAILURE( status ) ){
        return scope.Close( Throw( Str2Err( u_errorName( status ) ) ) );
    }
    else if( !( match = ucsdet_detect( cd->csd, &status ) ) ){
        return scope.Close( Undefined() );
    }
    
    enc = ucsdet_getName( match, &status );
    if( U_FAILURE( status ) ){
        return scope.Close( Throw( Str2Err( u_errorName( status ) ) ) );
    }
    
    return scope.Close( String::New( enc ) );
}

void CharsetDiscovery::Initialize( Handle<Object> target )
{
    HandleScope scope;
    Local<FunctionTemplate> t = FunctionTemplate::New( New );
    
    t->InstanceTemplate()->SetInternalFieldCount(1);
    t->SetClassName( String::NewSymbol("charset_discovery") );
    
    NODE_SET_PROTOTYPE_METHOD( t, "detect", Detect );
    
    target->Set( String::NewSymbol("charset_discovery"), t->GetFunction() );
}

static void init( Handle<Object> target ){
    HandleScope scope;
    CharsetDiscovery::Initialize( target );
}

NODE_MODULE( charset_discovery, init );

