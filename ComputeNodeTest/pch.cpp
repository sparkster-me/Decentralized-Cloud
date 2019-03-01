//
// pch.cpp
// Include the standard header and generate the precompiled header.
//
#pragma once
#include "pch.h"

JsProcessor* getJsProcessor() {
	jsProcessor = new JsProcessor();
	return jsProcessor;
}

void JsProcessor::fetchDocument(const v8::FunctionCallbackInfo<v8::Value>& args)
{
	printf("Logged: %s\n", "i called from js");
	if (args.Length() < 0) {
		return;
	}
	Isolate* isolate = args.GetIsolate();
	printf("*********** ", isolate->GetData(0));
	HandleScope scope(isolate);
	Local<Value> arg = args[0];
	String::Utf8Value value(isolate, arg);
	printf("Logged: %s\n js input : ", *value);
		
	Local<String> retval = String::NewFromUtf8(isolate, *value);
	args.GetReturnValue().Set(retval);
}

