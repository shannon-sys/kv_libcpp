#include <gtest/gtest.h>
#include <vector>
#include "swift/shannon_db.h"

std::string g_device_name = "/dev/kvdev0";
class KVLibCPPTest : public testing::Environment {
 public:
  virtual void SetUp() {
  }

  virtual void TearDown() {
  }
};

int GetFDCount(std::string device) {
  char cmd[256];

  snprintf(cmd, sizeof(cmd), "lsof %s | wc -l ", device.data());
  FILE* pf = popen(cmd, "r");
  if (!pf) {
    return -1;
  }
  std::string result;
  char tmp[1024];
  while (fgets(tmp, sizeof(tmp), pf) != NULL) {
    result.append(tmp, strlen(tmp));
  }
  pclose(pf);
  return std::atoi(result.data());
}

TEST(FDTest, FDLeakOpenDBCloseDB) {
  int before_fd_count = GetFDCount(g_device_name);
  ASSERT_GE(before_fd_count, 0);
  shannon::DB *db = NULL;
  shannon::Options options;
  options.create_if_missing = true;
  shannon::Status s = shannon::DB::Open(options, "testdb", g_device_name, &db);
  ASSERT_TRUE(s.ok());
  shannon::ColumnFamilyHandle* cf;
  s = db->CreateColumnFamily(shannon::ColumnFamilyOptions(), "test_cf", &cf);
  ASSERT_TRUE(s.ok());
  ASSERT_TRUE(cf != NULL);
  delete cf;
  delete db;
  cf = NULL;
  db = NULL;
  // reopen with column_family
  std::vector<shannon::ColumnFamilyDescriptor> column_families;
  std::vector<shannon::ColumnFamilyHandle*> handles;
  column_families.push_back(shannon::ColumnFamilyDescriptor(
        shannon::kDefaultColumnFamilyName, shannon::ColumnFamilyOptions()));
  column_families.push_back(shannon::ColumnFamilyDescriptor(
        "test_cf", shannon::ColumnFamilyOptions()));

  s = shannon::DB::Open(shannon::Options(), "testdb", g_device_name, column_families, &handles, &db);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(handles.size(), 2);
  for (auto handle : handles) {
    delete handle;
  }
  delete db;
  int after_fd_count = GetFDCount(g_device_name);
  ASSERT_GE(after_fd_count, 0);
  ASSERT_EQ(before_fd_count, after_fd_count);
}

TEST(FDTest, FDLeakListColumnFamilies) {
  int before_fd_count = GetFDCount(g_device_name);
  std::vector<std::string> column_families;
  shannon::Status s = shannon::DB::ListColumnFamilies(shannon::DBOptions(),
      "testdb", g_device_name, &column_families);
  ASSERT_TRUE(s.ok());
  ASSERT_EQ(column_families.size(), 2);
  int after_fd_count = GetFDCount(g_device_name);
  ASSERT_EQ(before_fd_count, after_fd_count);
}

TEST(FDTest, FDLeakGet_Set_SequenceNumber) {
  uint64_t sequence;
  int before_fd_count = GetFDCount(g_device_name);
  shannon::Status s = shannon::GetSequenceNumber(g_device_name, &sequence);
  ASSERT_TRUE(s.ok());
  std::cout << "GetSequenceNumber:" << sequence << std::endl;

  s = shannon::SetSequenceNumber(g_device_name, sequence);
  ASSERT_TRUE(s.IsIOError());
  s = shannon::SetSequenceNumber(g_device_name, sequence + 1);
  ASSERT_TRUE(s.ok());
  int after_fd_count = GetFDCount(g_device_name);
  ASSERT_EQ(before_fd_count, after_fd_count);
}

TEST(FDTest, FDLeakListDatabase) {
  int before_fd_count = GetFDCount(g_device_name);
  shannon::DatabaseList db_list;
  shannon::Status s = shannon::ListDatabase(g_device_name, &db_list);
  ASSERT_TRUE(s.ok());
  std::cout << "-------------------------" << std::endl;
  for (auto db_info : db_list) {
    std::cout << "db_index: " << db_info.index << std::endl;
    std::cout << "tiemstamp: " << db_info.timestamp << std::endl;
    std::cout << "name: " << db_info.name << std::endl;
    std::cout << "-------------------------" << std::endl;
  }
  int after_fd_count = GetFDCount(g_device_name);
  ASSERT_EQ(before_fd_count, after_fd_count);
}

TEST(FDTest, FDLeakDestroyDB) {
  int before_fd_count = GetFDCount(g_device_name);
  shannon::Status s = shannon::DestroyDB(g_device_name, "testdb", shannon::Options());
  ASSERT_TRUE(s.ok());
  int after_fd_count = GetFDCount(g_device_name);
  ASSERT_EQ(before_fd_count, after_fd_count);
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  testing::Environment* env = new KVLibCPPTest();
  testing::AddGlobalTestEnvironment(env);
  return RUN_ALL_TESTS();
}
