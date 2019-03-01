#include "pch.h"

std::string doctemplateId = "";
std::string documentId = "";
std::string const templateDef = "{\"entityInfo\":{\"tenantId\":\"292FEC76-5F1C-486F-85A5-09D88096F098\",\"timeStamp\":\"\",\"entity\":\"EntityFormation\",\"servicetype\":\"7C5DBC9D-8DEB-4147-838A-16F97F0CF867\"},\"collections\":{\"EntityObjectMap\":{\"rowset\":[{\"applicationObjectId\":\"db4585d0-4b57-4dea-b072-63d0643bc50c\",\"applicationEntityId\":\"af92c5ef-62ef-4f89-9ebc-4050a6a3a11a\",\"applicationEntityObjectId\":\"a450938a-2733-41fd-ae07-7b85b90ce2e3\",\"applicationIsLookupObject\":\"false\",\"applicationId\":\"0000000-0000-0000-0000-000000000000\",\"tenantId\":\"292FEC76-5F1C-486F-85A5-09D88096F098\"}],\"meta\":{\"fkName\":\"null\",\"parentReference\":\"null\",\"pkName\":\"null\"}},\"Entity\":{\"rowset\":[{\"tenantApplicationId\":\"0000000-0000-0000-0000-000000000000\",\"tenantId\":\"292FEC76-5F1C-486F-85A5-09D88096F098\",\"tenantEntityId\":\"af92c5ef-62ef-4f89-9ebc-4050a6a3a11a\",\"tenantEntityName\":\"abcde\",\"displayName\":\"Abcde\",\"entityType\":\"business_service\"}],\"meta\":{\"fkName\":\"null\",\"parentReference\":\"null\",\"pkName\":\"null\"}},\"EntityObjectRelationship\":{\"rowset\":[{\"tenantEntityObjectRelationshipId\":\"5832d226-6206-46d8-881f-745bd58d9bee\",\"tenantParentObjectId\":\"db4585d0-4b57-4dea-b072-63d0643bc50c\",\"tenantChildObjectId\":\"db4585d0-4b57-4dea-b072-63d0643bc50c\",\"tenantObjectRelationshipTypeId\":\"D7A139D1-662D-4BAD-9C8E-7CE80F5527F0\",\"tenantApplicationId\":\"0000000-0000-0000-0000-000000000000\",\"tenantId\":\"292FEC76-5F1C-486F-85A5-09D88096F098\"}],\"meta\":{\"fkName\":\"null\",\"parentReference\":\"null\",\"pkName\":\"null\"}}}}";
std::string const docPostDef = "{\"entityinfo\":{\"entity\":\"departments\",\"tenantid\":\"292FEC76-5F1C-486F-85A5-09D88096F098\",\"timestamp\":\"2015-12-15T10:16:06.322Z\"},\"collections\":{\"departments\":{\"rowset\":[{\"departmentname\":\"Customer Service\",\"country\":\"USA\",\"recordid\":\"d6f31aed-4093-4e57-b94c-eec6fd707a6e\"}],\"meta\":{\"parentreference\":\"***\",\"pkname\":\"***\",\"fkname\":\"***\"},\"rowfilter\":[]}}}";
std::string const docPutDef = "{\"entityinfo\":{\"entity\":\"departments\",\"tenantid\":\"292FEC76-5F1C-486F-85A5-09D88096F098\",\"timestamp\":\"2015-12-15T10:16:06.322Z\"},\"collections\":{\"departments\":{\"rowset\":[{\"departmentname\":\"Customer Report\",\"country\":\"UK\",\"recordid\":\"d6f31aed-4093-4e57-b94c-eec6fd707a6e\"}],\"meta\":{\"parentreference\":\"***\",\"pkname\":\"***\",\"fkname\":\"***\"},\"rowfilter\":[]}}}";

/*
Store template definition in storage node
*/
TEST(DocTemplate, AddDocTemplate) {
	std::string input = "c:6|txid:1234567890|k:0123456789|tid:9876543210|cmds:1 " + encode(templateDef);
	json response = getWSClient()->send(input);
	EXPECT_EQ(response["ack"]["id"].get<std::string>(), response["result"]["txid"].get<std::string>());
	doctemplateId = response["result"]["id"].get<std::string>();
	EXPECT_TRUE(true);
}

/*
Store document definition in storage node
*/
TEST(Document, AddDocument) {
	std::string input = "c:6|txid:1234567890|k:0123456789|tid:9876543210|cmds:2 " + doctemplateId + " " + encode(docPostDef);
	json response = getWSClient()->send(input);
	EXPECT_EQ(response["ack"]["id"].get<std::string>(), response["result"]["txid"].get<std::string>());
	documentId = response["result"]["id"].get<std::string>();
	EXPECT_TRUE(true);
}

/*
Retrieve document definition in storage node
*/
TEST(Document, fetchDocument) {
	std::string input = "c:12|txid:1234567890|k:0123456789|tid:9876543210|id:"+ doctemplateId +"|expression:\"id\"=\"" + documentId + "\"";
	json response = getWSClient()->send(input);
	EXPECT_EQ(response["ack"]["id"].get<std::string>(), response["result"]["txid"].get<std::string>());
	//EXPECT_EQ(response["result"]["data"].get<std::string>(), docPostDef);
	EXPECT_TRUE(true);
}

/*
Update document definition in storage node
*/
TEST(Document, updateDocument) {
	std::string input = "c:6|txid:1234567890|k:0123456789|tid:9876543210|cmds:5 " + doctemplateId + " \"id\"=\"" + documentId + "\" " + encode(docPutDef);
	json response = getWSClient()->send(input);
	EXPECT_EQ(response["ack"]["id"].get<std::string>(), response["result"]["txid"].get<std::string>());
	EXPECT_TRUE(true);
}

/*
Retrieve updated document definition in storage node
*/
TEST(Document, fetchUpdatedDocument) {
	std::string input = "c:12|txid:1234567890|k:0123456789|tid:9876543210|id:" + doctemplateId + "|expression:\"id\"=\"" + documentId + "\"";
	json response = getWSClient()->send(input);
	EXPECT_EQ(response["ack"]["id"].get<std::string>(), response["result"]["txid"].get<std::string>());
	//EXPECT_EQ(response["result"]["data"].get<std::string>(), docPutDef);
	EXPECT_TRUE(true);
}

/*
Delete document definition in storage node
*/
TEST(Document, deleteDocument) {
	std::string input = "c:6|txid:1234567890|k:0123456789|tid:9876543210|cmds:8 " + doctemplateId + " \"id\"=\"" + documentId + "\" ";
	json response = getWSClient()->send(input);
	EXPECT_EQ(response["ack"]["id"].get<std::string>(), response["result"]["txid"].get<std::string>());
	EXPECT_EQ(response["result"]["data"].get<std::string>(), "Documents are deleted successfully");
	EXPECT_TRUE(true);
}

/*
Retrieve deleted document definition in storage node with empty response
*/
TEST(Document, fetchDeletedDocument) {
	std::string input = "c:12|txid:1234567890|k:0123456789|tid:9876543210|id:" + doctemplateId + "|expression:\"id\"=\"" + documentId + "\" ";
	json response = getWSClient()->send(input);
	EXPECT_EQ(response["ack"]["id"].get<std::string>(), response["result"]["txid"].get<std::string>());
	EXPECT_EQ(response["result"]["data"].get<std::string>(), "");
	EXPECT_TRUE(true);
}