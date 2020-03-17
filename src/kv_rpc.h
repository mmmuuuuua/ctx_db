#pragma once

#include <grpcpp/grpcpp.h>
#include "grpc/client_messages.grpc.pb.h"
#include "grpc/client_messages.pb.h"
#include <memory>
#include <thread>

#include "kv_server.h"
#include "node.h"

using grpc::Server;
using grpc::Channel;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ClientContext;
using grpc::Status;
using grpc::ClientAsyncResponseReader;
using grpc::CompletionQueue;

struct ClientMessagesServiceImpl : public client_messages::ClientMessages::Service {
    KvServer * kv_server = nullptr;

    ClientMessagesServiceImpl(struct KvServer * _kv_server) : kv_server(_kv_server) {

    }
    ~ClientMessagesServiceImpl(){
        kv_server = nullptr;
    }
    
    Status HandleClient(ServerContext* context,const client_messages::HandleClientRequest* request,
                        client_messages::HandleClientResponse* respanse) override;
};

Status ClientMessagesServiceImpl::HandleClient(ServerContext* context,
        const client_messages::HandleClientRequest* request,
        client_messages::HandleClientResponse* response) 
{
    #if !defined(_HIDE_HEARTBEAT_NOTICE) && !defined(_HIDE_GRPC_NOTICE)
    debug("GRPC: Receive HandleClient from Peer %s\n", context->peer().c_str());
    #endif
    if(!kv_server){
        return Status::OK;
    }
    int response0 = kv_server->on_handle_client_request(response,*request);
    if(response0 == 0)
        return Status::OK;
    else
        return Status(grpc::StatusCode::UNAVAILABLE,"Peer is not ready for this request.");
}

struct ClientServerContext{
    ClientServerContext(KvServer * node);
    ~ClientServerContext();
    ClientMessagesServiceImpl * service;
    std::unique_ptr<Server> server;
    ServerBuilder * builder;
    std::thread wait_thread;
};


ClientServerContext::ClientServerContext(KvServer * kvServer){
    service = new ClientMessagesServiceImpl(kvServer);
#if !defined(_HIDE_GRPC_NOTICE)
    debug("GRPC: Listen to %s\n", kvServer->node->name.c_str());
#endif
    builder = new ServerBuilder();

    int length = (kvServer->node->name).size();
    std::string listenAddr = kvServer->node->name;
    listenAddr[length-3]='8';

    builder->AddListeningPort(listenAddr, grpc::InsecureServerCredentials());
    builder->RegisterService(service);
    server = std::unique_ptr<Server>{builder->BuildAndStart()};
}

ClientServerContext::~ClientServerContext()
{}
