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
#include <boost/uuid/uuid.hpp>            // uuid class
#include <boost/uuid/uuid_generators.hpp> // generators
#include <boost/uuid/uuid_io.hpp>         // streaming operators etc.

using json = nlohmann::json;

using namespace v8;
using namespace std;
static std::string cmdBuilderScript;
class JsProcessor
{
public:
	// Method to execute script. If execution successful return result else reason for failure
	json executeScript(const std::string& script, const std::string& input, const std::string& tenantid);
	// Method to retrieve function name from definition
	std::string getProcessFunName(std::string& script);
	// Method to insert input data into function definition
	std::string parseScript(const std::string& script, const std::string& input);
	// Method to find function logic start index
	int functionLogicStartIndex(const std::string& script);
	// Method to base64 decode given text  
	std::string decode(std::string s);
	// Method to base64 encode given text
	std::string encode(std::string s);
	static void encode(const v8::FunctionCallbackInfo<v8::Value>& args);
	// Method to generate random number
	static std::string generateUUID();
	// Method to call c++ function from javascript
	static void fetchDocument(const v8::FunctionCallbackInfo<v8::Value>& args);
	// Method to call c++ function from javascript
	static std::string loadCmdBuilderScript();
private:
	//Print error message
	void log(const char* event);
	Global<Context> context_;
	std::string stateChangeCmds = "stateChangeCmds";
	std::string functionEmptyResponse = "N/A";
};