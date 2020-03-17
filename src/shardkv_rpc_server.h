#pragma once

#include <grpcpp/grpcpp.h>
#include "grpc/shardmaster_messages.grpc.pb.h"
#include "grpc/shardmaster_messages.pb.h"
#include <memory>
#include <thread>

#include "shardkv_server.h"

using grpc::Server;
using grpc::Channel;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;
using grpc::CompletionQueue;

struct ShardkvMessagesServiceImpl : public shardkv_messages::ShardkvMessages::Service {
    ShardkvServer * shardkvServer_ = nullptr;

    ShardkvMessagesServiceImpl(struct ShardkvServer * shardkvServer) : shardkvServer_(shardkvServer) {

    }
    ~ShardkvMessagesServiceImpl(){
        shardkvServer_ = nullptr;
    }
    
    Status Get(ServerContext* context,const shardkv_messages::GetRequest* request,
                        shardkv_messages::GetResponse* response) override;
	Status PutAppend(ServerContext* context,const shardkv_messages::PutAppendRequest* request,
						shardkv_messages::PutAppendResponse* response) override;
};


Status ShardkvMessagesServiceImpl::Get(ServerContext* context,const shardkv_messages::GetRequest* request,
					shardkv_messages::GetResponse* response) 
{
	#if !defined(_HIDE_HEARTBEAT_NOTICE) && !defined(_HIDE_GRPC_NOTICE)
    debug("GRPC: Receive HandleClient from Peer %s\n", context->peer().c_str());
    #endif
    if(!shardkvServer_){
        return Status::OK;
    }
    int response0 = shardkvServer_->onGet(response,*request);
    if(response0 == 0)
        return Status::OK;
    else
        return Status(grpc::StatusCode::UNAVAILABLE,"Peer is not ready for this request.");
}
Status ShardkvMessagesServiceImpl::PutAppend(ServerContext* context,const shardkv_messages::PutAppendRequest* request,
					shardkv_messages::PutAppendResponse* response) 
{
	#if !defined(_HIDE_HEARTBEAT_NOTICE) && !defined(_HIDE_GRPC_NOTICE)
    debug("GRPC: Receive HandleClient from Peer %s\n", context->peer().c_str());
    #endif
    if(!shardkvServer_){
        return Status::OK;
    }
    int response0 = shardkvServer_->onPutAppend(response,*request);
    if(response0 == 0)
        return Status::OK;
    else
        return Status(grpc::StatusCode::UNAVAILABLE,"Peer is not ready for this request.");
}

struct ShardkvRpcServer{
    ShardkvRpcServer(ShardkvServer * shardkvServer,const std::string& addr);
    ~ShardkvRpcServer();
    ShardkvMessagesServiceImpl * service;
    std::unique_ptr<Server> server;
    ServerBuilder * builder;
    std::thread wait_thread;
	std::string listenAddr;
};


ShardkvRpcServer::ShardkvRpcServer(ShardkvServer * shardkvServer,const std::string& addr):listenAddr(addr)
{
    service = new ShardkvMessagesServiceImpl(shardkvServer);
#if !defined(_HIDE_GRPC_NOTICE)
    debug("GRPC: Listen to %s\n", shardkvServer->node->name.c_str());
#endif
    builder = new ServerBuilder();
    builder->AddListeningPort(listenAddr, grpc::InsecureServerCredentials());
    builder->RegisterService(service);
    server = std::unique_ptr<Server>{builder->BuildAndStart()};
}

ShardkvRpcServer::~ShardkvRpcServer()
{}
