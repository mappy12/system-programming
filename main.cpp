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

int in_list(const string& user, const string& list) {
    if (list.empty())
        return 0;

    size_t start = 0;

    while (true) {
        size_t pos = list.find(',', start);

        string current;

        if (pos == string::npos)
            current = list.substr(start);
        else
            current = list.substr(start, pos - start);

        if (!current.empty() && current == user)
            return 1;

        if (pos == string::npos)
            break;

        start = pos + 1;
    }

    return 0;
}

void load_passwd(FILE* fp) {
    char line[1024];

    while (fgets(line, sizeof(line), fp)) {
        char* save = nullptr;

        char* name = strtok_r(line, ":\n", &save);
        char* x = strtok_r(nullptr, ":\n", &save);
        char* uid = strtok_r(nullptr, ":\n", &save);
        strtok_r(nullptr, ":\n", &save);
        strtok_r(nullptr, ":\n", &save);
        char* home = strtok_r(nullptr, ":\n", &save);

        if (!name || !uid || !home) continue;

        User u;
        u.username = name;
        u.uid = atoi(uid);
        u.home = home;

        users.push_back(u);
    }
}

void load_shadow(FILE* fp) {
    char line[1024];

    while (fgets(line, sizeof(line), fp)) {
        char* save = nullptr;

        char* name = strtok_r(line, ":\n", &save);
        char* hash = strtok_r(nullptr, ":\n", &save);

        if (!name || !hash) continue;

        for (auto& u : users) {
            if (u.username == name) {
                u.passwordHash = hash;
                break;
            }
        }
    }
}

void load_gshadow(FILE* fp) {
    char line[2048];

    while (fgets(line, sizeof(line), fp)) {
        char* save = nullptr;

        char* name = strtok_r(line, ":\n", &save);
        strtok_r(nullptr, ":\n", &save);
        char* admins = strtok_r(nullptr, ":\n", &save);
        char* users_list = strtok_r(nullptr, ":\n", &save);

        if (!name) continue;

        Group g;
        g.groupname = name;
        if (admins) g.admins = admins;
        if (users_list) g.users = users_list;

        groups.push_back(g);
    }
}

void print_groups(const string& user) {
    for (const auto& g : groups) {
        int member = in_list(user, g.users);
        int admin = in_list(user, g.admins);

        if (member || admin) {
            cout << g.groupname;
            if (admin) cout << "(admin)";
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

    setuid(getuid()); // сброс прав

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