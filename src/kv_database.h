#pragma once

#include <cstdio>
#include <string>

//#include "node.h"
#include "rocksdb/db.h"
#include "rocksdb/slice.h"
#include "rocksdb/options.h"

class kv_database
{
    public:
        kv_database();
        kv_database(std::string & dbDir);
        ~kv_database()
        {
            delete db;
        }
        bool set(const std::string & key,const std::string & value);
        bool get(const std::string & key,const std::string & value);
    private:
        std::string dbDir;
        rocksdb::DB * db;
};
