#pragma once
#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <functional>   // std::hash for password hashing
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ══════════════════════════════════════════════════════════════
//  WHAT THIS FILE IS:
//  The User Module. Two classes live here:
//
//  1. User       — represents ONE person's identity and state.
//                  Knows: who they are, what level, how much XP.
//
//  2. UserStore  — manages ALL users.
//                  Knows: how to register, login, save to disk.
//
//  WHY SEPARATE?
//  User has no idea other users exist.
//  UserStore has no idea what a lesson or quiz is.
//  Each class does exactly one job. This is called Single
//  Responsibility — a core OOP principle.
// ══════════════════════════════════════════════════════════════
// ──────────────────────────────────────────────────────────────
//  ENUM: Level
//  Represents which CEFR tier the user is currently at.
//  Using an enum (not strings) means the compiler catches typos.
//  e.g.  Level::A1  not  "a1"  — no accidental "A 1" bugs.
// ──────────────────────────────────────────────────────────────
enum class Level {
    UNSET,          // brand new user, hasn't taken placement test yet
    A1,             // Beginner
    A2,             // Elementary
    B1,             // Intermediate
    B2              // Upper-Intermediate / Advanced
};

// Helper: convert Level enum ↔ string (needed for JSON serialization)
inline std::string levelToString(Level l) {
    switch (l) {
        case Level::A1: return "A1";
        case Level::A2: return "A2";
        case Level::B1: return "B1";
        case Level::B2: return "B2";
        default:        return "UNSET";
    }
}

inline Level stringToLevel(const std::string& s) {
    if (s == "A1") return Level::A1;
    if (s == "A2") return Level::A2;
    if (s == "B1") return Level::B1;
    if (s == "B2") return Level::B2;
    return Level::UNSET;
}

// Operator overloading — enables natural Level comparison syntax
// Maps each Level to an integer so <, >, <=, >= have an obvious total order.
// Without these, enum class gives no built-in relational operators — writing
// Level::A1 < Level::B2 would be a compile error.
inline int levelToInt(Level l) {
    switch (l) {
        case Level::A1: return 1;
        case Level::A2: return 2;
        case Level::B1: return 3;
        case Level::B2: return 4;
        default:        return 0;   // UNSET — treated as below A1
    }
}
inline bool operator<(Level a, Level b)  { return levelToInt(a) <  levelToInt(b); }
inline bool operator>(Level a, Level b)  { return levelToInt(a) >  levelToInt(b); }
inline bool operator<=(Level a, Level b) { return levelToInt(a) <= levelToInt(b); }
inline bool operator>=(Level a, Level b) { return levelToInt(a) >= levelToInt(b); }

// ──────────────────────────────────────────────────────────────
//  CLASS: User
//
//  This is the BASE CLASS of the entire system.
//  GermanLearner will inherit from this (via virtual inheritance)
//  to get the identity + XP data without duplication.
//
//  WHAT IT STORES:
//    username      — unique identifier, used as the key everywhere
//    passwordHash  — we never store the raw password. We store a
//                    hash (a scrambled number). When the user logs
//                    in we hash what they type and compare numbers.
//    level         — their current CEFR level (A1/A2/B1/B2)
//    xp            — total experience points earned across all sessions
//    streak        — how many consecutive days they've studied
// ──────────────────────────────────────────────────────────────
class User {
protected:
    std::string username;
    size_t      passwordHash;   // size_t is the type std::hash returns
    Level       currentLevel;
    int         xp;
    // NOTE: streak intentionally NOT here.
    // It lives in ProgressMixin so that GermanLearner (which inherits
    // both User and ProgressMixin) has no ambiguity.

public:
    // Default constructor — needed for std::map storage
    User()
        : username(""), passwordHash(0),
          currentLevel(Level::UNSET), xp(0) {}

    User(const std::string& uname, const std::string& password)
        : username(uname),
          passwordHash(std::hash<std::string>{}(password)),
          currentLevel(Level::UNSET),
          xp(0) {}

    // ── Getters (read-only access, marked const) ──────────────
    std::string getName()     const { return username; }
    Level       getLevel()    const { return currentLevel; }
    std::string getLevelStr() const { return levelToString(currentLevel); }
    int         getXP()       const { return xp; }

    // ── Setters ───────────────────────────────────────────────
    void setLevel(Level l)     { currentLevel = l; }
    void setLevel(const std::string& s) { currentLevel = stringToLevel(s); }
    void addXP(int pts)        { xp += pts; }

    // ── Password check ────────────────────────────────────────
    // Hash what they typed, compare to stored hash. Never compare
    // raw passwords — this is standard security practice.
    bool checkPassword(const std::string& attempt) const {
        return std::hash<std::string>{}(attempt) == passwordHash;
    }

    bool isNewUser() const { return currentLevel == Level::UNSET; }

    // ── Serialization: User → JSON ────────────────────────────
    // Called when saving to users.json on disk
    json toJson() const {
        return {
            {"username",     username},
            {"passwordHash", passwordHash},
            {"level",        levelToString(currentLevel)},
            {"xp",           xp}
        };
    }

    // ── Deserialization: JSON → User ──────────────────────────
    // Called when loading users.json on startup
    static User fromJson(const json& j) {
        User u;
        u.username     = j.value("username",     "");
        u.passwordHash = j.value("passwordHash", (size_t)0);
        u.currentLevel = stringToLevel(j.value("level", "UNSET"));
        u.xp           = j.value("xp",           0);
        return u;
    }
};
// ──────────────────────────────────────────────────────────────
//  CLASS: UserStore
//
//  Manages the complete collection of all registered users.
//  Acts like a simple in-memory database backed by a JSON file.
//
//  INTERNAL STORAGE:
//    std::map<string, User>  users
//    Key   = username  (unique, case-sensitive)
//    Value = User object
//
//  PERSISTENCE:
//    On startup   → loadFromDisk()  reads  data/users.json
//    On any change → saveToDisk()   writes data/users.json
//    This means data survives server restarts.
// ──────────────────────────────────────────────────────────────
class UserStore {
private:
    std::map<std::string, User> users;
    std::string filePath;

public:
    explicit UserStore(const std::string& path = "data/users.json")
        : filePath(path) {
        loadFromDisk();
    }

    // ── REGISTER ──────────────────────────────────────────────
    // Returns: true  = success
    //          false = username already taken
    bool registerUser(const std::string& username, const std::string& password) {
        if (username.empty() || password.empty()) return false;
        if (users.count(username)) return false;   // already exists

        users[username] = User(username, password);
        saveToDisk();
        return true;
    }

    // ── LOGIN ─────────────────────────────────────────────────
    // Returns pointer to User if credentials match, nullptr otherwise.
    // Pointer (not copy) so callers can modify the User in place.
    User* login(const std::string& username, const std::string& password) {
        auto it = users.find(username);
        if (it == users.end()) return nullptr;          // no such user
        if (!it->second.checkPassword(password)) return nullptr;  // wrong password
        return &it->second;
    }

    // ── GET USER ──────────────────────────────────────────────
    User* getUser(const std::string& username) {
        auto it = users.find(username);
        if (it == users.end()) return nullptr;
        return &it->second;
    }

    bool userExists(const std::string& username) const {
        return users.count(username) > 0;
    }

    // ── GET ALL USERS ─────────────────────────────────────────
    // Returns a copy of every User — used by the leaderboard route.
    // Returns by value (not pointers) so the caller can sort freely
    // without touching the map's internal ordering.
    std::vector<User> getAllUsers() const {
        std::vector<User> result;
        for (const auto& [name, user] : users)
            result.push_back(user);
        return result;
    }

    // ── SAVE TO DISK ──────────────────────────────────────────
    // Serializes the entire users map to JSON and writes to file.
    void saveToDisk() const {
        json j = json::array();
        for (const auto& [name, user] : users) {
            j.push_back(user.toJson());
        }
        std::ofstream f(filePath);
        if (f.is_open()) f << j.dump(2);   // dump(2) = pretty-print with 2-space indent
    }

    // ── LOAD FROM DISK ────────────────────────────────────────
    // Reads users.json and rebuilds the in-memory map.
    // Called once on server startup.
    void loadFromDisk() {
        std::ifstream f(filePath);
        if (!f.is_open()) return;   // file doesn't exist yet → start empty

        try {
            json j;
            f >> j;
            for (const auto& entry : j) {
                User u = User::fromJson(entry);
                users[u.getName()] = u;
            }
        } catch (...) {
            // Corrupted file — start fresh. In production you'd log this.
            users.clear();
        }
    }
};
