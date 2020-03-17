#pragma once

#include <map>
#include <vector>
#include <string>
#include <memory>

#include <grpcpp/grpcpp.h>
#include "grpc/shardmaster_messages.grpc.pb.h"
#include "grpc/shardmaster_messages.pb.h"

struct ShardmasterRpcClient : std::enable_shared_from_this<ShardmasterRpcClient>{
    using JoinRequest = ::shardmaster_messages::JoinRequest;
    using JoinResponse = ::shardmaster_messages::JoinResponse;
	
	using LeaveRequest = ::shardmaster_messages::LeaveRequest;
    using LeaveResponse = ::shardmaster_messages::LeaveResponse;
	
	using MoveRequest = ::shardmaster_messages::MoveRequest;
    using MoveResponse = ::shardmaster_messages::MoveResponse;
	
	using QueryRequest = ::shardmaster_messages::QueryRequest;
    using QueryResponse = ::shardmaster_messages::QueryResponse;

    //Nuke::ThreadExecutor * task_queue = nullptr;
    std::string addr_;

	bool Join(const JoinRequest& request,JoinResponse& response);
	bool Leave(const LeaveRequest& request,LeaveResponse& response);
	bool Move(const MoveRequest& request,MoveResponse& response);
	bool Query(const QueryRequest& request,QueryResponse& response);

    ShardmasterRpcClient(const char * addr);
    ShardmasterRpcClient(const std::string & addr);
    void shutdown(){}
    bool is_shutdown(){return true;}

    ~ShardmasterRpcClient() {
        //raft_node = nullptr;
    }
private:
    std::unique_ptr<shardmaster_messages::ShardmasterMessages::Stub> stub;
};

