#pragma once

#include "node.h"

struct RaftHandler {
    virtual ~RaftHandler();

    virtual void on_apply(const raft_messages::LogEntry & entry) = 0;
    virtual void on_shutdown();
    virtual void on_snapshot_save();
    virtual void on_snapshot_load();
    virtual void on_state(RaftNode * node, TermID term);
    virtual void on_leader();
    virtual void on_configuration_finish();
};