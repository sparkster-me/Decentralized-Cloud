//
// pch.h
// Header for standard system include files.
//

#pragma once

#include "gtest/gtest.h"
#include "../ComputeNodeLib/JsProcessor.h"

JsProcessor* jsProcessor = nullptr;
JsProcessor* getJsProcessor();

