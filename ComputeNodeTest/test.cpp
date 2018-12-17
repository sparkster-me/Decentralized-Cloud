#pragma once
#include "pch.h"

TEST(ExecuteFunction, noArguments) {
  std::string code = "function myFunction() {return 5 * 4;}";
  std::string i;
  std::string res = getJsProcessor()->executeScript(code, i);
  EXPECT_EQ("20", res);
  EXPECT_TRUE(true);
}

TEST(ExecuteFunction, withArguments) {
	std::string code = "function myFunction(p1, p2) {return p1 * p2;}";
	std::string input = getJsProcessor()->encode("{\"p1\":9,\"p2\":5}");
	std::string res = getJsProcessor()->executeScript(code, input);
	EXPECT_EQ("45", res);
	EXPECT_TRUE(true);
}

TEST(ExecuteFunction, deafultValArguments) {
	std::string code = "function myFunction(p1, p2, p3=10) {return p1 * p2 * p3;}";
	std::string input = getJsProcessor()->encode("{\"p1\":7,\"p2\":5}");
	std::string res = getJsProcessor()->executeScript(code, input);
	EXPECT_EQ("350", res);
	EXPECT_TRUE(true);
}

TEST(ExecuteFunction, objectAsArgument) {
	std::string code = "function myFunction(obj) {return obj.p1 * obj.p2;}";
	std::string input = getJsProcessor()->encode("{\"obj\": {\"p1\":11,\"p2\":5}}");
	std::string res = getJsProcessor()->executeScript(code, input);
	EXPECT_EQ("55", res);
	EXPECT_TRUE(true);
}

TEST(ExecuteFunction, objectDefaultValAsArgument) {
	std::string code = "function myFunction(obj={\"p1\":11,\"p2\":5}) {return obj.p1 * obj.p2;}";
	std::string i;
	std::string res = getJsProcessor()->executeScript(code, i);
	EXPECT_EQ("55", res);
	EXPECT_TRUE(true);
}

TEST(ExecuteFunction, defaultValWithDynamicArgument) {
	std::string code = "function myFunction(obj={\"p1\":2,\"p2\":3}, p3, p4) {return obj.p1 * obj.p2 * p3 * p4;}";
	std::string input = getJsProcessor()->encode("{\"p3\":4,\"p4\":5}");
	std::string res = getJsProcessor()->executeScript(code, input);
	EXPECT_EQ("120", res);
	EXPECT_TRUE(true);
}

int main(int argc, char* argv[]) {
	//Initialize js engine
	v8::V8::InitializeICUDefaultLocation(argv[0]);
	v8::V8::InitializeExternalStartupData(argv[0]);
	std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
	v8::V8::InitializePlatform(platform.get());
	v8::V8::Initialize();


	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}