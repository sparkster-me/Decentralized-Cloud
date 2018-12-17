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

