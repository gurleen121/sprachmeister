#pragma once
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include "user_module.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ══════════════════════════════════════════════════════════════
//  WHAT THIS FILE IS:
//  The Learning Module. Three things live here:
//
//  1. WordEntry      — one vocabulary item (german + english + hints)
//  2. Lesson         — a full lesson: topic name + list of WordEntrys
//  3. GermanLanguage — the trait class that OWNS all lesson data
//                      for every level. This is one of the parents
//                      in the multi-inheritance chain.
//  4. LessonManager  — knows how to load lessons from JSON files
//                      on disk and hand them to the server.
//
//  WHY LOAD FROM JSON FILES?
//  If vocabulary were hardcoded in C++, adding one new word means
//  recompiling the whole project. With JSON files you just edit
//  the text file and restart — the C++ code never changes.
// ══════════════════════════════════════════════════════════════


// ──────────────────────────────────────────────────────────────
//  STRUCT: WordEntry
//  The atomic unit of all lesson content.
//  Every flashcard, every quiz question is one WordEntry.
// ──────────────────────────────────────────────────────────────
struct WordEntry {
    std::string german;       // the German word/phrase
    std::string english;      // English translation
    std::string roman;        // pronunciation guide  e.g. "hah-loh"
    std::string example;      // example sentence in German
    std::string exampleTrans; // translation of that example
    std::string category;     // "greeting" | "number" | "color" | "food" | "alphabet" | "sentence"

    // WordEntry → JSON  (for sending to the browser)
    json toJson() const {
        return {
            {"german",       german},
            {"english",      english},
            {"roman",        roman},
            {"example",      example},
            {"exampleTrans", exampleTrans},
            {"category",     category}
        };
    }

    // JSON → WordEntry  (for loading from lesson files)
    static WordEntry fromJson(const json& j) {
        WordEntry w;
        w.german       = j.value("german",       "");
        w.english      = j.value("english",      "");
        w.roman        = j.value("roman",        "");
        w.example      = j.value("example",      "");
        w.exampleTrans = j.value("exampleTrans", "");
        w.category     = j.value("category",     "");
        return w;
    }
};


// ──────────────────────────────────────────────────────────────
//  STRUCT: Lesson
//  One complete lesson = a topic name + a list of WordEntrys.
//  Example:  Lesson{ "Colors", [{rot/red}, {blau/blue}, ...] }
//
//  A level (A1, A2...) contains MULTIPLE Lessons.
//  A1 contains:  Alphabets, Numbers, Colors, Greetings, Sentences, Food
// ──────────────────────────────────────────────────────────────
struct Lesson {
    std::string            id;          // unique key e.g. "a1_colors"
    std::string            title;       // display name e.g. "Colors"
    std::string            level;       // "A1" | "A2" | "B1" | "B2"
    std::string            description; // one-line summary shown on lesson card
    std::vector<WordEntry> words;       // all vocabulary in this lesson
    int                    xpReward;    // XP awarded on quiz pass

    // Lesson → JSON
    json toJson() const {
        json j;
        j["id"]          = id;
        j["title"]       = title;
        j["level"]       = level;
        j["description"] = description;
        j["xpReward"]    = xpReward;
        j["wordCount"]   = (int)words.size();
        json wArr = json::array();
        for (const auto& w : words) wArr.push_back(w.toJson());
        j["words"] = wArr;
        return j;
    }

    // JSON → Lesson
    static Lesson fromJson(const json& j) {
        Lesson l;
        l.id          = j.value("id",          "");
        l.title       = j.value("title",       "");
        l.level       = j.value("level",       "");
        l.description = j.value("description", "");
        l.xpReward    = j.value("xpReward",    10);
        if (j.contains("words")) {
            for (const auto& w : j["words"])
                l.words.push_back(WordEntry::fromJson(w));
        }
        return l;
    }
};


// ──────────────────────────────────────────────────────────────
//  CLASS: GermanLanguage
//
//  This is a TRAIT CLASS — it provides the language knowledge
//  but has no concept of a user, quiz, or progress.
//
//  MULTI-INHERITANCE ROLE:
//  GermanLearner (in classes.h) will inherit from this to get
//  all the vocabulary data. GermanLanguage itself doesn't know
//  it will be inherited — it just does its job cleanly.
//
//  INTERNAL STORAGE:
//    map<level_string, vector<Lesson>>
//    "A1" → [Lesson(Alphabets), Lesson(Colors), Lesson(Greetings)...]
//    "A2" → [...]
//    etc.
// ──────────────────────────────────────────────────────────────
class GermanLanguage {
protected:
    // Key: level string ("A1","A2","B1","B2")
    // Value: all lessons for that level, in order
    std::map<std::string, std::vector<Lesson>> levelLessons;

public:
    // ── Load all lessons for one level ────────────────────────
    void loadLessons(const std::string& level, const std::vector<Lesson>& lessons) {
        levelLessons[level] = lessons;
    }

    // ── Get all lessons for a given level ─────────────────────
    // Returns empty vector if level not loaded yet.
    const std::vector<Lesson>& getLessonsForLevel(const std::string& level) const {
        static std::vector<Lesson> empty;
        auto it = levelLessons.find(level);
        if (it == levelLessons.end()) return empty;
        return it->second;
    }

    // ── Get one specific lesson by its ID ─────────────────────
    // Returns nullptr if not found.
    const Lesson* getLessonById(const std::string& lessonId) const {
        for (const auto& [level, lessons] : levelLessons) {
            for (const auto& lesson : lessons) {
                if (lesson.id == lessonId) return &lesson;
            }
        }
        return nullptr;
    }

    // ── Check if a level has been loaded ──────────────────────
    bool hasLevel(const std::string& level) const {
        return levelLessons.count(level) > 0;
    }

    // ── List all available lesson IDs for a level ─────────────
    std::vector<std::string> getLessonIds(const std::string& level) const {
        std::vector<std::string> ids;
        auto it = levelLessons.find(level);
        if (it != levelLessons.end()) {
            for (const auto& l : it->second) ids.push_back(l.id);
        }
        return ids;
    }

    std::string getLanguageName() const { return "German"; }
};


// ──────────────────────────────────────────────────────────────
//  CLASS: LessonManager
//  Inherits GermanLanguage and adds disk I/O.
//  Called once at server startup to load all JSON lesson files.
//
//  WHY SEPARATE FROM GermanLanguage?
//  GermanLanguage is a pure data container — no file system code.
//  LessonManager knows about files. Keeping them separate means
//  you can test GermanLanguage without any files on disk.
// ──────────────────────────────────────────────────────────────
class LessonManager : public GermanLanguage {
private:
    std::string dataDir;

    // Load one JSON file and parse into vector<Lesson>
    std::vector<Lesson> loadFile(const std::string& path) {
        std::vector<Lesson> result;
        std::ifstream f(path);
        if (!f.is_open()) {
            // File missing → return empty. Server keeps running.
            return result;
        }
        try {
            json j;
            f >> j;
            // Each JSON file is an array of lesson objects
            if (j.is_array()) {
                for (const auto& entry : j)
                    result.push_back(Lesson::fromJson(entry));
            }
        } catch (...) {
            // Malformed JSON → skip this file
        }
        return result;
    }

public:
    explicit LessonManager(const std::string& dir = "data/lessons")
        : dataDir(dir) {}

    // Call this once when the server starts.
    // Loads a1_lesson.json → A1 content
    //        a2_lesson.json → A2 content   etc.
    void loadAll() {
        struct { std::string level; std::string file; } levels[] = {
            {"A1", dataDir + "/a1_lesson.json"},
            {"A2", dataDir + "/a2_lesson.json"},
            {"B1", dataDir + "/b1_lesson.json"},
            {"B2", dataDir + "/b2_lesson.json"},
        };
        for (const auto& entry : levels) {
            auto lessons = loadFile(entry.file);
            if (!lessons.empty())
                loadLessons(entry.level, lessons);
        }
    }

    // Build a summary JSON array for the frontend lesson-select screen.
    // Sends id, title, description, wordCount, xpReward — NOT the full words.
    // The full words are only sent when the user actually opens the lesson.
    json getLevelSummary(const std::string& level) const {
        json arr = json::array();
        for (const auto& lesson : getLessonsForLevel(level)) {
            arr.push_back({
                {"id",          lesson.id},
                {"title",       lesson.title},
                {"level",       lesson.level},
                {"description", lesson.description},
                {"wordCount",   (int)lesson.words.size()},
                {"xpReward",    lesson.xpReward}
            });
        }
        return arr;
    }
};
