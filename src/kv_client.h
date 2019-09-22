




struct KvClientSync : std::enable_shared_from_this<RaftMessagesClientSync>{
    // `RaftMessagesClientSync` defines how to make a sync RPC call, and how to handle its results.
    using HandleClientRequest = ::raft_messages::HandleClientRequest;
    using HandleClientResponse = ::raft_messages::HandleClientResponse;

    // Back reference to raft node.
    struct RaftNode * raft_node = nullptr;
    // std::shared_ptr<Nuke::ThreadExecutor> task_queue;
    Nuke::ThreadExecutor * task_queue = nullptr;
    std::string peer_name;

    void AsyncHandleClient(const HandleClientRequest& request);

    KvClientSync(const char * addr, struct RaftNode * _raft_node);
    KvClientSync(const std::string & addr, struct RaftNode * _raft_node);
    void shutdown(){}
    bool is_shutdown(){return true;}

    ~KvClientSync() {
        raft_node = nullptr;
    }
private:
    std::unique_ptr<raft_messages::RaftMessages::Stub> stub;
};

void KvClientSync::AsyncHandleClient(const HandleClientRequest& request)
{
        // A copy of `request` is needed
    // TODO Replace `std::thread` implementation with future.then implementation. 
    std::string peer_name = this->peer_name;
    auto strongThis = shared_from_this();
#if defined(USE_GRPC_SYNC_BARE)
    std::thread t = std::thread(
#else
    task_queue->add_task("V" + request.name() + peer_name,
#endif
    [strongThis, request, peer_name](){ 
        RequestVoteResponse response;
        ClientContext context;
        Status status = strongThis->stub->HandleClient(&context, request, &response);
        if (status.ok()) {
            if(!strongThis->raft_node){
                debug("GRPC: Old Message, Response RaftNode destructed.\n");
                return;
            }
            // Test whether raft_node_>peers[peer_name] is destructed.
            if(Nuke::contains(strongThis->raft_node->peers, peer_name)){
                if(response.time() < strongThis->raft_node->start_timepoint){
                    debug("GRPC: Old message, Response from previous request REJECTED.\n");
                }else{
                    monitor_delayed(request.time());
                    strongThis->raft_node->on_vote_response(response);
                }
            }
        } else {
            // #if !defined(_HIDE_GRPC_NOTICE)
            debug("GRPC error(RequestVote %s->%s) %d: %s\n", request.name().c_str(), peer_name.c_str(), status.error_code(), status.error_message().c_str());
            // #endif
        }
    }
#if defined(USE_GRPC_SYNC_BARE)
    , request);
    t.detach();
#else
    );
#endif
}