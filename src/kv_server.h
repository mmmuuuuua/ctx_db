
#include <vector>
#include "test_utils.inc.h"



struct RaftMessagesServiceImpl : public raft_messages::RaftMessages::Service {
    // `RaftMessagesServiceImpl` defines what we do when receiving a RPC call.
    struct RaftNode * raft_node = nullptr;

    RaftMessagesServiceImpl(struct RaftNode * _raft_node) : raft_node(_raft_node) {

    }
    ~RaftMessagesServiceImpl(){
        raft_node = nullptr;
    }
    ////////ctx
    Status HandleClient(ServerContext* context,const raft_messages::HandleClientRequest* request,
                        raft_messages::HandleClientResponse* respanse) override;
};


struct RaftServerContext{
    RaftMessagesServiceImpl * service;
    std::unique_ptr<Server> server;
    ServerBuilder * builder;
    RaftServerContext(struct RaftNode * node);
    std::thread wait_thread;
    ~RaftServerContext();
};


RaftServerContext::RaftServerContext(struct RaftNode * node){
    service = new RaftMessagesServiceImpl(node);
#if !defined(_HIDE_GRPC_NOTICE)
    debug("GRPC: Listen to %s\n", node->name.c_str());
#endif
    builder = new ServerBuilder();
    builder->AddListeningPort(node->name, grpc::InsecureServerCredentials());
    builder->RegisterService(service);
    server = std::unique_ptr<Server>{builder->BuildAndStart()};
}

RaftServerContext::~RaftServerContext(){
    debug("Wait shutdown\n");
    server->Shutdown();
    debug("Wait Join\n");
    wait_thread = std::thread([&](){
        server->Wait();
    });
    wait_thread.join();
    debug("wait_thread Joined\n");
    delete service;
    delete builder;
}

class kv_server
{
    public:
        kv_server();
        ~kv_server();
        RaftNode* new_leader(); //旧leader下线之后得到新的leader;
    private:
        std::vector<RaftNode*> nodes;
        RaftNode* leader;
};

kv_server::kv_server()
{
    MakeRaftNodes(5);
    WaitElection(nodes[0]);
}