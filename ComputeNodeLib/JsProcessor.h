#pragma once
#include <v8.h>
#include <libplatform/libplatform.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <string>
#include <chrono>
#include <iostream>
#include <sstream> 
#include <boost/algorithm/string.hpp>
#include <nlohmann/json.hpp>
#include <boost/beast/core/detail/base64.hpp>

using json = nlohmann::json;

using namespace v8;
using namespace std;

class JsProcessor
{
public:
	// Method to execute script. If execution successful return result else reason for failure
	std::string executeScript(std::string& script, std::string& input);
	// Method to retrieve function name from definition
	std::string getProcessFunName(std::string& script);
	// Method to insert input data into function definition
	std::string parseScript(std::string& script, std::string& input);
	// Method to find function logic start index
	int functionLogicStartIndex(std::string& script);
	std::string decode(std::string s);
	std::string encode(std::string s);

private:
	//Print error message
	void Log(const char* event);
	Global<Context> context_;
	std::string process_fun = "process_fun";
};