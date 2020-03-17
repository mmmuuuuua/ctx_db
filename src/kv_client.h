#pragma once

#include <map>
#include <vector>
#include <string>
#include <memory>

#include <grpcpp/grpcpp.h>
#include "grpc/raft_messages.grpc.pb.h"
#include "grpc/raft_messages.pb.h"
#include "grpc/client_messages.grpc.pb.h"
#include "grpc/client_messages.pb.h"

struct KvClientSync : std::enable_shared_from_this<KvClientSync>{
    using HandleClientRequest = ::client_messages::HandleClientRequest;
    using HandleClientResponse = ::client_messages::HandleClientResponse;

    Nuke::ThreadExecutor * task_queue = nullptr;
    std::string addr_;

    bool AsyncHandleClient(const HandleClientRequest& request,HandleClientResponse& response);
    bool SyncHandleClient(const HandleClientRequest& request,HandleClientResponse& response);

    KvClientSync(const char * addr);
    KvClientSync(const std::string & addr);
    void shutdown(){}
    bool is_shutdown(){return true;}

    ~KvClientSync() {
        //raft_node = nullptr;
    }
private:
    std::unique_ptr<client_messages::ClientMessages::Stub> stub;
};

bool KvClientSync::AsyncHandleClient(const HandleClientRequest& request,HandleClientResponse& response)
{
        // A copy of `request` is needed
    // TODO Replace `std::thread` implementation with future.then implementation. 
    std::string peer_name = this->addr_;
    auto strongThis = shared_from_this();
#if defined(USE_GRPC_SYNC_BARE)
    std::thread t = std::thread(
#else
    task_queue->add_task("V" + peer_name,
#endif
    [strongThis, request, peer_name,response](){ 
        //RequestVoteResponse response;
        ClientContext context;
        Status status = strongThis->stub->HandleClient(&context, request, &response);
        if (status.ok()) {
            return true;
        } else {
            return false;
        }
    }
#if defined(USE_GRPC_SYNC_BARE)
    , request);
    t.detach();
#else
    );
#endif
}

bool KvClientSync::SyncHandleClient(const HandleClientRequest& request,HandleClientResponse& response)
{
    ClientContext context;
    Status status = stub->HandleClient(&context, request, &response);
    if(status.ok())
        return true;
    else
        return false;
}


KvClientSync::KvClientSync(const char * addr) : addr_(addr) {
    std::shared_ptr<Channel> channel = grpc::CreateChannel(addr_, grpc::InsecureChannelCredentials());
    stub = client_messages::ClientMessages::NewStub(channel);
}

KvClientSync::KvClientSync(const std::string & addr) : KvClientSync(addr.c_str()) {

}


class KvClient
{
    public:
        /*
        enum GET_FLAG{
            GET_SUCCESS=0,
            GET_FAIL=1,
            GET_AGAIN=2,
        };

        enum SET_FLAG{
            SET_SUCCESS=0,
            SET_FAIL=1,
            SET_AGAIN=2,
        };
        */
        enum FLAG{
            SUCCESS=0,
            FAIL=1,
            AGAIN=2,
        };
        //KvClient(){}
        void AddKvNode(std::string kvNodeAddr)
        {
            //kvNodeAddr[kvNodeAddr.size()-3] = '8';//注释之前报错：assignment of read-only location，待解决
            addrs.push_back(kvNodeAddr);
            kvClients[kvNodeAddr]=std::make_shared<KvClientSync>(kvNodeAddr);
            if(currentAddr.empty())
                currentAddr = kvNodeAddr;
        }
        void SetCurrentAddr(const std::string& addr)
        {
            //addr[addr.size()-3]='8'; //注释之前报错：assignment of read-only location，待解决
            currentAddr = addr;
        }
        bool Get(const std::string& key,std::string& value);
        bool Set(const std::string& key,const std::string& value);
    private:
        std::vector<std::string> addrs;
        std::unordered_map<std::string,std::shared_ptr<KvClientSync>> kvClients;
        int currentIndex;////当前尝试发送的node
        std::string currentAddr;
};

bool KvClient::Get(const std::string& key,std::string& value)
{
    int x=10;
    while(x--)
    {
        client_messages::HandleClientRequest request;
        client_messages::HandleClientResponse response;
        request.set_key(key);
        request.set_value(value);
        request.set_request_type("GET");
        kvClients[currentAddr]->SyncHandleClient(request,response);
        int flag = response.flag();
        if(flag==AGAIN)
        {
            currentAddr = response.leader_name();
        }
        else if(flag==FAIL)
        {
            return false;
        }
        else if(flag==SUCCESS)
        {
            return true;
        }
    }
}

bool KvClient::Set(const std::string& key,const std::string& value)
{
    int x = 10;////fix me 
    while(x--)
    {
        client_messages::HandleClientRequest request;
        client_messages::HandleClientResponse response;
        request.set_key(key);
        request.set_value(value);
        request.set_request_type("SET");
        kvClients[currentAddr]->SyncHandleClient(request,response);
        int flag = response.flag();
        if(flag==AGAIN)
        {
            currentAddr = response.leader_name();
        }
        else if(flag==FAIL)
        {
            return false;
        }
        else if(flag==SUCCESS)
        {
            return true;
        }
    }
}







