// Disable the buffer overflow warning, since we're internally checking for it.
#pragma once
#pragma warning (disable: 4996)

#include "StorageNode.h"


// Returns a map of (edge name, hash) such that indexing by name will give the hash of the node where the edge leads.
UnorderedEdgeMap getUnorderedEdgeNamesOf(ipfs::Client& client,const std::string& nodeHash) {
	UnorderedEdgeMap namesToHashes;
	ipfs::Json links;
	try {
		ipfsClient->ObjectLinks(nodeHash, &links);
	}
	catch (exception e) {
		std::cerr << e.what();
	}
	// Next, we need to insert the links into a hash table.
	for (int i = 0; i < links.size(); i++) {
		// Now, insert hash and edge_name
		namesToHashes.insert(UnorderedEdgeMap::value_type(links[i]["Name"], links[i]["Hash"]));
	}
	return UnorderedEdgeMap(namesToHashes);
}

//Returns ordered map of (edge name, hash) such that indexing by name will give the hash of the node where the edge leads.
OrderedEdgeMap getOrderedEdgeNamesOf(ipfs::Client& client,const std::string& nodeHash) {
	OrderedEdgeMap namesToHashes;
	ipfs::Json links;
	try {
		ipfsClient->ObjectLinks(nodeHash, &links);
	}
	catch (exception e)	{
		std::cerr << e.what();
	}
	// Next, we need to insert the links into a hash table.
	for (int i = 0; i < links.size(); i++) {
		// Now, insert hash and edge_name
		namesToHashes.insert(OrderedEdgeMap::value_type(links[i]["Name"], links[i]["Hash"]));
	}	
	return OrderedEdgeMap(namesToHashes);
}

// Returns postfix expression format of the given expression
std::vector<std::string> getPostfixedExpressionOf(std::string& expr) {
	std::vector<std::string> postfix;
	int currentIndex = 0;
	size_t n = expr.length();
	std::string operand;
	std::string op;
	std::stack<std::string> operators;
	while (currentIndex < n) {
		char c = expr.at(currentIndex++);
		if (isspace(c))
			continue;
		// If we have a "string", we'll consume until and including the end of the quoted string.
		if (c == '"') {
			operand += c;
			while (expr.at(currentIndex) != '"') {
				operand += expr.at(currentIndex++);
			}
			operand += expr.at(currentIndex++); // Append the final quotation mark.
			postfix.push_back(std::string(operand));
			operand.clear();
		}
		else if (isdigit(c)) { // If we find a number, consume until the end of the number.
			operand += c;
			while (currentIndex < n && isdigit(expr.at(currentIndex))) {
				operand += expr.at(currentIndex++);
			}
			postfix.push_back(std::string(operand));
			operand.clear();
		}
		else if (c == '(') { // We push the left paren onto the stack.
			operators.push(std::string(1, c));
		}
		else if (c == ')') { // Pop until we find a left paren.
			std::string topOperator(operators.top());
			while (topOperator.compare("(") != 0) {
				postfix.push_back(topOperator);
				operators.pop();
				topOperator = operators.top();
			}
			operators.pop(); // Pop the left paren off the stack, but don't add it to the output.
		}
		else if (c == '<') {
			op += c;
			c = expr.at(currentIndex);
			if (c == '=' || c == '>') {
				op += c;
				currentIndex++;
			}
		}
		else if (c == '=') {
			op += c;
		}
		else if (c == '>') {
			op += c;
			c = expr.at(currentIndex);
			if (c == '=') {
				op += c;
				currentIndex++;
			}
		}
		else if (c == '&') {
			op += c;
		}
		else if (c == '|') {
			op += c;
		}
		if (!op.empty()) {
			if (!operators.empty()) {
				std::string topOperator(operators.top());
				// Checking precedence will yield true if a has higher precedence than b, false otherwise.
				// We need to insert a on the stack when its precedence is higher or equal to the operator currently on the stack.
				while (!precedence[op][topOperator]) { // While topOperator has higher precedence than op
					if (topOperator.compare("(") == 0) // Don't pop a left paren unless we find a right paren
						break;
					operators.pop();
					postfix.push_back(std::string(topOperator));
					if (operators.empty())
						break;
					topOperator = operators.top();					
				}
			}
			operators.push(std::string(op));
			op.clear();
		}
	}
	while (!operators.empty()) {
		postfix.push_back(std::string(operators.top()));
		operators.pop();
	}
	return std::vector<std::string>(postfix);
}

// Returns expression tree from the postfix expression format
TreeNode* getExpressionTreeFrom(std::vector<std::string>& postfix) {
	std::stack<TreeNode*> treePtrs;
	ExistingIndexesMap* fieldsMap = new ExistingIndexesMap;
	size_t n = 0;
	for (std::string element : postfix) {
		n = element.length();
		char c = element.at(0);
		if (c == '"' || isdigit(c)) { // If this term in the postfix expression is an operand
			//cerr << " adding as operand" << endl;
			// We'll create a tree whose root is the operand and the left and right pointers will be NULL.
			TreeNode* node = new TreeNode;
			node->root = std::string(element).substr(1, n - 2);
			node->left = NULL;
			node->right = NULL;
			treePtrs.push(node);
			// Next, add this to the fields map.
			if (c == '"') {
				fieldsMap->insert(ExistingIndexesMap::value_type(node->root, false));
			}
		}
		else { // This element is an operator
			// We'll create a tree whose left and right nodes point to operands A and B and whose root is the operator.
			TreeNode* node = new TreeNode;
			node->root = std::string(element);
			node->right = treePtrs.top();
			treePtrs.pop();
			node->left = treePtrs.top();
			treePtrs.pop();
			treePtrs.push(node);
		}
	}
	TreeNode* rootNode = treePtrs.top();
	rootNode->fields = fieldsMap;
	return rootNode;
}

// Given the set of all indexes and the root of an expression tree, this function will fill in the fields mapping such that true values will mean that
// the specified index exists, and false otherwise.
// This way, the caller only needs to check root->fields to see if the index associated with an operand exists.
void markExistingIndexes(UnorderedEdgeMap& indexes, TreeNode* root) {
	ExistingIndexesMap* marks = root->fields;
	for (ExistingIndexesMap::iterator it = marks->begin(); it != marks->end(); it++) {
		(*marks)[it->first] = (indexes.find(it->first) != indexes.end());
	}
}

// Fetch document from ipfs for the given hash
json fetchDocument(std::string& hash) {
	return json::parse(std::string(fetchObject(hash)));	
}

// Fetch document from ipfs based on hash and append the resultset to _rs 
void fetchDocument(std::string& hash, json& _rs) {
	json _json = fetchDocument(hash);
	_rs.push_back(_json);
}

// Fetches the list of documents from ipfs based on default index hash . Default index is assumed as 'id' for now
json fetchDocumentsFromDefaultIndex(ipfs::Client& client,UnorderedEdgeMap& indexes) {
	OrderedEdgeMap _values = getOrderedEdgeNamesOf(client,indexes["id"]);
	_values = getOrderedEdgeNamesOf(client,_values["__"]);
	json _default_index_rs = json::array();
	for (OrderedEdgeMap::iterator it = _values.begin(); it != _values.end(); it++) {
		// append the output to _default_index_rs
		fetchDocument(it->second, _default_index_rs);
	}
	return _default_index_rs;
}

// If _docs is null then it will fetch the documents from the default index 
// Sorts the documents based on the given operand
json sortDocumentsByOperand(ipfs::Client& client,json _docs, std::string operand, UnorderedEdgeMap& indexes) {
	if (_docs == json::value_t::null) {
		_docs = fetchDocumentsFromDefaultIndex(client,indexes);
	}
	json _rs = json::parse(_docs.dump());
	// sorting json data based on the operand
	std::sort(_rs.begin(), _rs.end(), Custom_Compare(operand));
	return _rs;
}

// Apply the binary search on the json to find the given value of the operand
boost::optional<boost::tuple<uint32, uint32, uint32> > getSearchIndex(json& _json, uint32 beginIndex, uint32 endIndex, string& operand, string& searchvalue) {
	if (endIndex >= beginIndex) {
		uint32 middlevalue = beginIndex + (endIndex - beginIndex) / 2;

		// If the element is present at the middle then return it
		if (_json[middlevalue][operand] == searchvalue) {
			return boost::make_tuple(beginIndex, middlevalue, endIndex);
		}

		// If element is smaller than mid, then 
		// it can only be present in left subarray
		if (_json[middlevalue][operand] > searchvalue)
			return getSearchIndex(_json, beginIndex, middlevalue - 1, operand, searchvalue);

		// Else the element can only be present
		// in right subarray
		return getSearchIndex(_json, middlevalue + 1, endIndex, operand, searchvalue);
	}

	// If element is not present in the array
	return boost::none;
}

// Find the lower index of the given operand value from the json
int getlowerIndex(json& _json, string& operand, string& value) {

	boost::optional<boost::tuple<uint32, uint32, uint32>> result = getSearchIndex(_json, 0, (uint32)(_json.size() - 1), operand, value);
	if (!result) {
		return -1;
	}

	int lastfoundat = result->get<1>();
	while (result.is_initialized()) {
		lastfoundat = result->get<1>();
		result = getSearchIndex(_json, result->get<0>(), result->get<1>() - 1, operand, value);
	}
	return lastfoundat;
}

// Find the upper index of the given operand value from the json
int getUpperIndex(json& _json, string& operand, string& value) {

	boost::optional<boost::tuple<uint32, uint32, uint32>> result = getSearchIndex(_json, 0, (uint32)(_json.size() - 1), operand, value);
	if (!result) {
		return -1;
	}

	int lastfoundat = result->get<1>();
	while (result.is_initialized()) {
		lastfoundat = result->get<1>();
		result = getSearchIndex(_json, result->get<1>() + 1, result->get<2>(), operand, value);
	}
	return lastfoundat;
}

// Fetches unique documents from both the sets
json union_(json& _json1, json _json2) {
	set<json> hs;

	// Inhsert the elements of _json1 to set hs
	for (int i = 0; i < _json1.size(); i++)
		hs.insert(_json1[i]);

	// Insert the elements of _json2 to set hs
	for (int i = 0; i < _json2.size(); i++)
		hs.insert(_json2[i]);
	// convert the hashset to json
	json j_set(hs);
	return j_set;
}

// Fetches common documents from both the sets
json intersection(json& _json1, json _json2) {
	set<json> hs;
	json _rs = json::array();
	// Insert the elements of _json1[] to set hs
	for (int i = 0; i < _json1.size(); i++)
		hs.insert(_json1[i]);

	for (int i = 0; i < _json2.size(); i++)
		// If element is present in set then push it to json
		if (hs.find(_json2[i]) != hs.end())
			_rs.push_back(_json2[i]);
	return _rs;
}

// Given the JSON documents,search operand and filter value applies not equal operation
json not_equals(json& _json, string operand, string value) {
	json _filtered = json::array();
	int from = getlowerIndex(_json, operand, value);
	if (from == -1) { return _filtered; }
	int to = getUpperIndex(_json, operand, value);
	for (int i = 0; i < from; i++) {
		_filtered.push_back(_json[i]);
	}
	for (int i = to + 1; i < _json.size(); i++) {
		_filtered.push_back(_json[i]);
	}

	//_json.clear();
	return _filtered;
}

// Given the JSON documents applies lessthan operation against operand and filter value
json lessthan(json& _json, string operand, string value) {
	json _filtered = json::array();
	int from = getlowerIndex(_json, operand, value);
	if (from == -1) { return _filtered; }
	for (int i = 0; i < from; i++) {
		_filtered.push_back(_json[i]);
	}
	return _filtered;
}

// Given the JSON documents applies lessthan and equals operation against operand and filter value
json lessthan_equals(json& _json, string operand, string value) {
	json _filtered = json::array();
	int from = getUpperIndex(_json, operand, value);
	if (from == -1) { return _filtered; }
	for (int i = 0; i <= from; i++) {
		_filtered.push_back(_json[i]);
	}
	return _filtered;
}

// Given the JSON documents,search operand and filter value applies greaterthan and equals operation 
json greaterthan_equals(json& _json, string operand, string value) {
	json _filtered = json::array();
	int from = getUpperIndex(_json, operand, value);
	if (from == -1) { return _filtered; }
	for (int i = from; i < _json.size(); i++) {
		_filtered.push_back(_json[i]);
	}
	return _filtered;
}

// Given the JSON documents,search operand and filter value applies greaterthan operation
json greaterthan(json& _json, string operand, string value) {
	json _filtered = json::array();
	int from = getUpperIndex(_json, operand, value);
	if (from == -1 || (_json.size() - 1) == from) { return _filtered; }
	for (int i = from; i < _json.size(); i++) {
		_filtered.push_back(_json[i]);
	}
	return _filtered;
}

json equals(json& _json, string operand, string value) {
	json _filtered = json::array();
	int from = getlowerIndex(_json, operand, value);
	if (from == -1) { return _filtered; }
	int to = getUpperIndex(_json, operand, value);
	for (int i = from; i <= to; i++) {
		_filtered.push_back(_json[i]);
	}

	return _filtered;
}

void printTree(TreeNode* node) {
	if (node->left != NULL)
		printTree(node->left);
	if (node->right != NULL)
		printTree(node->right);
	cerr << node->root << " ";
}

// find the documents for the given entity and expression
void findDocument(std::string&& source,std::string&& channelId, std::string&& txid, std::string&& tid, std::string&& entityId,std::string&& expression) {
	json docs=searchDocument(*ipfsClient,tid, entityId, std::move(expression));
	send(std::move(source),channelId, CommandType::fetchDocument, txid, entityId, (docs.type() == json::value_t::string) ? docs.get<std::string>() : docs.dump());
}

// Search documents in the given entity tree
json searchDocument(ipfs::Client& client,std::string & tid, std::string & entityId, std::string && expression){
	// Get the entityRoot hash
	std::string hash(dhtGet(dhtNode, std::string(ROOT_IDENTIFIER)+std::string(DATA_TREE)+std::string(CHARACTER_DELIMITER)+ tid + std::string(CHARACTER_DELIMITER) + entityId));
	if (hash.empty())
		return std::string(MSG_TEMPLATE_NOT_FOUND);
	// First, get the list of indexes on the template.
	UnorderedEdgeMap indexes = getUnorderedEdgeNamesOf(client,hash);
	std::vector<std::string> postfixform = getPostfixedExpressionOf(expression);
	TreeNode* filterCondition = getExpressionTreeFrom(postfixform);
	// Next, mark the hash map with true for those operands that refer to indexes that exist.
	markExistingIndexes(indexes, filterCondition);
	// Now, filterExpression->fields contains true / false values for all indexes that we might have to traverse.
	json _null(json::value_t::null);
	// Evaluate expression tree and get resulted documents
	json docs = evaluateExpression(client,filterCondition, indexes, _null);
	return docs;
}

// Evaluate the filter condition and return all the resulted documents
json evaluateExpression(ipfs::Client& client, TreeNode* filter, UnorderedEdgeMap& indexes, json& _docs) {
	if (!filter)
		return "";

	// leaf node i.e, operand or search value
	if (!filter->left && !filter->right) {
		return filter->root;
	}
	// Evaluate left subtree
	json left = evaluateExpression(client,filter->left, indexes, _docs);

	// Evaluate right subtree
	json right = evaluateExpression(client,filter->right, indexes, _docs);
	
	if (filter->root == "=") {
		if (indexes.find(left) != indexes.end()) { // index found			
			OrderedEdgeMap _values = getOrderedEdgeNamesOf(client,indexes[left]);
			if (left == "id")
				_values = getOrderedEdgeNamesOf(client,_values["__"]);
			auto search = _values.find(right);
			if (search != _values.end()) {
				json _rs = json::array();
				_rs.push_back(fetchDocument(search->second));
				return _rs;
			}
			else {
				return "";
			}
		}
		else { // index not found
			json _rs = sortDocumentsByOperand(client,_docs, left, indexes);
			// filtering
			_rs = equals(_rs, left, right);
			return _rs;
		}
	}
	else if (filter->root == "<>") {
		if (indexes.find(left) != indexes.end()) {
			OrderedEdgeMap _values = getOrderedEdgeNamesOf(client,indexes[left]);
			auto search = _values.equal_range(right);
			if (search.first != _values.end()) {
				json _rs = json::array();
				for (OrderedEdgeMap::iterator it = _values.begin(); it != search.first; it++) {
					// append the output to resultset
					fetchDocument(it->second, _rs);
				}
				for (OrderedEdgeMap::iterator it = search.second; it != _values.end(); it++) {
					// append the output to resultset _rs
					fetchDocument(it->second, _rs);
				}
				return   _rs;
			}
			else {
				return "";
			}
		}
		else {
			json _rs = sortDocumentsByOperand(client,_docs, left, indexes);
			_rs = not_equals(_rs, left, right);
			return _rs;
		}

	}
	else if (filter->root == ">") {
		if (indexes.find(left) != indexes.end()) {
			OrderedEdgeMap _values = getOrderedEdgeNamesOf(client,indexes[left]);
			auto search = _values.upper_bound(right);
			if (search != _values.end()) {
				json _rs = json::array();
				for (OrderedEdgeMap::iterator it = search; it != _values.end(); it++) {
					// append the output to resultset
					fetchDocument(it->second, _rs);
				}
				return  _rs;
			}
			else {
				return "";
			}
		}
		else {
			json _rs = sortDocumentsByOperand(client,_docs, left, indexes);
			_rs = greaterthan(_rs, left, right);
			return _rs;
		}
	}
	else if (filter->root == ">=") {
		if (indexes.find(left) != indexes.end()) {
			OrderedEdgeMap _values = getOrderedEdgeNamesOf(client,indexes[left]);
			auto search = _values.lower_bound(right);
			if (search != _values.end()) {
				json _rs = json::array();
				for (OrderedEdgeMap::iterator it = search; it != _values.end(); it++) {
					// append the output to resultset
					fetchDocument(it->second, _rs);
				}
				return  _rs;
			}
			else {
				return "";
			}
		}
		else {
			json _rs = sortDocumentsByOperand(client,_docs, left, indexes);
			_rs = greaterthan_equals(_rs, left, right);
			return _rs;
		}
	}
	else if (filter->root == "<=") {
		if (indexes.find(left) != indexes.end()) {
			OrderedEdgeMap _values = getOrderedEdgeNamesOf(client,indexes[left]);
			auto search = _values.lower_bound(right);
			if (search != _values.end()) {
				json _rs = json::array();
				search++;
				for (OrderedEdgeMap::iterator it = _values.begin(); it != search; it++) {
					// append the output to resultset
					fetchDocument(it->second, _rs);
				}
				return  _rs;
			}
			else {
				return "";
			}
		}
		else {
			json _rs = sortDocumentsByOperand(client,_docs, left, indexes);
			_rs = lessthan_equals(_rs, left, right);
			return _rs;
		}
	}
	else if (filter->root == "<") {
		if (indexes.find(left) != indexes.end()) {
			OrderedEdgeMap _values = getOrderedEdgeNamesOf(client,indexes[left]);
			auto search = _values.lower_bound(right);
			if (search != _values.end()) {
				json _rs = json::array();
				for (OrderedEdgeMap::iterator it = _values.begin(); it != search; it++) {
					// append the output to resultset
					fetchDocument(it->second, _rs);
				}
				return _rs;
			}
			else {
				return "";
			}
		}
		else {
			json _rs = sortDocumentsByOperand(client,_docs, left, indexes);
			_rs = lessthan(_rs, left, right);
			return _rs;
		}
	}
	else if (filter->root == "|") {
		// Remove duplicates from the both the result sets and return result set	
		if (left.empty() || right.empty())
			return left.empty() ? right : left;
		return union_(left, right);
	}
	else if (filter->root == "&") {
		if (left.empty() || right.empty())
			return json::array();
		return intersection(left, right);
	}
	return json::array();
}

// Given some data, adds a new merckle object to the DAG.
// Returns the hash of the object.
std::string addObject(ipfs::Client& client,const std::string& data) {
	try {
		ipfs::Json object;
		ipfs::Json input;
		input["Data"] = data;
		client.ObjectPut(input, &object);
		return object["Hash"];
	}
	catch (exception e) {
		std::cout << "exception while comitting object to IPFS" << endl;
	}
	return std::string("");
}

// Given some data, adds a new merckle object to the DAG and adds links to the given object
// Returns the hash of the object.
std::string addObjectAndLink(ipfs::Client& client,const std::string& data,std::string& child,std::string& link) {
	ipfs::Json object;
	ipfs::Json input;
	input["Data"] = data;
	input["Links"] = json::array();
	json j;
	j["Hash"] = child;
	j["Name"] = link;
	input["Links"].push_back(j);
	client.ObjectPut(input, &object);
	return object["Hash"];
}

// Fetch the defintion from the ipfs for the given hash
std::string fetchObject(const std::string &hash) {
		ipfs::Client* client=getIPFSClient();
		std::string output= fetchObject(*client, hash);
		delete client;
		return output;
}

// Fetch the defintion from the ipfs for the given hash
std::string fetchObject(ipfs::Client& client,const std::string &hash) {
	try {
		std::string output;
		client.ObjectData(hash, &output);
		return output;
	}
	catch (std::exception e) {
		std::cout << "Exception while fetching object from IPFS " << std::endl;
	}
	return std::string("");
}

// Given template Id returns the defintion of the template
void fetchTemplate(std::string&& source,std::string&& channelId, std::string&& txid, std::string&& tid, std::string&& templateId) {
	dhtGet(dhtNode, std::string(std::string(TEMPLATE_TREE) + std::string(CHARACTER_DELIMITER) + tid + std::string(CHARACTER_DELIMITER) + templateId), [&source,&channelId, &txid, &templateId](const std::string& val) {
		if (val.empty()){
			send(std::move(source),channelId, CommandType::fetchTemplate, txid, templateId, std::string(MSG_TEMPLATE_NOT_FOUND));
			return;
		}
		send(std::move(source), channelId, CommandType::fetchTemplate, txid, templateId, fetchObject(val));
	});
}

// Given the template and document  id,updates or deletes the document
void updateOrDeleteDocument(ipfs::Client& client,std::string& source,std::string& channelId, std::string& txid, std::string& tid, std::string& entityId, std::string& documentId, std::string&& documentJson,std::string op) {
	// Add template entity if entityRoot is empty
	std::string entityRoot = dhtGet(dhtNode, std::string(ROOT_IDENTIFIER)+std::string(DATA_TREE)+ std::string(CHARACTER_DELIMITER) + tid + std::string(CHARACTER_DELIMITER) + entityId);
	if (entityRoot.empty())
	{
		send(std::move(std::string(source)), channelId, CommandType::exception, txid, documentId, std::string(MSG_DOCUMENT_ROOT_NOT_FOUND));
		return;
	}
	std::string expression = "\"id\"=\"" + documentId + "\"";
	json docs=searchDocument(client, tid, entityId, std::move(expression));
	if (docs.empty() || docs.size() <= 0)
	{
		// send message to the caller that there is no document in the system for the given id
		send(std::string(source),channelId, CommandType::exception, txid, documentId, "No document found for the given id");
		return;
	}
	json doc = docs[0];
	std::string document;
	document = (op == "update") ? (updateValues(doc.dump(), documentJson)) : doc.dump();
	createOrDeleteDocument(client, source,channelId, txid, tid, entityId, documentId, std::move(document), op);
}

// Given the template Id and document definion persists in IPFS
void createOrDeleteDocument(ipfs::Client& client,std::string& source,std::string& channelId, std::string& txid, std::string& tid,std::string& entityId, std::string& documentId, std::string&& documentJson,std::string op) {
	
	// Get template entity if entityRoot is empty return error
	std::string entityRoot = dhtGet(dhtNode, std::string(ROOT_IDENTIFIER)+std::string(DATA_TREE)+ std::string(CHARACTER_DELIMITER) + tid + std::string(CHARACTER_DELIMITER) + entityId);
	if (entityRoot.empty())	{
		send(std::move(std::string(source)), channelId, CommandType::exception, txid, documentId, std::string(MSG_TEMPLATE_NOT_FOUND));
		return;
	}
	std::string documentHash="";
	if (op != "delete") {
		// First, create the document object D by providing the JSON to stdin.
		documentHash = addObject(client,documentJson);
		// update document hash in dht
		dhtPut(dhtNode, std::string(DATA_TREE) + std::string(CHARACTER_DELIMITER) + tid + std::string(CHARACTER_DELIMITER) + entityId + std::string(CHARACTER_DELIMITER) + documentId, documentHash);
		send(std::move(std::string(source)), channelId, CommandType::stateChange, txid, documentId, documentHash);
	}
	else {
		// clear document hash entry in dht
		dhtPut(dhtNode, std::string(DATA_TREE) + std::string(CHARACTER_DELIMITER) + tid + std::string(CHARACTER_DELIMITER) + entityId + std::string(CHARACTER_DELIMITER) + documentId, documentHash);
		send(std::move(std::string(source)), channelId, CommandType::stateChange, txid, documentId, std::string(MSG_DELETE_SUCCESS));
	
	}
		
	UnorderedEdgeMap a_b = getUnorderedEdgeNamesOf(client,entityRoot);
	json inputJson = json::parse(documentJson);
	// We'll go through each index and check if the index name exists on the document. If it does,
	// we'll update the index.
	for (UnorderedEdgeMap::iterator mapDef = a_b.begin(); mapDef != a_b.end(); mapDef++) {
		const std::string& index = mapDef->first;
		json::iterator target = inputJson.find(index);
		bool found = target != inputJson.end(); // Does this index exist on the new document to create?
		if (found) {
			const std::string& value = target->get<std::string>();
			// Next, we need to check if an edge from b to c exists with this value.
			const std::string& bNode = mapDef->second;
			UnorderedEdgeMap b_c = getUnorderedEdgeNamesOf(client,bNode);
			UnorderedEdgeMap::iterator it;
			// We'll check if the __ index value exists. If it does, this index value is unique by nature and all index values should lead from the (B,C) edge where the label is __.
			if ((it = b_c.find("__")) == b_c.end()) {
				it = b_c.find(value);
			}
			//const std::string& documentId = inputJson["id"].get<std::string>();
			if (it != b_c.end()) { // We found an index with the specified value.
				// Next, we need to insert this document under the C node.
				const std::string& indexValue = it->first;
				std::string& cNode = it->second;
				if(op == "update" || op == "delete")
				 	client.ObjectPatchRmLink(cNode, documentId, &cNode);
				if (op != "delete")
					client.ObjectPatchAddLink(cNode,documentId,documentHash,&cNode);
				// Next, remove the (B, C) edge labeled with the index value since C's hash has changed.
				std::string newBNode;
				client.ObjectPatchRmLink(bNode, indexValue,&newBNode);
				// Next, re-add the (B, C) edge, keeping the same value as before.				
				client.ObjectPatchAddLink(newBNode, indexValue, cNode, &newBNode);
				// Next, we need to update edge (A, B) since the hash of B has changed.
				// First, remove (A, B) labeled with the index name.
				client.ObjectPatchRmLink(entityRoot,index,&entityRoot);
				// Finally, re-add (A, B) keeping the same index name as before.
				client.ObjectPatchAddLink(entityRoot, index, newBNode, &entityRoot);
				// Link entityRoot and tenant, tenant and data root
				// update the dht with new entityroot hash
				dhtPut(dhtNode, std::string(ROOT_IDENTIFIER)+std::string(DATA_TREE)+ std::string(CHARACTER_DELIMITER) + tid + std::string(CHARACTER_DELIMITER) + entityId, entityRoot);
				
			}
			else { // the index value doesn't exist
 // First, create the (C, D) edge and label it with the id of this document
				std::string cNode(addObject(client,entityRoot));
				if (op == "update" || op == "delete")
					client.ObjectPatchRmLink(cNode, documentId,&cNode);
				if (op != "delete")
					client.ObjectPatchAddLink(cNode, documentId, documentHash, &cNode);
				// Next, create (b, c) and label it with the value of this index.
				std::string newBNode;
				client.ObjectPatchAddLink(bNode, value, cNode, &newBNode);
				// Next, remove the old (A, B) labeled with the index name.
				client.ObjectPatchRmLink(entityRoot, index, &entityRoot);
				// Finally, re-add (A, B) labeled with the name of the index but pointing to the new hash of B.
				client.ObjectPatchAddLink(entityRoot, index, newBNode, &entityRoot);
				// update the dht with new entityroot hash
				dhtPut(dhtNode, std::string(ROOT_IDENTIFIER)+std::string(DATA_TREE)+ std::string(CHARACTER_DELIMITER) + tid + std::string(CHARACTER_DELIMITER) + entityId, entityRoot);
								
			}
		}
	}
}

// Links the given object to the specified tree. If root not found then it creates the root and links the object to root.
void linkToRoot(ipfs::Client& client,const std::string& tree, std::string &objectId, std::string &objectHash,const std::string& txid) {

	// Add the effected object hash and tx in dht
	if(not txid.empty())
	dhtPut(dhtNode, std::string(TRANSACTION_TREE) + std::string(CHARACTER_DELIMITER) + objectHash, txid);

	// Add root entity if rootHash is empty
	std::string rootHash(dhtGet(dhtNode, std::string(std::string(ROOT_IDENTIFIER) + tree)));
	if (rootHash.empty()) {
		rootHash = addObjectAndLink(client, tree, objectHash, objectId);
	}
	else {
		// root Hash and tenant hash will be labeled with tenantId
		client.ObjectPatchAddLink(rootHash,objectId,objectHash,&rootHash);
	}

	// update the root hash in dht
	dhtPut(dhtNode, std::string(ROOT_IDENTIFIER) + tree, rootHash);
	
}

// Removes the links between the root and given object and updates the root hash in DHT
void unLinkToRoot(ipfs::Client& client,std::string tree,std::string txid, std::string &rootHash, std::string &objectId) {
	// root Hash and tenant hash will be labeled with tenantId
	std::string newRootHash;
	client.ObjectPatchRmLink(rootHash, objectId, &newRootHash);
	// update the effected root hash and tx in dht
	dhtPut(dhtNode, std::string(TRANSACTION_TREE)+ std::string(CHARACTER_DELIMITER) + rootHash, txid);

	// update the root hash in dht
	dhtPut(dhtNode, std::string(ROOT_IDENTIFIER) + tree, newRootHash);

}

// Send off response to the requested node/client
void send(const std::string&& source,const std::string &channelId,CommandType cmd,const std::string& txid,const std::string& objectId,const std::string val){
	json j;
	j["txid"] = txid;
	(cmd != CommandType::exception)?j["data"] = val:j["error"] = val;
	if(not objectId.empty())
	j["id"] = objectId;	
	(source =="1")?wsServer->send(channelId, j.dump()): gossip(dhtNode, channelId, cmd, { {"txid",txid}, {"id",objectId}, {"data",val} });	
}

// Add child to IPFS
void addChildNode(ipfs::Client& client, std::string& source,const std::string& channelId ,std::string& txid, std::string& tid,const std::string& tree, std::string& objectId, std::string&& object) {
	// Add the object definition 
	std::string objectHash(addObject(client,object));
	// Add the objectHash to dht
	dhtPut(dhtNode, std::string(tree + std::string(CHARACTER_DELIMITER) + tid + std::string(CHARACTER_DELIMITER) + objectId), objectHash);

	send(std::move(std::string(source)), channelId, CommandType::stateChange, txid, objectId, objectHash);
	pCount++;
	// Link the object to root
	linkToRoot(client, tree + std::string(CHARACTER_DELIMITER)+tid, objectId, objectHash,txid);
	// If template then build the new data tree 
	if (tree == std::string(TEMPLATE_TREE)) {
		std::string dataRootHash(addObject(client,std::string(std::string(ROOT_IDENTIFIER) + std::string(DATA_TREE) + std::string(CHARACTER_DELIMITER) + tid + std::string(CHARACTER_DELIMITER) + objectId)));

		// Add the ID index node.
		std::string hashB(addObject(client, "id"));
		// Add the collections node.
		std::string hashC(addObject(client, "collection"));
		// Now connect (B, C) and (A, B)
		std::string b_c;
		client.ObjectPatchAddLink(hashB, "__", hashC, &b_c);		
		// (A, B) will be labeled "id".
		std::string newDataRootHash;
		client.ObjectPatchAddLink(dataRootHash, "id", b_c, &newDataRootHash);		
		// Add new dataroot hash in dht
		dhtPut(dhtNode, std::string(ROOT_IDENTIFIER) + std::string(DATA_TREE) + std::string(CHARACTER_DELIMITER) + tid + std::string(CHARACTER_DELIMITER) + objectId, newDataRootHash);
	}

}

// updates the object in the given IPFS DAG
void updateChildNode(ipfs::Client& client,const std::string& source,const std::string& tree,const std::string& channelId, std::string& txid, std::string& tid, std::string& objectId, std::string&& object) {
	// Fetch root hash
	std::string rootHash(dhtGet(dhtNode, std::string(std::string(ROOT_IDENTIFIER)+tree+ std::string(CHARACTER_DELIMITER) + tid)));
	if (rootHash.empty()){
		send(std::move(std::string(source)), channelId, CommandType::exception, txid, objectId, std::string(MSG_INVALID_OBJECT_ID));
		return;
	}
	// Get edges of the hash
	UnorderedEdgeMap map= getUnorderedEdgeNamesOf(client,rootHash);
	std::string existingObjectHash(map.at(objectId));
	if (existingObjectHash.empty())	{
		send(std::move(std::string(source)), channelId, CommandType::exception, txid, objectId, std::string(MSG_INVALID_OBJECT_ID));
		return;
	}
	std::string oldObject(fetchObject(existingObjectHash));
	std::string updatedObject(updateValues(oldObject, object));
	// Add the object 
	std::string objectHash(addObject(client,updatedObject));

	// Add the object hash to dht
	dhtPut(dhtNode, std::string(tree+ std::string(CHARACTER_DELIMITER) + tid + std::string(CHARACTER_DELIMITER) + objectId), objectHash);

	send(std::move(std::string(source)), channelId, CommandType::stateChange, txid, objectId, objectHash);

	//TODO: Remove the existing link labeled with function Id	
	//rootHash = exec(("ipfs object patch rm-link " + rootHash + " \"" + objectId + "\"").c_str(), NULL);

	// Link the object to tree
	linkToRoot(client, tree + std::string(CHARACTER_DELIMITER) + tid, objectId, objectHash, txid);
}

// delete the object for the given IPFS DAG
void deleteObject(ipfs::Client& client,const std::string& source,const std::string& tree,const std::string& channelId, std::string& txid, std::string& tid, std::string& objectId) {
	// Add the functionHash to dht
	dhtPut(dhtNode, std::string(tree + std::string(CHARACTER_DELIMITER) + tid + std::string(CHARACTER_DELIMITER) + objectId), "");
	send(std::move(std::string(source)), channelId, CommandType::stateChange, txid, objectId, "deleted successfully");
	// Fetch root hash
	std::string rootHash(dhtGet(dhtNode, std::string(std::string(ROOT_IDENTIFIER)+tree+ std::string(CHARACTER_DELIMITER) + tid)));
	unLinkToRoot(client,tree+ std::string(CHARACTER_DELIMITER) + tid,txid, rootHash, objectId);
	
	if (tree == std::string(TEMPLATE_TREE)) {
		// clearing the document root hash in DHT.
		dhtPut(dhtNode, std::string(ROOT_IDENTIFIER)+std::string(DATA_TREE)+ std::string(CHARACTER_DELIMITER) + tid + std::string(CHARACTER_DELIMITER) + objectId, "");
	}
}

// Fetch function definition based on the given tenant and functionId
void fetchFunction(const std::string&& source,std::string&& channelId ,std::string&& txid,std::string&& tid, std::string&& functionId,std::string&& hash){
	if (not hash.empty()){
		std::string data(fetchObject(hash));
		send(std::move(source),channelId, CommandType::fetchFunction, txid, functionId, data);
		return;
	}
	dhtGet(dhtNode, std::string(std::string(FUNCTION_TREE) + std::string(CHARACTER_DELIMITER)+ tid + std::string(CHARACTER_DELIMITER) + functionId), [&source,&channelId,&txid,&functionId](const std::string& val) {
		  if (val.empty()) {
			 send(std::move(source),channelId, CommandType::fetchFunction, txid, functionId, std::string(MSG_FUNCTION_NOT_FOUND));
			 return;
		 }
		 std::string data(fetchObject(val));
		 send(std::move(source),channelId,CommandType::fetchFunction,txid,functionId, data);
	});	
}

// Fetch key values from the new object and update in the new 
std::string updateValues(std::string oldObject,std::string newObject) {

	json oldObj = json::parse(oldObject);
	json newObj = json::parse(newObject);	
	for (json::iterator it = newObj.begin(); it != newObj.end(); ++it) {
		oldObj[it.key()] = it.value();
	}

	return oldObj.dump();
}

// Fetch transaction details for the given transaction id
void fetchTransaction(std::string&& source,std::string&& channelId, std::string&& txid) {
	dhtGet(dhtNode, std::string(TRANSACTION_TREE)+std::string(CHARACTER_DELIMITER) + txid, [&source,&channelId, &txid](const std::string& hash) {
		if (hash.empty()) {
			send(std::move(source),channelId, CommandType::exception, txid, "", std::string(MSG_TRANSACTION_NOT_FOUND));
			return;
		}
		std::string data(fetchObject(hash));
		send(std::move(source),channelId, CommandType::fetchTx, txid, "", data);
	});
}

// Fetch block details for the given block number
void fetchBlock(std::string&& source, std::string&& channelId, std::string&& txid, std::string&& b) {
	dhtGet(dhtNode, std::string(BLOCK_TREE) + std::string(CHARACTER_DELIMITER) + b, [&source,&channelId,&txid](const std::string& hash) {
		if (not hash.empty()) {
			std::string data(fetchObject(hash));
			send(std::move(source), channelId, CommandType::fetchBlock, txid, "", data);
			return;
		}
		send(std::move(source), channelId, CommandType::exception, txid, "", std::string(MSG_BLOCK_NOT_FOUND));
	});
	
}

// Adds the given transaction to the transaction tree in IPFS
void addToTxTree(ipfs::Client& client ,json& transaction) {
	// Add the function definition 
	std::string txHash(addObject(client, transaction.dump()));
	std::string txid=transaction["txid"];
	std::string tid = transaction["tid"];
	// Add the IPFS tx hash to the DHT
	dhtPut(dhtNode, std::string(TRANSACTION_TREE)+ std::string(CHARACTER_DELIMITER) + txid, txHash);
	
	std::string txRootHash(dhtGet(dhtNode, std::string(ROOT_IDENTIFIER)+std::string(TRANSACTION_TREE)+ std::string(CHARACTER_DELIMITER) + tid));
	if (txRootHash.empty()) {
		txRootHash = addObject(client,(std::string(ROOT_IDENTIFIER) + std::string(TRANSACTION_TREE) + std::string(CHARACTER_DELIMITER) +tid));
	}
	std::string newTxRootHash;
	client.ObjectPatchAddLink(txRootHash, txid, txHash, &newTxRootHash);
	dhtPut(dhtNode, std::string(TRANSACTION_TREE) + std::string(CHARACTER_DELIMITER) + newTxRootHash, txid);
	dhtPut(dhtNode, std::string(std::string(ROOT_IDENTIFIER) + std::string(TRANSACTION_TREE) + std::string(CHARACTER_DELIMITER) + tid), newTxRootHash);
	
}

// Adds list of transactions to the IPFS
void addTransactions(ipfs::Client& client,std::string& input) {
	json block = json::parse(input);
	json transactions = block["data"];	
	for (int i = 0; i < transactions.size(); i++) {
		addToTxTree(client,transactions[i]);
	}
	// Add block to the IPFS
	std::string rootHash = dhtGet(dhtNode, std::string(ROOT_IDENTIFIER) + std::string(BLOCK_TREE));
	if (rootHash.empty()) {
		rootHash = addObject(client,std::string(ROOT_IDENTIFIER) + std::string(BLOCK_TREE));
	}
	std::string blockId = std::to_string(block["n"].get<int>());

	std::string objectHash = addObject(client,block.dump());
	dhtPut(dhtNode, std::string(BLOCK_TREE) + std::string(CHARACTER_DELIMITER) + blockId, objectHash); 
	linkToRoot(client, std::string(BLOCK_TREE), blockId, objectHash, "");
	json j = block["v"][0];
	j["n"] = blockId;	
	gossip(dhtNode, "master21", CommandType::verifyBlock, { {"b",j.dump()} });
}

// Queues the received state change commands from the clients
void onStateChange(FieldValueMap&& theFields){
	std::string& tid = theFields.at("tid");
	std::string& cmds = theFields.at("cmds");
	std::string txid = theFields.at("txid"); // Use copy here so when we move this memory later on, we don't invalidate the original string in the map.
	std::string& source = theFields.at("ws");	
	std::string& channel = theFields.at("k");
	std::vector<std::string> commands;
	boost::split(commands, cmds, boost::is_any_of(","));
	// Parse each command and push to the queue
	std::vector<std::string> command;
	std::string key;	
	for (int i = 0; i < commands.size(); i++) {
		boost::split(command, commands[i], boost::is_any_of(" "));	
		if ((command[0] == "2" || command[0] == "5" || command[0] == "8")) {
			key = tid + command[1];
			if(source =="1")
			send(std::move(std::string(source)), channel, CommandType::stateChange, txid, command[2], "ack");
		}
		else {
			key = tid + command[0];
			if (source == "1")
			send(std::move(std::string(source)), channel, CommandType::stateChange, txid, command[1], "ack");
		};
		
		std::lock_guard<std::mutex> lock(stateUpdateMutex);
		UnorderedQueueMap::iterator it = stateChangeQueueMap.find(key);
		
		// If queue found then push the new transaction to the queue
		DoubleLong tm = std::stoll(theFields["tm"]);
		if (it != stateChangeQueueMap.end()){
			try {
				if(it->second != nullptr)
					it->second->push(std::make_pair(txid, std::make_pair(std::make_pair(theFields,std::move(command)), tm)));
				//TODO: Planned to investigate this
				else std::cout << "This shouldn't be ocurring..Trying to push to the null pointer queue" << endl;
			}
			catch (exception e) {
				std::cout << "Error occurred while inserting into queue " << endl;
			}
		}
		else {			
			// Create a new queue and push the transaction to the queue
			try {
				OrderedPriorityQueue* queue = new OrderedPriorityQueue();
				queue->push(std::make_pair(std::move(txid), std::make_pair(std::make_pair(std::move(theFields), std::move(command)), tm)));
				stateChangeQueueMap[key] = std::move(queue);
				// start asynchrous thread to process this queue
				std::thread t(processStateUpdates, key);
				t.detach();
			}
			catch (exception e) {
				std::cout << "exception occurred while creating queue" << e.what();
			}
		}
	}

}

// Execute the state change commands
void executeStateChangeCommand(ipfs::Client& client, std::vector<std::string>&& command,std::string& source, std::string & channelId, std::string & txid, std::string & tid){
	try {
		Command cmd = static_cast<Command>(stoi(command[0]));
		switch (cmd) {
		case Command::addFunction: {
			try {
				addChildNode(client, source, channelId, txid, tid, std::string(FUNCTION_TREE), command[1], std::move(base64_decode(command[2])));
			}
			catch (exception e) {
				std::cout << "Exception occurred while executing function" << endl;
			}
			break;
		}
		case Command::addDocument: {
			createOrDeleteDocument(client, source, channelId, txid, tid, command[1], command[2], std::move(base64_decode(command[3])));
			break;
		}
		case Command::addTemplate: {
			addChildNode(client, source, channelId, txid, tid, std::string(TEMPLATE_TREE), command[1], std::move(base64_decode(command[2])));
			break;
		}
		case Command::updateFunction: {
			updateChildNode(client, source, std::string(FUNCTION_TREE), channelId, txid, tid, command[1], std::move(base64_decode(command[2])));
			break;
		}
		case Command::updateTemplate: {
			updateChildNode(client, source, std::string(TEMPLATE_TREE), channelId, txid, tid, command[1], std::move(base64_decode(command[2])));
			break;
		}
		case Command::updateDocument: {
			updateOrDeleteDocument(client, source, channelId, txid, tid, command[1], command[2], std::move(base64_decode(command[3])), "update");
			break;
		}
		case Command::deleteFunction: {
			deleteObject(client, source, std::string(FUNCTION_TREE), channelId, txid, tid, command[1]);
			break;
		}
		case Command::deleteTemplate: {
			deleteObject(client, source, std::string(TEMPLATE_TREE), channelId, txid, tid, command[1]);
			break;
		}
		case Command::deleteDocument: {
			updateOrDeleteDocument(client, source, channelId, txid, tid, command[1], command[2], std::move(""), "delete");
			break;
		}

		}
	}
	catch (std::exception e) {
		std::cout << "exception inside execute state change" << e.what();

	}
}

// Process all pending status updates
void processStateUpdates(const std::string& key) {
	OrderedPriorityQueue* queue = stateChangeQueueMap.at(key);	
	ipfs::Client* client = getIPFSClient();
	while (!queue->empty()) {
		    pair<std::string, pair<pair<FieldValueMap,vector<std::string>>, long>> top = queue->top();			
			FieldValueMap theFields = top.second.first.first;	
			//std::vector<std::string> command(top.second.first.second.begin(), top.second.first.second.end());
			executeStateChangeCommand(*client,std::move(top.second.first.second) , theFields["ws"], theFields["k"], theFields["txid"], theFields["tid"]);
			std::lock_guard<std::mutex> lock(stateUpdateMutex);
			queue->pop();
		}
		// Time to flush out the queue as there is nothing in the queue to process	
	std::lock_guard<std::mutex> lock(stateUpdateMutex);
	delete stateChangeQueueMap[key];
	stateChangeQueueMap.erase(key);
	delete client;
}

// Processes the blocks received by master node and commits to the IPFS
void processBlocks(PriorityPairQueue* queue) {
	ipfs::Client* client= getIPFSClient();
	while (true) {
		while (!queue->empty()) {
			std::lock_guard<std::mutex> lock(blockCommitMutex);
			std::pair<int, std::string> block = queue->top();
			addTransactions(*client,block.second);
			// clear the block from queue			
			queue->pop();
		}

	}

	delete client;
}

// Send Connectionn Request details to masternodes and storagenodes in the network
void sendConnectionRequest(int& port){
	FieldValueMap theFields;
	char cmdstr[5];
	sprintf(cmdstr, "%d", static_cast<uint8_t>(BlockChainNode::NodeType::storageNode));
	theFields.insert(FieldValueMap::value_type(std::string("t"), std::string(cmdstr)));
	// uri of the storage node
	std::string ip(getPublicIP());
	boost::trim(ip);
	theFields.insert(FieldValueMap::value_type(std::string("u"),std::string("ws://"+ip+std::string(":")+std::to_string(port))));
	gossip(dhtNode,"masternodes", CommandType::connectionRequest, theFields);
	gossip(dhtNode,"storagenodes", CommandType::connectionRequest, theFields);
}

// Returns new IPFS connection object
ipfs::Client* getIPFSClient() {
	ipfs::Client* client=new ipfs::Client(ipfsAddress->first,ipfsAddress->second);
	return std::move(client);
}

int main(int argc, char *argv[]) {
	try {
		dhtNode = new dht::DhtRunner();
		std::cout << "Initializing keypair..." << std::endl;
		nodeIdentity = initializeKeypair();
		std::string test_wsIP;
		command_params params;
		opt::options_description optionsList("Storage Node command options");
		optionsList.add_options()
			("i", opt::value<std::string>()->notifier([](std::string address) {
			std::pair<std::string,std::string> _address = dht::splitPort(boost::any_cast<std::string>(address));
			ipfsAddress->first = _address.first;
			ipfsAddress->first = _address.second;
			if (not _address.first.empty() and _address.second.empty()) {
				ipfsAddress->second = IPFS_DEFAULT_PORT;
			}
		}), "IPFS Address");
			
		parseArgs(argc, argv,&params, optionsList);
		if(ipfsAddress==nullptr || ipfsAddress->first.empty())
			ipfsAddress = &std::make_pair(std::string(IPFS_DEFAULT_IP), (long)IPFS_DEFAULT_PORT);
		dhtNode->run(params.port, *nodeIdentity, true);
		if(not params.bootstrap.first.empty()){
			dhtNode->bootstrap(params.bootstrap.first, params.bootstrap.second);			
		}		
		//Subscribing to common storage node channel
		subscribeTo(dhtNode, "storagenodes");
		// Listening on own channel
		subscribeTo(dhtNode, dhtNode->getId().toString());
		//send connection request to masternodes and storageNodes
		sendConnectionRequest(params.wsPort);
		registerDataHandlers({{ CommandType::stateChange, [](FieldValueMap&& theFields) {
				std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
				onStateChange(std::move(theFields));
			}},{CommandType::fetchFunction, [](FieldValueMap&& theFields) {
				FieldValueMap::iterator id= theFields.find("id");
				FieldValueMap::iterator h = theFields.find("h");
				fetchFunction(std::move(theFields.at("ws")),std::move(theFields.at("k")), std::move(theFields.at("txid")), std::move(theFields.at("tid")), std::move((id != theFields.end()) ? id->second : ""), std::move((h != theFields.end()) ? h->second : ""));
			}},{CommandType::fetchTemplate, [](FieldValueMap&& theFields) {
				fetchTemplate(std::move(theFields.at("ws")),std::move(theFields.at("k")),std::move(theFields.at("txid")), std::move(theFields.at("tid")), std::move(theFields.at("id")));
			}},{CommandType::fetchDocument, [](FieldValueMap&& theFields) {
				findDocument(std::move(theFields.at("ws")),std::move(theFields.at("k")),std::move(theFields.at("txid")), std::move(theFields.at("tid")),std::move(theFields.at("id")), std::move(theFields.at("expression")));
			}},{CommandType::closeBlock, [](FieldValueMap&& theFields) {
				// Close the block	
				try {
				std::string decoded(base64_decode(theFields["b"]));
				json _block = json::parse(decoded);
				int blocknumber = _block["n"].get<int>();
				// Add the verification details
				json verifier;
				verifier["k"] = theFields.at("key");
				verifier["si"] = theFields.at("si");
				json arr = json::array();
				arr.push_back(verifier);
				_block["v"] = arr;
				std::lock_guard<std::mutex> lock(blockCommitMutex);
				blocksToCommit.push(std::move(std::make_pair(std::move(blocknumber), _block.dump())));
				}
				catch (exception e) {
					std::cerr << "Exception while adding block: " << theFields["b"] << std::endl;
				}
			}},{CommandType::verifyBlock, [](FieldValueMap&& theFields) {
				// verify the block				
				json block = json::parse(base64_decode(theFields.at("d")));				
				std::string blockId = theFields.at("b");
				std::string hash=dhtGet(dhtNode, std::string(BLOCK_TREE) + std::string(CHARACTER_DELIMITER) + blockId);
				ipfs::Client* client= getIPFSClient();
				std::string obj = fetchObject(*client,hash);
				json object_json = json::parse(obj);
				object_json["v"].push_back(block);					
				std::string objectHash = addObject(*client,object_json.dump());
				// update DHT with new block hash
				dhtPut(dhtNode, std::string(BLOCK_TREE) + std::string(CHARACTER_DELIMITER) + blockId, objectHash);
				std::string rootHash = dhtGet(dhtNode, std::string(ROOT_IDENTIFIER) + std::string(BLOCK_TREE));
				linkToRoot(*client, std::string(BLOCK_TREE), blockId, objectHash, "");
				delete client;
			}}});		
		std::cout << "Running block processor " << endl;
		std::future<void> result(std::async(processBlocks, &blocksToCommit));
		std::cout << "Node is up and running..." << endl;
		ipfsClient=new ipfs::Client(ipfsAddress->first,ipfsAddress->second);	
		wsServer = new WebSocketServer();
		wsServer->run(params.wsPort);		
	}
	catch (const std::exception e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}
	return 0;	
}


