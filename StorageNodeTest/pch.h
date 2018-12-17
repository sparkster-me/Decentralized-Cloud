//
// pch.h
// Header for standard system include files.
//

#pragma once

#include "gtest/gtest.h"
#include "WSClient.cpp"
#include <nlohmann/json.hpp>
#include <boost/uuid/uuid.hpp>            // uuid class
#include <boost/uuid/uuid_generators.hpp> // generators
#include <boost/uuid/uuid_io.hpp>         // streaming operators etc.
#include <boost/beast/core/detail/base64.hpp>

using json = nlohmann::json;

WSClient* wsClient = nullptr;

std::string encode(std::string text);
std::string random_generator();
WSClient* getWSClient();