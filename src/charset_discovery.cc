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

#define TryCallback(cb,self,argc,args)({\
    TryCatch try_catch; \
    cb->Call( self, argc, args ); \
    if (try_catch.HasCaught()){ \
        FatalException( try_catch ); \
    } \
})

typedef struct {
    int argc;
    Local<String> str;
    size_t len;
    Local<Function> callback;
} Args_t;

enum TaskType_e {
    T_NAME = 1 << 0,
};

typedef struct {
    uv_work_t req;
    pthread_mutex_t *mutex;
    pthread_cond_t *cond;
    Persistent<Function> callback;
    int task;
    UCharsetDetector *csd;
    const char *str;
    size_t len;
    UErrorCode status;
    const UCharsetMatch *match;
    void *data;
} Baton_t;

// interface
class CharsetDiscovery : public ObjectWrap 
{
    public:
        CharsetDiscovery();
        ~CharsetDiscovery();
        static void Initialize( Handle<Object> target );
        
    private:
        UCharsetDetector *csd;
        
        Handle<Value> Prepare( Args_t *args, const Arguments& argv );
        Handle<Value> Task( int task, const Arguments& argv );
        
        static Handle<Value> New( const Arguments& argv );
        static Handle<Value> GetName( const Arguments& argv );
        static void Detect( Baton_t *baton );
        static void TaskWork( uv_work_t* req );
        static void TaskAfter( uv_work_t* req );
};


// implements
// public
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

void CharsetDiscovery::Initialize( Handle<Object> target )
{
    HandleScope scope;
    Local<FunctionTemplate> t = FunctionTemplate::New( New );
    
    t->InstanceTemplate()->SetInternalFieldCount(1);
    t->SetClassName( String::NewSymbol("charset_discovery") );
    
    NODE_SET_PROTOTYPE_METHOD( t, "getName", GetName );
    
    target->Set( String::NewSymbol("charset_discovery"), t->GetFunction() );
}

// private
Handle<Value> CharsetDiscovery::Prepare( Args_t *args, const Arguments& argv )
{
    args->argc = argv.Length();
    
    if( !args->argc ){
        return Str2Err( "undefined arguments" );
    }
    else if( !argv[0]->IsString() ){
        return Str2TypeErr( "invalid type of arguments" );
    }
    
    args->str = argv[0]->ToString();
    args->len = args->str->Utf8Length();
    if( args->argc > 1 )
    {
        if( !argv[1]->IsFunction() ){
            return Str2TypeErr( "invalid type of arguments" );
        }
        args->callback = Local<Function>::Cast( argv[1] );
    }
    
    return Undefined();
}

Handle<Value> CharsetDiscovery::Task( int task, const Arguments& argv )
{
    Args_t args;
    Handle<Value> retval = Prepare( &args, argv );
    
    if( !retval->IsUndefined() ){
        return Throw( retval );
    }
    else if( !args.len )
    {
        if( !args.callback->IsUndefined() ){
            Local<Value> arg[] = {};
            // Context::GetCurrent()->Global()
            args.callback->Call( args.callback, 0, arg );
        }
    }
    else if( args.argc < 2 )
    {
        Baton_t baton;
        baton.task = task;
        baton.csd = csd;
        baton.str = *String::Utf8Value( args.str );
        baton.len = args.len;
        
        Detect( &baton );
        
        if( U_FAILURE( baton.status ) ){
            retval = Throw( Str2Err( u_errorName( baton.status ) ) );
        }
        else if( baton.data )
        {
            switch( task ){
                case T_NAME:
                    retval = String::New( (char*)baton.data );
                break;
            }
        }
    }
    else
    {
        char *str = (char*)malloc( args.len * sizeof(char) + 1 );
        
        if( str )
        {
            int rc;
            Baton_t *baton = new Baton_t;
            
            memcpy( (void*)str, (void*)*String::Utf8Value( args.str ), args.len );
            str[args.len] = 0;
            
            baton->req.data = baton;
            baton->callback = Persistent<Function>::New( args.callback );
            baton->task = task;
            baton->csd = csd;
            baton->str = (const char*)str;
            baton->len = args.len;
            
            rc = uv_queue_work( uv_default_loop(),
                                &baton->req,
                                TaskWork,
                                (uv_after_work_cb)TaskAfter );
            assert( rc == 0 );
        }
        else {
            retval = String::New( strerror( errno ) );
        }
    }
    
    return retval;
}

Handle<Value> CharsetDiscovery::New( const Arguments& argv )
{
    HandleScope scope;
    Handle<Value> retval = Undefined();
    CharsetDiscovery *cd = new CharsetDiscovery();
    UErrorCode status = U_ZERO_ERROR;
    
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

Handle<Value> CharsetDiscovery::GetName( const Arguments& argv )
{
    HandleScope scope;
    CharsetDiscovery *cd = ObjUnwrap( CharsetDiscovery, argv.This() );
    
    return scope.Close( cd->Task( T_NAME, argv ) );
}

void CharsetDiscovery::Detect( Baton_t *baton )
{
    baton->status = U_ZERO_ERROR;
    baton->data = NULL;
    baton->match = NULL;
    ucsdet_setText( baton->csd, baton->str, baton->len, &baton->status );
    if( !U_FAILURE( baton->status ) )
    {
        baton->match = ucsdet_detect( baton->csd, &baton->status );
        baton->status = U_ZERO_ERROR;
        if( baton->match )
        {
            if( baton->task & T_NAME ){
                baton->data = (void*)ucsdet_getName( baton->match, 
                                                     &baton->status );
            }
        }
    }
}

void CharsetDiscovery::TaskWork( uv_work_t* req )
{
    Baton_t *baton = static_cast<Baton_t*>( req->data );
    
    Detect( baton );
}

void CharsetDiscovery::TaskAfter( uv_work_t* req )
{
    HandleScope scope;
    Baton_t *baton = static_cast<Baton_t*>( req->data );
    
    free( (void*)baton->str );
    if( U_FAILURE( baton->status ) )
    {
        Local<Value> argv[1] = {
            Str2Err( u_errorName( baton->status ) )
        };
        
        TryCallback( baton->callback, Context::GetCurrent()->Global(), 1, argv );
    }
    else if( baton->data )
    {
        switch( baton->task ){
            case T_NAME:
                Local<Value> argv[2] = {
                    Local<Value>::New( Undefined() ),
                    Local<Value>::New( String::New( (char*)baton->data ) )
                };
                TryCallback( baton->callback, Context::GetCurrent()->Global(), 
                             2, argv );
            break;
        }
    }
    else {
        Local<Value> argv[] = {};
        TryCallback( baton->callback, Context::GetCurrent()->Global(), 0, argv );
    }
    
    baton->callback.Dispose();
    delete baton;
}



static void init( Handle<Object> target ){
    HandleScope scope;
    CharsetDiscovery::Initialize( target );
}

NODE_MODULE( charset_discovery, init );

