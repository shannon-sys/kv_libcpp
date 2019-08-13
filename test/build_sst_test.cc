#include <iostream>
#include <sstream>
#include <string>
#include <getopt.h>
#include <time.h>
#include <swift/shannon_db.h>
#include <sys/stat.h>
using namespace std;
using namespace shannon;
using shannon::Status;
using shannon::Options;
using shannon::Slice;
using shannon::WriteOptions;
using shannon::WriteBatch;
using shannon::WriteBatchNonatomic;
using shannon::ReadOptions;
using shannon::Snapshot;
using shannon::Iterator;
using shannon::ColumnFamilyHandle;
using shannon::ColumnFamilyDescriptor;
using shannon::ColumnFamilyDescriptor;

std::string toString1(int t) {
  stringstream oss;
  oss << t;
  return oss.str();
}

std::string get_file_name(std::string &name, uint64_t number,
                          const char *cfname, const char *suffix) {
  char buf[100];
  snprintf(buf, sizeof(buf), "/%s%06llu.%s", cfname,
           static_cast<unsigned long long>(number), suffix);
  return name + buf;
}
long int file_size(const char *filename) {
  struct stat statbuf;
  stat(filename, &statbuf);
  long int size = statbuf.st_size;
  return size;
}

void usage() {
  printf("Description:\n");
  printf("Usage:\n");
  printf("\t <options>\n");
  printf("Options:\n");
  printf("\t--dbname=test.db \n"
         "\t\tTest target, which is the name of shannondb .\n");
  printf("\t--path = /tmp/sst \n"
         "\t\tthe path that you build_sst, so you must give the path .\n");
  printf("\t--key_size \n"
         "\t\t you shouled give the --key-size  between 1---128 .\n");
  printf("\t--value_size \n"
         "\t\t you shouled give the --value-size  between 1---4M .\n");
  printf("\t--num \n"
         "\t\t the num of kv that you put in db to generate sstfile .\n");
  printf("\t--max_filesize \n"
         "\t\t the max_filesize of sstfile that you generate .\n");
  printf("\tUsageCase: ./build_sst_test --dbname=test.db --path=/tmp/sst/  "
         "--num=100000 --key_size=10 --value_size=2k --max_filesize=1G \n");
}

Status s;
void drop_handle(DB *db, vector<ColumnFamilyHandle *> *handles) {
  Status st;
  for (ColumnFamilyHandle *hand : *handles) {
    if (hand->GetName() != "default") {
      s = db->DropColumnFamily(hand);
      if (!s.ok())
        st = s;
    }
  }
}

#define OPT_TARGET 'a'
#define OPT_NUM_THREADS 'c'
#define OPT_VALUE_SIZE 'd'
#define OPT_KEY_SIZE 'e'
#define OPT_HELP 'z'
#define OPT_NUM 'f'
#define OPT_LOG 'h'
#define OPT_MAXSEQ 'i'
#define OPT_TARGET_PATH 'g'
#define OPT_BATCH_COUNT 'n'
#define OPT_DATABASE 'b'

static struct option lopts[] = {
  { "dbname", required_argument, NULL, OPT_TARGET },
  { "path", required_argument, NULL, OPT_TARGET_PATH },
  { "key_size", required_argument, NULL, OPT_KEY_SIZE },
  { "value_size", required_argument, NULL, OPT_VALUE_SIZE },
  { "num", required_argument, NULL, OPT_NUM },
  { "max_filesize", required_argument, NULL, OPT_MAXSEQ },
  { "help", no_argument, NULL, OPT_HELP },
  { 0, 0, 0, 0 },
};

void get_optstr(struct option *long_opt, char *optstr, int optstr_size) {
  int c = 0;
  struct option *opt;
  memset(optstr, 0x00, optstr_size);
  optstr[c++] = ':';
  for (opt = long_opt; NULL != opt->name; opt++) {
    optstr[c++] = opt->val;
    if (required_argument == opt->has_arg) {
      optstr[c++] = ':';
    } else if (optional_argument == opt->has_arg) {
      optstr[c++] = ':';
      optstr[c++] = ':';
    }
    assert(c < optstr_size);
  }
}

int main(int argc, char *argv[]) {
  char optstr[256];
  int opt;
  double k;
  opterr = 1;
  unsigned long int len = 0;
  shannon::DB *db;
  shannon::Options options;
  shannon::DBOptions db_options;
  shannon::Status status;
  std::string device = "/dev/kvdev0";
  std::string dirname = "/tmp/sst";
  std::string dbname = "test.db";
  int verify = 1;
  int i = 0;
  char keybuf[100];
  char valbuf[100];
  char *key;
  char *value;
  int decode_cfname = 0;
  unsigned long int key_size = 0;
  unsigned long int value_size = 0;
  unsigned long int max_filesize = 0;
  unsigned long int times = 0;
  unsigned long int rawsize = 0;
  double run_time = 0;
  long int filesize = 0;
  std::vector<ColumnFamilyHandle *> handles;
  std::vector<ColumnFamilyDescriptor> cf_des;
  Iterator *iter;
  ColumnFamilyOptions cf_option;
  char *filename;
  if (argc <= 1) {
    usage();
    exit(EXIT_FAILURE);
  }
  get_optstr(lopts, optstr, sizeof(optstr));
  while ((opt = getopt_long_only(argc, argv, optstr, lopts, NULL)) != -1) {
    char *endptr = NULL;
    switch (opt) {
    case OPT_TARGET:
      dbname = optarg;
      break;
    case OPT_TARGET_PATH:
      dirname = optarg;
      break;
    case OPT_KEY_SIZE:
      key_size = strtoul(optarg, NULL, 10);
      break;
    case OPT_VALUE_SIZE:
      len = strtol(optarg, &endptr, 10);
      switch (*endptr) {
      case 'g':
      case 'G':
        len <<= 10;
      case 'm':
      case 'M':
        len <<= 10;
      case 'k':
      case 'K':
        len <<= 10;
      }
      value_size = len;
      break;
    case OPT_NUM:
      times = strtoul(optarg, NULL, 10);
      break;
    case OPT_MAXSEQ:
      len = strtol(optarg, &endptr, 10);
      switch (*endptr) {
      case 'g':
      case 'G':
        len <<= 10;
      case 'm':
      case 'M':
        len <<= 10;
      case 'k':
      case 'K':
        len <<= 10;
      }
      max_filesize = len;
      break;
    case OPT_HELP:
      usage();
      exit(0);
    }
  }
  if (dbname.empty() || dirname.empty() || key_size == 0 || value_size == 0 ||
      max_filesize == 0 || times == 0) {
    fprintf(stderr, "the parameter is wrong, please input again. \n");
    return -EINVAL;
  }
  if (max_filesize < options.target_file_size_base) {
    fprintf(stderr,
            "the max_filesize is %ld less than %ld , please input again . \n",
            max_filesize, options.target_file_size_base);
    return -EINVAL;
  }
  cout << "key_size=" << key_size << ", value_size=" << value_size << endl;
  value = (char *)malloc(value_size);
  if (!value) {
    printf("----malloc value failed !!!!\n");
    return -EINVAL;
  }
  key = (char *)malloc(key_size);
  if (!key) {
    printf("----malloc key failed !!!!\n");
    return -EINVAL;
  }
  memset(key, 'k', key_size);
  memset(value, 'v', value_size);
  options.create_if_missing = true;
  status = shannon::DB::Open(options, dbname, device, &db);
  status = DestroyDB(device, dbname, options);
  assert(status.ok());
  status = shannon::DB::Open(options, dbname, device, &db);
  assert(status.ok());

  cf_des.push_back(ColumnFamilyDescriptor("default", cf_option));
  for (i = 0; i < times; i++) {
    snprintf(key, key_size, "key%d", i);
    snprintf(valbuf, sizeof(valbuf), "value%d", i);
    memcpy(value, valbuf, strlen(valbuf));
    status = db->Put(shannon::WriteOptions(), Slice(key, strlen(key)), Slice(value, value_size));
  }
  cout << "fill db finished" << endl;
  delete db;
  status = shannon::DB::Open(db_options, dbname, device, cf_des, &handles, &db);
  iter = db->NewIterator(shannon::ReadOptions(), handles[0]);
  assert(status.ok());
  iter->SeekToFirst();
  clock_t begin = clock();
  status = db->BuildSstFile(dirname, handles[0]->GetName(), handles[0], iter,
                            max_filesize);
  clock_t end = clock();
  assert(status.ok());
  run_time = (double)(end - begin) / CLOCKS_PER_SEC * 1000;
  filesize = file_size(
      get_file_name(dirname, 1, handles[0]->GetName().data(), "sst").data());
  rawsize = times * (key_size + value_size);
  cout << "Running time: " << run_time << "ms" << endl;
  cout << "Number of kv build: " << times << endl;
  cout << "RawSize of kv build: " << rawsize << endl;
  cout << "FileSize of sst: " << filesize << endl;
  printf("Average: write iops: %.2f k/s  bw %.2f MB/s raw_bw %.2f MB/s \n",
         times / ((double)(run_time)),
         (filesize * 1000) / ((double)1.0 * run_time) / (1024.0 * 1024),
         rawsize / ((double)run_time * 1024));
  drop_handle(db, &handles);
  for (auto handle : handles) {
    delete handle;
  }
  delete iter;
  delete db;
  cout << "ok!" << endl;
}
