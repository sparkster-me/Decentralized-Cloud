#pragma once
#include <unordered_map>
#include <common/blockChainNode.h>
enum class CommandType : char {
	connectionRequest=1, // Used for notification of new node connecting to network.
	codeDelivery, // Code is being delivered somewhere, whether to master or verifier node
	choseRandomNumber, // A master node has chosen a new random number to see if it will be included in the list of 21 nodes.
	verificationResult, // Verifier is telling a master node about verification results of a piece of code.
	closeBlock, // A storage node is being notified that a block is closing, given the list of transactions to include in the block.
	stateChange, // Compute node tells storage node about the state changes to be made
	syncNewBlock, // A storage node in the network tells other storage nodes to sync newly committed block
	blockRequest, // A storage node in the network requests a random storage nodes to sync the blocks
	blockResponse, // A storage node returns the blocks to the requested node
	fetchFunction,  // Command to return the function definition for the given function Id
	fetchTemplate,  // Command to return the entity definition for the given entity Id
	fetchDocument, // Command to return the document for the given entityId and document search criteria
	deterministicCodeDelivery, // The Compute Node has executed the nondeterministic code and is delivering the deterministic version back to the Master Node.
	canCommit, // The Master Node is notifying a Compute Node that it can commit a particular transaction. This occurs after the transaction has been verified.
	txNotification, // Master node is telling the other 20 master nodes about a transaction it has created. This occurs after the Compute Node returns the deterministic code back to the Master Node, so the code field contains deterministic code.
	verifyBlock,  // Master node tells the storage node about block hash verification
	exception,  // An exception occurred during code execution on the Node
	storageurl, // Requesting master node to return storage node url
	exeFunction, //used execute user functions
	syncNodes, //sync nodes data for newly joined master node
	stateChangeResponse // Storage node returns the response back to the requester node, such as returning an ID for a newly created template
};

// Sub commands for stateChange command
enum class Command :char {
	addTemplate = 1,  // Tell Storage node to add new template
	addDocument,      // Tell Storage node to add new document    
	addFunction,      // Tell Storage node to add new function
	updateTemplate,  // Tell Storage node to update template
	updateDocument,  // Tell Storage node to update document
	updateFunction,  // Tell Storage node to update function
	deleteTemplate,   // Tell Storage node to delete template
	deleteDocument,  // Tell Storage node to delete document
	deleteFunction   // Tell Storage node to delete function
};


typedef std::unordered_map<std::string, std::string> FieldValueMap; // Used for generic field..value mapping.
typedef std::unordered_map<std::string, BlockChainNode> NodesMap; // mapping of public key..BlockChainNode
typedef std::unordered_map<CommandType, const std::function<void(FieldValueMap&&)>> CommandFunctionMap; // Used to associate a command with a function, so that given a specified command, the given function will run. The FieldValueMap passed to the function contains the fields and values parsed from the incoming command.