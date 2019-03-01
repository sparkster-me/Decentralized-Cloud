#pragma once
#include "pch.h"

TEST(ExecuteFunction, noArguments) {
	std::string code = "function myFunction() {return 5 * 4;}";
	//std::string code = "function myFunction() {return btoa('hello world');}";
	//std::string code = "function myFunction() {return Buffer.from('Hello World!').toString('base64');}";
	std::string i;
	json res = getJsProcessor()->executeScript(code, i, "292fec76-5f1c-486f-85a5-09d88096f098");
	EXPECT_EQ("20", res["result"].get<std::string>());
	EXPECT_TRUE(true);
}

TEST(ExecuteFunction, withArguments) {
	std::string code = "function myFunction(p1, p2) {return p1 * p2;}";
	std::string input = getJsProcessor()->encode("{\"p1\":9,\"p2\":5}");
	json res = getJsProcessor()->executeScript(code, input, "292fec76-5f1c-486f-85a5-09d88096f098");
	EXPECT_EQ("45", res["result"].get<std::string>());
	EXPECT_TRUE(true);
}

TEST(ExecuteFunction, deafultValArguments) {
	std::string code = "function myFunction(p1, p2, p3=10) {return p1 * p2 * p3;}";
	std::string input = getJsProcessor()->encode("{\"p1\":7,\"p2\":5}");
	json res = getJsProcessor()->executeScript(code, input, "292fec76-5f1c-486f-85a5-09d88096f098");
	EXPECT_EQ("350", res["result"].get<std::string>());
	EXPECT_TRUE(true);
}

TEST(ExecuteFunction, objectAsArgument) {
	std::string code = "function myFunction(obj) {return obj.p1 * obj.p2;}";
	std::string input = getJsProcessor()->encode("{\"obj\": {\"p1\":11,\"p2\":5}}");
	json res = getJsProcessor()->executeScript(code, input, "292fec76-5f1c-486f-85a5-09d88096f098");
	EXPECT_EQ("55", res["result"].get<std::string>());
	EXPECT_TRUE(true);
}

TEST(ExecuteFunction, objectDefaultValAsArgument) {
	std::string code = "function myFunction(obj={\"p1\":11,\"p2\":5}) {return obj.p1 * obj.p2;}";
	std::string i;
	json res = getJsProcessor()->executeScript(code, i, "292fec76-5f1c-486f-85a5-09d88096f098");
	EXPECT_EQ("55", res["result"].get<std::string>());
	EXPECT_TRUE(true);
}

TEST(ExecuteFunction, defaultValWithDynamicArgument) {
	std::string code = "function myFunction(obj={\"p1\":2,\"p2\":3}, p3, p4) {return obj.p1 * obj.p2 * p3 * p4;}";
	std::string input = getJsProcessor()->encode("{\"p3\":4,\"p4\":5}");
	json res = getJsProcessor()->executeScript(code, input, "292fec76-5f1c-486f-85a5-09d88096f098");
	EXPECT_EQ("120", res["result"].get<std::string>());
	EXPECT_TRUE(true);
}

TEST(ExecuteFunction, invalidInput) {
	EXPECT_THROW({
		try
		{
			std::string code = "function myFunction(obj={\"p1\":2,\"p2\":3}, p3, p4) {return obj.p1 * obj.p2 * p3 * p4;}";
			std::string input = getJsProcessor()->encode("{\"p3\":4,\"p4\":5");
			json res = getJsProcessor()->executeScript(code, input, "292fec76-5f1c-486f-85a5-09d88096f098");
		}
		catch (const exception& e)
		{
			// and this tests that it has the correct message
			EXPECT_STREQ("Invalid input", e.what());
			throw;
		}
		}, exception);
}

TEST(ExecuteFunction, invalidScript) {
	EXPECT_THROW({
		try
		{
			std::string code = "function myFunction(obj={\"p1\":2,\"p2\":3}, p3, p4) {return obj.p1 * obj.p2 * p3 * p4;";
			std::string input = getJsProcessor()->encode("{\"p3\":4,\"p4\":5}");
			json res = getJsProcessor()->executeScript(code, input, "292fec76-5f1c-486f-85a5-09d88096f098");
		}
		catch (const exception& e)
		{
			// and this tests that it has the correct message
			EXPECT_STREQ("SyntaxError: Unexpected end of input", e.what());
			throw;
		}
		}, exception);
}

TEST(ExecuteFunction, cFunctionBind) {
	std::string code = "function myFunction() { var k = 'test val '; var s = fetchDocument(k); return s + 5 + 4;}";
	std::string i;
	json res = getJsProcessor()->executeScript(code, i, "292fec76-5f1c-486f-85a5-09d88096f098");
	EXPECT_EQ("test val 54", res["result"].get<std::string>());
	EXPECT_TRUE(true);
}

TEST(ExecuteFunction, getInputCmdBuilderFile) {
	std::string res = getJsProcessor()->loadCmdBuilderScript();
	std::cout << res << std::endl;
	EXPECT_FALSE(res.empty());
	EXPECT_TRUE(true);
}


TEST(ExecuteFunction, inputCmdBuild) {
	std::string cmd = "execute('{\"entityinfo\":{\"entity\":\"servicemetadata\",\"tenantid\":\"292fec76-5f1c-486f-85a5-09d88096f098\",\"timestamp\":\"2015-08-31 15:06:33\"},\"collections\":{\"servicedirectory\":{\"meta\":{\"parentreference\":\"****\",\"pkname\":\"****\",\"fkname\":\"****\"},\"rowset\":[],\"rowfilter\":[{\"attribute\":\"servicename\",\"operator\":\"==\",\"value\":\"doctest009\"}]}}}',125, \"template\", \"POST\", false)";
	std::string code = "function myFunction() { var cmd = " + cmd + "; var s = fetchDocument(cmd); return s;}";
	std::string i;
	json res = getJsProcessor()->executeScript(code, i, "292fec76-5f1c-486f-85a5-09d88096f098");
	std::cout << res["result"].get<std::string>() << std::endl;
	std::string vec = res["statechangecmds"].get<std::string>();
	std::cout << vec << std::endl;
	//EXPECT_EQ("1154", res);
	EXPECT_TRUE(true);
}

TEST(ExecuteFunction, cVariableBind) {
	std::string code = "function myFunction() { var k = tenantid; return k;}";
	std::string i;
	json res = getJsProcessor()->executeScript(code, i, "292fec76-5f1c-486f-85a5-09d88096f098");
	EXPECT_EQ("292fec76-5f1c-486f-85a5-09d88096f098", res["result"].get<std::string>());
	EXPECT_TRUE(true);
}


int main(int argc, char* argv[]) {
	//Initialize js engine
	v8::V8::InitializeICUDefaultLocation(argv[0]);
	v8::V8::InitializeExternalStartupData(argv[0]);
	std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
	v8::V8::InitializePlatform(platform.get());
	v8::V8::Initialize();

	JsProcessor::loadCmdBuilderScript();

	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}