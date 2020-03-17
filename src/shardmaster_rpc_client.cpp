#include "shardmaster_rpc_client.h"

bool ShardmasterRpcClient::Join(const JoinRequest& request,JoinResponse& response)
{
	grpc::ClientContext context;
    grpc::Status status = stub->Join(&context, request, &response);
    if(status.ok())
        return true;
    else
        return false;
}
bool ShardmasterRpcClient::Leave(const LeaveRequest& request,LeaveResponse& response)
{
	grpc::ClientContext context;
    grpc::Status status = stub->Leave(&context, request, &response);
    if(status.ok())
        return true;
    else
        return false;
}
bool ShardmasterRpcClient::Move(const MoveRequest& request,MoveResponse& response)
{
	grpc::ClientContext context;
    grpc::Status status = stub->Move(&context, request, &response);
    if(status.ok())
        return true;
    else
        return false;
}
bool ShardmasterRpcClient::Query(const QueryRequest& request,QueryResponse& response)
{
	grpc::ClientContext context;
    grpc::Status status = stub->Query(&context, request, &response);
    if(status.ok())
        return true;
    else
        return false;
}



ShardmasterRpcClient::ShardmasterRpcClient(const char * addr) : addr_(addr) {
    std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(addr_, grpc::InsecureChannelCredentials());
    stub = shardmaster_messages::ShardmasterMessages::NewStub(channel);
}

ShardmasterRpcClient::ShardmasterRpcClient(const std::string & addr) : ShardmasterRpcClient(addr.c_str()) {

}