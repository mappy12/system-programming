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
        if (!str.empty() && str.back() == '\n') str.pop_back();

        vector<string> parts = split(str, ':');
        if (parts.size() < 7) continue;

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
        if (!str.empty() && str.back() == '\n') str.pop_back();

        vector<string> parts = split(str, ':');
        if (parts.size() < 2) continue;

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
        if (!str.empty() && str.back() == '\n') str.pop_back();

        vector<string> parts = split(str, ':');
        if (parts.size() < 4) continue;

        Group g;
        g.groupname = parts[0];
        g.admins = parts[2];
        g.users = parts[3];
        groups.push_back(g);
    }
}

void printGroups(const string& username) {
    for (const auto& g : groups) {
        bool member = userInList(username, g.users);
        bool admin = userInList(username, g.admins);

        if (member || admin) {
            cout << g.groupname;
            if (admin) cout << "(admin)";
            cout << " ";
        }
    }
}

int main() {
    cout << getuid() << " " << geteuid() << endl;

    FILE* fpPasswd = fopen("/etc/passwd", "r");
    FILE* fpShadow = fopen("/etc/shadow", "r");
    FILE* fpGshadow = fopen("/etc/gshadow", "r");

    if (!fpPasswd || !fpShadow || !fpGshadow) {
        perror("fopen");
        return 1;
    }

    setuid(getuid()); // сбрасываем рут права сразу после открытия файла

    loadPasswd(fpPasswd);
    loadShadow(fpShadow);
    loadGshadow(fpGshadow);

    for (const auto& u : users) {
        cout << "====================\n";
        cout << u.username << "\n";
        cout << u.uid << "\n";
        cout << u.home << "\n";
        cout << u.passwordHash << "\n";
        printGroups(u.username);
        cout << "\n";
    }

    fclose(fpPasswd);
    fclose(fpShadow);
    fclose(fpGshadow);

    return 0;
}