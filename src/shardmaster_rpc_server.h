#pragma once

#include <thread>
#include <memory>

#include <grpcpp/grpcpp.h>
#include "grpc/shardmaster_messages.grpc.pb.h"
#include "grpc/shardmaster_messages.pb.h"
#include <memory>
#include <thread>

#include "shardmaster_server.h"
//#include "node.h"

using grpc::Server;
using grpc::Channel;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;
using grpc::CompletionQueue;

struct ShardmasterMessagesServiceImpl : public shardmaster_messages::ShardmasterMessages::Service {
    ShardmasterServer * shardmasterServer_ = nullptr;

    ShardmasterMessagesServiceImpl(struct ShardmasterServer * shardmasterServer) : shardmasterServer_(shardmasterServer) {

    }
    ~ShardmasterMessagesServiceImpl(){
        shardmasterServer_ = nullptr;
    }
    
    Status Join(ServerContext* context,const shardmaster_messages::JoinRequest* request,
                        shardmaster_messages::JoinResponse* response) override;
	Status Leave(ServerContext* context,const shardmaster_messages::LeaveRequest* request,
						shardmaster_messages::LeaveResponse* response) override;
	Status Move(ServerContext* context,const shardmaster_messages::MoveRequest* request,
                        shardmaster_messages::MoveResponse* response) override;
	Status Query(ServerContext* context,const shardmaster_messages::QueryRequest* request,
                        shardmaster_messages::QueryResponse* response) override;
};


Status ShardmasterMessagesServiceImpl::Join(ServerContext* context,const shardmaster_messages::JoinRequest* request,
					shardmaster_messages::JoinResponse* response)
{
    #if !defined(_HIDE_HEARTBEAT_NOTICE) && !defined(_HIDE_GRPC_NOTICE)
    debug("GRPC: Receive HandleClient from Peer %s\n", context->peer().c_str());
    #endif
    if(!shardmasterServer_){
        return Status::OK;
    }
    int response0 = shardmasterServer_->onJoin(response,*request);
    if(response0 == 0)
        return Status::OK;
    else
        return Status(grpc::StatusCode::UNAVAILABLE,"Peer is not ready for this request.");
}

Status ShardmasterMessagesServiceImpl::Leave(ServerContext* context,const shardmaster_messages::LeaveRequest* request,
					shardmaster_messages::LeaveResponse* response)
{
        #if !defined(_HIDE_HEARTBEAT_NOTICE) && !defined(_HIDE_GRPC_NOTICE)
    debug("GRPC: Receive HandleClient from Peer %s\n", context->peer().c_str());
    #endif
    if(!shardmasterServer_){
        return Status::OK;
    }
    int response0 = shardmasterServer_->onLeave(response,*request);
    if(response0 == 0)
        return Status::OK;
    else
        return Status(grpc::StatusCode::UNAVAILABLE,"Peer is not ready for this request.");
}

Status ShardmasterMessagesServiceImpl::Move(ServerContext* context,const shardmaster_messages::MoveRequest* request,
					shardmaster_messages::MoveResponse* response) 
{
    #if !defined(_HIDE_HEARTBEAT_NOTICE) && !defined(_HIDE_GRPC_NOTICE)
    debug("GRPC: Receive HandleClient from Peer %s\n", context->peer().c_str());
    #endif
    if(!shardmasterServer_){
        return Status::OK;
    }
    int response0 = shardmasterServer_->onMove(response,*request);
    if(response0 == 0)
        return Status::OK;
    else
        return Status(grpc::StatusCode::UNAVAILABLE,"Peer is not ready for this request.");
}

Status ShardmasterMessagesServiceImpl::Query(ServerContext* context,const shardmaster_messages::QueryRequest* request,
					shardmaster_messages::QueryResponse* response) 
{
    #if !defined(_HIDE_HEARTBEAT_NOTICE) && !defined(_HIDE_GRPC_NOTICE)
    debug("GRPC: Receive HandleClient from Peer %s\n", context->peer().c_str());
    #endif
    if(!shardmasterServer_){
        return Status::OK;
    }
    int response0 = shardmasterServer_->onQuery(response,*request);
    if(response0 == 0)
        return Status::OK;
    else
        return Status(grpc::StatusCode::UNAVAILABLE,"Peer is not ready for this request.");
}

struct ShardmasterRpcServer{
    ShardmasterRpcServer(ShardmasterServer * shardmasterServer,const std::string& addr);
    ~ShardmasterRpcServer();
    ShardmasterMessagesServiceImpl * service;
    std::unique_ptr<Server> server;
    ServerBuilder * builder;
    std::thread wait_thread;
	std::string listenAddr;
};


ShardmasterRpcServer::ShardmasterRpcServer(ShardmasterServer * shardmasterServer,const std::string& addr):listenAddr(addr)
{
    service = new ShardmasterMessagesServiceImpl(shardmasterServer);
#if !defined(_HIDE_GRPC_NOTICE)
    debug("GRPC: Listen to %s\n", shardmasterServer->node->name.c_str());
#endif
    builder = new ServerBuilder();
    builder->AddListeningPort(listenAddr, grpc::InsecureServerCredentials());
    builder->RegisterService(service);
    server = std::unique_ptr<Server>{builder->BuildAndStart()};
}

ShardmasterRpcServer::~ShardmasterRpcServer()
{}
