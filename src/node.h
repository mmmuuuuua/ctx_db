#pragma once

#include <cstring>
#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <map>
#include <functional>
#include <algorithm>

#include "kv_server.h"
#include "shardmaster_server.h"
#include "shardkv_server.h"

#include <tuple>
#include <utility>




struct RaftNode {
    ///ctx
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

    enum NodeState {
        Follower = 0,
        Candidate = 1,
        Leader = 2,
        NotRunning = 3,
    };

    enum ElectionState{
        ELE_NONE, // other state
        // How the node detect its election period starts?
        ELE_NEW, // start election itself
        ELE_AGA, // re-start election itself
        ELE_VOTE, // detect other node's election
        // How the node detect its election ends?
        ELE_SUC, // win election 
        ELE_T, // my election is timeout
        ELE_FAIL, // lose election as a candidate
        ELE_SUC_OB, // observed other's election success
    }; // Only used in callbacks to suggest an event related to election(its or others) has happend.

    static const char * node_state_name(NodeState s) {
        switch(s){
            case NodeState::Follower:
                return "Follower";
            case NodeState::Candidate:
                return "Candidate";
            case NodeState::Leader:
                return "Leader";
            case NodeState::NotRunning:
                return "NotRunning";
            default:
                return "Error";
        }
    }

    struct Configuration{
        static Configuration from_string(const std::string & conf_str){
            std::vector<std::string> v1 = Nuke::split(conf_str, "\n");
            std::vector<std::string> app = Nuke::split(v1[0], ";");
            std::vector<std::string> rem = Nuke::split(v1[1], ";");
            std::vector<std::string> old = Nuke::split(v1[2], ";");
            return {app, rem, old};
        }
        static std::string to_string(const Configuration & conf){
            std::string conf_str;
            conf_str += Nuke::join(conf.app.begin(), conf.app.end(), ";") + "\n";
            conf_str += Nuke::join(conf.rem.begin(), conf.rem.end(), ";") + "\n";
            conf_str += Nuke::join(conf.old.begin(), conf.old.end(), ";") + "\n";
            return conf_str; 
        }
        void init(){
            oldvote_thres = old.size(); // Including myself
            newvote_thres = oldvote_thres;
            for(auto & s: app){
                if(!Nuke::contains(old, s)){
                    newvote_thres++;
                }
            }
            for(auto & s: rem){
                if(Nuke::contains(old, s)){
                    newvote_thres--;
                }
            }
        }
        Configuration(const std::vector<std::string> & a, const std::vector<std::string> & r, const std::vector<NodeName> & o) : app(a), rem(r), old(o){
            init();
        }
        Configuration(const std::vector<std::string> & a, const std::vector<std::string> & r, const std::map<NodeName, NodePeer*> & o, const NodeName & leader_exclusive) : app(a), rem(r){
            for(auto && p: o){
                old.push_back(p.first);
            }
            // Include myself
            old.push_back(leader_exclusive);
            init();
        }
        std::vector<std::string> app;
        std::vector<std::string> rem;
        std::vector<std::string> old;
        // If `commit_index >= index`, then C_{old,new} is comitted.
        IndexID index = -1;
        // If `commit_index >= index2`, then C_{new} Configuration is comitted.
        IndexID index2 = -1;
        size_t oldvote_thres = 0;
        size_t newvote_thres = 0;
        enum State{
            // This state means `trans_conf == nullptr`.
            // No cluster membership changes.
            // This state is used when we initialize a node, especially from a persist storage.
            BLANK = 0,
            // In staging period, no entries are logged.
            STAGING = 1,
            // Now entry for joint consensus is logged, but not committed.
            OLD_JOINT = 2,
            // Now entry for joint consensus is committed.
            JOINT = 3,
            // Now entry for new configuration are sent.
            // ref update_configuration_new
            JOINT_NEW = 4,
            NEW,
        };
        State state = STAGING;
        bool is_in_old(const std::string & peer_name) const {
            return Nuke::contains(old, peer_name); 
        }
        bool is_in_new(const std::string & peer_name) const {
            return (is_in_old(peer_name) && !Nuke::contains(rem, peer_name)) || Nuke::contains(app, peer_name);
        }
        bool is_in_joint(const std::string & peer_name) const{
            return is_in_old(peer_name) || is_in_new(peer_name);
        }
    };

    // Standard
    TermID current_term = default_term_cursor;
    NodeName vote_for = vote_for_none;
    std::vector<raft_messages::LogEntry> logs;
    std::string leader_name;
    struct Configuration * trans_conf = nullptr;
    struct Persister * persister = nullptr; // If set to nulltr then persist is not enabled.

    // RSM usage
    IndexID commit_index = default_index_cursor;
    IndexID last_applied = default_index_cursor;

    // Leader exclusive

    // Node infomation
    NodeName name;
    NodeState state;
    bool paused = false;
    bool tobe_destructed = false;
    uint64_t last_tick = 0; // Reset every `default_heartbeat_interval`
    uint64_t elect_timeout_due = 0; // At this point the Follower start election
    uint64_t election_fail_timeout_due = 0; // At this point the Candidate's election timeout

    // Candidate exclusive
    size_t vote_got = 0;
    size_t new_vote = 0;
    size_t old_vote = 0;

    // Network
    std::map<NodeName, NodePeer*> peers;
    std::vector<NodePeer*> tobe_removed_peers;
    #if defined(USE_GRPC_STREAM)
    RaftStreamServerContext * raft_message_server = nullptr;
    #else
    RaftServerContext * raft_message_server = nullptr;
    #endif
    std::thread timer_thread;
    uint64_t start_timepoint; // Reject RPC from previous test cases.
    // Ref. elaborate_strange_erase.log
    uint64_t last_timepoint; // TODO Reject RPC from previous meeage from current Leader.
    // Last seq received from Leader
    // NOTICE Why seq number? See https://groups.google.com/forum/#!topic/raft-dev/kn5vRAtmoSc.
    // "In the context of Raft, RPC is useful to know which responses comes from 
    // which requests. If you don't know this, then you can have scenarios like"
    // "In order to differentiate the messages, there are several approaches 
    // (non-exhaustive list) : 
    //  - use an RPC mechanism that gives you the correspondence 
    //  - use message IDs in requests that are also sent back in responses 
    //  ..."
    uint64_t last_seq;

    // RPC utils for sync
#if defined(USE_GRPC_SYNC) && !defined(USE_GRPC_STREAM) && !defined(USE_GRPC_SYNC_BARE) 
    // All peer's RaftMessagesClient share one `sync_client_task_queue`.
    // std::shared_ptr<Nuke::ThreadExecutor> sync_client_task_queue;
    Nuke::ThreadExecutor * sync_client_task_queue = nullptr;
#endif

    // Utils
    // Multiple thread can access the same object, use mutex for protection.
    mutable std::mutex mut;

    // 
    std::condition_variable cv;

    // API
    NuftCallbackFunc callbacks[NUFT_CB_SIZE];
    bool debugging = false;

    void print_state(std::lock_guard<std::mutex> & guard){
        printf("%15s %12s %5s %9s %7s %7s %6s %6s\n",
                "Name", "State", "Term", "log size", "commit", "peers", "run", "trans");
        printf("%15s %12s %5llu %9u %7lld %7u %6s %6d\n", name.c_str(),
                RaftNode::node_state_name(state), current_term,
                logs.size(), commit_index, peers.size(),
                is_running(guard)? "T" : "F", (!trans_conf) ? 0 : trans_conf->state);
    }

    NuftResult invoke_callback(NUFT_CB_TYPE type, NuftCallbackArg arg){
        if(callbacks[type]){
            // If is callable
            return (callbacks[type])(type, &arg);
        }
        return NUFT_OK;
    }

    // TODO NOTICE This function can block, see seq.election.lost.core.7100.noresponse.log, and NuCut's kv
    void set_callback(std::lock_guard<std::mutex> & guard, NUFT_CB_TYPE type, NuftCallbackFunc func){
        callbacks[type] = func;
    }
    void set_callback(NUFT_CB_TYPE type, NuftCallbackFunc func){
        // fprintf(stderr, "set_callback\n");
        GUARD
        // fprintf(stderr, "set_callback granted lock\n"); // Original "set_callback gg"
        set_callback(guard, type, func);
        // fprintf(stderr, "set_callback finish\n");
    }

    IndexID get_base_index() const{
        if(logs.size() == 0){
            return default_index_cursor;
        }
        return logs[0].index();    
    }
    IndexID last_log_index() const {
        if(logs.size() == 0){
            return default_index_cursor;
        }
        // This assertion is tested to be true.
        // However it will fail when log is compacted.
        if(get_base_index() == 0){
            assert(logs.size() == logs.back().index() + 1);
        }
        return logs.back().index();
    }
    IndexID last_log_term() const {
        return logs.size() == 0 ? default_term_cursor : logs.back().term();
    }

    ::raft_messages::LogEntry & gl(IndexID index){
        // Find logs[i].index() == logs[i].index()
        IndexID base = get_base_index();
        if(index - base >= 0){
            return logs[index - base];
        } else{
            // TODO Retrieve from persisted store.
            debug_node("Access index %lld\n", index);
            assert(false);
        }
    }
    size_t find_entry(IndexID index, TermID term){
        for(int i = logs.size() - 1; i >= 0; i--){
            if(logs[i].term() == term && logs[i].index() == index){
                return i;
            }
        }
        return default_index_cursor;
    }
    size_t cluster_size() const {
        return peers.size() + 1;
    }

    bool enough_votes(size_t vote) const {
        // Is `vote` votes enough
        // TODO votes can be cached.
        size_t voters = compute_votes_thres();
        return vote > voters / 2;
    }
    size_t compute_votes_thres() const {
        size_t voters = 1;
        for(auto & pr: peers){
            if(pr.second->voting_rights){
                voters ++;
            }
        }
        return voters;
    }
    
    int enough_votes_trans_conf(std::function<bool(const NodePeer &)> f);
    
    NodeName get_leader_name() const{
        // This function helps Clients to locate Leader.
        // Keep in mind that in raft, Clients only communicate with Leader.
        if(state == NodeState::Leader){
            return name;
        }
        // There's no Leader.
        return leader_name;
    }
    bool test_election();
    void on_timer();
    
    void run(std::lock_guard<std::mutex> & guard);
    void run(){
        GUARD
        run(guard);
    }
    void run(std::lock_guard<std::mutex> & guard, const std::string & new_name){
        name = new_name;
        persister->Load(guard);
        run(guard);
    }
    void run(const std::string & new_name){
        GUARD
        run(guard, new_name);
    }

    void apply_conf(std::lock_guard<std::mutex> & guard, const Configuration & conf);
    void apply_conf(const Configuration & conf);

    void stop_unguarded() {
        paused = true;
        debug_node("Stop node of %s\n", name.c_str());
    }
    void stop() {
        GUARD
        stop(guard);
    }
    void stop(std::lock_guard<std::mutex> & guard) {
        stop_unguarded();
    }
    void resume(std::lock_guard<std::mutex> & guard) {
        paused = false;
        debug_node("Resume node of %s\n", name.c_str());
    }
    bool is_running_unguard() const{
        return (state != NodeState::NotRunning) && (!paused) 
            && (!tobe_destructed); // Prevent Ghost RPC called by `RaftMessagesServiceImpl::RequestVote` etc
    }
    bool is_running(std::lock_guard<std::mutex> & guard) const{
        return (state != NodeState::NotRunning) && (!paused) 
            && (!tobe_destructed); // Prevent Ghost RPC called by `RaftMessagesServiceImpl::RequestVote` etc
    }
    bool is_running() const{
        GUARD
        return is_running(guard);
    }

    void enable_receive(std::lock_guard<std::mutex> & guard, const std::string & peer_name){
        if(Nuke::contains(peers, peer_name)){
            peers.at(peer_name)->receive_enabled = true;
        }
    }
    void disable_receive(std::lock_guard<std::mutex> & guard, const std::string & peer_name){
        if(Nuke::contains(peers, peer_name)){
            peers.at(peer_name)->receive_enabled = false;
        }
    }
    void enable_send(std::lock_guard<std::mutex> & guard, const std::string & peer_name){
        if(Nuke::contains(peers, peer_name)){
            peers.at(peer_name)->send_enabled = true;
        }
    }
    void disable_send(std::lock_guard<std::mutex> & guard, const std::string & peer_name){
        if(Nuke::contains(peers, peer_name)){
            peers.at(peer_name)->send_enabled = false;
        }
    }
    bool is_peer_receive_enabled(std::lock_guard<std::mutex> & guard, const std::string & peer_name) const{
        if(!Nuke::contains(peers, peer_name)) return false;
        return peers.at(peer_name)->receive_enabled;
    }
    bool is_peer_send_enabled(std::lock_guard<std::mutex> & guard, const std::string & peer_name) const{
        if(!Nuke::contains(peers, peer_name)) return false;
        return peers.at(peer_name)->send_enabled;
    }

    void enable_receive(const std::string & peer_name){
        GUARD
        enable_receive(guard, peer_name);
    }
    void disable_receive(const std::string & peer_name){
        GUARD
        disable_receive(guard, peer_name);
    }
    void enable_send(const std::string & peer_name){
        GUARD
        enable_send(guard, peer_name);
    }
    void disable_send(const std::string & peer_name){
        GUARD
        disable_send(guard, peer_name);
    }
    bool is_peer_receive_enabled(const std::string & peer_name) const{
        GUARD
        return is_peer_receive_enabled(guard, peer_name);
    }
    bool is_peer_send_enabled(const std::string & peer_name) const{
        GUARD
        return is_peer_send_enabled(guard, peer_name);
    }

    void reset_election_timeout();

    NodePeer * add_peer(std::lock_guard<std::mutex> & guard, const std::string & peer_name);
    NodePeer * add_peer(const std::string & peer_name);
    void remove_peer(std::lock_guard<std::mutex> & guard, const std::string & peer_name);
    void remove_peer(const std::string & peer_name);
    void reset_peers(std::lock_guard<std::mutex> & guard);
    void reset_peers();
    void clear_removed_peers(std::lock_guard<std::mutex> & guard);
    void clear_removed_peers();
    void wait_clients_shutdown();
    
    void do_apply(bool from_snapshot = false);
    void do_apply(std::lock_guard<std::mutex> & guard, bool from_snapshot = false);

    void send_heartbeat(std::lock_guard<std::mutex> & guard);
    void switch_to(std::lock_guard<std::mutex> & guard, NodeState new_state);
    void become_follower(std::lock_guard<std::mutex> & guard, TermID new_term);
    void become_leader(std::lock_guard<std::mutex> & guard);

    void truncate_log(IndexID last_included_index, TermID last_included_term);
    NuftResult do_install_snapshot(std::lock_guard<std::mutex> & guard, IndexID last_included_index, const std::string & state_machine_state);
    NuftResult do_install_snapshot(IndexID last_included_index, const std::string & state_machine_state);
    void do_send_install_snapshot(std::lock_guard<std::mutex> & guard, NodePeer & peer);
    int on_install_snapshot_request(raft_messages::InstallSnapshotResponse* response_ptr, const raft_messages::InstallSnapshotRequest& request);
    void on_install_snapshot_response(const raft_messages::InstallSnapshotResponse & response);

    void do_election(std::lock_guard<std::mutex> & guard);
    int on_vote_request(raft_messages::RequestVoteResponse * response_ptr, const raft_messages::RequestVoteRequest & request);
    void on_vote_response(const raft_messages::RequestVoteResponse & response);


    template <typename R>
    bool handle_request_routine(std::lock_guard<std::mutex> & guard, const R & request){
        if (request.term() < current_term) {
            debug_node("Reject AppendEntriesRequest/InstallSnapshotRequest from %s, because it has older term %llu, me %llu. It maybe a out-dated Leader\n", request.name().c_str(), request.term(), current_term);
            return false;
        }

        // **Current** Leader is still alive.
        // We can't reset_election_timeout by a out-dated AppendEntriesRPC.
        reset_election_timeout();

        if (request.term() > current_term) {
            // Discover a request with newer term.
            print_state(guard);
            debug_node("Become Follower: Receive AppendEntriesRequest/InstallSnapshotRequest from %s with newer term %llu, me %llu, my time %llu, peer time %llu.\n", 
                request.name().c_str(), request.term(), current_term, (uint64_t)start_timepoint % 10000, (uint64_t)request.time() % 10000);
            become_follower(guard, request.term());
            leader_name = request.name();
            // Observe the emergence of a new Leader.
            invoke_callback(NUFT_CB_ELECTION_END, {this, &guard, ELE_SUC_OB});
        }
        if (request.term() == current_term && state == NodeState::Follower && leader_name == ""){
            // In this case, this node is a Follower, maybe through `RequestVoteRequest`.
            // Now new leader is elected and not knowing the elected leader.
            // This function is mostly for debugging/API, in order to trigger callback, otherwise the debugging thread may block forever at `cv.wait()`
            // IMPORTANT: We can't call `become_follower` here because it will reset vote_for.
            // Consider the following situation(total 3 nodes):
            // 1. A ask for B's vote, B granted, A become leader.
            // 2. A don't have enough time to send AppendEntriesRequest
            // 3. C timeout without acknowledging A's leadership
            // 4. C ask for A's vote
            // 5. If we call become_follower, B will forgot it has already voted for A
            // Error: become_follower(request.term());
            leader_name = request.name();
            invoke_callback(NUFT_CB_ELECTION_END, {this, &guard, ELE_SUC_OB});
        }
        if (request.term() == current_term && state == NodeState::Candidate) {
            // In election and a Leader has won. 
            debug_node("Become Follower: Receive AppendEntriesRequest/InstallSnapshotRequest from %s with term %llu. I lose election of my candidate term.\n", request.name().c_str(), request.term());
            become_follower(guard, request.term());
            leader_name = request.name();
            invoke_callback(NUFT_CB_ELECTION_END, {this, &guard, ELE_FAIL});
        }
        return true;
    }
    std::string print_logs(){
        std::string r;
        for(int i = 0; i < logs.size(); i++){
            r += std::to_string(logs[i].index()) + "(" + std::to_string(logs[i].term()) + ");";
        }
        return r;
    }

    void do_append_entries(std::lock_guard<std::mutex> & guard, bool heartbeat);
    int on_append_entries_request(raft_messages::AppendEntriesResponse * response_ptr, const raft_messages::AppendEntriesRequest & request);
    void on_append_entries_response(const raft_messages::AppendEntriesResponse & response, bool heartbeat);

    NuftResult get_log(std::lock_guard<std::mutex> & guard, IndexID index, ::raft_messages::LogEntry & l) {
        if(index <= last_log_index()){
            l = gl(index);
            return NUFT_OK;
        }
        return -NUFT_FAIL;
    }
    NuftResult get_log(IndexID index, ::raft_messages::LogEntry & l) {
        GUARD
        return get_log(guard, index, l);
    }

    // Major
    NuftResult do_log(std::lock_guard<std::mutex> & guard, ::raft_messages::LogEntry entry, std::function<void(RaftNode*)> f, int command);
    NuftResult do_log(std::lock_guard<std::mutex> & guard, const std::string & log_string, std::function<void(RaftNode*)> f){
        ::raft_messages::LogEntry entry;
        entry.set_cmd("normal");
        entry.set_data(log_string);
        return do_log(guard, entry, f, NUFT_CMD_NORMAL);
    }
    NuftResult do_log(std::lock_guard<std::mutex> & guard, const std::string & log_string, std::function<void(RaftNode*)> f, int command){
        ::raft_messages::LogEntry entry;
        entry.set_data(log_string);
        return do_log(guard, entry, f, command);
    }

    // For API usage
    NuftResult do_log(::raft_messages::LogEntry entry, std::function<void(RaftNode*)> f, int command){
        GUARD
        return do_log(guard, entry, f, command);
    }
    NuftResult do_log(const std::string & log_string, std::function<void(RaftNode*)> f){
        GUARD
        return do_log(guard, log_string, f);
    }
    NuftResult do_log(const std::string & log_string, std::function<void(RaftNode*)> f, int command){
        GUARD
        return do_log(guard, log_string, f, command);
    }
    NuftResult do_log(::raft_messages::LogEntry entry, int command){
        GUARD
        return do_log(guard, entry, [](RaftNode *){}, command);
    }
    NuftResult do_log(const std::string & log_string){
        GUARD
        return do_log(guard, log_string, [](RaftNode *){});
    }
    NuftResult do_log(const std::string & log_string, int command){
        GUARD
        return do_log(guard, log_string, [](RaftNode *){}, command);
    }

    NuftResult do_log(std::string cmd,const ::shardmaster_messages::JoinRequest& request);
    NuftResult do_log(std::string cmd,const ::shardmaster_messages::LeaveRequest& request);
    NuftResult do_log(std::string cmd,const ::shardmaster_messages::MoveRequest& request);
    NuftResult do_log(std::string cmd,const ::shardmaster_messages::QueryRequest& request);
    NuftResult do_log(std::string cmd,const ::shardkv_messages::GetRequest& request);
    NuftResult do_log(std::string cmd,const ::shardkv_messages::PutAppendRequest& request);
    NuftResult do_log(std::string cmd,const Config& newConfig);
    NuftResult do_log(std::string cmd,const ::shardkvinner_messages::ShardMigrationResponse& response);
    NuftResult do_log(std::string cmd,const ::shardkvinner_messages::ShardCleanupRequest& request);

    // API
    NuftResult update_configuration(const std::vector<std::string> & app, const std::vector<std::string> & rem);
    void update_configuration_joint(std::lock_guard<std::mutex> & guard);
    void on_update_configuration_joint(std::lock_guard<std::mutex> & guard, const raft_messages::LogEntry & entry);
    void on_update_configuration_joint_committed(std::lock_guard<std::mutex> & guard);
    void update_configuration_new(std::lock_guard<std::mutex> & guard);
    void on_update_configuration_new(std::lock_guard<std::mutex> & guard, const raft_messages::LogEntry & entry);
    void update_configuration_finish(std::lock_guard<std::mutex> & guard);
    void on_update_configuration_finish(std::lock_guard<std::mutex> & guard);

    void safe_leave(std::lock_guard<std::mutex> & guard);
    void safe_leave(){
        GUARD
        safe_leave(guard);
    }

    template<typename T>
    void set_seq_nr(NodePeer & peer, T & request){
        if(peer.seq == 1){
            request.set_initial(true);
        }else{
            request.set_initial(false);
        }
        request.set_seq(peer.seq);
        peer.seq++;
    }
    bool valid_seq(uint64_t seq, bool initial);


	///根据情况选择使用下面哪一个指针。
    NUFT_SERVER_TYPE type_;
    void* server_; //可能是下述三种情况中的一种
	//KvServer* kv_server; 
	//ShardmasterServer* shardmasterServer_;  
	//ShardkvServer* shardkvServer_;
	
    RaftNode(const std::string & addr,void* server,NUFT_SERVER_TYPE type);
    RaftNode(const std::string & addr);
    RaftNode(const RaftNode &) = delete;
    ~RaftNode();
};

RaftNode * make_raft_node(const std::string & addr);
