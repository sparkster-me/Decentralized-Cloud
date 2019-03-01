#include "JsProcessor.h"

json JsProcessor::executeScript(const std::string& fundef, const std::string& input, const std::string& tenantid) {
	try {
		std::string code;
		if (input.empty()) {
			code = fundef;
		}
		else {
			//inject input parameters in funtion definition
			std::string decoded = decode(input);
			code = parseScript(fundef, decoded);
		}

		code = code + cmdBuilderScript;

		// Create a new isolate object and bind it to current scope
		Isolate::CreateParams create_params;
		create_params.array_buffer_allocator =
			v8::ArrayBuffer::Allocator::NewDefaultAllocator();
		Isolate* isolate_ = Isolate::New(create_params);
		Isolate::Scope isolate_scope(isolate_);
		// Create a stack-allocated handle scope.
		HandleScope handle_scope(isolate_);

		// Create a string containing the JavaScript source code.
		const char* src = code.c_str();
		v8::Local < v8::String > script = v8::String::NewFromUtf8(isolate_, src, v8::NewStringType::kNormal).ToLocalChecked();

		// Create a template for the global object where we set the
		// built-in global functions.
		Local<ObjectTemplate> global = ObjectTemplate::New(isolate_);
		global->Set(String::NewFromUtf8(isolate_, "fetchDocument", NewStringType::kNormal)
			.ToLocalChecked(),
			FunctionTemplate::New(isolate_, fetchDocument));
		global->Set(String::NewFromUtf8(isolate_, "encode", NewStringType::kNormal)
			.ToLocalChecked(),
			FunctionTemplate::New(isolate_, encode));
		global->Set(String::NewFromUtf8(isolate_, "tenantid", NewStringType::kNormal)
			.ToLocalChecked(), v8::String::NewFromUtf8(isolate_, tenantid.c_str(), v8::NewStringType::kNormal).ToLocalChecked());

		// Each processor gets its own context so different processors don't
		// affect each other. Context::New returns a persistent handle which
		// is what we need for the reference to remain after we return from
		// this method. That persistent handle has to be disposed in the
		// destructor.
		v8::Local<v8::Context> context = Context::New(isolate_, NULL, global);
		context_.Reset(isolate_, context);
		// Enter the new context so all the following operations take place
		// within it.
		Context::Scope context_scope(context);

		// We're just about to compile the script; set up an error handler to
		// catch any exceptions the script might throw.
		TryCatch try_catch(isolate_);
		// Compile the script and check for errors.
		Local<Script> compiled_script;
		if (!Script::Compile(context, script).ToLocal(&compiled_script)) {
			String::Utf8Value error(isolate_, try_catch.Exception());
			log(*error);
			// The script failed to compile; bail out.
			throw exception(*error);
		}

		// Run the script!
		Local<Value> result;
		if (!compiled_script->Run(context).ToLocal(&result)) {
			// The TryCatch above is still in effect and will have caught the error.
			String::Utf8Value error(isolate_, try_catch.Exception());
			log(*error);
			// Running the script failed; bail out.
			throw exception(*error);
		}

		// The script compiled and ran correctly.  Now we fetch out the
		// Process function from the global object.
		std::string function_name = getProcessFunName(code);
		Local<String> process_name =
			String::NewFromUtf8(isolate_, function_name.c_str(), NewStringType::kNormal)
			.ToLocalChecked();

		Local<Value> process_val;
		// If there is no Process function, or if it is not a function,
		// bail out
		if (!context->Global()->Get(context, process_name).ToLocal(&process_val) ||
			!process_val->IsFunction()) {
			return false;
		}

		// It is a function; cast it to a Function
		Local<Function> process_fun = Local<Function>::Cast(process_val);

		//execute process and store result
		Local<Value> js_result;
		if (!process_fun->Call(context, context->Global(), 0, NULL).ToLocal(&js_result)) {
			String::Utf8Value error(isolate_, try_catch.Exception());
			log(*error);
			throw exception(*error);
		}

		v8::String::Utf8Value utf8(isolate_, js_result);
		std::string finalresult = std::string(*utf8);
		printf("Function Result : %s  \n", finalresult.c_str());
		// Get the js object
		v8::Local<v8::String> local_val = Local<v8::String>::Cast(context->Global()->Get(String::NewFromUtf8(isolate_, stateChangeCmds.c_str(), NewStringType::kNormal)
			.ToLocalChecked()));
		v8::String::Utf8Value utfstr(isolate_, local_val);
		std::string stateChangeCmd = std::string(*utfstr);

		process_fun.Clear();
		context.Clear();

		json j;
		j["result"] = (finalresult.empty() || finalresult == "undefined") ? functionEmptyResponse : finalresult;
		j["statechangecmds"] = stateChangeCmd;
		return j;
	}
	catch (std::exception e) {
		throw e;
	}
}

std::string JsProcessor::getProcessFunName(std::string& script) {
	try {
		string x = "function ";
		std::string name = script.substr(x.length());
		name = name.substr(0, name.find('('));
		boost::trim_right(name);
		return name;
	}
	catch (std::exception e) {
		throw e;
	}
}

std::string JsProcessor::parseScript(const std::string & script, const std::string & input) {
	json js;
	try {
		js = json::parse(input);
	}
	catch (std::exception e) {
		throw exception("Invalid input");
	}
	std::string statement = "";
	for (auto it = js.begin(); it != js.end(); ++it) {
		std::string key = it.key();
		statement = statement + " var " + key + " = " + js[key].dump() + ";";
	}
	std::string code = script;
	code.insert(functionLogicStartIndex(code) + 1, statement);
	return code;
}

int JsProcessor::functionLogicStartIndex(const std::string& script) {
	try {
		char closeParanSym = ')';
		bool closeParanSymFound = false;
		char openBraceSym = '{';
		for (int i = 0; i < script.length(); i++) {
			char c = script[i];
			if (closeParanSym == c) {
				closeParanSymFound = true;
			}
			else if (openBraceSym == c) {
				if (closeParanSymFound) {
					return i;
				}
			}
		}
	}
	catch (std::exception e) {
		throw e;
	}
}

std::string JsProcessor::decode(std::string s) {
	return boost::beast::detail::base64_decode(s);
}

std::string JsProcessor::encode(std::string s) {
	return boost::beast::detail::base64_encode(s);
}

void JsProcessor::encode(const v8::FunctionCallbackInfo<v8::Value>& args) {
	Isolate* isolate = args.GetIsolate();
	HandleScope scope(isolate);
	Local<Value> arg = args[0];
	String::Utf8Value value(isolate, arg);
	std::string input(*value);
	std::string s = boost::beast::detail::base64_encode(input);
	Local<String> retval = String::NewFromUtf8(isolate, s.c_str());
	args.GetReturnValue().Set(retval);
}

std::string JsProcessor::generateUUID() {
	boost::uuids::uuid uuid = boost::uuids::random_generator()();
	return boost::uuids::to_string(uuid);
}

string getFilePath(const string& fileName) {
	try {
		string path = __FILE__; //gets source code path, include file name
		path = path.substr(0, 1 + path.find_last_of('\\')); //removes file name
		path += fileName; //adds input file to path
		//path = "\\" + path;
		return path;
	}
	catch (std::exception e) {
		throw e;
	}
}

std::string JsProcessor::loadCmdBuilderScript() {
	try {
		if (cmdBuilderScript.empty()) {
			std::string fpath = getFilePath("CommandBuilder.txt");
			std::ifstream in(fpath);
			cmdBuilderScript = static_cast<std::stringstream const&>(std::stringstream() << in.rdbuf()).str();
			in.close();
		}
		return cmdBuilderScript;
	}
	catch (std::exception e) {
		throw e;
	}
}

void JsProcessor::log(const char* event) {
	printf("Logged: %s\n", event);
}
