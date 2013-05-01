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

typedef struct {
    Local<String> str;
    size_t len;
    UErrorCode status;
    const UCharsetMatch *match;
} Detect_t;

// interface
class CharsetDiscovery : public ObjectWrap 
{
    public:
        CharsetDiscovery();
        ~CharsetDiscovery();
        static void Initialize( Handle<Object> target );
    
    private:
        UCharsetDetector *csd;
        
        Handle<Value> Prepare( Detect_t *d, const Arguments& argv );
        UErrorCode Detect( Detect_t *d );
        
        static Handle<Value> New( const Arguments& argv );
        static Handle<Value> GetName( const Arguments& argv );
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

Handle<Value> CharsetDiscovery::Prepare( Detect_t *d, const Arguments& argv )
{
    int argc = argv.Length();
    
    if( !argc ){
        return Str2Err( "undefined arguments" );
    }
    else if( !argv[0]->IsString() ){
        return Str2TypeErr( "invalid type of arguments" );
    }
    
    d->str = argv[0]->ToString();
    d->len = d->str->Utf8Length();
    
    return Undefined();
}

UErrorCode CharsetDiscovery::Detect( Detect_t *d )
{
    d->status = U_ZERO_ERROR;
    ucsdet_setText( csd, *String::Utf8Value( d->str ), d->len, &d->status );
    if( !U_FAILURE( d->status ) ){
        d->match = ucsdet_detect( csd, &d->status );
        d->status = U_ZERO_ERROR;
    }
    
    return d->status;
}

Handle<Value> CharsetDiscovery::GetName( const Arguments& argv )
{
    HandleScope scope;
    CharsetDiscovery *cd = ObjUnwrap( CharsetDiscovery, argv.This() );
    Detect_t d;
    Handle<Value> retval = cd->Prepare( &d, argv );
    
    if( !retval->IsUndefined() ){
        return scope.Close( Throw( retval ) );
    }
    else if( !d.len ){
        return scope.Close( Undefined() );
    }
    else if( U_FAILURE( cd->Detect( &d ) ) ){
        return scope.Close( Throw( Str2Err( u_errorName( d.status ) ) ) );
    }
    else if( d.match )
    {
        const char *enc = ucsdet_getName( d.match, &d.status );
        
        if( U_FAILURE( d.status ) ){
            return scope.Close( Throw( Str2Err( u_errorName( d.status ) ) ) );
        }
    
        return scope.Close( String::New( enc ) );
    }
    
    return scope.Close( Undefined() );
}

void CharsetDiscovery::Initialize( Handle<Object> target )
{
    HandleScope scope;
    Local<FunctionTemplate> t = FunctionTemplate::New( New );
    
    t->InstanceTemplate()->SetInternalFieldCount(1);
    t->SetClassName( String::NewSymbol("charset_discovery") );
    
    NODE_SET_PROTOTYPE_METHOD( t, "getName", GetName );
    
    target->Set( String::NewSymbol("charset_discovery"), t->GetFunction() );
}

static void init( Handle<Object> target ){
    HandleScope scope;
    CharsetDiscovery::Initialize( target );
}

NODE_MODULE( charset_discovery, init );

