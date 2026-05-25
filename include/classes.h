#pragma once
#include "user_module.h"
#include "learning_module.h"
#include "assessment_module.h"
#include "progress_module.h"
#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>

// ══════════════════════════════════════════════════════════════
//  WHAT THIS FILE IS:
//  The CORE of the entire project. Three things here:
//
//  1. GermanLearner  — THE multi-inheritance class.
//                      Inherits from ALL five parents simultaneously.
//                      One object that can do everything.
//
//  2. A1Learner, A2Learner, B1Learner, B2Learner
//                    — Level-specialised subclasses of GermanLearner.
//                      Demonstrate POLYMORPHISM: each overrides
//                      getWelcomeMessage() and getTopics() differently.
//
//  3. SessionManager — holds one GermanLearner per active user.
//                      The server creates one and keeps it alive.
//
//  ─────────────────────────────────────────────────────────────
//  THE INHERITANCE DIAMOND — AND HOW virtual SOLVES IT:
//
//  User has data fields: username, xp, level...
//  GermanLanguage, QuizMixin, PlacementMixin, ProgressMixin
//  are traits — they have no "user" data, just behaviours.
//
//  Without virtual:
//    GermanLearner gets User's fields FIVE TIMES (one per parent).
//    Compiler error: ambiguous access to username, xp, etc.
//
//  With "public virtual User" on GermanLearner:
//    Only ONE shared copy of User's fields exists.
//    All five parents agree to share that one copy.
//  ─────────────────────────────────────────────────────────────
// ══════════════════════════════════════════════════════════════


// ──────────────────────────────────────────────────────────────
//  CLASS: GermanLearner   ← THE MULTI-INHERITANCE CLASS
//
//  Method sources after inheriting all five:
//    From User:            getName(), getXP(), getLevel(), addXP()
//    From GermanLanguage:  getLessonsForLevel(), getLessonById()
//    From QuizMixin:       buildQuiz(), evaluate(), getWrongAnswers()
//    From PlacementMixin:  buildPlacementTest(), evaluatePlacement()
//    From ProgressMixin:   recordQuizResult(), canAccessLevel()
// ──────────────────────────────────────────────────────────────
class GermanLearner
    : public virtual User           // identity, XP, level — virtual = shared copy
    , public GermanLanguage         // all vocabulary data
    , public QuizMixin              // quiz engine
    , public PlacementMixin         // placement test
    , public ProgressMixin          // progress tracking
{
public:
    GermanLearner() : User() {}
    GermanLearner(const std::string& name, const std::string& password)
        : User(name, password) {}

    // ── initFromUser ──────────────────────────────────────────
    // After loading a User from UserStore, copy its identity here.
    void initFromUser(const User& u) {
        username     = u.getName();
        passwordHash = 0;
        currentLevel = u.getLevel();
        xp           = u.getXP();
        // streak lives in ProgressMixin — loaded from disk via ProgressStore
    }

    // ── startLessonQuiz ───────────────────────────────────────
    // Finds the lesson by ID (GermanLanguage), feeds words to QuizMixin.
    bool startLessonQuiz(const std::string& lessonId) {
        const Lesson* lesson = getLessonById(lessonId);
        if (!lesson || lesson->words.empty()) return false;
        buildQuiz(lesson->words);
        return true;
    }

    // ── submitQuizAnswer ──────────────────────────────────────
    // One method that combines QuizMixin::evaluate + User::addXP.
    QuizResult submitQuizAnswer(const std::string& answer) {
        QuizResult r = evaluate(answer);
        if (r.correct) addXP(r.xpAwarded);
        return r;
    }

    // ── finishQuiz ────────────────────────────────────────────
    // After quiz ends: save result to ProgressMixin.
    void finishQuiz(const std::string& lessonId) {
        int xpEarned = getCorrectCount() * 2;
        recordQuizResult(lessonId, getCorrectCount(), getTotalQuestions(), xpEarned);
    }

    // ── startPlacement ────────────────────────────────────────
    // Collects words across all levels → PlacementMixin.
    void startPlacement() {
        std::map<std::string, std::vector<WordEntry>> pool;
        for (const std::string& lvl : {"A1", "A2", "B1", "B2"}) {
            for (const auto& lesson : getLessonsForLevel(lvl))
                for (const auto& w : lesson.words)
                    if (w.category != "alphabet")
                        pool[lvl].push_back(w);
        }
        buildPlacementTest(pool);
    }

    // ── applyPlacementResult ──────────────────────────────────
    std::string applyPlacementResult() {
        std::string recommended = getRecommendedLevel();
        setLevel(recommended);
        setUnlockedLevel(recommended);
        return recommended;
    }

    // ── syncProgressLessonIds ─────────────────────────────────
    // Tell ProgressMixin which lesson IDs exist per level.
    void syncProgressLessonIds() {
        for (const std::string& lvl : {"A1", "A2", "B1", "B2"}) {
            auto ids = getLessonIds(lvl);
            registerLevelLessons(lvl, ids);
        }
    }

    // ── Virtual methods — overridden by level subclasses ──────
    virtual std::string getWelcomeMessage() const {
        return "Welcome to German learning!";
    }
    virtual std::vector<std::string> getTopics() const { return {}; }
    virtual std::string getLevelName() const { return "General"; }

    virtual ~GermanLearner() = default;
};


// ──────────────────────────────────────────────────────────────
//  LEVEL SUBCLASSES — POLYMORPHISM
//
//  Each subclass inherits ALL of GermanLearner's capabilities
//  and overrides only what is level-specific.
//
//  POLYMORPHISM IN ACTION:
//    GermanLearner* p = new B1Learner();
//    p->getWelcomeMessage();  // → B1Learner's version runs
//    p->buildQuiz(words);     // → QuizMixin's version runs (shared)
//    p->addXP(10);            // → User's version runs (shared)
// ──────────────────────────────────────────────────────────────

class A1Learner : public GermanLearner {
public:
    A1Learner() : GermanLearner() {}
    A1Learner(const std::string& n, const std::string& p) : GermanLearner(n, p) {}

    std::string getWelcomeMessage() const override {
        return "Willkommen! Starting from scratch. We cover alphabets, "
               "numbers, colors, greetings, food, and your first sentences.";
    }
    std::vector<std::string> getTopics() const override {
        return { "Alphabets & Pronunciation", "Numbers 1–20", "Colors",
                 "Basic Greetings", "Simple Sentences", "Food Words" };
    }
    std::string getLevelName() const override { return "A1 – Beginner"; }
};

class A2Learner : public GermanLearner {
public:
    A2Learner() : GermanLearner() {}
    A2Learner(const std::string& n, const std::string& p) : GermanLearner(n, p) {}

    std::string getWelcomeMessage() const override {
        return "Gut! You know the basics. Now: daily conversations, "
               "past tense (Perfekt), and common phrases.";
    }
    std::vector<std::string> getTopics() const override {
        return { "Daily Conversations", "Past Tense (Perfekt)",
                 "Common Phrases", "Family & Relationships",
                 "Time & Dates", "Shopping & Money" };
    }
    std::string getLevelName() const override { return "A2 – Elementary"; }
};

class B1Learner : public GermanLearner {
public:
    B1Learner() : GermanLearner() {}
    B1Learner(const std::string& n, const std::string& p) : GermanLearner(n, p) {}

    std::string getWelcomeMessage() const override {
        return "Sehr gut! Grammar cases (Nom/Akk/Dat), modal verbs, "
               "and writing short paragraphs.";
    }
    std::vector<std::string> getTopics() const override {
        return { "Grammar Cases (Nom/Akk/Dat)", "Modal Verbs",
                 "Full Conversations", "Writing Paragraphs",
                 "Opinions & Descriptions", "Travel & Directions" };
    }
    std::string getLevelName() const override { return "B1 – Intermediate"; }
};

class B2Learner : public GermanLearner {
public:
    B2Learner() : GermanLearner() {}
    B2Learner(const std::string& n, const std::string& p) : GermanLearner(n, p) {}

    std::string getWelcomeMessage() const override {
        return "Ausgezeichnet! Complex grammar, debate language, "
               "jobs, travel, and native expressions.";
    }
    std::vector<std::string> getTopics() const override {
        return { "Konjunktiv II", "Debate & Opinions",
                 "Workplace German", "Travel Scenarios",
                 "Native Idioms", "Extended Writing" };
    }
    std::string getLevelName() const override { return "B2 – Upper Intermediate"; }
};


// ──────────────────────────────────────────────────────────────
//  FACTORY: makeLearner
//  Returns the right subclass as a GermanLearner* based on level.
//  Caller doesn't need to know which subclass — that's the factory pattern.
// ──────────────────────────────────────────────────────────────
inline std::unique_ptr<GermanLearner> makeLearner(
    const std::string& level,
    const std::string& username = "",
    const std::string& password = "")
{
    if (level == "A2") return std::make_unique<A2Learner>(username, password);
    if (level == "B1") return std::make_unique<B1Learner>(username, password);
    if (level == "B2") return std::make_unique<B2Learner>(username, password);
    return std::make_unique<A1Learner>(username, password);
}


// ──────────────────────────────────────────────────────────────
//  CLASS: SessionManager
//  One GermanLearner per logged-in user, kept alive across requests.
//  The server holds exactly one SessionManager for its lifetime.
// ──────────────────────────────────────────────────────────────
class SessionManager {
private:
    std::unordered_map<std::string, std::unique_ptr<GermanLearner>> sessions;

    // WHY THIS MUTEX EXISTS:
    // Crow runs with .multithreaded(), meaning multiple HTTP requests execute
    // simultaneously on different OS threads. std::unordered_map is NOT
    // thread-safe: two concurrent login requests could both call operator[]
    // at the same moment, causing a data race and undefined behaviour (silent
    // corruption or a crash). The mutex serialises all access to `sessions`
    // so only one thread touches the map at a time.
    //
    // mutable: allows locking inside const methods (e.g. hasSession).
    mutable std::mutex sessionMutex;

public:
    GermanLearner* createSession(const std::string& username,
                                  const std::string& level) {
        // RAII lock: acquired here, released automatically when `lock` goes
        // out of scope — even if an exception is thrown inside this method.
        std::lock_guard<std::mutex> lock(sessionMutex);
        sessions[username] = makeLearner(level, username);
        return sessions[username].get();
    }

    GermanLearner* getSession(const std::string& username) {
        // RAII lock: prevents a concurrent createSession/removeSession from
        // invalidating the iterator while we are reading the map.
        std::lock_guard<std::mutex> lock(sessionMutex);
        auto it = sessions.find(username);
        if (it == sessions.end()) return nullptr;
        return it->second.get();
    }

    bool hasSession(const std::string& username) const {
        // RAII lock: const method, so sessionMutex is declared mutable.
        std::lock_guard<std::mutex> lock(sessionMutex);
        return sessions.count(username) > 0;
    }

    void removeSession(const std::string& username) {
        // RAII lock: erase modifies the map's internal structure; unprotected
        // concurrent erase + find is undefined behaviour.
        std::lock_guard<std::mutex> lock(sessionMutex);
        sessions.erase(username);
    }
};
