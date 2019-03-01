#include "pch.h"

std::string templateId = "";
std::string const tempPostDef = "{\"entityInfo\":{\"tenantId\":\"292FEC76-5F1C-486F-85A5-09D88096F098\",\"timeStamp\":\"\",\"entity\":\"EntityFormation\",\"servicetype\":\"7C5DBC9D-8DEB-4147-838A-16F97F0CF867\"},\"collections\":{\"EntityObjectMap\":{\"rowset\":[{\"applicationObjectId\":\"db4585d0-4b57-4dea-b072-63d0643bc50c\",\"applicationEntityId\":\"af92c5ef-62ef-4f89-9ebc-4050a6a3a11a\",\"applicationEntityObjectId\":\"a450938a-2733-41fd-ae07-7b85b90ce2e3\",\"applicationIsLookupObject\":\"false\",\"applicationId\":\"0000000-0000-0000-0000-000000000000\",\"tenantId\":\"292FEC76-5F1C-486F-85A5-09D88096F098\"}],\"meta\":{\"fkName\":\"null\",\"parentReference\":\"null\",\"pkName\":\"null\"}},\"Entity\":{\"rowset\":[{\"tenantApplicationId\":\"0000000-0000-0000-0000-000000000000\",\"tenantId\":\"292FEC76-5F1C-486F-85A5-09D88096F098\",\"tenantEntityId\":\"af92c5ef-62ef-4f89-9ebc-4050a6a3a11a\",\"tenantEntityName\":\"abcde\",\"displayName\":\"Abcde\",\"entityType\":\"business_service\"}],\"meta\":{\"fkName\":\"null\",\"parentReference\":\"null\",\"pkName\":\"null\"}},\"EntityObjectRelationship\":{\"rowset\":[{\"tenantEntityObjectRelationshipId\":\"5832d226-6206-46d8-881f-745bd58d9bee\",\"tenantParentObjectId\":\"db4585d0-4b57-4dea-b072-63d0643bc50c\",\"tenantChildObjectId\":\"db4585d0-4b57-4dea-b072-63d0643bc50c\",\"tenantObjectRelationshipTypeId\":\"D7A139D1-662D-4BAD-9C8E-7CE80F5527F0\",\"tenantApplicationId\":\"0000000-0000-0000-0000-000000000000\",\"tenantId\":\"292FEC76-5F1C-486F-85A5-09D88096F098\"}],\"meta\":{\"fkName\":\"null\",\"parentReference\":\"null\",\"pkName\":\"null\"}}}}";
std::string const tempPutDef = "{\"entityInfo\":{\"tenantId\":\"292FEC76-5F1C-486F-85A5-09D88096F098\",\"timeStamp\":\"\",\"entity\":\"EntityFormation\",\"servicetype\":\"7C5DBC9D-8DEB-4147-838A-16F97F0CF867\"},\"collections\":{\"EntityObjectMap\":{\"rowset\":[{\"applicationObjectId\":\"db4585d0-4b57-4dea-b072-63d0643bc50c\",\"applicationEntityId\":\"af92c5ef-62ef-4f89-9ebc-4050a6a3a11a\",\"applicationEntityObjectId\":\"a450938a-2733-41fd-ae07-7b85b90ce2e3\",\"applicationIsLookupObject\":\"true\",\"applicationId\":\"0000000-0000-0000-0000-000000000000\",\"tenantId\":\"292FEC76-5F1C-486F-85A5-09D88096F098\"}],\"meta\":{\"fkName\":\"null\",\"parentReference\":\"null\",\"pkName\":\"null\"}},\"Entity\":{\"rowset\":[{\"tenantApplicationId\":\"0000000-0000-0000-0000-000000000000\",\"tenantId\":\"292FEC76-5F1C-486F-85A5-09D88096F098\",\"tenantEntityId\":\"af92c5ef-62ef-4f89-9ebc-4050a6a3a11a\",\"tenantEntityName\":\"123456\",\"displayName\":\"654123\",\"entityType\":\"business_service\"}],\"meta\":{\"fkName\":\"null\",\"parentReference\":\"null\",\"pkName\":\"null\"}},\"EntityObjectRelationship\":{\"rowset\":[{\"tenantEntityObjectRelationshipId\":\"5832d226-6206-46d8-881f-745bd58d9bee\",\"tenantParentObjectId\":\"db4585d0-4b57-4dea-b072-63d0643bc50c\",\"tenantChildObjectId\":\"db4585d0-4b57-4dea-b072-63d0643bc50c\",\"tenantObjectRelationshipTypeId\":\"D7A139D1-662D-4BAD-9C8E-7CE80F5527F0\",\"tenantApplicationId\":\"0000000-0000-0000-0000-000000000000\",\"tenantId\":\"292FEC76-5F1C-486F-85A5-09D88096F098\"}],\"meta\":{\"fkName\":\"null\",\"parentReference\":\"null\",\"pkName\":\"null\"}}}}";

/*
Store template definition in storage node
*/
TEST(Template, AddTemplate) {
	std::string input = "c:6|txid:1234567890|k:0123456789|tid:9876543210|cmds:1 " +  encode(tempPostDef);
	json response = getWSClient()->send(input);	
	EXPECT_EQ(response["ack"]["id"].get<std::string>(), response["result"]["txid"].get<std::string>());
	templateId = response["result"]["id"].get<std::string>();
	EXPECT_TRUE(true);
}

/*
Retrieve template definition in storage node
*/
TEST(Template, fetchTemplate) {
	std::string input = "c:11|txid:1234567890|k:0123456789|tid:9876543210|id:" + templateId;
	json response = getWSClient()->send(input);
	EXPECT_EQ(response["ack"]["id"].get<std::string>(), response["result"]["txid"].get<std::string>());
	EXPECT_EQ(response["result"]["data"].get<std::string>(), tempPostDef);
	EXPECT_TRUE(true);
}

/*
Update template definition in storage node
*/
TEST(Template, updateTemplate) {
	std::string input = "c:6|txid:1234567890|k:0123456789|tid:9876543210|cmds:4 " + templateId + " " + encode(tempPutDef);
	json response = getWSClient()->send(input);
	EXPECT_EQ(response["ack"]["id"].get<std::string>(), response["result"]["txid"].get<std::string>());
	EXPECT_TRUE(true);
}

/*
Retrieve updated template definition in storage node
*/
TEST(Template, fetchUpdatedTemplate) {
	std::string input = "c:11|txid:1234567890|k:0123456789|tid:9876543210|id:" + templateId;
	json response = getWSClient()->send(input);
	EXPECT_EQ(response["ack"]["id"].get<std::string>(), response["result"]["txid"].get<std::string>());
	//EXPECT_EQ(response["result"]["data"].get<std::string>(), tempPutDef);
	EXPECT_TRUE(true);
}

/*
Delete template definition in storage node
*/
TEST(Template, deleteTemplate) {
	std::string input = "c:6|txid:1234567890|k:0123456789|tid:9876543210|cmds:7 " + templateId;
	json response = getWSClient()->send(input);
	EXPECT_EQ(response["ack"]["id"].get<std::string>(), response["result"]["txid"].get<std::string>());
	EXPECT_EQ(response["result"]["data"].get<std::string>(), "deleted successfully");
	EXPECT_TRUE(true);
}

/*
Retrieve deleted template definition in storage node with empty response
*/
TEST(Template, fetchDeletedTemplate) {
	std::string input = "c:11|txid:1234567890|k:0123456789|tid:9876543210|id:" + templateId;
	json response = getWSClient()->send(input);
	EXPECT_EQ(response["ack"]["id"].get<std::string>(), response["result"]["txid"].get<std::string>());
	EXPECT_EQ(response["result"]["data"].get<std::string>(), "No Entity found for the given Id");
	EXPECT_TRUE(true);
}