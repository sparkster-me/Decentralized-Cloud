#include "pch.h"

std::string const functionId = random_generator();
std::string const funPostDef = "function myFunction(x, y) { return x+y;}";
std::string const funPutDef = "function myFunction(x, y) { return x*y;}";

/*
Store function definition in storage node
*/
TEST(Function, AddFunction) {
  std::string input = "c:6|txid:1234567890|k:0123456789|tid:9876543210|tm:1944631469190|cmds:3 " + functionId + " " + encode(funPostDef);
  json response = getWSClient()->send(input);
  EXPECT_EQ(response["txid"].get<std::string>(), "1234567890");
  EXPECT_TRUE(true);
}

/*
Retrieve function definition in storage node
*/
TEST(Function, fetchFunction) {
	std::string input = "c:10|txid:1234567890|k:0123456789|tid:9876543210|tm:1944631469190|id:" + functionId;
	json response = getWSClient()->get(input);
	EXPECT_EQ(response["txid"].get<std::string>(), "1234567890");
	EXPECT_EQ(response["data"].get<std::string>(), funPostDef);
	EXPECT_TRUE(true);
}

/*
Update function definition in storage node
*/
TEST(Function, updateFunction) {
	std::string input = "c:6|txid:1234567890|k:0123456789|tid:9876543210|tm:1944631469190|cmds:6 " + functionId + " " + encode(funPutDef);
	json response = getWSClient()->send(input);
	EXPECT_EQ(response["txid"].get<std::string>(), "1234567890");
	
	EXPECT_TRUE(true);
}

/*
Retrieve updated function definition in storage node
*/
TEST(Function, fetchUpdatedFunction) {
	std::string input = "c:10|txid:1234567890|k:0123456789|tid:9876543210|tm:1944631469190|id " + functionId;
	json response = getWSClient()->get(input);
	EXPECT_EQ(response["txid"].get<std::string>(), "1234567890");
	EXPECT_EQ(response["data"].get<std::string>(), funPutDef);
	EXPECT_TRUE(true);
}

/*
Delete function definition in storage node
*/
TEST(Function, deleteFunction) {
	std::string input = "c:10|txid:1234567890|k:0123456789|tid:9876543210|tm:1944631469190|cmds:9 " + functionId;
	json response = getWSClient()->send(input);
	EXPECT_EQ(response["txid"].get<std::string>(), "1234567890");
	EXPECT_EQ(response["data"].get<std::string>(), "deleted successfully");
	EXPECT_TRUE(true);
}

/*
Retrieve deleted function definition in storage node with empty response
*/
TEST(Function, fetchDeletedFunction) {
	std::string input = "c:10|txid:1234567890|k:0123456789|tid:9876543210|tm:1944631469190|id " + functionId;
	json response = getWSClient()->get(input);
	EXPECT_EQ(response["txid"].get<std::string>(), "1234567890");
	EXPECT_EQ(response["data"].get<std::string>(), "");
	EXPECT_TRUE(true);
}