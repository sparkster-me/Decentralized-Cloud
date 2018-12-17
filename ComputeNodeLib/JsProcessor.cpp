#include "JsProcessor.h"

std::string JsProcessor::executeScript(std::string& fundef, std::string& input) {
	try {
		std::string code;
		if (input.empty()) {
			code = fundef;
		} else {
			//inject input parameters in funtion definition
			std::string decoded = decode(input);
			code = parseScript(fundef, decoded);
		}
		// Create a new isolate object and bind it to current scope
		Isolate::CreateParams create_params;
		create_params.array_buffer_allocator =
			v8::ArrayBuffer::Allocator::NewDefaultAllocator();
		Isolate* isolate_ = Isolate::New(create_params);
		Isolate::Scope isolate_scope(isolate_);
		HandleScope scope(isolate_);

		// Create a string containing the JavaScript source code.
		const char* src = code.c_str();
		v8::Local < v8::String > script = v8::String::NewFromUtf8(isolate_, src, v8::NewStringType::kNormal).ToLocalChecked();

		// Create a stack-allocated handle scope.
		HandleScope handle_scope(isolate_);

		// Create a template for the global object where we set the
		// built-in global functions.
		Local<ObjectTemplate> global = ObjectTemplate::New(isolate_);
		global->Set(String::NewFromUtf8(isolate_, "log", NewStringType::kNormal)
			.ToLocalChecked(),
			FunctionTemplate::New(isolate_));

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
			Log(*error);
			// The script failed to compile; bail out.
			throw std::string(*error);
		}

		// Run the script!
		Local<Value> result;
		if (!compiled_script->Run(context).ToLocal(&result)) {
			// The TryCatch above is still in effect and will have caught the error.
			String::Utf8Value error(isolate_, try_catch.Exception());
			Log(*error);
			// Running the script failed; bail out.
			throw std::string(*error);
		}

		// The script compiled and ran correctly.  Now we fetch out the
		// Process function from the global object.
		process_fun = getProcessFunName(code);
		Local<String> process_name =
			String::NewFromUtf8(isolate_, process_fun.c_str(), NewStringType::kNormal)
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
			Log(*error);
			throw std::string(*error);
		}

		v8::String::Utf8Value utf8(isolate_, js_result);
		printf("Function Result : %s  \n", *utf8);

		return std::string(*utf8);
	} catch (const std::exception& e) {
		std::cout << e.what() << std::endl;
		throw e;
	}
}

std::string JsProcessor::getProcessFunName(std::string& script) {
	string x = "function ";
	std::string name = script.substr(x.length());
	name = name.substr(0, name.find('('));
	boost::trim_right(name);
	return name;
}

std::string JsProcessor::parseScript(std::string & script, std::string & input) {
	json js;
	try {
		js = json::parse(input);
	} catch (std::exception e) {
		throw exception("Invalid input");
	}
	std::string statement = "";
	for (auto it = js.begin(); it != js.end(); ++it) {
		std::string key = it.key();
		statement = statement + " var " + key + " = " + js[key].dump() + ";";
	}
	std::string code = script.insert(functionLogicStartIndex(script) + 1, statement);
	return code;
}

int JsProcessor::functionLogicStartIndex(std::string& script) {
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

std::string JsProcessor::decode(std::string s) {
	return boost::beast::detail::base64_decode(s);
}

std::string JsProcessor::encode(std::string s) {
	return boost::beast::detail::base64_encode(s);
}

void JsProcessor::Log(const char* event) {
	printf("Logged: %s\n", event);
}