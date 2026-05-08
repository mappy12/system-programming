#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

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

vector<string> split(const string& str, char delimiter) {
    vector<string> tokens;
    string token;
    stringstream ss(str);

    while (getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}

bool userInList(const string& username, const string& list) {
    vector<string> users = split(list, ',');

    for (const auto& user : users) {
        if (user == username)
            return true;
    }

    return false;
}

void loadPasswd(FILE* fp) {
    char line[1024];

    while (fgets(line, sizeof(line), fp)) {
        string str(line);

        if (!str.empty() && str.back() == '\n')
            str.pop_back();

        vector<string> parts = split(str, ':');

        if (parts.size() < 7)
            continue;

        User user;

        user.username = parts[0];
        user.uid = stoi(parts[2]);
        user.home = parts[5];

        users.push_back(user);
    }
}

void loadShadow(FILE* fp) {
    char line[1024];

    while (fgets(line, sizeof(line), fp)) {
        string str(line);

        if (!str.empty() && str.back() == '\n')
            str.pop_back();

        vector<string> parts = split(str, ':');

        if (parts.size() < 2)
            continue;

        for (auto& user : users) {
            if (user.username == parts[0]) {
                user.passwordHash = parts[1];
                break;
            }
        }
    }
}

void loadGshadow(FILE* fp) {
    char line[2048];

    while (fgets(line, sizeof(line), fp)) {
        string str(line);

        if (!str.empty() && str.back() == '\n')
            str.pop_back();

        vector<string> parts = split(str, ':');

        if (parts.size() < 4)
            continue;

        Group group;

        group.groupname = parts[0];
        group.admins = parts[2];
        group.users = parts[3];

        groups.push_back(group);
    }
}

void printGroups(const string& username) {
    for (const auto& group : groups) {
        bool isMember = userInList(username, group.users);
        bool isAdmin = userInList(username, group.admins);

        if (isMember || isAdmin) {
            cout << group.groupname;

            if (isAdmin)
                cout << " (admin)";

            cout << " ";
        }
    }
}

int main() {
    cout << "Real UID: " << getuid() << endl;
    cout << "Effective UID: " << geteuid() << endl << endl;

    int fdPasswd = open("/etc/passwd", O_RDONLY);
    int fdShadow = open("/etc/shadow", O_RDONLY);
    int fdGshadow = open("/etc/gshadow", O_RDONLY);

    if (fdPasswd < 0 || fdShadow < 0 || fdGshadow < 0) {
        perror("open");
        return 1;
    }

    if (setuid(getuid()) != 0) {
        perror("setuid");
        return 1;
    }

    cout << "Privileges dropped" << endl;
    cout << "Current Effective UID: " << geteuid() << endl << endl;

    FILE* fpPasswd = fdopen(fdPasswd, "r");
    FILE* fpShadow = fdopen(fdShadow, "r");
    FILE* fpGshadow = fdopen(fdGshadow, "r");

    if (!fpPasswd || !fpShadow || !fpGshadow) {
        perror("fdopen");
        return 1;
    }

    loadPasswd(fpPasswd);
    loadShadow(fpShadow);
    loadGshadow(fpGshadow);

    for (const auto& user : users) {
        cout << "=====================================" << endl;
        cout << "Username: " << user.username << endl;
        cout << "UID: " << user.uid << endl;
        cout << "Home directory: " << user.home << endl;
        cout << "Password hash: " << user.passwordHash << endl;

        cout << "Groups: ";
        printGroups(user.username);
        cout << endl;
    }

    fclose(fpPasswd);
    fclose(fpShadow);
    fclose(fpGshadow);

    return 0;
}