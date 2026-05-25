#pragma once
#include <string>
#include <set>
#include <map>
#include <vector>
#include <fstream>
#include <chrono>
#include <ctime>
#include <cstring>
#include "user_module.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ══════════════════════════════════════════════════════════════
//  WHAT THIS FILE IS:
//  The Progress Module. Two classes live here:
//
//  1. ProgressMixin  — tracks everything about one user's learning
//                      journey: which lessons done, XP earned,
//                      streak, and whether they can unlock the next level.
//                      This is a MIXIN — designed to be inherited.
//
//  2. ProgressStore  — manages saving/loading progress for ALL users.
//                      Each user gets their own JSON file on disk:
//                      data/progress/username.json
//
//  WHY SAVE PER USER?
//  If all progress were in one file, every save would rewrite
//  the entire file — slow and risky. One file per user means
//  saving Rohan's progress doesn't touch Gurleen's file.
// ══════════════════════════════════════════════════════════════


// ──────────────────────────────────────────────────────────────
//  STRUCT: LessonProgress
//  Records what happened when a user completed one lesson.
// ──────────────────────────────────────────────────────────────
struct LessonProgress {
    std::string lessonId;
    int         bestScore;      // highest % they ever got on this quiz
    int         attempts;       // how many times they've tried this quiz
    int         xpEarned;       // total XP earned from this lesson
    bool        passed;         // did they pass (≥70%) at least once?

    json toJson() const {
        return {
            {"lessonId",   lessonId},
            {"bestScore",  bestScore},
            {"attempts",   attempts},
            {"xpEarned",   xpEarned},
            {"passed",     passed}
        };
    }

    static LessonProgress fromJson(const json& j) {
        LessonProgress lp;
        lp.lessonId  = j.value("lessonId",  "");
        lp.bestScore = j.value("bestScore", 0);
        lp.attempts  = j.value("attempts",  0);
        lp.xpEarned  = j.value("xpEarned",  0);
        lp.passed    = j.value("passed",    false);
        return lp;
    }
};


// ──────────────────────────────────────────────────────────────
//  CLASS: ProgressMixin
//
//  Tracks the full learning journey of ONE user.
//  This is designed to be inherited by GermanLearner alongside
//  QuizMixin and PlacementMixin — that's how the final object
//  gets all this behaviour through multi-inheritance.
//
//  LEVEL UNLOCK RULES:
//    A2 unlocks when: all A1 lessons passed
//    B1 unlocks when: all A2 lessons passed
//    B2 unlocks when: all B1 lessons passed
//
//  STREAK RULES:
//    Streak = consecutive calendar days with at least one quiz passed.
//    If a day is skipped, streak resets to 0.
//    (For simplicity here we increment streak on every quiz pass —
//     a real app would check calendar dates.)
// ──────────────────────────────────────────────────────────────
class ProgressMixin {
protected:
    // lessonId → LessonProgress (one entry per lesson attempted)
    std::map<std::string, LessonProgress> lessonHistory;

    int  totalXP;
    int  streak;
    int  timeSpentSeconds;
    Level       unlockedLevel;   // highest level this user can access (enum, not string)
    std::string lastStudyDate;   // "YYYY-MM-DD" of the last day a quiz was passed

    // Which lesson IDs belong to each level — set by GermanLearner
    // so ProgressMixin knows what "all A1 lessons" means.
    std::map<std::string, std::vector<std::string>> levelLessonIds;

public:
    ProgressMixin()
        : totalXP(0), streak(0), timeSpentSeconds(0),
          unlockedLevel(Level::A1), lastStudyDate("") {} // production value

    // ── Called by GermanLearner after loading lessons ─────────
    // Tells the progress tracker which lesson IDs exist per level
    void registerLevelLessons(const std::string& level,
                               const std::vector<std::string>& ids) {
        levelLessonIds[level] = ids;
    }

    // ── recordQuizResult ──────────────────────────────────────
    // Called after every quiz completion.
    // Updates lesson history, XP, streak, and checks level unlock.
    void recordQuizResult(const std::string& lessonId,
                          int  score,     // number correct
                          int  total,     // total questions
                          int  xpEarned)
    {
        int pct = (total > 0) ? (score * 100 / total) : 0;
        bool passed = (pct >= 70);   // 70% to pass

        // Update or create the history entry
        auto it = lessonHistory.find(lessonId);
        if (it == lessonHistory.end()) {
            // First attempt
            LessonProgress lp;
            lp.lessonId  = lessonId;
            lp.bestScore = pct;
            lp.attempts  = 1;
            lp.xpEarned  = xpEarned;
            lp.passed    = passed;
            lessonHistory[lessonId] = lp;
        } else {
            // Subsequent attempt — update bests
            it->second.attempts++;
            it->second.xpEarned += xpEarned;
            if (pct > it->second.bestScore) it->second.bestScore = pct;
            if (passed) it->second.passed = true;
        }

        totalXP += xpEarned;

        if (passed) {
            // Get today's date as "YYYY-MM-DD"
            auto now = std::chrono::system_clock::now();
            std::time_t t = std::chrono::system_clock::to_time_t(now);
            char buf[11];
            std::strftime(buf, sizeof(buf), "%Y-%m-%d", std::localtime(&t));
            std::string today(buf);

            // Compute yesterday's date string
            std::time_t yesterday_t = t - 86400;
            char ybuf[11];
            std::strftime(ybuf, sizeof(ybuf), "%Y-%m-%d", std::localtime(&yesterday_t));
            std::string yesterday(ybuf);

            if (lastStudyDate.empty() || lastStudyDate == today) {
                // First study ever, or already studied today — no double-increment
                lastStudyDate = today;
            } else if (lastStudyDate == yesterday) {
                // Studied yesterday and again today — extend streak
                streak++;
                lastStudyDate = today;
            } else {
                // Missed one or more days — streak broken, start fresh
                streak = 1;
                lastStudyDate = today;
            }
        }

        // Check if a new level should unlock
        checkLevelUnlock();
    }

    // ── hasPassedLesson ───────────────────────────────────────
    bool hasPassedLesson(const std::string& lessonId) const {
        auto it = lessonHistory.find(lessonId);
        if (it == lessonHistory.end()) return false;
        return it->second.passed;
    }

    // ── hasAttemptedLesson ────────────────────────────────────
    bool hasAttemptedLesson(const std::string& lessonId) const {
        return lessonHistory.count(lessonId) > 0;
    }

    // ── canAccessLevel ────────────────────────────────────────
    // A user can access their unlocked level and everything below.
    // operator<= compares Level enums via the overloaded operators in user_module.h.
    bool canAccessLevel(const std::string& level) const {
        return stringToLevel(level) <= unlockedLevel;
    }

    // ── checkLevelUnlock ──────────────────────────────────────
    // After each quiz, check if the user has passed all lessons
    // in their current level and should move to the next.
    void checkLevelUnlock() {
        if (unlockedLevel >= Level::B2) return; // already at max

        // Check if all lessons in the current unlocked level are passed
        std::string current = levelToString(unlockedLevel);
        auto it = levelLessonIds.find(current);
        if (it == levelLessonIds.end() || it->second.empty()) return;

        bool allPassed = true;
        for (const std::string& id : it->second) {
            if (!hasPassedLesson(id)) { allPassed = false; break; }
        }

        if (allPassed) {
            // Advance to next level. levelToInt maps A1→1, A2→2, B1→3, B2→4.
            // next[i] gives the Level one step above index i.
            static const Level next[] = {
                Level::A1,   // 0 (UNSET) — guard above prevents reaching here
                Level::A2,   // 1 (A1) → A2
                Level::B1,   // 2 (A2) → B1
                Level::B2,   // 3 (B1) → B2
                Level::B2,   // 4 (B2) → guarded above, safety fallback
            };
            unlockedLevel = next[levelToInt(unlockedLevel)];
        }
    }

    // ── Getters ───────────────────────────────────────────────
    int         getTotalXP()       const { return totalXP; }
    int         getStreak()        const { return streak; }
    // Returns the string form so callers (JSON, HTTP responses) need no change.
    std::string getUnlockedLevel() const { return levelToString(unlockedLevel); }

    // Two overloads: callers that already have a Level enum use the first;
    // callers that work with strings (placement result, JSON load) use the second.
    void setUnlockedLevel(Level l)              { unlockedLevel = l; }
    void setUnlockedLevel(const std::string& s) { unlockedLevel = stringToLevel(s); }
    void setTotalXP(int x)  { totalXP = x; }
    void setStreak(int s)   { streak  = s; }
    void addTimeSpent(int seconds) { timeSpentSeconds += seconds; }
    int  getTimeSpentSeconds() const { return timeSpentSeconds; }

    // ── Build progress summary JSON for the browser ───────────
    json toProgressJson() const {
        json j;
        j["totalXP"]          = totalXP;
        j["streak"]           = streak;
        j["timeSpentSeconds"] = timeSpentSeconds;
        j["unlockedLevel"]    = levelToString(unlockedLevel);
        j["lastStudyDate"]    = lastStudyDate;

        json history = json::array();
        for (const auto& [id, lp] : lessonHistory)
            history.push_back(lp.toJson());
        j["lessonHistory"] = history;

        return j;
    }

    void loadFromProgressJson(const json& j) {
        totalXP          = j.value("totalXP",          0);
        streak           = j.value("streak",           0);
        timeSpentSeconds = j.value("timeSpentSeconds", 0);
        unlockedLevel    = stringToLevel(j.value("unlockedLevel", "A1"));
        lastStudyDate    = j.value("lastStudyDate",   "");

        lessonHistory.clear();
        if (j.contains("lessonHistory")) {
            for (const auto& entry : j["lessonHistory"]) {
                LessonProgress lp = LessonProgress::fromJson(entry);
                lessonHistory[lp.lessonId] = lp;
            }
        }
    }

};


// ──────────────────────────────────────────────────────────────
//  CLASS: ProgressStore
//
//  Saves and loads progress for ALL users.
//  One JSON file per user: data/progress/{username}.json
//
//  USED BY: main.cpp routes (/progress/save, /progress/load)
//  The GermanLearner object in the session holds the live
//  ProgressMixin state. ProgressStore only handles disk I/O.
// ──────────────────────────────────────────────────────────────
class ProgressStore {
private:
    std::string baseDir;

    std::string pathFor(const std::string& username) const {
        return baseDir + "/" + username + ".json";
    }

public:
    explicit ProgressStore(const std::string& dir = "data/progress")
        : baseDir(dir) {}

    // Save a ProgressMixin's state for a given user
    void save(const std::string& username, const ProgressMixin& pm) {
        json j = pm.toProgressJson();
        std::ofstream f(pathFor(username));
        if (f.is_open()) f << j.dump(2);
    }

    // Load into a ProgressMixin — returns false if no save file exists
    bool load(const std::string& username, ProgressMixin& pm) {
        std::ifstream f(pathFor(username));
        if (!f.is_open()) return false;
        try {
            json j;
            f >> j;
            pm.loadFromProgressJson(j);
            return true;
        } catch (...) {
            return false;
        }
    }
};
