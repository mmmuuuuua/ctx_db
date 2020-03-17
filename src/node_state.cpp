#include "node.h"

void RaftNode::switch_to(std::lock_guard<std::mutex> & guard, NodeState new_state) {
    NodeState o = state;
    state = new_state;
    invoke_callback(NUFT_CB_STATE_CHANGE, {this, &guard, o});
}


void RaftNode::become_follower(std::lock_guard<std::mutex> & guard, TermID new_term) {
    if (state == NodeState::Leader) {
        // A splitted leader got re-connection,
        // Or step down, as required by the Raft Paper Chapter 6 Issue 2.
        // TODO: must stop heartbeat immediately
    } else {
        // Lose the election

    }
    switch_to(guard, NodeState::Follower);
    current_term = new_term;
    vote_for = vote_for_none;
    last_seq = 0;
}

void RaftNode::become_leader(std::lock_guard<std::mutex> & guard) {
    switch_to(guard, NodeState::Leader);
    // IMPORTANT: We should not reset vote_for.
    // Error: vote_for = vote_for_none;
    leader_name = name;
    if(trans_conf){
        debug_node("Now I am the Leader of term %llu, because I got vote new %u thres %u old %u thres %u.\n", 
            current_term, new_vote, trans_conf->newvote_thres, old_vote, trans_conf->oldvote_thres);
    }else{
        debug_node("Now I am the Leader of term %llu, because I got %llu votes.\n", current_term, vote_got);
    }
    for (auto & pp : this->peers) {
        // Actually we don't know how many logs have been copied,
        // So we assume no log are copied to peer.
        NodePeer & peer = *pp.second;
        peer.match_index = default_index_cursor;
        // "When a leader first comes to power,
        // it initializes all nextIndex values to the index just after the
        // last one in its log."
#define USE_COMMIT_INDEX
#if !defined(USE_COMMIT_INDEX)
        peer.next_index = last_log_index() + 1;
        assert(peer.next_index >= 0);
#else
        // This strategy is from Mushroom's implementation.
        peer.next_index = commit_index + 1;
#endif
        peer.seq = SEQ_START;
    }

    // Heartbeat my leadership
    // Do not call `do_append_entries` or cause deadlock.
    last_tick = 0;
}