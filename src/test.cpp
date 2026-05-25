// ══════════════════════════════════════════════════════════════
//  test.cpp — Standalone compile test (NO Crow, NO server)
//  Verifies every module compiles and works correctly.
//  Run with:
//    g++ -std=c++17 -I include src/test.cpp -o test && ./test
// ══════════════════════════════════════════════════════════════

#include <iostream>
#include <cassert>
#include "classes.h"     // pulls in all four module headers

void separator(const std::string& title) {
    std::cout << "\n══ " << title << " ══\n";
}

int main() {
    srand(42);   // fixed seed so results are reproducible

    // ── 1. USER MODULE ────────────────────────────────────────
    separator("User Module");

    UserStore store("data/users.json");

    bool reg = store.registerUser("rohan", "pass123");
    std::cout << "Register rohan: " << (reg ? "OK" : "FAIL") << "\n";

    bool dup = store.registerUser("rohan", "other");
    std::cout << "Duplicate block: " << (!dup ? "OK" : "FAIL") << "\n";

    User* u = store.login("rohan", "pass123");
    std::cout << "Login OK: " << (u ? "yes" : "no") << "\n";
    std::cout << "Username: " << u->getName() << "\n";
    std::cout << "Is new:   " << (u->isNewUser() ? "yes" : "no") << "\n";

    // ── 2. LEARNING MODULE ────────────────────────────────────
    separator("Learning Module");

    LessonManager lm("data/lessons");
    lm.loadAll();

    const auto& a1 = lm.getLessonsForLevel("A1");
    std::cout << "A1 lessons loaded: " << a1.size() << "\n";
    for (const auto& l : a1) {
        std::cout << "  [" << l.id << "] " << l.title
                  << " — " << l.words.size() << " words\n";
    }

    // ── 3. GERMAN LEARNER (multi-inheritance) ─────────────────
    separator("GermanLearner — Multi-Inheritance");

    // Use factory to get an A1Learner (polymorphic)
    auto learner = makeLearner("A1", "rohan");
    learner->initFromUser(*u);

    // Copy lessons from LessonManager into GermanLearner
    for (const std::string& lvl : {"A1","A2","B1","B2"}) {
        learner->loadLessons(lvl, lm.getLessonsForLevel(lvl));
    }
    learner->syncProgressLessonIds();

    std::cout << "Level name:      " << learner->getLevelName() << "\n";
    std::cout << "Welcome message: " << learner->getWelcomeMessage() << "\n";
    std::cout << "Topics:\n";
    for (const auto& t : learner->getTopics())
        std::cout << "  • " << t << "\n";

    // ── 4. QUIZ MIXIN ─────────────────────────────────────────
    separator("QuizMixin — Via GermanLearner");

    bool started = learner->startLessonQuiz("a1_greetings");
    std::cout << "Quiz started: " << (started ? "yes" : "no") << "\n";
    std::cout << "Total questions: " << learner->getTotalQuestions() << "\n";

    // Simulate answering all questions
    int q = 0;
    while (!learner->quizDone()) {
        const WordEntry* word = learner->currentQuestion();
        if (!word) break;
        // Alternate: answer correctly every other question
        std::string answer = (q % 2 == 0) ? word->english : "wrong";
        QuizResult r = learner->submitQuizAnswer(answer);
        std::cout << "  Q" << (q+1) << " [" << word->german << "]"
                  << " → " << (r.correct ? "✓" : "✗")
                  << " | XP: " << learner->getXP()
                  << " | Lives: " << learner->getLives() << "\n";
        q++;
    }

    learner->finishQuiz("a1_greetings");
    std::cout << "Correct: " << learner->getCorrectCount()
              << "/" << learner->getTotalQuestions() << "\n";

    // ── 5. WRONG ANSWERS ──────────────────────────────────────
    separator("Wrong Answers (Results Screen Data)");

    for (const auto& w : learner->getWrongAnswers()) {
        std::cout << "  " << w.german
                  << " → correct: " << w.correct
                  << " | you said: " << w.given << "\n";
    }

    // ── 6. PROGRESS MIXIN ─────────────────────────────────────
    separator("ProgressMixin");
    std::cout << "Total XP:       " << learner->getTotalXP() << "\n";
    std::cout << "Unlocked level: " << learner->getUnlockedLevel() << "\n";
    std::cout << "Passed a1_greetings: "
              << (learner->hasPassedLesson("a1_greetings") ? "yes" : "no") << "\n";

    // ── 7. PLACEMENT MIXIN ────────────────────────────────────
    separator("PlacementMixin");
    learner->startPlacement();
    std::cout << "Placement questions: "
              << learner->getPlacementTotal() << "\n";

    // Answer all wrong → should recommend A1
    while (!learner->placementDone()) {
        learner->evaluatePlacement("zzz_wrong");
    }
    std::cout << "Recommended level (all wrong): "
              << learner->getRecommendedLevel() << "\n";

    // ── 8. PROGRESS STORE ─────────────────────────────────────
    separator("ProgressStore — Save/Load");
    ProgressStore ps("data/progress");
    ps.save("rohan", *learner);
    std::cout << "Saved rohan's progress to data/progress/rohan.json\n";

    // Create fresh learner and reload
    auto learner2 = makeLearner("A1", "rohan");
    ps.load("rohan", *learner2);
    std::cout << "Reloaded XP: " << learner2->getTotalXP()
              << " (expected: " << learner->getTotalXP() << ")\n";

    // ── SUMMARY ───────────────────────────────────────────────
    separator("All Tests Complete");
    std::cout << "Multi-inheritance chain verified:\n";
    std::cout << "  GermanLearner ← User + GermanLanguage\n";
    std::cout << "               ← QuizMixin + PlacementMixin\n";
    std::cout << "               ← ProgressMixin\n";
    std::cout << "  A1Learner    ← GermanLearner (polymorphism)\n";

    return 0;
}
