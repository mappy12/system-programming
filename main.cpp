#include <iostream>
#include <vector>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include "check.hpp"

using namespace std;

struct User {
    string username;
    int uid;
    string home;
    string passwordHash;
};

struct Group {
    string groupname;
    string admins;
    string users;
};

vector<User> users;
vector<Group> groups;

vector<string> split(const string& line, char delim) {
    vector<string> parts;
    size_t start = 0;

    while (true) {
        size_t pos = line.find(delim, start);

        if (pos == string::npos) {
            parts.push_back(line.substr(start));
            break;
        }

        parts.push_back(line.substr(start, pos - start));
        start = pos + 1;
    }

    return parts;
}

int in_list(const string& user, const string& list) {
    if (list.empty())
        return 0;

    size_t start = 0;

    while (true) {
        size_t pos = list.find(',', start);

        string cur;

        if (pos == string::npos)
            cur = list.substr(start);
        else
            cur = list.substr(start, pos - start);

        if (cur == user)
            return 1;

        if (pos == string::npos)
            break;

        start = pos + 1;
    }

    return 0;
}

void load_passwd(FILE* fp) {
    char line[1024];

    while (check(fgets(line, sizeof(line), fp)) != nullptr) {

        string s = line;
        if (!s.empty() && s.back() == '\n')
            s.pop_back();

        vector<string> p = split(s, ':');

        if (p.size() < 7)
            continue;

        User u;

        u.username = p[0];
        u.uid = atoi(p[2].c_str());
        u.home = p[5];

        users.push_back(u);
    }
}

void load_shadow(FILE* fp) {
    char line[1024];

    while (check(fgets(line, sizeof(line), fp)) != nullptr) {

        string s = line;
        if (!s.empty() && s.back() == '\n')
            s.pop_back();

        vector<string> p = split(s, ':');

        if (p.size() < 2)
            continue;

        for (auto& u : users) {
            if (u.username == p[0]) {
                u.passwordHash = p[1];
                break;
            }
        }
    }
}

void load_gshadow(FILE* fp) {
    char line[2048];

    while (check(fgets(line, sizeof(line), fp)) != nullptr) {

        string s = line;
        if (!s.empty() && s.back() == '\n')
            s.pop_back();

        vector<string> p = split(s, ':');

        if (p.size() < 4)
            continue;

        Group g;

        g.groupname = p[0];
        g.admins = p[2];
        g.users = p[3];

        groups.push_back(g);
    }
}

void print_groups(const string& user) {
    for (const auto& g : groups) {
        int member = in_list(user, g.users);
        int admin = in_list(user, g.admins);

        if (member || admin) {
            cout << g.groupname;
            if (admin)
                cout << "(admin)";
            cout << " ";
        }
    }
}

int main() {
    cout << "UID: " << getuid() << " EUID: " << geteuid() << "\n";

    int fdPasswd = check(open("/etc/passwd", O_RDONLY));
    int fdShadow = check(open("/etc/shadow", O_RDONLY));
    int fdGshadow = check(open("/etc/gshadow", O_RDONLY));

    FILE* fpPasswd = check(fdopen(fdPasswd, "r"));
    FILE* fpShadow = check(fdopen(fdShadow, "r"));
    FILE* fpGshadow = check(fdopen(fdGshadow, "r"));

    setuid(getuid()); // drop root privileges ASAP

    cout << "Privileges dropped\n\n";

    load_passwd(fpPasswd);
    load_shadow(fpShadow);
    load_gshadow(fpGshadow);

    for (const auto& u : users) {
        cout << "====================\n";
        cout << "User: " << u.username << "\n";
        cout << "UID: " << u.uid << "\n";
        cout << "Home: " << u.home << "\n";
        cout << "Hash: " << u.passwordHash << "\n";

        cout << "Groups: ";
        print_groups(u.username);
        cout << "\n";
    }

    fclose(fpPasswd);
    fclose(fpShadow);
    fclose(fpGshadow);

    return 0;
}