#include "pch.h"
#include "../MasterNodeLib/masterNodeLib.h"
class MasterNodeTest : public ::testing::Test {
protected:
	json keys;
	void SetUp() {
		keys = generateAccount();
		if (nodeIdentity != nullptr)
			return;
		dhtNode = new dht::DhtRunner();
		nodeIdentity = initializeKeypair();
		dhtNode->run(14100, *nodeIdentity, true);
	}

	void TearDown() {
		randomNumbers.clear();
		blockProducers.clear();
		transactions.clear();
		inVotingPhase = false;
		ignoreThisVotingRound = false;
		// for the open voting interval test, master21 is != nullptr because we don't exit the interval at the end.
		master21 = nullptr;
	}
};

// Makes sure we didn't forget to remove test values from the node.
TEST_F(MasterNodeTest, RemovedTestValues) {
	ASSERT_EQ(TOTAL_MASTER_NODES, 21);
	ASSERT_EQ(openVotingStartMinute, 35);
}

// Tests whether or not m21 is being cycled through as voting rounds elapse.
TEST_F(MasterNodeTest, ChangeBlockProducers) {
	EXPECT_EQ(getCurrentBlockProducer(), -1);
	blockProducers.push_back(Voter("s", dhtNode->getId().toString(), 0, true));
	blockProducers.push_back(Voter("s", "k2", 0, true));
	currentTimestamp = 0;
	EXPECT_EQ(getCurrentBlockProducer(), 0);
	EXPECT_TRUE(isBlockProducer());
	currentTimestamp = 1;
	EXPECT_EQ(getCurrentBlockProducer(), 1);
	currentTimestamp = 10;
	EXPECT_EQ(getCurrentBlockProducer(), 0);
	currentTimestamp = 15;
	EXPECT_EQ(getCurrentBlockProducer(), 1);
}

// Tests whether or not open voting begins and ends properly. Also tests whether or not the test node is added to the list of block producers as part of m21.
// If correct, the test node should be the only one in the list of block producers.
TEST_F(MasterNodeTest, OpenAndCloseVoting) {
	currentTimestamp = openVotingStartMinute * 60;
	enterAndExitVotingPhase();
	ASSERT_TRUE(inVotingPhase);
	ASSERT_EQ(randomNumbers.size(), 1);
	currentTimestamp += (15 * 60);
	enterAndExitVotingPhase();
	EXPECT_FALSE(inVotingPhase);
	EXPECT_EQ(dhtNode->getId().toString(), blockProducers[0].publicKey);
}

// Tets whether or not another node successfully passes verification and enters the list of block producers.
// Tests x509 signature verification and also the voting number generation based on a node's signature.
TEST_F(MasterNodeTest, AddM21) {
	dht::crypto::Identity ident(dht::crypto::generateIdentity());
	nextVotingTimestampStr = std::string(std::to_string(currentTimestamp = openVotingStartMinute * 60));
	enterAndExitVotingPhase();
	std::string signature(createX509Signature(nextVotingTimestampStr, &ident));
	std::string key(ident.first->getPublicKey().toString());
	registerVote(std::move(signature), std::move(key));
	EXPECT_EQ(randomNumbers.size(), 2);
}

// Tests whether or not the next timestamp that potential m21 nodes must sign is updated properly.
// Tests whether or not open voting is true for the proper interval.
TEST_F(MasterNodeTest, NextVotingTime) {
	currentTimestamp = 60 * openVotingStartMinute;
	ASSERT_TRUE(inVotingTime());
	currentTimestamp += (3600 - getSecondsPastHour());
	ASSERT_FALSE(inVotingTime());
	updateNextVotingTimestamp();
	ASSERT_EQ(std::to_string(getCurrentTimestamp() + openVotingStartMinute * 60), nextVotingTimestampStr);
	currentTimestamp += (3600 - getSecondsPastHour());
	currentTimestamp += 2;
	updateNextVotingTimestamp();
	currentTimestamp -= 2;
	ASSERT_EQ(std::to_string(getCurrentTimestamp() + openVotingStartMinute * 60), nextVotingTimestampStr);
}

// Tests whether or not a piece of code is entered into the transaction list correctly.
// Tests ed25519 signature verification.
TEST_F(MasterNodeTest, ProcessCode) {
	ASSERT_EQ(transactions.size(), 0);
	std::cerr << keys.dump() << std::endl;
	std::string tenantId = "123";
	std::string code = "coolCode";
	std::string inputs = "coolInputs";
	std::string message = std::string("c:") + std::to_string(static_cast<uint8_t>(CommandType::exeFunction)) + "|tid:" + tenantId + "|h:" + code + "|i:" + inputs;
	std::string signature = createEd25519Signature(message, keys["PublicKey"].get<std::string>(), keys["PrivateKey"].get<std::string>());
	std::cerr << signature << std::endl;
	ASSERT_TRUE(processCodePacket(tenantId, code, inputs, signature, keys["PublicKey"].get<std::string>()));
	ASSERT_EQ(transactions.size(), 1);
}

TEST_F(MasterNodeTest, IncomingTransaction) {
	ASSERT_EQ(transactions.size(), 0);
	json j = {
	{ "txid", "anid" },
	{ "t", 5 },
	{ "h", "hash" },
	{ "mid", "masterNodeId" },
	{ "cid", "computeNode" },
	{ "tid", "tenant id" },
	{ "s", "sig" },
	{ "k", "key" },
	{ "co", "func" },
	{"o", "output"},
	{"i", "input"},
	{"e", "exceptinal!"}
	};
	std::string data(j.dump());
	Transaction tx;
	transactions.insert(TransactionsMap::value_type("anid", std::move(tx)));
	ASSERT_EQ(transactions.size(), 1);
	processIncomingTransaction(data);
	ASSERT_EQ(transactions.size(), 1);
	ASSERT_EQ(transactions.at("anid").getCode(), "func");
	j["txid"] = "bnid";
	data = std::string(j.dump());
	processIncomingTransaction(data);
	ASSERT_EQ(transactions.size(), 2);
	ASSERT_TRUE(transactions.find("bnid") != transactions.end());
}

// Tests what would happen if a Master Node boots up straight into open voting.
// The expected behavior here is that the node will not participate at all in the voting process and will let this open voting interval elapse.
TEST_F(MasterNodeTest, IgnoringOpenVotingIntervals) {
	currentTimestamp = openVotingStartMinute * 60;
	dht::crypto::Identity ident(dht::crypto::generateIdentity());
	if (inVotingTime())
		ignoreThisVotingRound = true;
	ASSERT_TRUE(ignoreThisVotingRound);
	ASSERT_FALSE(canParticipateInVoting());
	enterAndExitVotingPhase();
	ASSERT_TRUE(inVotingPhase);
	ASSERT_FALSE(canParticipateInVoting());
	ASSERT_EQ(randomNumbers.size(), 0);
	std::string signature(createX509Signature(nextVotingTimestampStr, &ident));
	std::string key(ident.first->getPublicKey().toString());
	registerVote(std::move(signature), std::move(key));
	ASSERT_EQ(randomNumbers.size(), 0);
	currentTimestamp = (openVotingStartMinute - 1) * 60;
	enterAndExitVotingPhase();
	ASSERT_EQ(blockProducers.size(), 0);
	ASSERT_FALSE(inVotingPhase);
	ASSERT_FALSE(ignoreThisVotingRound);
	ASSERT_EQ(std::to_string(openVotingStartMinute * 60), nextVotingTimestampStr);
	currentTimestamp = openVotingStartMinute * 60;
	enterAndExitVotingPhase();
	ASSERT_TRUE(inVotingPhase);
	ASSERT_TRUE(canParticipateInVoting());
	signature = createX509Signature(nextVotingTimestampStr, &ident);
	key = ident.first->getPublicKey().toString();
	registerVote(std::move(signature), std::move(key));
	currentTimestamp = (openVotingStartMinute - 1) * 60;
	enterAndExitVotingPhase();
	ASSERT_EQ(blockProducers.size(), 2);
	ASSERT_FALSE(inVotingPhase);
	ASSERT_FALSE(canParticipateInVoting());
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}