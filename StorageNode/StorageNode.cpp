// Disable the buffer overflow warning, since we're internally checking for it.
#pragma once
#pragma warning (disable: 4996)

#include "StorageNode.h"

// Most of this function is from https://stackoverflow.com/questions/478898/how-to-execute-a-command-and-get-output-of-command-within-c-using-posix
// We extend it to allow for input from stdin.
std::string exec(const char* cmd, const char* standardInput) {
	std::array<char, 128> buffer;
	std::string result;
	std::string extendedCmd(cmd);
	if (standardInput != NULL) {
		// We need to ECHO the data into the stdin.
		extendedCmd = std::string("echo '") + standardInput + std::string("' | ") + extendedCmd;
	}
	extendedCmd = extendedCmd + std::string(" 2>&1");
	const char* extendedCmdPtr = extendedCmd.c_str();	
	FILE* pipe = _popen(extendedCmdPtr, "r");
	if (pipe == NULL) throw std::runtime_error("popen() failed!");
	while (!feof(pipe)) {
		if (fgets(buffer.data(), 128, pipe) != nullptr)
			result += buffer.data();
	}
	_pclose(pipe);
	boost::trim(result);	
	return std::string(result);
}

// runs ipfs node in the system
void runIpfsDaemon() {
	ipfsService = _popen("ipfs daemon", "r");
	int d = fileno(ipfsService);		
#ifdef defined(_WIN32) || defined(_WIN64)
	unsigned long ul = 1;
	ioctlsocket(d, FIONBIO, (unsigned long *)&ul);
#elif __linux__
	fcntl(d, F_SETFL, _O_NONBLOCK);
#endif // 

	
}

// Returns a map of (edge name, hash) such that indexing by name will give the hash of the node where the edge leads.
UnorderedEdgeMap getUnorderedEdgeNamesOf(ipfs::Client& client, const std::string& nodeHash) {
	UnorderedEdgeMap namesToHashes;
	ipfs::Json links;
	try {
		client.ObjectLinks(nodeHash, &links);
	}
	catch (exception e) {
		logError("getUnorderedEdgeNamesOf: %s",e.what());
	}
	// Next, we need to insert the links into a hash table.
	for (int i = 0; i < links.size(); i++) {
		// Now, insert hash and edge_name
		namesToHashes.insert(UnorderedEdgeMap::value_type(links[i]["Name"], links[i]["Hash"]));
	}
	return UnorderedEdgeMap(namesToHashes);
}

//Returns ordered map of (edge name, hash) such that indexing by name will give the hash of the node where the edge leads.
OrderedEdgeMap getOrderedEdgeNamesOf(ipfs::Client& client, const std::string& nodeHash) {
	OrderedEdgeMap namesToHashes;
	ipfs::Json links;
	try {
		client.ObjectLinks(nodeHash, &links);
	}
	catch (exception e) {
		logError("getOrderedEdgeNamesOf: ",e.what());
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
json fetchDocument(ipfs::Client& client, std::string& hash) {
	return json::parse(std::string(fetchObject(client, hash)));
}

// Fetch document from ipfs based on hash and append the resultset to _rs 
void fetchDocument(ipfs::Client& client, std::string& hash, json& _rs) {
	json _json = fetchDocument(client, hash);
	_rs.push_back(_json);
}

// Fetches the list of documents from ipfs based on default index hash . Default index is assumed as 'id' for now
json fetchDocumentsFromDefaultIndex(ipfs::Client& client, UnorderedEdgeMap& indexes) {
	OrderedEdgeMap _values = getOrderedEdgeNamesOf(client, indexes["id"]);
	_values = getOrderedEdgeNamesOf(client, _values["__"]);
	json _default_index_rs = json::array();
	for (OrderedEdgeMap::iterator it = _values.begin(); it != _values.end(); it++) {
		// append the output to _default_index_rs
		fetchDocument(client,it->second, _default_index_rs);
	}
	return _default_index_rs;
}

// If _docs is null then it will fetch the documents from the default index 
// Sorts the documents based on the given operand
json sortDocumentsByOperand(ipfs::Client& client, json _docs, std::string operand, UnorderedEdgeMap& indexes) {
	if (_docs == json::value_t::null) {
		_docs = fetchDocumentsFromDefaultIndex(client, indexes);
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
		logDebug("Search value not found in the given result: %s",value.c_str());
		return -1;
	}

	int lastfoundat = result->get<1>();	
	while (result.is_initialized()) {
		lastfoundat = result->get<1>();
		if (lastfoundat == 0) break;			
		result = getSearchIndex(_json, result->get<0>(), result->get<1>() - 1, operand, value);
	}
	return lastfoundat;
}

// Find the upper index of the given operand value from the json
int getUpperIndex(json& _json, string& operand, string& value) {

	boost::optional<boost::tuple<uint32, uint32, uint32>> result = getSearchIndex(_json, 0, (uint32)(_json.size() - 1), operand, value);
	if (!result) {
		logDebug("Search value not found in the given result");
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
	logDebug("From Index: %d ,to Index: %d",from,to);
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
void findDocument(std::string&& source, std::string&& channelId, std::string&& txid, std::string&& tid, std::string&& entityId, std::string&& expression) {
	ipfs::Client* client= getIPFSClient();
	json docs = searchDocument(*client, tid, entityId, std::move(expression));
	logDebug("Sending list of docs: %s", docs.dump().c_str());
	send(std::move(source), channelId, CommandType::fetchDocument, txid, entityId, (docs.type() == json::value_t::string) ? docs.get<std::string>() : docs.dump());
}

// Search documents in the given entity tree
json searchDocument(ipfs::Client& client, std::string & tid, std::string & entityId, std::string && expression) {
	// Get the entityRoot hash
	std::string hash(dhtGet(std::string(ROOT_IDENTIFIER) + std::string(DATA_TREE) + std::string(CHARACTER_DELIMITER) + tid + std::string(CHARACTER_DELIMITER) + entityId));
	if (hash.empty())
		return std::string(MSG_TEMPLATE_NOT_FOUND);
	// First, get the list of indexes on the template.
	UnorderedEdgeMap indexes = getUnorderedEdgeNamesOf(client, hash);
	std::vector<std::string> postfixform = getPostfixedExpressionOf(expression);
	TreeNode* filterCondition = getExpressionTreeFrom(postfixform);
	// Next, mark the hash map with true for those operands that refer to indexes that exist.
	markExistingIndexes(indexes, filterCondition);
	// Now, filterExpression->fields contains true / false values for all indexes that we might have to traverse.
	json _null(json::value_t::null);
	// Evaluate expression tree and get resulted documents
	try {
		json docs = evaluateExpression(client, filterCondition, indexes, _null);
		return docs;
	}
	catch (std::exception e) {
		logError("Issue while searching for document: %s", e.what());
	}
	return "";
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
	json left = evaluateExpression(client, filter->left, indexes, _docs);

	// Evaluate right subtree
	json right = evaluateExpression(client, filter->right, indexes, _docs);
	if (filter->root == "=") {
		if (indexes.find(left) != indexes.end()) { // index found			
			OrderedEdgeMap _values = getOrderedEdgeNamesOf(client, indexes[left]);
			if (left == "id")
				_values = getOrderedEdgeNamesOf(client, _values["__"]);
			auto search = _values.find(right);
			if (search != _values.end()) {
				json _rs = json::array();
				_rs.push_back(fetchDocument(client,search->second));
				return _rs;
			}
			else {
				return "";
			}
		}
		else { // index not found
			json _rs = sortDocumentsByOperand(client, _docs, left, indexes);
			if(_rs.size() >0)
			// filtering
			_rs = equals(_rs, left, right);
			return _rs;
		}
	}
	else if (filter->root == "<>") {
		if (indexes.find(left) != indexes.end()) {
			OrderedEdgeMap _values = getOrderedEdgeNamesOf(client, indexes[left]);
			auto search = _values.equal_range(right);
			if (search.first != _values.end()) {
				json _rs = json::array();
				for (OrderedEdgeMap::iterator it = _values.begin(); it != search.first; it++) {
					// append the output to resultset
					fetchDocument(client,it->second, _rs);
				}
				for (OrderedEdgeMap::iterator it = search.second; it != _values.end(); it++) {
					// append the output to resultset _rs
					fetchDocument(client,it->second, _rs);
				}
				return   _rs;
			}
			else {
				return "";
			}
		}
		else {
			json _rs = sortDocumentsByOperand(client, _docs, left, indexes);
			if (_rs.size() > 0)
			_rs = not_equals(_rs, left, right);
			return _rs;
		}

	}
	else if (filter->root == ">") {
		if (indexes.find(left) != indexes.end()) {
			OrderedEdgeMap _values = getOrderedEdgeNamesOf(client, indexes[left]);
			auto search = _values.upper_bound(right);
			if (search != _values.end()) {
				json _rs = json::array();
				for (OrderedEdgeMap::iterator it = search; it != _values.end(); it++) {
					// append the output to resultset
					fetchDocument(client,it->second, _rs);
				}
				return  _rs;
			}
			else {
				return "";
			}
		}
		else {
			json _rs = sortDocumentsByOperand(client, _docs, left, indexes);
			if (_rs.size() > 0)
			_rs = greaterthan(_rs, left, right);
			return _rs;
		}
	}
	else if (filter->root == ">=") {
		if (indexes.find(left) != indexes.end()) {
			OrderedEdgeMap _values = getOrderedEdgeNamesOf(client, indexes[left]);
			auto search = _values.lower_bound(right);
			if (search != _values.end()) {
				json _rs = json::array();
				for (OrderedEdgeMap::iterator it = search; it != _values.end(); it++) {
					// append the output to resultset
					fetchDocument(client,it->second, _rs);
				}
				return  _rs;
			}
			else {
				return "";
			}
		}
		else {
			json _rs = sortDocumentsByOperand(client, _docs, left, indexes);
			if (_rs.size() > 0)
			_rs = greaterthan_equals(_rs, left, right);
			return _rs;
		}
	}
	else if (filter->root == "<=") {
		if (indexes.find(left) != indexes.end()) {
			OrderedEdgeMap _values = getOrderedEdgeNamesOf(client, indexes[left]);
			auto search = _values.lower_bound(right);
			if (search != _values.end()) {
				json _rs = json::array();
				search++;
				for (OrderedEdgeMap::iterator it = _values.begin(); it != search; it++) {
					// append the output to resultset
					fetchDocument(client,it->second, _rs);
				}
				return  _rs;
			}
			else {
				return "";
			}
		}
		else {
			json _rs = sortDocumentsByOperand(client, _docs, left, indexes);
			if (_rs.size() > 0)
			_rs = lessthan_equals(_rs, left, right);
			return _rs;
		}
	}
	else if (filter->root == "<") {
		if (indexes.find(left) != indexes.end()) {
			OrderedEdgeMap _values = getOrderedEdgeNamesOf(client, indexes[left]);
			auto search = _values.lower_bound(right);
			if (search != _values.end()) {
				json _rs = json::array();
				for (OrderedEdgeMap::iterator it = _values.begin(); it != search; it++) {
					// append the output to resultset
					fetchDocument(client,it->second, _rs);
				}
				return _rs;
			}
			else {
				return "";
			}
		}
		else {
			json _rs = sortDocumentsByOperand(client, _docs, left, indexes);
			if (_rs.size() > 0)
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
std::string addObject(ipfs::Client& client, const std::string& data) {
	try {
		ipfs::Json object;
		ipfs::Json input;
		input["Data"] = data;
		client.ObjectPut(input, &object);
		return object["Hash"];
	}
	catch (exception e) {
		logError("exception while comitting object to IPFS: %s",e.what());
	}
	return std::string("");
}

// Given some data, adds a new merckle object to the DAG and adds links to the given object
// Returns the hash of the object.
std::string addObjectAndLink(ipfs::Client& client, const std::string& data, std::string& child, std::string& link) {
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
std::string fetchObject(ipfs::Client& client, const std::string &hash) {
	try {
		std::string output;
		client.ObjectData(hash, &output);
		return output;
	}
	catch (std::exception e) {
		logError("Exception while fetching object from IPFS: %s ",hash.c_str());
	}
	return std::string("");
}

// Given template Id returns the defintion of the template
void fetchTemplate(std::string&& source, std::string&& channelId, std::string&& txid, std::string&& tid, std::string&& templateId) {
	dhtGet(std::string(std::string(TEMPLATE_TREE) + std::string(CHARACTER_DELIMITER) + tid + std::string(CHARACTER_DELIMITER) + templateId), [source, channelId, txid, templateId](const std::string& val) {
		if (val.empty()) {
			send(std::move(source), channelId, CommandType::fetchTemplate, txid, templateId, std::string(MSG_TEMPLATE_NOT_FOUND));
			return;
		}
		ipfs::Client* client = getIPFSClient();
		send(std::move(source), channelId, CommandType::fetchTemplate, txid, templateId, fetchObject(*client,val));
	});
}

// Given the template and document  id,updates or deletes the document
void updateOrDeleteDocument(ipfs::Client& client, std::string& source, std::string& channelId, std::string& txid, std::string& tid, std::string& entityId, std::string& expression, std::string&& documentJson, std::string op) {
	// Fetch root entity if entityRoot is empty then return
	std::string entityRoot = dhtGet(std::string(ROOT_IDENTIFIER) + std::string(DATA_TREE) + std::string(CHARACTER_DELIMITER) + tid + std::string(CHARACTER_DELIMITER) + entityId);
	if (entityRoot.empty()) {
		send(std::move(std::string(source)), channelId, CommandType::exception, txid, "", std::string(MSG_DOCUMENT_ROOT_NOT_FOUND));
		return;
	}
	//std::string expression = "\"id\"=\"" + documentId + "\"";
	json docs = searchDocument(client, tid, entityId, std::move(expression));
	if (docs.empty() || docs.size() <= 0) {
		// send message to the caller that there is no document in the system for the given id
		send(std::string(source), channelId, CommandType::exception, txid, "", std::string(MSG_DOCUMENT_NOT_FOUND));
		return;
	}
	std::string docId;
	if (op == "update")	{
		for (int i = 0; i < docs.size(); i++) {
			json& doc = docs[i];
			docId = doc["id"].get<std::string>();
			createOrDeleteDocument(client, source, channelId, txid, tid, entityId, docId, std::move(updateValues(doc.dump(), documentJson)), op);
		}
	}
	else {
		for (int i = 0; i < docs.size(); i++) {
			json& doc = docs[i];
			docId = doc["id"].get<std::string>();			
			createOrDeleteDocument(client, source, channelId, txid, tid, entityId, docId,std::move(doc.dump()) , op);
		}
	}
	// Send result to client
	send(std::move(std::string(source)), channelId, CommandType::stateChange, txid, "", (op == "update")? std::string(MSG_DOCUMENT_UPDATE):std::string(MSG_DOCUMENT_DELETE));
}

// Given the template Id and document definion persists in IPFS
void createOrDeleteDocument(ipfs::Client& client, std::string& source, std::string& channelId, std::string& txid, std::string& tid, std::string& entityId, std::string& documentId, std::string&& documentJson, std::string op) {

	// Get template entity if entityRoot is empty return error
	std::string entityRoot = dhtGet(std::string(ROOT_IDENTIFIER) + std::string(DATA_TREE) + std::string(CHARACTER_DELIMITER) + tid + std::string(CHARACTER_DELIMITER) + entityId);
	if (entityRoot.empty()) {
		send(std::move(std::string(source)), channelId, CommandType::exception, txid, documentId, std::string(MSG_TEMPLATE_NOT_FOUND));
		return;
	}
	std::string documentHash = "";
	if (op == "") { 
		documentId = getGUID(36);
		try {
			json j = json::parse(documentJson);
			j["id"] = documentId;
			documentJson = j.dump();
		}
		catch (std::exception e) {
			send(std::move(std::string(source)), channelId, CommandType::exception, txid, "", std::string(MSG_INVALID_DOCUMENT_STRUCTURE));
		}
	}
	if (op != "delete") {
		// First, create the document object D by providing the JSON to stdin.
		documentHash = addObject(client, documentJson);
		// update document hash in dht		
		dhtPut(std::string(DATA_TREE) + std::string(CHARACTER_DELIMITER) + tid + std::string(CHARACTER_DELIMITER) + entityId + std::string(CHARACTER_DELIMITER) + documentId, documentHash, true);
		if(op == "")send(std::move(std::string(source)), channelId, CommandType::stateChange, txid, documentId, documentHash);
	}
	else {
		// clear document hash entry in dht
		dhtPut(std::string(DATA_TREE) + std::string(CHARACTER_DELIMITER) + tid + std::string(CHARACTER_DELIMITER) + entityId + std::string(CHARACTER_DELIMITER) + documentId, documentHash, true);		
	}

	UnorderedEdgeMap a_b = getUnorderedEdgeNamesOf(client, entityRoot);
	json inputJson = json::parse(documentJson);
	// We'll go through each index and check if the index name exists on the document. If it does,
	// we'll update the index.
	for (UnorderedEdgeMap::iterator mapDef = a_b.begin(); mapDef != a_b.end(); mapDef++) {
		const std::string& index = mapDef->first;		
		json::iterator target = inputJson.find(index);
		bool found = target != inputJson.end(); // Does this index exist on the new document to create?		
		if (found) {
			const std::string& value = target->get<std::string>();
			logDebug("Index value is %s",value.c_str());
			// Next, we need to check if an edge from b to c exists with this value.
			const std::string& bNode = mapDef->second;
			UnorderedEdgeMap b_c = getUnorderedEdgeNamesOf(client, bNode);
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
				logDebug("The index value %s exists", indexValue.c_str());
				if (op == "update" || op == "delete")
					client.ObjectPatchRmLink(cNode, documentId, &cNode);
				if (op != "delete")
					client.ObjectPatchAddLink(cNode, documentId, documentHash, &cNode);
				// Next, remove the (B, C) edge labeled with the index value since C's hash has changed.
				std::string newBNode;
				client.ObjectPatchRmLink(bNode, indexValue, &newBNode);
				// Next, re-add the (B, C) edge, keeping the same value as before.				
				client.ObjectPatchAddLink(newBNode, indexValue, cNode, &newBNode);
				// Next, we need to update edge (A, B) since the hash of B has changed.
				// First, remove (A, B) labeled with the index name.
				client.ObjectPatchRmLink(entityRoot, index, &entityRoot);
				// Finally, re-add (A, B) keeping the same index name as before.
				client.ObjectPatchAddLink(entityRoot, index, newBNode, &entityRoot);
				// Link entityRoot and tenant, tenant and data root
				// update the dht with new entityroot hash
				dhtPut(std::string(ROOT_IDENTIFIER) + std::string(DATA_TREE) + std::string(CHARACTER_DELIMITER) + tid + std::string(CHARACTER_DELIMITER) + entityId, entityRoot, true);

			}
			else { 
				   // the index value doesn't exist
				  // First, create the (C, D) edge and label it with the id of this document
				logDebug("The index value doesn't exist");
				std::string cNode(addObject(client, entityRoot));
				if (op == "update" || op == "delete")
					client.ObjectPatchRmLink(cNode, documentId, &cNode);
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
				dhtPut(std::string(ROOT_IDENTIFIER) + std::string(DATA_TREE) + std::string(CHARACTER_DELIMITER) + tid + std::string(CHARACTER_DELIMITER) + entityId, entityRoot, true);

			}
		}
	}
	logInfo("New entity data root is %s" ,entityRoot.c_str());
}

// Links the given object to the specified tree. If root not found then it creates the root and links the object to root.
void linkToRoot(ipfs::Client& client, const std::string& tree, std::string &objectId, std::string &objectHash, const std::string& txid) {

	// Add the effected object hash and tx in dht
	if (not txid.empty())
		dhtPut(std::string(TRANSACTION_TREE) + std::string(CHARACTER_DELIMITER) + objectHash, txid, true);

	// Add root entity if rootHash is empty
	std::string rootHash(dhtGet(std::string(std::string(ROOT_IDENTIFIER) + tree)));
	if (rootHash.empty()) {
		rootHash = addObjectAndLink(client, tree, objectHash, objectId);
	}
	else {
		// root Hash and tenant hash will be labeled with tenantId
		client.ObjectPatchAddLink(rootHash, objectId, objectHash, &rootHash);
	}

	// update the root hash in dht
	dhtPut(std::string(ROOT_IDENTIFIER) + tree, rootHash, true);

	logInfo("New root hash of the tree . %s  is %s", tree.c_str(),rootHash.c_str());
}

// Removes the links between the root and given object and updates the root hash in DHT
void unLinkToRoot(ipfs::Client& client, std::string tree, std::string txid, std::string &rootHash, std::string &objectId) {
	// root Hash and tenant hash will be labeled with tenantId
	std::string newRootHash;
	client.ObjectPatchRmLink(rootHash, objectId, &newRootHash);
	// update the effected root hash and tx in dht
	dhtPut(std::string(TRANSACTION_TREE) + std::string(CHARACTER_DELIMITER) + rootHash, txid, true);

	// update the root hash in dht
	dhtPut(std::string(ROOT_IDENTIFIER) + tree, newRootHash, true);

	logInfo("New root hash of the tree . %s is " ,newRootHash);
}

// Send off response to the requested node/client
void send(const std::string&& source, const std::string &channelId, CommandType cmd, const std::string& txid, const std::string& objectId, const std::string val) {
	json j;
	j["txid"] = txid;
	(cmd != CommandType::exception) ? j["data"] = val : j["error"] = val;
	if (not objectId.empty())
		j["id"] = objectId;
	(source == "1") ? wsServer.send(channelId, j.dump()) : gossip(channelId, cmd, { {"txid",txid}, {"id",objectId}, {"data",val} });
}

// Add child to IPFS
void addChildNode(ipfs::Client& client, std::string& source, const std::string& channelId, std::string& txid, std::string& tid, const std::string& tree,std::string&& object) {
	// Add the object definition 
	std::string objectHash(addObject(client, object));
	std::string objectId = getGUID(36);
	// Add the objectHash to dht
	dhtPut(std::string(tree + std::string(CHARACTER_DELIMITER) + tid + std::string(CHARACTER_DELIMITER) + objectId), objectHash, true);

	send(std::move(std::string(source)), channelId, CommandType::stateChange, txid, objectId, objectHash);
	// Link the object to root
	linkToRoot(client, tree + std::string(CHARACTER_DELIMITER) + tid, objectId, objectHash, txid);
	// If template then build the new data tree 
	if (tree == std::string(TEMPLATE_TREE)) {
		std::string dataRootHash(addObject(client, std::string(std::string(ROOT_IDENTIFIER) + std::string(DATA_TREE) + std::string(CHARACTER_DELIMITER) + tid + std::string(CHARACTER_DELIMITER) + objectId)));

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
		dhtPut(std::string(ROOT_IDENTIFIER) + std::string(DATA_TREE) + std::string(CHARACTER_DELIMITER) + tid + std::string(CHARACTER_DELIMITER) + objectId, newDataRootHash, true);
	}

}

// updates the object in the given IPFS DAG
void updateChildNode(ipfs::Client& client, const std::string& source, const std::string& tree, const std::string& channelId, std::string& txid, std::string& tid, std::string& objectId, std::string&& object) {
	// Fetch root hash
	std::string rootHash(dhtGet(std::string(std::string(ROOT_IDENTIFIER) + tree + std::string(CHARACTER_DELIMITER) + tid)));
	if (rootHash.empty()) {
		send(std::move(std::string(source)), channelId, CommandType::exception, txid, objectId, std::string(MSG_INVALID_OBJECT_ID));
		return;
	}
	// Get edges of the hash
	UnorderedEdgeMap map = getUnorderedEdgeNamesOf(client, rootHash);
	std::string existingObjectHash(map.at(objectId));
	if (existingObjectHash.empty()) {
		send(std::move(std::string(source)), channelId, CommandType::exception, txid, objectId, std::string(MSG_INVALID_OBJECT_ID));
		return;
	}
	std::string oldObject(fetchObject(client,existingObjectHash));
	std::string updatedObject(updateValues(oldObject, object));
	// Add the object 
	std::string objectHash(addObject(client, updatedObject));

	// Add the object hash to dht
	dhtPut(std::string(tree + std::string(CHARACTER_DELIMITER) + tid + std::string(CHARACTER_DELIMITER) + objectId), objectHash, true);

	send(std::move(std::string(source)), channelId, CommandType::stateChange, txid, objectId, objectHash);

	//TODO: Remove the existing link labeled with function Id	
	//rootHash = exec(("ipfs object patch rm-link " + rootHash + " \"" + objectId + "\"").c_str(), NULL);

	// Link the object to tree
	linkToRoot(client, tree + std::string(CHARACTER_DELIMITER) + tid, objectId, objectHash, txid);
}

// delete the object for the given IPFS DAG
void deleteObject(ipfs::Client& client, const std::string& source, const std::string& tree, const std::string& channelId, std::string& txid, std::string& tid, std::string& objectId) {
	// Add the functionHash to dht
	dhtPut(std::string(tree + std::string(CHARACTER_DELIMITER) + tid + std::string(CHARACTER_DELIMITER) + objectId), "", true);
	send(std::move(std::string(source)), channelId, CommandType::stateChange, txid, objectId, "deleted successfully");
	// Fetch root hash
	std::string rootHash(dhtGet(std::string(std::string(ROOT_IDENTIFIER) + tree + std::string(CHARACTER_DELIMITER) + tid)));
	unLinkToRoot(client, tree + std::string(CHARACTER_DELIMITER) + tid, txid, rootHash, objectId);

	if (tree == std::string(TEMPLATE_TREE)) {
		// clearing the document root hash in DHT.
		dhtPut(std::string(ROOT_IDENTIFIER) + std::string(DATA_TREE) + std::string(CHARACTER_DELIMITER) + tid + std::string(CHARACTER_DELIMITER) + objectId, "", true);
	}
}

// Fetch function definition based on the given tenant and functionId
void fetchFunction(const std::string&& source, std::string&& channelId, std::string&& txid, std::string&& tid, std::string&& functionId, std::string&& hash) {
	if (not hash.empty()) {
		ipfs::Client* client = getIPFSClient();
		std::string data(fetchObject(*client,hash));
		send(std::move(source), channelId, CommandType::fetchFunction, txid, functionId, data);
		return;
	}
	dhtGet(std::string(std::string(FUNCTION_TREE) + std::string(CHARACTER_DELIMITER) + tid + std::string(CHARACTER_DELIMITER) + functionId), [source, channelId, txid, functionId](const std::string& val) {
		if (val.empty()) {
			send(std::move(source), channelId, CommandType::fetchFunction, txid, functionId, std::string(MSG_FUNCTION_NOT_FOUND));
			return;
		}
		ipfs::Client* client = getIPFSClient();
		std::string data(fetchObject(*client,val));
		send(std::move(source), channelId, CommandType::fetchFunction, txid, functionId, data);
	});
}

// Fetch key values from the new object and update in the new 
std::string updateValues(std::string oldObject, std::string newObject) {

	json oldObj = json::parse(oldObject);
	json newObj = json::parse(newObject);
	for (json::iterator it = newObj.begin(); it != newObj.end(); ++it) {
		oldObj[it.key()] = it.value();
	}

	return oldObj.dump();
}

// Adds the given transaction to the transaction tree in IPFS
void addToTxTree(ipfs::Client& client, json& transaction) {
	// Add the function definition 
	std::string txHash(addObject(client, transaction.dump()));
	std::string txid = transaction["txid"];
	std::string tid = transaction["tid"];
	// Add the IPFS tx hash to the DHT
	dhtPut(std::string(TRANSACTION_TREE) + std::string(CHARACTER_DELIMITER) + txid, txHash, true);
	logDebug("Added new tx ipfs hash to DHT under the key: %s", (std::string(TRANSACTION_TREE) + std::string(CHARACTER_DELIMITER) + txid).c_str());
	std::string txRootHash(dhtGet(std::string(ROOT_IDENTIFIER) + std::string(TRANSACTION_TREE) + std::string(CHARACTER_DELIMITER) + tid));
	if (txRootHash.empty()) {
		txRootHash = addObject(client, (std::string(ROOT_IDENTIFIER) + std::string(TRANSACTION_TREE) + std::string(CHARACTER_DELIMITER) + tid));
	}
	std::string newTxRootHash;
	client.ObjectPatchAddLink(txRootHash, txid, txHash, &newTxRootHash);
	dhtPut(std::string(TRANSACTION_TREE) + std::string(CHARACTER_DELIMITER) + newTxRootHash, txid, true);
	dhtPut(std::string(std::string(ROOT_IDENTIFIER) + std::string(TRANSACTION_TREE) + std::string(CHARACTER_DELIMITER) + tid), newTxRootHash, true);
	logInfo("New tx root hash for the tenant %s is %s",tid.c_str() ,newTxRootHash.c_str());
}

// Adds list of transactions to the IPFS
void addTransactions(ipfs::Client& client, std::string& input) {
	logDebug("Received block to commit: %s",input.c_str());
	json block = json::parse(input);
	json transactions = block["data"];
	for (int i = 0; i < transactions.size(); i++) {
		addToTxTree(client, transactions[i]);
	}
	// Add block to the IPFS
	std::string blockId = std::to_string(block["n"].get<long long>());

	std::string objectHash = addObject(client, block.dump());
	dhtPut(std::string(BLOCK_TREE) + std::string(CHARACTER_DELIMITER) + blockId, objectHash, true);
	logDebug("Added block IPFS hash in DHT: %s", (std::string(BLOCK_TREE) + std::string(CHARACTER_DELIMITER) + blockId).c_str());
	linkToRoot(client, std::string(BLOCK_TREE), blockId, objectHash, "");
	json j = block["v"][0];
	j["n"] = blockId;
	gossip("master21", CommandType::verifyBlock, { {"b", base64_encodeString(j.dump())} });
}

// Queues the received state change commands from the clients
void onStateChange(FieldValueMap&& theFields) {	
	std::string& tid = theFields.at("tid");
	std::string& cmds = theFields.at("cmds");
	std::string txid = theFields.at("txid"); // Use copy here so when we move this memory later on, we don't invalidate the original string in the map.
	std::string& source = theFields.at("ws");
	std::string& channel = theFields.at("k");
	std::vector<std::string> commands;
	boost::split(commands, cmds, boost::is_any_of(","));
	theFields.erase("cmds");
	std::string key;
	try {
		DoubleLong tm = std::stoll(theFields.at("tm"));
		for (auto const& val : commands) {
			// Parse each command and push to the queue
			std::vector<std::string> command;
			boost::split(command, val, boost::is_any_of(" "));
			if (command.size() > 0) {
				std::string& commandIndex = command.at(0);
				if (commandIndex == "2" || commandIndex == "5" || commandIndex == "8") {
					key = tid + command.at(1);
					/*if (source == "1")
						send(std::move(std::string(source)), channel, CommandType::stateChange, txid, "", "ack");*/
				}
				else {
					key = tid + commandIndex;
					/*if (source == "1")
						send(std::move(std::string(source)), channel, CommandType::stateChange, txid, "", "ack");*/
				};

				std::lock_guard<std::mutex> lock(stateUpdateMutex);
				UnorderedQueueMap::iterator it = stateChangeQueueMap.find(key);

				// If queue found then push the new transaction to the queue			
				if (it != stateChangeQueueMap.end()) {
					logDebug("Pushing to the existing queue, size: %d",it->second->size()) ;	
					try {
						if (it->second != nullptr) {							
							CommandInfo commandInfo(theFields, tm, std::move(command));
							it->second->push(std::move(commandInfo));
						}
						//TODO: Planned to investigate this
						else logError("This shouldn't be ocurring..Trying to push to the null pointer queue");
					}
					catch (exception e) {
						logError("Error occurred while inserting into queue ");
					}
				}
				else {
					// Create a new queue and push the transaction to the queue
					try {
						OrderedPriorityQueue* queue = new OrderedPriorityQueue();
						CommandInfo commandInfo(theFields, tm, std::move(command));
						queue->push(std::move(commandInfo));
						stateChangeQueueMap.insert(UnorderedQueueMap::value_type(key, queue));
						// start asynchrous thread to process this queue
						std::thread t(processStateUpdates, key);
						t.detach();
					}
					catch (exception e) {
						logError("exception occurred while creating queue %s", e.what());
					}
				}
			}
		}
	}
	catch (std::exception e) {
		logError("Issue while parsing command: %s",e.what());
	}
}

// Execute the state change commands
void executeStateChangeCommand(ipfs::Client& client, std::vector<std::string>&& command, std::string& source, std::string & channelId, std::string & txid, std::string & tid) {
	try {
		Command cmd = static_cast<Command>(stoi(command[0]));
		switch (cmd) {
		case Command::addFunction: {
			try {
				addChildNode(client, source, channelId, txid, tid, std::string(FUNCTION_TREE),std::move(base64_decode(command[1])));
			}
			catch (exception e) {
				logError("Exception occurred while executing function: ",e.what());
			}
			break;
		}
		case Command::addDocument: {
			std::string dummy("");
			createOrDeleteDocument(client, source, channelId, txid, tid, command[1],dummy,std::move(base64_decode(command[2])));
			break;
		}
		case Command::addTemplate: {
			addChildNode(client, source, channelId, txid, tid, std::string(TEMPLATE_TREE), std::move(base64_decode(command[1])));
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
		std::string s;
		for (const auto &piece : command) s += piece;
		logError("exception inside execute state change: %s", e.what());

	}
}

// Process all pending status updates
void processStateUpdates(const std::string key) {
	OrderedPriorityQueue* queue = stateChangeQueueMap.at(key);
	//OrderedPriorityQueue local;	
	ipfs::Client* client = getIPFSClient();
	std::chrono::time_point<std::chrono::steady_clock> idleTime = std::chrono::steady_clock::now();
	bool sleep = false;
	while (true) {
		CommandInfo top;
		if (sleep) {
			sleep = false;
			std::this_thread::sleep_for(std::chrono::nanoseconds(1));
		}
		{ // Critical section: Adding items to the queue happens in another thread so we could get a data race here.
			std::lock_guard<std::mutex> lock(stateUpdateMutex);
			if (queue->empty()) {
				// Time to flush out the queue as there is nothing in the queue to process	
				logDebug("Removing queue: %",key.c_str());
				
				// Check idle time if greater than or equals 2 then exit
				if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - idleTime).count() >= 2){
					stateChangeQueueMap.erase(key);
					delete client;
					break;
				}
				sleep = true;
				continue;
			}
			idleTime = std::chrono::steady_clock::now();
			// Initialize top here or we could get malformed data if another thread is still writing to top.
			top = queue->top();
		} 
		// Critical section
		FieldValueMap& theFields = top.getFields();
		try {
			executeStateChangeCommand(*client, std::move(top.getCommand()), theFields.at("ws"), theFields.at("k"), theFields.at("txid"), theFields.at("tid"));
		}
		catch (std::out_of_range e) {

			logError("exception while firing execute state change command: %s",e.what());
		}
		std::lock_guard<std::mutex> lock(stateUpdateMutex);
		queue->pop();
		std::this_thread::yield();
	}
}

// Processes the blocks received by master node and commits to the IPFS
void processBlocks(PriorityPairQueue* queue) {
	ipfs::Client* client = getIPFSClient();
	while (true) {
		if (!queue->empty()) {
			std::lock_guard<std::mutex> lock(blockCommitMutex);
			std::pair<DoubleLong, std::string> block = queue->top();
			addTransactions(*client, block.second);
			// clear the block from queue			
			queue->pop();
		}
		std::this_thread::sleep_for(std::chrono::nanoseconds(60));
	}

	delete client;
}

// Send Connectionn Request details to masternodes and storagenodes in the network
void sendConnectionRequest(int& port, std::string& ip) {
	FieldValueMap theFields;
	char cmdstr[5];
	sprintf(cmdstr, "%d", static_cast<uint8_t>(BlockChainNode::NodeType::storageNode));
	theFields.insert(FieldValueMap::value_type(std::string("t"), std::string(cmdstr)));
	// uri of the storage node
	/*std::string ip(getPublicIP());
	boost::trim(ip)*/;
	theFields.insert(FieldValueMap::value_type(std::string("u"), std::string("ws://" + ip + std::string(":") + std::to_string(port))));
	gossip("masternodes", CommandType::connectionRequest, theFields);
	gossip("storagenodes", CommandType::connectionRequest, theFields);
}

// Returns new IPFS connection object
ipfs::Client* getIPFSClient() {
	ipfs::Client* client = new ipfs::Client(ipfsAddress->first, ipfsAddress->second);
	return std::move(client);
}

void registerShutdownHandler() {
	std::atexit([]() {
		logInfo("Shutting down Ipfs");
		_pclose(ipfsService);
	});
}

int main(int argc, char *argv[]) {
	try {
		logInfo("Initializing keypair...");
		nodeIdentity = initializeKeypair();
		std::string test_wsIP;
		std::vector<std::string> ipfsBootstrapNodes;
		command_params params;
		opt::options_description optionsList("Storage Node command options");
		optionsList.add_options()
			("i", opt::value<std::string>()->notifier([](std::string address) {
			std::pair<std::string, std::string> _address = dht::splitPort(boost::any_cast<std::string>(address));
			ipfsAddress->first = _address.first;
			ipfsAddress->first = _address.second;
			if (not _address.first.empty() and _address.second.empty()) {
				ipfsAddress->second = IPFS_DEFAULT_PORT;
			}
		}), "IPFS Address")
				("ib", opt::value<std::vector<std::string>>(&ipfsBootstrapNodes)->multitoken())
				("wi", opt::value<std::string>(&test_wsIP), "Websocket IP");
		parseArgs(argc, argv, &params, optionsList);
		if (ipfsAddress == nullptr || ipfsAddress->first.empty())
			ipfsAddress = new std::pair<std::string, long>(std::string(IPFS_DEFAULT_IP), (long)IPFS_DEFAULT_PORT);
		dhtNode.run(params.port, *nodeIdentity, true, NETWORK_ID);
		if (not params.bootstrap.first.empty()) {
			dhtNode.bootstrap(params.bootstrap.first.c_str(), params.bootstrap.second.c_str());
		}
		//Subscribing to common storage node channel
		subscribeTo("storagenodes");
		// Listening on own channel
		subscribeTo(dhtNode.getId().toString());				
		//dht::log::enableFileLogging(*dhtNode, "dht.log");	
		//dht::log::enableLogging(*dhtNode);
		COMM_PROTOCOL_INIT;		
		logInfo("id: %s", dhtNode.getId().toString().c_str());
		boost::algorithm::trim(test_wsIP);
		//send connection request to masternodes and storageNodes
		sendConnectionRequest(params.wsPort, test_wsIP);
		registerDataHandlers({ { CommandType::stateChange, [](FieldValueMap&& theFields) {
				std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
					onStateChange(std::move(theFields));
				}},{CommandType::fetchFunction, [](FieldValueMap&& theFields) {
					logDebug("Received function fetch request");
					FieldValueMap::iterator id = theFields.find("id");
					FieldValueMap::iterator h = theFields.find("h");
					fetchFunction(std::move(theFields.at("ws")),std::move(theFields.at("k")), std::move(theFields.at("txid")), std::move(theFields.at("tid")), std::move((id != theFields.end()) ? id->second : ""), std::move((h != theFields.end()) ? h->second : ""));
				}},{CommandType::fetchTemplate, [](FieldValueMap&& theFields) {
					fetchTemplate(std::move(theFields.at("ws")),std::move(theFields.at("k")),std::move(theFields.at("txid")), std::move(theFields.at("tid")), std::move(theFields.at("id")));
				}},{CommandType::fetchDocument, [](FieldValueMap&& theFields) {
					findDocument(std::move(theFields.at("ws")),std::move(theFields.at("k")),std::move(theFields.at("txid")), std::move(theFields.at("tid")),std::move(theFields.at("id")), std::move(theFields.at("expression")));
				}},{CommandType::closeBlock, [](FieldValueMap&& theFields) {
					// Close the block	
					try {
						std::string decoded((base64_decode(theFields.at("b"))));
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
						blocksToCommit.push(std::make_pair(std::move(blocknumber), _block.dump()));
					}
					catch (exception e) {
						logError("Exception while adding block : %s", theFields["b"].c_str());
					}
					}},{CommandType::verifyBlock, [](FieldValueMap&& theFields) {
						// verify the block				
						json block = json::parse(base64_decode(theFields.at("d")));
						std::string blockId = theFields.at("b");
						std::string hash = dhtGet(std::string(BLOCK_TREE) + std::string(CHARACTER_DELIMITER) + blockId);
						ipfs::Client* client = getIPFSClient();
						std::string obj = fetchObject(*client,hash);
						json object_json = json::parse(obj);
						object_json["v"].push_back(block);
						std::string objectHash = addObject(*client,object_json.dump());
						// update DHT with new block hash
						dhtPut(std::string(BLOCK_TREE) + std::string(CHARACTER_DELIMITER) + blockId, objectHash, true);
						std::string rootHash = dhtGet(std::string(ROOT_IDENTIFIER) + std::string(BLOCK_TREE));
						linkToRoot(*client, std::string(BLOCK_TREE), blockId, objectHash, "");
						delete client;
					}} });
					logInfo("Running block processor");
					std::future<void> result(std::async(processBlocks, &blocksToCommit));
					// Cleans existing ipfs bootstrap nodes
					//ipfs::Json ack;
					//ipfsClient->ClearBootstrapNodes(&ack);
					logDebug("Cleaned up all existing ipfs bootstrap nodes");
					logInfo("Adding Ipfs bootstrap nodes if provided any...");
					for (std::string node: ipfsBootstrapNodes) {
						string output(exec(("ipfs bootstrap add "+node).c_str(), NULL));
						logInfo(output);
					}					
					logInfo("Running ipfs if not already started");
					runIpfsDaemon();
					registerShutdownHandler();					
					ipfsClient = new ipfs::Client(ipfsAddress->first, ipfsAddress->second);
					logInfo("Successfully connected to ipfs");
					dhtNode.setOnStatusChanged([](dht::NodeStatus status, dht::NodeStatus status2) {
						auto nodeStatus = std::max(status, status2);
						if (nodeStatus == dht::NodeStatus::Connected)
							logTrace("Connected");
						else if (nodeStatus == dht::NodeStatus::Disconnected)
							logTrace("Disconnected");
						else
							logTrace("Connecting");
					});
					
					logInfo("Node is up and running on port %d ! Press CTRL+C to quit.", params.port);
					wsServer.run(params.wsPort);
				}
				catch (const std::exception e) {
					logError("In main: %s", e.what());
					return 1;
				}
				return 0;
}


