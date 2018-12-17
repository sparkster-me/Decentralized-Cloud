# sparkster-block-chain

[Sparkster logo](images/sparkster.png)

[![standard-readme compliant](https://img.shields.io/badge/standard--readme-OK-green.svg?style=flat-square)](https://github.com/RichardLitt/standard-readme)

> The set of nodes that make up the Sparkster block chain.

This repository contains three nodes that are discussed on this page. The nodes have been tested on Windows x64 platforms, and the Visual Studio solution is configured to build for an x64 architecture. The nodes are not guaranteed to run on 32-bit architectures.

**Note**: The current release is still in functional and integration testing and is not considered a stable release. No warranties provided.

## Table of Contents

- [Background](#background)
    - [Clients](#clients)
    - [Signatures](#signatures)
    - [Master Node](#master-node)
        - [Block Producer](#block-producer)
    - [Compute & Verifier Node](#compute-verifier-node)
    - [Storage Node](#storage-node)
- [Status](#status)
- [Install](#install)
- [Usage](#usage)
- [Future Work](#future-work)
- [Known Issues](#known-issues)
- [Outstanding Tasks](#outstanding-tasks)
- [Future Research](#future-research)
- [Maintainers](#maintainers)
- [Contributing](#contributing)
- [License](#license)

## Background

Our block chain consists of four nodes. These are the Master Node, Compute & Verifier Node, Storage Node and Boot Node.

Clients (entities with public and private keypairs) execute transactions on the block chain.

At any given point in time, 21 Master Nodes will exist that facilitate consensus on transactions and blocks; we will call these Master Nodes `m21`. The nodes in `m21` are selected every hour through an automated voting process (see [the section on Block Producers](#block-producer) for detailed information.)

We follow the Byzantine Fault Tolerance methodology for consensus. For a transaction or block to reach consensus, two thirds of `m21` must agree on the validity of the transaction or correctness of the block. How this information is verified is discussed further in this document.

We will refer to individual nodes in `m21` as `mN`.

### Clients

A single client is represented by a public and private keypair, generated using the [Ed25519 key scheme](https://ed25519.cr.yp.to/). A client's account ID is the base-64 encoded public key.

### Signatures

In order to keep the client's account ID short and facilitate fast key generation and signing, we've opted to use the Ed25519 key scheme for clients.

We use [OpenDHT](https://github.com/savoirfairelinux/opendht) for node to node communication, so when a node wants to verify another node's signature, we use the [generic X509 key scheme](https://en.wikipedia.org/wiki/X.509) whose generation is facilitated by OpenDHT. The public and private keypair of a node is generated once and then saved to a file in the working directory of the node. When the node next boots up, it will use that public and private keypair unless the files in question are deleted.

### Master Node

The Master Nodes are responsible for handling incoming requests from the client. One client will hit one Master Node, called `mi`, which exists in `m21`. When a Master Node receives a request from a client such as a function to execute on the block chain, it will first verify the signature of the client. Next, it will create a transaction ID to keep track of the request. This transaction ID is embedded into a Transaction object that represents the request. We'll call the transaction `tx`. The flow then proceeds as follows.
- First, `mi` will select a random verifier node and send the transaction to the verifier. We will call this node `c`, for "Compute Node."
- Next, `mi` will send `tx` to `m21`.
- Each `mN` in `m21` will select a random verifier node, `vN` where `N` is a number from 0 to 19.
- Each `mN` will independently verify the validity of `tx` (see [the section on Verifier Nodes](#verifier-node) for details.)
- Once `mN` verifies a transaction, it will add it to its block ledger. If `mN` has contact with `c`, it will tell `c` to commit the state changes it collected from executing the code contained in `tx`.
- If `mN` is a [Block Producer](#block-producer), it will send the block to a storage node to be committed to the block chain once the time arrives to close a block.
    - The Block Producer will also send a notification to `m21` that it has committed a block.
    - Each `mN` will verify the block by fetching the Block Producer's signature and public key from the Storage Node, verifying that the retrieved public key is supposed to be the Block Producer at this point in time, computing its own hash against the block, and finally making sure signature verification succeeds. This will be done independently, on each `mN` in `m21`.
    - If verification passes, `mN` will notify the Storage Node that it has validated the block in question.

The Master Node is made up of two projects in the repository. The first of these is the `MasterNode` projects that builds the `MasterNode.exe` executable, and the second one is the `MasterNodeLib` project that builds `MasterNodeLib.lib`. The tests for the Master Node and the executable project statically link against `MasterNodeLib.lib`.

**Note**: In the current release, we have disallowed the ability to create transactions and execute code on the network. The Master Nodes will allow you to send code to the block chain once we complete testing this part of the network.

#### Block Producer

A Master Node is able to become a Block Producer by participating in a voting process that selects 21 Master Nodes. This process is executed at a fixed interval inside an hour and is known as open voting. The interval starts at timestamp `tA` and extends through the remainder of the hour, `tB`; therefore, `tB` is the zeroth second of the current hour into which the interval extends. Once `[tA, tB]` elapses, a new set of 21 Master Nodes become the Master Nodes for the next hour. Voting proceeds as follows.
- When voting opens, a Master Node will sign `tA`. Because the interval always starts at a fixed point, every Master Node is able to predict the next timestamp when voting will begin by using the formula `n + u - 60(60 - m)` where `n` is the current timestamp, `u` is the number of seconds until the next hour, and `m` is the number of minutes past the hour that represents `tA`. From this, it follows that every Master Node that casts a vote will sign the same `tA`, provided that the Master Node is being thruthful.
- The Master Node will notify all other Master Nodes of its signature. In this manner, all Master Nodes will receive all signatures from all other Master Nodes.
- The receiving Master Node will first take `tA`, the sending Master Node's signature and the sending Master Node's public key. Given this information, it will verify the sending node's signature and make sure that the sending node signed the agreed upon timestamp.
- Next, the receiving Master Node will compute a number based on the sending node's signature. This number is the sum of the ASCII values of the characters in the sending nodes signature.
- Once `tB` elapses, the Master Node will sort its list of voters based on the computed numbers described earlier.
- The 21 lowest numbers will be the Master Nodes for the hour containing `tB`.

Once the 21 Master Nodes are selected, a new Block Producer is selected every second in a round-robin fashion.

When a Block Producer notifies `m21` that it has closed a block, each `mN` in `m21`, as part of its verification check, will determine if the Block Producer is even allowed to close the blocks. Since every Master Node has independently determined the list of 21 Master Nodes, every Master Node is able to tell straight away who should close the block in a given round by using the formula `(tC - tB) mod 21`, where `tB` is the end of the voting interval and `tC` is the timestamp at which the block was closed. If the public key of the node returned by this formula matches the public key of the node that closed the block, the node is allowed to close the block.

### Compute & Verifier Node

A Compute & Verifier Node is responsible for running code and committing transactions.
- When a Compute & Verifier Node receives a request to execute a function, it will fetch the function from a Storage Node.
- It will execute the function using the given input parameters.
- It will hash the function's output.
- It will notify `m21` of the function's hash.
- If the Verifier Node is a Compute Node, it will additionally perform these steps:
    - Along with the function's hash, it will also send the function's output. The Master Nodes will include this output in `tx`.
    - It will make a note of state changes that are performed by the function. These state changes will be applied once `tx` is verified.

The Compute & Verifier Node is made up of two projects in the repository. The first of these is the `ComputeNode` projects that builds the `ComputeNode.exe` executable, and the second one is the `ComputeNodeLib` project that builds `ComputeNodeLib.lib`. The tests for the Compute & Verifier Node and the executable project statically link against `ComputeNodeLib.lib`.

### Storage Node

A Storage Node is responsible for archiving data and executing state changes against the datastore. In addition, it is responsible for retrieving data and delivering it back to the client. We use IPFS as our distributed graph of data, so every Storage Node runs along side an IPFS node.

- For a create operation, the Storage Node is able to store document templates (entities,) and actual documents. For a template definition, it will directly commit it to IPFS. For a document derived from a template, the storage node is responsible for not only storing the document but also for creating indices in such a manner where retrieval and future updates are fast.
- In cases where the client passes a filter expression to the Storage Node such as in the case of document updates and retrievals, the Storage Node will expect the filter expression in standard C-style form. It will then perform the following steps.
    - It will convert the expression from infixed to postfixed notation.
    - It will create an expression tree from the resulting postfixed expression.
    - It will evaluate the expression against the data DAG for a given tenant ID by using the standard binary search to look up indices in the graph.
    - It will eventually arrive at the document to retrieve or update.
    - If it has to update a document, it will update the indices during the update of the document so that the data is always current.

## Status

We are conducting functional and integration testing; this code is not considered stable. Please see [the section on known issues](#known-issues) for issues that will be addressed in future updates.

## Install

The block chain is written in C++. You will need a compiler that supports C++ 2017 or later.
1. Clone this repository or download the zip file.
2. Open `BlockChainComponents.sln` in Visual Studio and build the solution. We're using Visual Studio 2017 for this project.
3. You can choose to build the full solution or individual nodes. Visual Studio will automatically download any NuGet packages it needs. For the cases where NuGet is not an option such as with `OpenDHT,` the solution already points to it and it will be linked to the binaries.
4. The individual nodes are placed in `BlockChainComponents/x64`.

## Usage

```
node.exe --help
Usage: node.exe [--p port] [--ws port] [--b bootstrap_host[:port]] [--createaccount]

--w: Set the port that this node's web socket server listens on. If this node has no web socket server, this option will be ignored.
--p: Set the port that this node's OpenDHT instance listens on. This is the port that other nodes will use to connect to this node.
--b: Set a bootstrap node. The port given here should be the port supplied with --p.
--createaccount: Generate an account ID and associated private key. This command will show the account seed, account ID and private key. The private key and seed should be kept in a secure location and can be used to recover the account ID.
```

## Future Work

Items not included in this release, but will be included in future releases:
- Linux Build
- Token Integration
- Node selection and associated mathematics based on tokens staked
- Staking contract
- Multiple storage nodes
- Storage node selection using consistent hashing

## Known Issues

- Nodes sometimes disconnect from their peers and messages are lost. We are working with the developers of OpenDHT to resolve this issue.

## Outstanding Tasks

This section lists features that didn't make into this release but are slated to be implemented in a future update.
- User-friendly exception messages. Right now, messages are too developer-centric.
- Right now, the only way to change the node's options are through the command line; options are neither saved to a file, nor is there a way to read options from a configuration file.

## Future Research

Areas that we've identified as areas for further research:
- Consensus algorithms including Paxos and Flexible Paxos (Heidi)
- LSH vs CS
- TOR Relays for connectivity
- Vulnerability minimization: Spectre, Meltdown and Side Channel attacks among others.
- Network optimization algorithms reducing latency, replication and overhead.
- Delayed consistency. 

## Maintainers

- [Sethuraman Babu](https://github.com/Sethu226)
- [Munawar Bijani](https://github.com/munawarb)
- [Mithun Debnath](https://github.com/mithundebnath01)
- [Monika Krishnappa](https://github.com/monika-123)
- [Prasad Nadendla](https://github.com/nadendlaprasad)
- [Praveen Namoju](https://github.com/praveen-namoju)
- [Nagendra Obbu](https://github.com/nagendraobbu)
- [Swarup Roul](https://github.com/swarup-roul)
- [Baggiaraj Sevinthilingam](https://github.com/baggiaraj)
- [Akshay Tiwari](https://github.com/akshay-tiwari-git)
- [Siddharth Ubale ](https://github.com/siddharth-ub)

## Contributing

See [the contributing file](contributing.md)!

PRs accepted.

Small note: If editing the README, please conform to the [standard-readme](https://github.com/RichardLitt/standard-readme) specification.

## License

AGPL 3.0 © 2018 Sparkster
