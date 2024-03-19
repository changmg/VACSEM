#include "my_util.h"


using namespace std;


int ExecSystComm(const string && cmd) {
    pid_t status;
    cout << "Execute system command: " << cmd << endl;
    status = system(cmd.c_str());
    if (status == -1) {
        cout << "System error!" << endl;
        return 0;
    }
    else {
        cout << "Exit status value = " << status << endl;
        if (WIFEXITED(status)) {
            if (!WEXITSTATUS(status)) {
                cout << "Run shell script successfully." << endl;
                return 1;
            }
            else {
                cout << "Run shell script fail, script exit code: " << WEXITSTATUS(status) << endl;
                return 0;
            }
        }
        else {
            cout << "Exit status = " << WEXITSTATUS(status) << endl;
            return 0;
        }
    }
}


bool IsPathExist(const string & _path) {
    filesystem::path path(_path);
    return filesystem::exists(path); 
}


void CreatePath(const std::string & _path) {
    filesystem::path path(_path);
    if (!filesystem::exists(_path))
        filesystem::create_directories(_path);
}


void FixPath(std::string & _path) {
    if (_path.back() != '/')
        _path += "/";
}