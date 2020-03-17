#pragma once

#include <grpcpp/grpcpp.h>
#include "grpc/shardkvinner_messages.grpc.pb.h"
#include "grpc/shardkvinner_messages.pb.h"
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

struct ShardkvinnerMessagesServiceImpl : public shardkvinner_messages::ShardkvinnerMessages::Service {
    ShardkvServer * shardkvServer_ = nullptr;

    ShardkvinnerMessagesServiceImpl(struct ShardkvServer * shardkvServer) : shardkvServer_(shardkvServer) {

    }
    ~ShardkvinnerMessagesServiceImpl(){
        shardkvServer_ = nullptr;
    }
    
    Status ShardMigration(ServerContext* context,const shardkvinner_messages::ShardMigrationRequest* request,
                        shardkvinner_messages::ShardMigrationResponse* response) override;
	Status ShardCleanup(ServerContext* context,const shardkvinner_messages::ShardCleanupRequest* request,
						shardkvinner_messages::ShardCleanupResponse* response) override;
};


Status ShardkvinnerMessagesServiceImpl::ShardMigration(ServerContext* context,const shardkvinner_messages::ShardMigrationRequest* request,
					shardkvinner_messages::ShardMigrationResponse* response) 
{
	 #if !defined(_HIDE_HEARTBEAT_NOTICE) && !defined(_HIDE_GRPC_NOTICE)
    debug("GRPC: Receive HandleClient from Peer %s\n", context->peer().c_str());
    #endif
    if(!shardkvServer_){
        return Status::OK;
    }
    int response0 = shardkvServer_->onShardMigration(response,*request);
    if(response0 == 0)
        return Status::OK;
    else
        return Status(grpc::StatusCode::UNAVAILABLE,"Peer is not ready for this request.");
}

Status ShardkvinnerMessagesServiceImpl::ShardCleanup(ServerContext* context,const shardkvinner_messages::ShardCleanupRequest* request,
					shardkvinner_messages::ShardCleanupResponse* response) 
{
	#if !defined(_HIDE_HEARTBEAT_NOTICE) && !defined(_HIDE_GRPC_NOTICE)
    debug("GRPC: Receive HandleClient from Peer %s\n", context->peer().c_str());
    #endif
    if(!shardkvServer_){
        return Status::OK;
    }
    int response0 = shardkvServer_->onShardCleanup(response,*request);
    if(response0 == 0)
        return Status::OK;
    else
        return Status(grpc::StatusCode::UNAVAILABLE,"Peer is not ready for this request.");
}

struct ShardkvInnerRpcServer{
    ShardkvInnerRpcServer(ShardkvServer * shardkvServer,const std::string& addr);
    ~ShardkvInnerRpcServer();
    ShardkvinnerMessagesServiceImpl * service;
    std::unique_ptr<Server> server;
    ServerBuilder * builder;
    std::thread wait_thread;
	std::string listenAddr;
};


ShardkvInnerRpcServer::ShardkvInnerRpcServer(ShardkvServer * shardkvServer,const std::string& addr):listenAddr(addr)
{
    service = new ShardkvinnerMessagesServiceImpl(shardkvServer);
#if !defined(_HIDE_GRPC_NOTICE)
    debug("GRPC: Listen to %s\n", shardkvServer->node->name.c_str());
#endif
    builder = new ServerBuilder();
    builder->AddListeningPort(listenAddr, grpc::InsecureServerCredentials());
    builder->RegisterService(service);
    server = std::unique_ptr<Server>{builder->BuildAndStart()};
}

ShardkvInnerRpcServer::~ShardkvInnerRpcServer()
{}
