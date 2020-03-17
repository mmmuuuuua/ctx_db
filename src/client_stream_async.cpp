#include "grpc_utils.h"
#include "node.h"
#include <iostream>

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;

using RequestVoteResponse = ::raft_messages::RequestVoteResponse;
using RequestVoteRequest = ::raft_messages::RequestVoteRequest;
using AppendEntriesRequest = ::raft_messages::AppendEntriesRequest;
using AppendEntriesResponse = ::raft_messages::AppendEntriesResponse;
using InstallSnapshotRequest = ::raft_messages::InstallSnapshotRequest;
using InstallSnapshotResponse = ::raft_messages::InstallSnapshotResponse;
