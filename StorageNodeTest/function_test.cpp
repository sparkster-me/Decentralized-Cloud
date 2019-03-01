#include "pch.h"

std::string functionId = random_generator();
std::string const funPostDef = "{\"code\":\"function myFunction(x, y) { return x+y;}\"}";
std::string const funPutDef = "{\"code\":\"function myFunction(x, y) { return x*y;}\"}";

/*
Store function definition in storage node
*/
TEST(Function, AddFunction) {
  std::string input = "c:6|txid:1234567890|k:0123456789|tid:9876543210|cmds:3 " + encode(funPostDef);
  json response = getWSClient()->send(input);
  EXPECT_EQ(response["ack"]["id"].get<std::string>(), response["result"]["txid"].get<std::string>());
  functionId = response["result"]["id"].get<std::string>();
  EXPECT_TRUE(true);
}

/*
Retrieve function definition in storage node
*/
TEST(Function, fetchFunction) {
	std::string input = "c:10|txid:1234567890|k:0123456789|tid:9876543210|id:" + functionId;
	json response = getWSClient()->send(input);
	EXPECT_EQ(response["ack"]["id"].get<std::string>(), response["result"]["txid"].get<std::string>());
	EXPECT_EQ(response["result"]["data"].get<std::string>(), funPostDef);
	EXPECT_TRUE(true);
}

/*
Update function definition in storage node
*/
TEST(Function, updateFunction) {
	std::string input = "c:6|txid:1234567890|k:0123456789|tid:9876543210|cmds:6 " + functionId + " " + encode(funPutDef);
	json response = getWSClient()->send(input);
	EXPECT_EQ(response["ack"]["id"].get<std::string>(), response["result"]["txid"].get<std::string>());	
	EXPECT_TRUE(true);
}

/*
Retrieve updated function definition in storage node
*/
TEST(Function, fetchUpdatedFunction) {
	std::string input = "c:10|txid:1234567890|k:0123456789|tid:9876543210|id:" + functionId;
	json response = getWSClient()->send(input);
	EXPECT_EQ(response["ack"]["id"].get<std::string>(), response["result"]["txid"].get<std::string>());
	EXPECT_EQ(response["result"]["data"].get<std::string>(), funPutDef);
	EXPECT_TRUE(true);
}

/*
Delete function definition in storage node
*/
TEST(Function, deleteFunction) {
	std::string input = "c:6|txid:1234567890|k:0123456789|tid:9876543210|cmds:9 " + functionId;
	json response = getWSClient()->send(input);
	EXPECT_EQ(response["ack"]["id"].get<std::string>(), response["result"]["txid"].get<std::string>());
	EXPECT_EQ(response["result"]["data"].get<std::string>(), "deleted successfully");
	EXPECT_TRUE(true);
}

/*
Retrieve deleted function definition in storage node with empty response
*/
TEST(Function, fetchDeletedFunction) {
	std::string input = "c:10|txid:1234567890|k:0123456789|tid:9876543210|id:" + functionId;
	json response = getWSClient()->send(input);
	EXPECT_EQ(response["ack"]["id"].get<std::string>(), response["result"]["txid"].get<std::string>());
	EXPECT_EQ(response["result"]["data"].get<std::string>(), "No function found for the given Id");
	EXPECT_TRUE(true);
}