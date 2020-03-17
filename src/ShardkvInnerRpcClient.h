#pragma once

#include <map>
#include <vector>
#include <string>
#include <memory>


#include <grpcpp/grpcpp.h>
#include "grpc/shardkvinner_messages.grpc.pb.h"
#include "grpc/shardkvinner_messages.pb.h"

struct ShardkvInnerRpcClient : std::enable_shared_from_this<ShardkvInnerRpcClient>{
    using ShardMigrationRequest = ::shardkvinner_messages::ShardMigrationRequest;
    using ShardMigrationResponse = ::shardkvinner_messages::ShardMigrationResponse;
	using ShardCleanupRequest = ::shardkvinner_messages::ShardCleanupRequest;
    using ShardCleanupResponse = ::shardkvinner_messages::ShardCleanupResponse;

    //Nuke::ThreadExecutor * task_queue = nullptr;
    std::string addr_;

	bool ShardMigration(const ShardMigrationRequest& request,ShardMigrationResponse& response);
	bool ShardCleanup(const ShardCleanupRequest& request,ShardCleanupResponse& response);

    ShardkvInnerRpcClient(const char * addr);
    ShardkvInnerRpcClient(const std::string & addr);
    void shutdown(){}
    bool is_shutdown(){return true;}

    ~ShardkvInnerRpcClient() {
        //raft_node = nullptr;
    }
private:
    std::unique_ptr<shardkvinner_messages::ShardkvinnerMessages::Stub> stub;
};












