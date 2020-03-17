#include "grpc_utils.h"
#include "node.h"
#include <iostream>


#if defined(USE_GRPC_ASYNC) && !defined(USE_GRPC_STREAM)

void RaftMessagesClientAsync::AsyncCompleteRpc()
{
    void * got_tag;
    bool ok = false;
    // NOTICE `std::bad_weak_ptr` will throw when use `shared_from_this()`. We can't call `shared_from_this()` in constructor!
    auto strongThis = this;
    // auto strongThis = shared_from_this();
    while (strongThis->cq.Next(&got_tag, &ok))
    {
        AsyncClientCallBase * call_base = static_cast<AsyncClientCallBase *>(got_tag);
        if (call_base->type == 1) {
            // This is a response to RequestVote call
            AsyncClientCall<RequestVoteResponse> * call =
                dynamic_cast<AsyncClientCall<RequestVoteResponse> *>(call_base);
            if(call->status.ok()){
                // debug("Receive Async RequestVoteResponse from Peer %s\n", call->context.peer().c_str());
                if(strongThis->raft_node && Nuke::contains(strongThis->raft_node->peers, peer_name)){
                    if(call->response.time() < strongThis->raft_node->start_timepoint){
                        debug("GRPC: Old message, Response from previous request REJECTED.\n");
                    }else{
                        // monitor_delayed(request.time());
                        strongThis->raft_node->on_vote_response(call->response);
                    }
                }
            }
        } else if (call_base->type == 2) {
            // This is a response to AppendEntries call
            AsyncClientCall<AppendEntriesResponse> * call =
                dynamic_cast<AsyncClientCall<AppendEntriesResponse> *>(call_base);
            if(call->status.ok()){
                // debug("Receive Async AppendEntriesResponse from Peer %s\n", call->context.peer().c_str());
                // TODO Replace true by real value of heartbeat
                if(strongThis->raft_node && Nuke::contains(strongThis->raft_node->peers, peer_name)){
                    if(call->response.time() < strongThis->raft_node->start_timepoint){
                        debug("GRPC: Old message, Response from previous request REJECTED.\n");
                    }else{
                        // monitor_delayed(request.time());
                        strongThis->raft_node->on_append_entries_response(call->response, true);
                    }
                }
            }
        } else if (call_base->type == 3) {
            // This is a response to InstallSnapshot call
            AsyncClientCall<InstallSnapshotResponse> * call =
                dynamic_cast<AsyncClientCall<InstallSnapshotResponse> *>(call_base);
            if(call->status.ok()){
                if(strongThis->raft_node && Nuke::contains(strongThis->raft_node->peers, peer_name)){
                    if(call->response.time() < strongThis->raft_node->start_timepoint){
                        debug("GRPC: Old message, Response from previous request REJECTED.\n");
                    }else{
                        // monitor_delayed(request.time());
                        strongThis->raft_node->on_install_snapshot_response(call->response);
                    }
                }
            }
        }
        delete call_base;
    }
    debug("Loop quit.\n");
}

void RaftMessagesClientAsync::AsyncRequestVote(const RequestVoteRequest& request)
{
    // Call will be removed from CompletionQueue
    AsyncClientCall<RequestVoteResponse> * call = new AsyncClientCall<RequestVoteResponse>();
    call->type = 1;
    // TODO NOTICE When analyse with `-fsanitize`, it shows data races between here and AsyncCompleteRpc
    call->response_reader = stub->AsyncRequestVote(&call->context, request, &cq);
    call->response_reader->Finish(&call->response, &call->status, (void*)call);
}

void RaftMessagesClientAsync::AsyncAppendEntries(const AppendEntriesRequest& request, bool heartbeat)
{
    AsyncClientCall<AppendEntriesResponse> * call = new AsyncClientCall<AppendEntriesResponse>();
    call->type = 2;
    call->response_reader = stub->AsyncAppendEntries(&call->context, request, &cq);
    call->response_reader->Finish(&call->response, &call->status, (void*)call);
}

void RaftMessagesClientAsync::AsyncInstallSnapshot(const InstallSnapshotRequest& request){
    AsyncClientCall<InstallSnapshotResponse> * call = new AsyncClientCall<InstallSnapshotResponse>();
    call->type = 3;
    call->response_reader = stub->AsyncInstallSnapshot(&call->context, request, &cq);
    call->response_reader->Finish(&call->response, &call->status, (void*)call);
}

RaftMessagesClientAsync::RaftMessagesClientAsync(const char * addr, struct RaftNode * _raft_node) : raft_node(_raft_node), peer_name(addr) {
    std::shared_ptr<Channel> channel = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
    debug("Create channel from host %s to remote %s\n", raft_node->name.c_str(), addr);
    stub = raft_messages::RaftMessages::NewStub(channel);
    cq_thread = std::thread(&RaftMessagesClientAsync::AsyncCompleteRpc, this);
}

RaftMessagesClientAsync::RaftMessagesClientAsync(const std::string & addr, struct RaftNode * _raft_node) : RaftMessagesClientAsync(addr.c_str(), _raft_node) {

}

#endif