
/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights
 reserved.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */

#pragma once

#include <assert.h>

#include <v8.h>

#include "v8_binder.h"
#include "JsConverter.h"
#include "async_common.h"

using namespace v8;


/** These classes wrap various sorts of C and C++ functions for use as either
  * synchronous or asynchronous JavaScript methods.
  *
  * There are two class hierarchies declared here.
  * The first hierarchy is:
  *    AsyncMethodCall
  *     -> Call_Returning<R>           template over return type
  *       -> NativeMethodCall<R, C>      template over class
  *
  * The second hierarchy is a set of alternate classes
  *     Call_1_<A0> , Call_2_<A0,A1> , ..  Call_8_<A0,A1, .. A7>
  *     expressing the set of arguments to a function or method call
  *
  * The base class AsyncMethodCall wraps the worker-thread run() routine
  * and the main thread (post-run) doAsyncCallback() needed for async execution.
  *
  * The run() method, declared void run(void), will be scheduled to run in a
  * uv worker thread.
  *
  * The doAsyncCallback() method will take a JavaScript context.  It is expected
  * to prepare the result and call the user's callback function.
  *
  * The templated class AsyncCall_Returning<R> inherits from AsyncMethodCall and
  * adds a return type, which is initialized at 0.
  */


/** Base class
**/
class AsyncMethodCall {
  public:
    /* Member variables */
    Persistent<Function> callback;
    Envelope * envelope;

    /* Constructors */
    AsyncMethodCall() : callback(), envelope(0) {}

    AsyncMethodCall(Local<Value> f) {
      // FIXME: raise an error if you use the wrong type as a callback
      callback = Persistent<Function>::New(Local<Function>::Cast(f));
    }

    /* Destructor */
    virtual ~AsyncMethodCall() {
      callback.Dispose();
    }

    /* Methods (Pure virtual) */
    virtual void run(void) = 0;
    virtual void doAsyncCallback(Local<Object>) = 0;

    /* Base Class Methods */
    virtual void handleErrors(void) { }

    void runAsync() {
      uv_work_t * req = new uv_work_t;
      req->data = (void *) this;
      uv_queue_work(uv_default_loop(), req, work_thd_run, main_thd_complete);
    }
};


/** First-level template class;
    templated over return types
**/
template <typename RETURN_TYPE>
class AsyncCall_Returning : public AsyncMethodCall {
public:
  /* Member variables */
  NativeCodeError *error;
  RETURN_TYPE return_val;

  /* Constructor */
  AsyncCall_Returning<RETURN_TYPE>(Local<Value> callback) :
    AsyncMethodCall(callback), error(0), return_val(0) {}

  /* Destructor */
  virtual ~AsyncCall_Returning<RETURN_TYPE>() {
    if(error) delete error;
  }

  /* Methods */
  Local<Value> jsReturnVal() {
    HandleScope scope;

    if(isPointer(return_val)) {
      DEBUG_ASSERT(envelope);
      Local<Object> obj = envelope->newWrapper();
      wrapPointerInObject(return_val, *envelope, obj);
      return scope.Close(obj);
    }
    else {
      return scope.Close(toJS(return_val));
    }
  }

  /* doAsyncCallback() is an async callback, run by main_thread_complete().
  */
  void doAsyncCallback(Local<Object> context) {
    HandleScope scope;
    DEBUG_PRINT("doAsyncCallback() for %s",
                (envelope && envelope->classname) ?
                envelope->classname : "{?}");
    Handle<Value> cb_args[2];

    if(error) cb_args[0] = error->toJS();
    else      cb_args[0] = Null();

    cb_args[1] = jsReturnVal();

    callback->Call(context, 2, cb_args);
  }
};


/** Second-level template class for C++ method calls:
    templated over class of native object.
    This class is the home of error handling for C++ code.
**/
template <typename R, typename C>
class NativeMethodCall : public AsyncCall_Returning<R> {
public:
  /* Member variables */
  C * native_obj;
  NativeCodeError * (*errorHandler)(R, C *);

  /* Constructor */
  NativeMethodCall<R, C>(const Arguments &args, int callback_idx) :
    AsyncCall_Returning<R>(args[callback_idx]),  /*callback*/
    errorHandler(0)
  {
    native_obj = unwrapPointer<C *>(args.Holder());
  }

  /* Methods */
  void handleErrors(void) {
    if(errorHandler) AsyncCall_Returning<R>::error = 
      errorHandler(AsyncCall_Returning<R>::return_val, native_obj);
  }
};


/** Base class templated over arguments
*/
template <typename A0, typename A1, typename A2, typename A3,
          typename A4, typename A5, typename A6, typename A7>
class Call_8_ {
public:
  /* Member variables */
  JsValueConverter<A0> arg0converter;       A0 arg0;
  JsValueConverter<A1> arg1converter;       A1 arg1;
  JsValueConverter<A2> arg2converter;       A2 arg2;
  JsValueConverter<A3> arg3converter;       A3 arg3;
  JsValueConverter<A4> arg4converter;       A4 arg4;
  JsValueConverter<A5> arg5converter;       A5 arg5;
  JsValueConverter<A6> arg6converter;       A6 arg6;
  JsValueConverter<A7> arg7converter;       A7 arg7;

  /* Constructor */
  Call_8_<A0, A1, A2, A3, A4, A5, A6, A7>(const Arguments &args) :
    arg0converter(args[0]),
    arg1converter(args[1]),
    arg2converter(args[2]),
    arg3converter(args[3]),
    arg4converter(args[4]),
    arg5converter(args[5]),
    arg6converter(args[6]),
    arg7converter(args[7])
  {
    arg0 = arg0converter.toC();
    arg1 = arg1converter.toC();
    arg2 = arg2converter.toC();
    arg3 = arg3converter.toC();
    arg4 = arg4converter.toC();
    arg5 = arg5converter.toC();
    arg6 = arg6converter.toC();
    arg7 = arg7converter.toC();
  }
};


template <typename A0, typename A1, typename A2, typename A3,
          typename A4, typename A5, typename A6>
class Call_7_ {
public:
  /* Member variables */
  JsValueConverter<A0> arg0converter;       A0 arg0;
  JsValueConverter<A1> arg1converter;       A1 arg1;
  JsValueConverter<A2> arg2converter;       A2 arg2;
  JsValueConverter<A3> arg3converter;       A3 arg3;
  JsValueConverter<A4> arg4converter;       A4 arg4;
  JsValueConverter<A5> arg5converter;       A5 arg5;
  JsValueConverter<A6> arg6converter;       A6 arg6;

  /* Constructor */
  Call_7_<A0, A1, A2, A3, A4, A5, A6>(const Arguments &args) :
    arg0converter(args[0]),
    arg1converter(args[1]),
    arg2converter(args[2]),
    arg3converter(args[3]),
    arg4converter(args[4]),
    arg5converter(args[5]),
    arg6converter(args[6])
  {
    arg0 = arg0converter.toC();
    arg1 = arg1converter.toC();
    arg2 = arg2converter.toC();
    arg3 = arg3converter.toC();
    arg4 = arg4converter.toC();
    arg5 = arg5converter.toC();
    arg6 = arg6converter.toC();
  }
};


template <typename A0, typename A1, typename A2, typename A3,
          typename A4, typename A5>
class Call_6_ {
public:
  /* Member variables */
  JsValueConverter<A0> arg0converter;       A0 arg0;
  JsValueConverter<A1> arg1converter;       A1 arg1;
  JsValueConverter<A2> arg2converter;       A2 arg2;
  JsValueConverter<A3> arg3converter;       A3 arg3;
  JsValueConverter<A4> arg4converter;       A4 arg4;
  JsValueConverter<A5> arg5converter;       A5 arg5;

  /* Constructor */
  Call_6_<A0, A1, A2, A3, A4, A5>(const Arguments &args) :
    arg0converter(args[0]),
    arg1converter(args[1]),
    arg2converter(args[2]),
    arg3converter(args[3]),
    arg4converter(args[4]),
    arg5converter(args[5])
  {
    arg0 = arg0converter.toC();
    arg1 = arg1converter.toC();
    arg2 = arg2converter.toC();
    arg3 = arg3converter.toC();
    arg4 = arg4converter.toC();
    arg5 = arg5converter.toC();
  }
};


template <typename A0, typename A1, typename A2, typename A3, typename A4>
class Call_5_ {
public:
  /* Member variables */
  JsValueConverter<A0> arg0converter;       A0 arg0;
  JsValueConverter<A1> arg1converter;       A1 arg1;
  JsValueConverter<A2> arg2converter;       A2 arg2;
  JsValueConverter<A3> arg3converter;       A3 arg3;
  JsValueConverter<A4> arg4converter;       A4 arg4;

  /* Constructor */
  Call_5_<A0, A1, A2, A3, A4>(const Arguments &args) :
    arg0converter(args[0]),
    arg1converter(args[1]),
    arg2converter(args[2]),
    arg3converter(args[3]),
    arg4converter(args[4])
  {
    arg0 = arg0converter.toC();
    arg1 = arg1converter.toC();
    arg2 = arg2converter.toC();
    arg3 = arg3converter.toC();
    arg4 = arg4converter.toC();
  }
};


template <typename A0, typename A1, typename A2, typename A3>
class Call_4_ {
public:
  /* Member variables */
  JsValueConverter<A0> arg0converter;       A0 arg0;
  JsValueConverter<A1> arg1converter;       A1 arg1;
  JsValueConverter<A2> arg2converter;       A2 arg2;
  JsValueConverter<A3> arg3converter;       A3 arg3;

  /* Constructor */
  Call_4_<A0, A1, A2, A3>(const Arguments &args) :
    arg0converter(args[0]),
    arg1converter(args[1]),
    arg2converter(args[2]),
    arg3converter(args[3])
  {
    arg0 = arg0converter.toC();
    arg1 = arg1converter.toC();
    arg2 = arg2converter.toC();
    arg3 = arg3converter.toC();
  }
};


template <typename A0, typename A1, typename A2>
class Call_3_ {
public:
  /* Member variables */
  JsValueConverter<A0> arg0converter;       A0 arg0;
  JsValueConverter<A1> arg1converter;       A1 arg1;
  JsValueConverter<A2> arg2converter;       A2 arg2;

  /* Constructor */
  Call_3_<A0, A1, A2>(const Arguments &args) :
    arg0converter(args[0]),
    arg1converter(args[1]),
    arg2converter(args[2])
  {
    arg0 = arg0converter.toC();
    arg1 = arg1converter.toC();
    arg2 = arg2converter.toC();
  }
};


template <typename A0, typename A1>
class Call_2_ {
public:
  /* Member variables */
  JsValueConverter<A0> arg0converter;       A0 arg0;
  JsValueConverter<A1> arg1converter;       A1 arg1;

  /* Constructor */
  Call_2_<A0, A1>(const Arguments &args) :
    arg0converter(args[0]),
    arg1converter(args[1])
  {
    arg0 = arg0converter.toC();
    arg1 = arg1converter.toC();
  }
};


template <typename A0>
class Call_1_ {
public:
  /* Member variables */
  JsValueConverter<A0> arg0converter;       A0 arg0;

  /* Constructor */
  Call_1_<A0>(const Arguments &args) :
    arg0converter(args[0])
  {
    arg0 = arg0converter.toC();
  }
};

