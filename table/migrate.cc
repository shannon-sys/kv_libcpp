#include <iostream>
#include <swift/shannon_db.h>
#include <string>
#include <iostream>

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

int main() {
    shannon::DB* db;
    shannon::Options options;
    shannon::DBOptions db_options;
    shannon::Status status;
    std::string device = "/dev/kvdev0";
    int verify = 1;
    char* filename;
    int decode_cfname = 0;
    std::vector <ColumnFamilyHandle*> handles;
    std::vector <ColumnFamilyDescriptor> cf_des;
    ColumnFamilyOptions cf_option;

    char dbname[100];
    char chr;
    cout << "please cin dbname" << endl;
    options.create_if_missing = true;
    string strname;
    scanf("%s%c", dbname, &chr);
    strname.assign(dbname,strlen(dbname));

    status = shannon::DB::Open(options, strname, device, &db);
    if (!status.ok()) {
        shannon::DestroyDB(device, strname, options);
        delete db;
        status = shannon::DB::Open(options, strname, device, &db);
        assert(status.ok());
    }
    cf_des.push_back(ColumnFamilyDescriptor("default", cf_option));
    cout << "please cin countfimily name split with a blank, exit with \"default\"(mean use default db cf)" << endl;
    chr = ' ';
    char cofname[100];
    vector <string> cf_name;

    while (true) {
        scanf("%s%c", cofname, &chr);
        string name(cofname);
        if (name == "default") break;
        ColumnFamilyHandle* hand;
        status = db->CreateColumnFamily(cf_option, string(cofname, strlen(cofname)), &hand);
        delete hand;
        cf_des.push_back(ColumnFamilyDescriptor(string(cofname, strlen(cofname)), cf_option));
        if (chr == '\n' || chr == '\r') break;
    }
    delete db;
    status = shannon::DB::Open(db_options, dbname, device, cf_des, &handles, &db);
    assert(status.ok());
    cout << "please cin sst file split with a blank" << endl;
    char filepath[100];
    while (true) {
        scanf("%s%c", filepath, &chr);
        status = db->IngestExternFile(filepath, verify, &handles);
        if (chr == '\n' || chr == '\r') break;
        assert(status.ok());
    }
    cout << "status : " << status.ToString() << endl;
    cout << "exit...." << endl;
}