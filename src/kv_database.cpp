#include "kv_database.h"

kv_database::kv_database()
{
    dbDir = "/tmp/default";
    raftnode_=NULL;
}
kv_database::kv_database(std::string & dbDir,RaftNode * raftnode)  //对dbDir赋值 
{
    rocksdb::Options options;
    // Optimize RocksDB. This is the easiest way to get RocksDB to perform well
    options.IncreaseParallelism();
    options.OptimizeLevelStyleCompaction();
    // create the DB if it's not already present
    options.create_if_missing = true;

    // open DB
    rocksdb::Status s = rocksdb::DB::Open(options, dbDir, &db);
    assert(s.ok());
    raftnode_ = raftnode;
}

bool kv_database::set(const std::string & key,const std::string & value)
{
    rocksdb::Status s;
    s = db->Put(rocksdb::WriteOptions(), key, value);
    return s.ok();
} 

bool kv_database::get(const std::string & key,const std::string & value)
{
    rocksdb::Status s;
    s = db->Get(rocksdb::ReadOptions(), key, &value);
    return s.ok();
}