#include "ShardkvInnerRpcClient.h"

bool ShardkvInnerRpcClient::ShardMigration(const ShardMigrationRequest& request,ShardMigrationResponse& response)
{
	grpc::ClientContext context;
    grpc::Status status = stub->ShardMigration(&context, request, &response);
    if(status.ok())
        return true;
    else
        return false;
}

bool ShardkvInnerRpcClient::ShardCleanup(const ShardCleanupRequest& request,ShardCleanupResponse& response)
{
	grpc::ClientContext context;
    grpc::Status status = stub->ShardCleanup(&context, request, &response);
    if(status.ok())
        return true;
    else
        return false;
}

ShardkvInnerRpcClient::ShardkvInnerRpcClient(const char * addr) : addr_(addr) {
    std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(addr_, grpc::InsecureChannelCredentials());
    stub = shardkvinner_messages::ShardkvinnerMessages::NewStub(channel);
}

ShardkvInnerRpcClient::ShardkvInnerRpcClient(const std::string & addr) : ShardkvInnerRpcClient(addr.c_str()) {

}