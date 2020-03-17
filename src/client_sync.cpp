#include "grpc_utils.h"
#include "node.h"
#include <iostream>

#if defined(USE_GRPC_SYNC) && !defined(USE_GRPC_STREAM)

void RaftMessagesClientSync::AsyncRequestVote(const RequestVoteRequest& request)
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
        Status status = strongThis->stub->RequestVote(&context, request, &response);
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

void RaftMessagesClientSync::AsyncAppendEntries(const AppendEntriesRequest& request, bool heartbeat)
{
    std::string peer_name = this->peer_name;
    auto strongThis = shared_from_this();
#if !defined(_HIDE_GRPC_NOTICE)
    // if(task_queue->workload() == task_queue->capacity){
    //     debug("GRPC: TaskQueue Full.\n");
    // }else{
    //     debug("GRPC: TaskQueue Running %d Capacity %d Queue %d\n", task_queue->workload(), task_queue->capacity, task_queue->in_queue());
    // }
#endif
    if(task_queue->in_queue() > task_queue->capacity * 10){
        debug("GRPC: Too much in TaskQueue: Running %d Capacity %d Queue %d.\n", task_queue->workload(), task_queue->capacity, task_queue->in_queue());
    }
#if defined(USE_GRPC_SYNC_BARE)
    std::thread t = std::thread(
#else
    task_queue->add_task("A" + request.name() + peer_name,
#endif
    [strongThis, heartbeat, request, peer_name](){
        AppendEntriesResponse response;
        ClientContext context;
        Status status = strongThis->stub->AppendEntries(&context, request, &response);
        #if !defined(_HIDE_GRPC_NOTICE)
        debug("GRPC send %s -> %s\n", request.name().c_str(), peer_name.c_str());
        #endif
        if (status.ok()) {
            // NOTICE datarace between this and `remove_peer`, see addpeer.core.log
            if(!strongThis->raft_node){
                debug("GRPC: Old Message, Response RaftNode destructed.\n");
                return;
            }
            if(Nuke::contains(strongThis->raft_node->peers, peer_name)){
                if(response.time() < strongThis->raft_node->start_timepoint){
                    debug("GRPC: Old message, Response from previous request REJECTED.\n");
                }else{
                    monitor_delayed(request.time());
                    strongThis->raft_node->on_append_entries_response(response, heartbeat);
                }
            }
        } else {
            // #if !defined(_HIDE_GRPC_NOTICE)
            if(!heartbeat) debug("GRPC error(AppendEntries %s->%s) %d: %s\n", request.name().c_str(), peer_name.c_str(), status.error_code(), status.error_message().c_str());
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

void RaftMessagesClientSync::AsyncInstallSnapshot(const InstallSnapshotRequest& request)
{
    std::string peer_name = this->peer_name;
    auto strongThis = shared_from_this();
#if defined(USE_GRPC_SYNC_BARE)
    std::thread t = std::thread(
#else
    task_queue->add_task("I" + request.name() + peer_name,
#endif
    [strongThis, request, peer_name](){
        InstallSnapshotResponse response;
        ClientContext context;
        Status status = strongThis->stub->InstallSnapshot(&context, request, &response);
        if (status.ok()) {
            if(!strongThis->raft_node){
                debug("GRPC: Old Message, Response RaftNode destructed.\n");
                return;
            }
            if(Nuke::contains(strongThis->raft_node->peers, peer_name)){
                if(response.time() < strongThis->raft_node->start_timepoint){
                    debug("GRPC: Old message, Response from previous request REJECTED.\n");
                }else{
                    monitor_delayed(request.time());
                    strongThis->raft_node->on_install_snapshot_response(response);
                }
            }
        } else {
            #if !defined(_HIDE_GRPC_NOTICE)
            debug("GRPC error %d: %s\n", status.error_code(), status.error_message().c_str());
            #endif
        }
    }
#if defined(USE_GRPC_SYNC_BARE)
    , request);
    t.detach();
#else
    );
#endif
}


RaftMessagesClientSync::RaftMessagesClientSync(const char * addr, struct RaftNode * _raft_node) : raft_node(_raft_node), peer_name(addr) {
    std::shared_ptr<Channel> channel = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
#if !defined(_HIDE_GRPC_NOTICE)
    debug("GRPC: Create channel between host %s and remote %s\n", raft_node->name.c_str(), addr);
#endif
    stub = raft_messages::RaftMessages::NewStub(channel);
}

RaftMessagesClientSync::RaftMessagesClientSync(const std::string & addr, struct RaftNode * _raft_node) : RaftMessagesClientSync(addr.c_str(), _raft_node) {

}

#endif