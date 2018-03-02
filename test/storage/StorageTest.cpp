#include "gtest/gtest.h"
#include <iostream>
#include <set>
#include <vector>

#include <storage/MapBasedGlobalLockImpl.h>
#include <afina/execute/Get.h>
#include <afina/execute/Set.h>
#include <afina/execute/Add.h>
#include <afina/execute/Append.h>
#include <afina/execute/Delete.h>

using namespace Afina::Backend;
using namespace Afina::Execute;
using namespace std;

TEST(StorageTest, PutGet) {
    MapBasedGlobalLockImpl storage;

    storage.Put("KEY1", "val1");
    storage.Put("KEY2", "val2");

    std::string value;
    EXPECT_TRUE(storage.Get("KEY1", value));
    EXPECT_TRUE(value == "val1");

    EXPECT_TRUE(storage.Get("KEY2", value));
    EXPECT_TRUE(value == "val2");
}

TEST(StorageTest, PutOverwrite) {
    MapBasedGlobalLockImpl storage;

    storage.Put("KEY1", "val1");
    storage.Put("KEY1", "val2");

    std::string value;
    EXPECT_TRUE(storage.Get("KEY1", value));
    EXPECT_TRUE(value == "val2");
}

TEST(StorageTest, PutIfAbsent) {
    MapBasedGlobalLockImpl storage;

    storage.Put("KEY1", "val1");
    storage.PutIfAbsent("KEY1", "val2");

    std::string value;
    EXPECT_TRUE(storage.Get("KEY1", value));
    EXPECT_TRUE(value == "val1");
}

TEST(StorageTest, BigTest) {
    MapBasedGlobalLockImpl storage(100000, false);

    std::stringstream ss;

    for(long i=0; i<100000; ++i)
    {
        ss << "Key" << i;
        std::string key = ss.str();
        ss.str("");
        ss << "Val" << i;
        std::string val = ss.str();
        ss.str("");
        storage.Put(key, val);
    }

    for(long i=99999; i>=0; --i)
    {
        ss << "Key" << i;
        std::string key = ss.str();
        ss.str("");
        ss << "Val" << i;
        std::string val = ss.str();
        ss.str("");

        std::string res;
        storage.Get(key, res);

        EXPECT_TRUE(val == res);
    }

}

TEST(StorageTest, MaxTest) {
    MapBasedGlobalLockImpl storage(1000, false);

    std::stringstream ss;

    for(long i=0; i<1100; ++i)
    {
        ss << "Key" << i;
        std::string key = ss.str();
        ss.str("");
        ss << "Val" << i;
        std::string val = ss.str();
        ss.str("");
        storage.Put(key, val);
    }

    for(long i=100; i<1100; ++i) {
        ss << "Key" << i;
        std::string key = ss.str();
        ss.str("");
        ss << "Val" << i;
        std::string val = ss.str();
        ss.str("");

        std::string res;
        storage.Get(key, res);

        EXPECT_TRUE(val == res);
    }

    for(long i=0; i<100; ++i)
    {
        ss << "Key" << i;
        std::string key = ss.str();
        ss.str("");

        std::string res;
        EXPECT_FALSE(storage.Get(key, res));
    }
}

TEST(StorageTest, MaxTest1){
    MapBasedGlobalLockImpl storage(14000, true);
    std::stringstream ss;

    for(long i=1000; i<2100; ++i)
    {
        ss << "Key" << i;
        std::string key = ss.str();
        ss.str("");
        ss << "Val" << i;
        std::string val = ss.str();
        ss.str("");
        storage.Put(key, val);
    }

    for(long i=1100; i<2100; ++i) {
        ss << "Key" << i;
        std::string key = ss.str();
        ss.str("");
        ss << "Val" << i;
        std::string val = ss.str();
        ss.str("");

        std::string res;
        storage.Get(key, res);

        EXPECT_TRUE(val == res);
    }

    for(long i=1000; i<1100; ++i)
    {
        ss << "Key" << i;
        std::string key = ss.str();
        ss.str("");

        std::string res;
        EXPECT_FALSE(storage.Get(key, res));
    }
}



TEST(StorageTest, TestRefill){
    MapBasedGlobalLockImpl storage(14, true);

    storage.Put("KEY1", "val");
    storage.PutIfAbsent("KEY2", "vul");
    storage.Put("KEY3", "valvalval");

    std::string value;
    EXPECT_TRUE(storage.Get("KEY3", value));
    EXPECT_FALSE(storage.Get("KEY1", value));
    EXPECT_TRUE(value == "valvalval");
}

TEST(StorageTest, TestNoMem){
    MapBasedGlobalLockImpl storage(10);
    std::string val;
    EXPECT_FALSE(storage.Put("LONGKEY","LONGVAL"));
    EXPECT_TRUE(storage.Put("SHKEY","SHVAL"));
    EXPECT_FALSE(storage.Get("LONGKEY",val));
    EXPECT_TRUE(storage.Get("SHKEY",val));
    EXPECT_TRUE(storage.Delete("SHKEY"));
}