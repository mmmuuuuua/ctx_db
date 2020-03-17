#pragma once

#include <map>
#include <vector>
#include <string>
#include <memory>

#include <grpcpp/grpcpp.h>
#include "grpc/shardkv_messages.grpc.pb.h"
#include "grpc/shardkv_messages.pb.h"

struct ShardkvRpcClient : std::enable_shared_from_this<ShardkvRpcClient>{
    using GetRequest = ::shardkv_messages::GetRequest;
    using GetResponse = ::shardkv_messages::GetResponse;
	
	using PutAppendRequest = ::shardkv_messages::PutAppendRequest;
    using PutAppendResponse = ::shardkv_messages::PutAppendResponse;
	
    //Nuke::ThreadExecutor * task_queue = nullptr;
    std::string addr_;

	bool Get(const GetRequest& request,GetResponse& response);
	bool PutAppend(const PutAppendRequest& request,PutAppendResponse& response);

    ShardkvRpcClient(const char * addr);
    ShardkvRpcClient(const std::string & addr);
    void shutdown(){}
    bool is_shutdown(){return true;}

    ~ShardkvRpcClient() {
        //raft_node = nullptr;
    }
private:
    std::unique_ptr<shardkv_messages::ShardkvMessages::Stub> stub;
};

bool ShardkvRpcClient::Get(const GetRequest& request,GetResponse& response)
{
	ClientContext context;
    Status status = stub->Get(&context, request, &response);
    if(status.ok())
        return true;
    else
        return false;
}
bool ShardkvRpcClient::PutAppend(const PutAppendRequest& request,PutAppendResponse& response)
{
	ClientContext context;
    Status status = stub->PutAppend(&context, request, &response);
    if(status.ok())
        return true;
    else
        return false;
}


ShardkvRpcClient::ShardkvRpcClient(const char * addr) : addr_(addr) {
    std::shared_ptr<Channel> channel = grpc::CreateChannel(addr_, grpc::InsecureChannelCredentials());
    stub = shardkv_messages::ShardkvMessages::NewStub(channel);
}

ShardkvRpcClient::ShardkvRpcClient(const std::string & addr) : ShardkvRpcClient(addr.c_str()) {

}














