#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include "learning_module.h"

// ══════════════════════════════════════════════════════════════
//  WHAT THIS FILE IS:
//  The Assessment Module. Two MIXIN classes live here.
//
//  WHAT IS A MIXIN?
//  A mixin is a class designed specifically to be INHERITED,
//  not used on its own. It contributes behaviour (methods)
//  to a derived class without being a "base" in the traditional
//  sense. Mixins have no user identity, no language content —
//  they only provide one focused set of behaviours.
//
//  1. QuizMixin      — knows how to run a quiz over any word list.
//                      Shuffle, ask, evaluate, track wrong answers.
//
//  2. PlacementMixin — knows how to run a placement test.
//                      Picks questions across all levels, scores them,
//                      and returns the appropriate starting level.
//
//  HOW MULTI-INHERITANCE USES THESE:
//  GermanLearner will inherit BOTH. So one GermanLearner object
//  can call startQuiz(), submitAnswer(), runPlacement() — all of
//  which are defined in these two mixin classes — without
//  GermanLearner itself implementing any of that logic.
// ══════════════════════════════════════════════════════════════


// ──────────────────────────────────────────────────────────────
//  STRUCT: QuizResult
//  What the server sends back to the browser after each answer.
// ──────────────────────────────────────────────────────────────
struct QuizResult {
    bool        correct;
    int         xpAwarded;
    std::string correctAnswer;
    bool        quizFinished;
    int         questionNum;
    int         totalQuestions;
};

// ──────────────────────────────────────────────────────────────
//  STRUCT: WrongEntry
//  Stored for every wrong answer so the results screen can
//  show "You said X, correct answer was Y."
// ──────────────────────────────────────────────────────────────
struct WrongEntry {
    std::string german;
    std::string correct;
    std::string given;
};


// ──────────────────────────────────────────────────────────────
//  CLASS: QuizMixin
//
//  Pure quiz engine. Knows NOTHING about:
//    - which user is playing
//    - what language the words are from
//    - what level the quiz is for
//
//  It only knows: here is a list of words, shuffle them,
//  ask them one by one, compare answers, track score.
//
//  This design means QuizMixin would work identically for
//  a French quiz, a Spanish quiz, anything — you just give
//  it different words.
//
//  STATE IT TRACKS:
//    quizWords   — the shuffled word list for this quiz session
//    quizIndex   — which question we're on right now
//    wrongAnswers — collected wrong answers for the results screen
//    correctCount — running tally of correct answers
// ──────────────────────────────────────────────────────────────
class QuizMixin {
protected:
    std::vector<WordEntry> quizWords;
    int                    quizIndex;
    std::vector<WrongEntry> wrongAnswers;
    int                    correctCount;

public:
    QuizMixin() : quizIndex(0), correctCount(0) {}

    // ── buildQuiz ─────────────────────────────────────────────
    // Takes a lesson's word list, shuffles it, resets all state.
    // Call this once before asking any questions.
    void buildQuiz(const std::vector<WordEntry>& words) {
        quizWords    = words;
        quizIndex    = 0;
        correctCount = 0;
        wrongAnswers.clear();

        // Fisher-Yates shuffle — every permutation equally likely
        // WHY NOT std::random_shuffle? It's deprecated in C++17.
        for (int i = (int)quizWords.size() - 1; i > 0; i--) {
            int j = rand() % (i + 1);
            std::swap(quizWords[i], quizWords[j]);
        }
    }

    // ── currentQuestion ───────────────────────────────────────
    // Returns the WordEntry the user needs to translate right now.
    const WordEntry* currentQuestion() const {
        if (quizIndex >= (int)quizWords.size()) return nullptr;
        return &quizWords[quizIndex];
    }

    // ── quizDone ──────────────────────────────────────────────
    bool quizDone() const {
        return quizIndex >= (int)quizWords.size();
    }

    // ── evaluate ──────────────────────────────────────────────
    // The core logic: compare the user's answer to the correct one.
    //
    // NORMALISATION:
    //   "Hello" == "hello" == "  Hello  " → all accepted.
    //   The correct answer may have slash-separated variants:
    //   "Please / You're welcome" — either part is accepted.
    //
    // NOTE: This method advances quizIndex, so after calling it
    // currentQuestion() returns the NEXT word automatically.
    QuizResult evaluate(const std::string& userAnswer, int xpPerCorrect = 2) {
        QuizResult result;
        result.quizFinished   = false;
        result.xpAwarded      = 0;
        result.questionNum    = quizIndex + 1;
        result.totalQuestions = (int)quizWords.size();

        if (quizDone()) {
            result.quizFinished = true;
            result.correct = false;
            return result;
        }

        const WordEntry& word = quizWords[quizIndex];
        result.correctAnswer  = word.english;

        // Normalise: lowercase + trim whitespace
        auto normalise = [](std::string s) -> std::string {
            // trim
            size_t start = s.find_first_not_of(" \t\r\n");
            size_t end   = s.find_last_not_of(" \t\r\n");
            if (start == std::string::npos) return "";
            s = s.substr(start, end - start + 1);
            // lowercase
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            return s;
        };

        std::string normUser = normalise(userAnswer);
        bool hit = false;

        // Split correct answer on '/' and test each variant
        std::string remaining = word.english;
        while (!remaining.empty() && !hit) {
            auto slashPos = remaining.find('/');
            std::string variant = (slashPos == std::string::npos)
                                  ? remaining
                                  : remaining.substr(0, slashPos);
            if (normalise(variant) == normUser) hit = true;
            if (slashPos == std::string::npos) break;
            remaining = remaining.substr(slashPos + 1);
        }

        result.correct = hit;

        if (hit) {
            correctCount++;
            result.xpAwarded = xpPerCorrect;
        } else {
            wrongAnswers.push_back({ word.german, word.english, userAnswer });
        }

        quizIndex++;
        result.quizFinished = quizDone();
        return result;
    }

    // ── Getters ───────────────────────────────────────────────
    int getCorrectCount()   const { return correctCount; }
    int getTotalQuestions() const { return (int)quizWords.size(); }
    int getQuizIndex()      const { return quizIndex; }

    const std::vector<WrongEntry>& getWrongAnswers() const {
        return wrongAnswers;
    }

    // Build the final results JSON for the browser
    nlohmann::json buildResultsJson(int totalXP) const {
        nlohmann::json j;
        j["correct"]     = correctCount;
        j["total"]       = (int)quizWords.size();
        j["xp"]          = totalXP;
        j["pct"]         = quizWords.empty() ? 0
                           : (correctCount * 100 / (int)quizWords.size());

        nlohmann::json wrongArr = nlohmann::json::array();
        for (const auto& w : wrongAnswers) {
            wrongArr.push_back({
                {"german",  w.german},
                {"correct", w.correct},
                {"given",   w.given}
            });
        }
        j["wrongAnswers"] = wrongArr;
        return j;
    }
};


// ──────────────────────────────────────────────────────────────
//  CLASS: PlacementMixin
//
//  Runs a placement test — 12 questions sampled across A1→B2.
//  Based on score, returns the recommended starting level.
//
//  SAMPLING STRATEGY:
//    3 A1 questions  (easiest)
//    3 A2 questions
//    3 B1 questions
//    3 B2 questions  (hardest)
//
//  SCORING:
//    0-3  correct → A1  (start from the very beginning)
//    4-6  correct → A2  (knows basics)
//    7-9  correct → B1  (conversational)
//    10-12 correct → B2 (advanced)
//
//  WHY A MIXIN AND NOT JUST PART OF QuizMixin?
//  Placement test logic is completely different from lesson quizzes:
//    - Different sampling (across levels, not one lesson)
//    - Different scoring (maps to a level, not a pass/fail)
//    - Only run ONCE per user (at registration)
//  Keeping them separate means each class stays small and focused.
// ──────────────────────────────────────────────────────────────
class PlacementMixin {
protected:
    std::vector<WordEntry> placementWords;
    int                    placementIndex;
    int                    placementCorrect;
    bool                   placementRunning;

public:
    PlacementMixin()
        : placementIndex(0), placementCorrect(0), placementRunning(false) {}

    // ── buildPlacementTest ────────────────────────────────────
    // allLevelWords: map of level → word list
    // Samples 3 words from each level randomly.
    void buildPlacementTest(
        const std::map<std::string, std::vector<WordEntry>>& allLevelWords)
    {
        placementWords.clear();
        placementIndex   = 0;
        placementCorrect = 0;
        placementRunning = true;

        // For each level in order, pick up to 3 random words
        for (const std::string& level : {"A1", "A2", "B1", "B2"}) {
            auto it = allLevelWords.find(level);
            if (it == allLevelWords.end() || it->second.empty()) continue;

            // Copy and shuffle to pick random sample
            std::vector<WordEntry> pool = it->second;
            for (int i = (int)pool.size() - 1; i > 0; i--) {
                int j = rand() % (i + 1);
                std::swap(pool[i], pool[j]);
            }

            int take = std::min(3, (int)pool.size());
            for (int i = 0; i < take; i++)
                placementWords.push_back(pool[i]);
        }
    }

    // ── currentPlacementQuestion ──────────────────────────────
    const WordEntry* currentPlacementQuestion() const {
        if (placementIndex >= (int)placementWords.size()) return nullptr;
        return &placementWords[placementIndex];
    }

    bool placementDone() const {
        return placementIndex >= (int)placementWords.size();
    }

    // ── evaluatePlacement ─────────────────────────────────────
    // Returns true if correct, false if wrong.
    // Automatically advances to the next question.
    bool evaluatePlacement(const std::string& userAnswer) {
        if (placementDone()) return false;

        const WordEntry& word = placementWords[placementIndex];

        auto normalise = [](std::string s) {
            size_t start = s.find_first_not_of(" \t");
            size_t end   = s.find_last_not_of(" \t");
            if (start == std::string::npos) return std::string("");
            s = s.substr(start, end - start + 1);
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            return s;
        };

        bool hit = normalise(userAnswer) == normalise(word.english);
        if (hit) placementCorrect++;
        placementIndex++;
        return hit;
    }

    // ── getRecommendedLevel ───────────────────────────────────
    // Maps score → CEFR level string.
    std::string getRecommendedLevel() const {
        int total = (int)placementWords.size();
        if (total == 0) return "A1";

        // Calculate percentage correct
        int pct = (placementCorrect * 100) / total;

        if (pct < 30)  return "A1";   // 0-29%  → start at A1
        if (pct < 55)  return "A2";   // 30-54% → start at A2
        if (pct < 80)  return "B1";   // 55-79% → start at B1
        return "B2";                   // 80%+   → jump to B2
    }

    int getPlacementScore() const { return placementCorrect; }
    int getPlacementTotal() const { return (int)placementWords.size(); }

    nlohmann::json placementCurrentJson() const {
        const WordEntry* w = currentPlacementQuestion();
        if (!w) return {{"done", true}};
        return {
            {"done",         false},
            {"german",       w->german},
            {"roman",        w->roman},
            {"questionNum",  placementIndex + 1},
            {"total",        (int)placementWords.size()}
        };
    }
};
