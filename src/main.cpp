// ══════════════════════════════════════════════════════════════
//  main.cpp — Crow HTTP Server
//  The bridge between the browser and all C++ modules.
//
//  WHAT THIS FILE DOES:
//  1. Creates one instance of every module (UserStore, LessonManager,
//     ProgressStore, SessionManager) — alive for the server's lifetime.
//  2. Defines HTTP routes — each route is one URL the browser calls.
//  3. Each route: parses JSON body → calls C++ method → returns JSON.
//
//  ROUTES:
//    POST /api/user/register       → UserStore::registerUser()
//    POST /api/user/login          → UserStore::login() + SessionManager
//    POST /api/placement/start     → GermanLearner::startPlacement()
//    POST /api/placement/answer    → PlacementMixin::evaluatePlacement()
//    GET  /api/lessons/:level      → LessonManager::getLevelSummary()
//    GET  /api/lesson/:id          → GermanLanguage::getLessonById()
//    GET  /api/topics/:level       → A1Learner::getTopics() (polymorphic)
//    POST /api/quiz/start          → GermanLearner::startLessonQuiz()
//    POST /api/quiz/answer         → GermanLearner::submitQuizAnswer()
//    POST /api/quiz/finish         → GermanLearner::finishQuiz()
//    POST /api/quiz/retake         → restart quiz
//    GET  /api/progress/:username  → ProgressMixin::toProgressJson()
//    GET  /                        → serves index.html
// ══════════════════════════════════════════════════════════════

#include "crow.h"
#include "classes.h"
#include <fstream>
#include <sstream>
#include <ctime>
#include <algorithm>

// ──────────────────────────────────────────────────────────────
//  Helpers
// ──────────────────────────────────────────────────────────────
void addCors(crow::response& res) {
    res.set_header("Access-Control-Allow-Origin",  "*");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    res.set_header("Content-Type",                 "application/json");
}

crow::response errResp(int code, const std::string& msg) {
    crow::response res(code, "{\"ok\":false,\"error\":\"" + msg + "\"}");
    addCors(res);
    return res;
}

crow::response okResp(json body) {
    body["ok"] = true;
    crow::response res(body.dump(2));
    addCors(res);
    return res;
}

std::string readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// ══════════════════════════════════════════════════════════════
//  MAIN
// ══════════════════════════════════════════════════════════════
int main() {
    srand(static_cast<unsigned>(time(nullptr)));

    // Module instances — ONE each, live for the whole server lifetime
    UserStore      userStore("data/users.json");
    LessonManager  lessonManager("data/lessons");
    ProgressStore  progressStore("data/progress");
    SessionManager sessionManager;

    lessonManager.loadAll();   // read all *.json lesson files on startup

    crow::SimpleApp app;

    // ══════════════════════════════════════════════════════════
    //  SERVE FRONTEND
    // ══════════════════════════════════════════════════════════
    CROW_ROUTE(app, "/")([]() {
        std::string html = readFile("public/index.html");
        if (html.empty()) return crow::response(404, "index.html not found");
        crow::response res(html);
        res.set_header("Content-Type", "text/html; charset=utf-8");
        return res;
    });

    // ══════════════════════════════════════════════════════════
    //  USER: REGISTER
    //  POST /api/user/register
    //  Body: { "username": "rohan", "password": "abc123" }
    // ══════════════════════════════════════════════════════════
    CROW_ROUTE(app, "/api/user/register")
    .methods(crow::HTTPMethod::POST)
    ([&userStore](const crow::request& req) {
        auto body = json::parse(req.body, nullptr, false);
        if (body.is_discarded()) return errResp(400, "Invalid JSON");

        std::string username = body.value("username", "");
        std::string password = body.value("password", "");

        if (username.empty() || password.empty())
            return errResp(400, "Username and password required");
        if (username.length() < 3)
            return errResp(400, "Username must be at least 3 characters");
        if (password.length() < 4)
            return errResp(400, "Password must be at least 4 characters");

        // C++ call: creates User, hashes password, writes users.json
        if (!userStore.registerUser(username, password))
            return errResp(409, "Username already taken");

        return okResp({{"username", username},
                       {"message",  "Account created!"}});
    });

    // ══════════════════════════════════════════════════════════
    //  USER: LOGIN
    //  POST /api/user/login
    //  Body: { "username": "rohan", "password": "abc123" }
    // ══════════════════════════════════════════════════════════
    CROW_ROUTE(app, "/api/user/login")
    .methods(crow::HTTPMethod::POST)
    ([&](const crow::request& req) {
        auto body = json::parse(req.body, nullptr, false);
        if (body.is_discarded()) return errResp(400, "Invalid JSON");

        std::string username = body.value("username", "");
        std::string password = body.value("password", "");

        // C++ call: hash attempt, compare to stored hash
        User* user = userStore.login(username, password);
        if (!user) return errResp(401, "Incorrect username or password");

        std::string levelStr = user->isNewUser() ? "A1" : user->getLevelStr();

        // Factory: creates right subclass (A1Learner, A2Learner...)
        GermanLearner* learner = sessionManager.createSession(username, levelStr);
        learner->initFromUser(*user);

        // Copy all lesson data into learner's GermanLanguage parent
        for (const std::string& lvl : {"A1","A2","B1","B2"})
            learner->loadLessons(lvl, lessonManager.getLessonsForLevel(lvl));
        learner->syncProgressLessonIds();

        // Load saved progress from disk into ProgressMixin
        progressStore.load(username, *learner);

        return okResp({
            {"username",  username},
            {"level",     levelStr},
            {"xp",        user->getXP()},
            {"isNewUser", user->isNewUser()},
            {"levelName", learner->getLevelName()},
            {"welcome",   learner->getWelcomeMessage()}
        });
    });

    // ══════════════════════════════════════════════════════════
    //  USER: LOGOUT
    //  POST /api/user/logout
    //  Body: { "username": "rohan" }
    //  Saves progress to disk, then frees the in-memory session.
    // ══════════════════════════════════════════════════════════
    CROW_ROUTE(app, "/api/user/logout")
    .methods(crow::HTTPMethod::POST)
    ([&](const crow::request& req) {
        auto body = json::parse(req.body, nullptr, false);
        if (body.is_discarded()) return errResp(400, "Invalid JSON");

        std::string username = body.value("username", "");
        if (username.empty()) return errResp(400, "Username required");

        GermanLearner* learner = sessionManager.getSession(username);
        if (learner) {
            progressStore.save(username, *learner);
            sessionManager.removeSession(username);
        }

        return okResp({{"message", "Logged out successfully"}});
    });

    // ══════════════════════════════════════════════════════════
    //  PLACEMENT TEST: START
    //  POST /api/placement/start
    //  Body: { "username": "rohan" }
    // ══════════════════════════════════════════════════════════
    CROW_ROUTE(app, "/api/placement/start")
    .methods(crow::HTTPMethod::POST)
    ([&sessionManager](const crow::request& req) {
        auto body = json::parse(req.body, nullptr, false);
        std::string username = body.value("username", "");

        GermanLearner* learner = sessionManager.getSession(username);
        if (!learner) return errResp(404, "Session not found. Please log in.");

        // Collects words from all levels → PlacementMixin::buildPlacementTest()
        learner->startPlacement();

        json q = learner->placementCurrentJson();
        return okResp({
            {"question",    q},
            {"instruction", "Translate the German word to English"}
        });
    });

    // ══════════════════════════════════════════════════════════
    //  PLACEMENT TEST: ANSWER
    //  POST /api/placement/answer
    //  Body: { "username": "rohan", "answer": "Hello" }
    // ══════════════════════════════════════════════════════════
    CROW_ROUTE(app, "/api/placement/answer")
    .methods(crow::HTTPMethod::POST)
    ([&](const crow::request& req) {
        auto body = json::parse(req.body, nullptr, false);
        std::string username = body.value("username", "");
        std::string answer   = body.value("answer",   "");

        GermanLearner* learner = sessionManager.getSession(username);
        if (!learner) return errResp(404, "Session not found. Please log in.");

        bool correct = learner->evaluatePlacement(answer);

        if (learner->placementDone()) {
            std::string level = learner->applyPlacementResult();

            User* user = userStore.getUser(username);
            if (user) { user->setLevel(level); userStore.saveToDisk(); }
            progressStore.save(username, *learner);

            return okResp({
                {"placementDone", true},
                {"correct",       correct},
                {"score",         learner->getPlacementScore()},
                {"total",         learner->getPlacementTotal()},
                {"assignedLevel", level},
                {"levelName",     learner->getLevelName()},
                {"welcome",       learner->getWelcomeMessage()}
            });
        }

        json next = learner->placementCurrentJson();
        return okResp({
            {"placementDone", false},
            {"correct",       correct},
            {"nextQuestion",  next}
        });
    });

    // ══════════════════════════════════════════════════════════
    //  LEARNING: GET LESSON LIST FOR A LEVEL
    //  GET /api/lessons/A1?user=rohan
    // ══════════════════════════════════════════════════════════
    CROW_ROUTE(app, "/api/lessons/<string>")
    ([&](const crow::request& req, const std::string& level) {
        std::string username = req.url_params.get("user") ?
                               req.url_params.get("user") : "";

        json summary = lessonManager.getLevelSummary(level);

        GermanLearner* learner = sessionManager.getSession(username);
        if (learner) {
            for (auto& lesson : summary) {
                std::string id = lesson["id"].get<std::string>();
                lesson["passed"]    = learner->hasPassedLesson(id);
                lesson["attempted"] = learner->hasAttemptedLesson(id);
                lesson["unlocked"]  = learner->canAccessLevel(level);
            }
        }

        return okResp({{"level", level}, {"lessons", summary}});
    });

    // ══════════════════════════════════════════════════════════
    //  LEARNING: GET ONE FULL LESSON (for flashcard screen)
    //  GET /api/lesson/a1_colors?user=rohan
    // ══════════════════════════════════════════════════════════
    CROW_ROUTE(app, "/api/lesson/<string>")
    ([&sessionManager](const crow::request& req,
                        const std::string& lessonId) {
        std::string username = req.url_params.get("user") ?
                               req.url_params.get("user") : "";

        GermanLearner* learner = sessionManager.getSession(username);
        if (!learner) return errResp(404, "Session not found. Please log in.");

        const Lesson* lesson = learner->getLessonById(lessonId);
        if (!lesson) return errResp(404, "Lesson not found: " + lessonId);

        return okResp({{"lesson", lesson->toJson()}});
    });

    // ══════════════════════════════════════════════════════════
    //  LEARNING: GET TOPICS (polymorphic — subclass override)
    //  GET /api/topics/A1?user=rohan
    // ══════════════════════════════════════════════════════════
    CROW_ROUTE(app, "/api/topics/<string>")
    ([&sessionManager](const crow::request& req,
                        const std::string& level) {
        std::string username = req.url_params.get("user") ?
                               req.url_params.get("user") : "";

        GermanLearner* learner = sessionManager.getSession(username);
        if (!learner) return errResp(404, "Session not found.");

        // Use a temporary learner for the requested level so topics,
        // levelName and welcome reflect the TAB level, not the user's
        // session level (which always returns the user's placed level).
        auto tempLearner = makeLearner(level);
        json topics = json::array();
        for (const auto& t : tempLearner->getTopics()) topics.push_back(t);

        return okResp({
            {"level",     level},
            {"levelName", tempLearner->getLevelName()},
            {"topics",    topics},
            {"welcome",   tempLearner->getWelcomeMessage()}
        });
    });

    // ══════════════════════════════════════════════════════════
    //  QUIZ: START
    //  POST /api/quiz/start
    //  Body: { "username": "rohan", "lessonId": "a1_colors" }
    // ══════════════════════════════════════════════════════════
    CROW_ROUTE(app, "/api/quiz/start")
    .methods(crow::HTTPMethod::POST)
    ([&sessionManager](const crow::request& req) {
        auto body = json::parse(req.body, nullptr, false);
        if (body.is_discarded()) return errResp(400, "Invalid JSON");

        std::string username = body.value("username", "");
        std::string lessonId = body.value("lessonId", "");

        GermanLearner* learner = sessionManager.getSession(username);
        if (!learner) return errResp(404, "Session not found. Please log in.");

        // GermanLanguage::getLessonById() → QuizMixin::buildQuiz()
        if (!learner->startLessonQuiz(lessonId))
            return errResp(404, "Lesson not found: " + lessonId);

        const WordEntry* q = learner->currentQuestion();
        if (!q) return errResp(500, "Quiz has no questions");

        return okResp({
            {"question",    {{"german", q->german}, {"roman", q->roman}}},
            {"questionNum", 1},
            {"total",       learner->getTotalQuestions()},
            {"xp",          learner->getXP()},
            {"lessonId",    lessonId}
        });
    });

    // ══════════════════════════════════════════════════════════
    //  QUIZ: ANSWER
    //  POST /api/quiz/answer
    //  Body: { "username": "rohan", "answer": "red" }
    // ══════════════════════════════════════════════════════════
    CROW_ROUTE(app, "/api/quiz/answer")
    .methods(crow::HTTPMethod::POST)
    ([&sessionManager](const crow::request& req) {
        auto body = json::parse(req.body, nullptr, false);
        if (body.is_discarded()) return errResp(400, "Invalid JSON");

        std::string username = body.value("username", "");
        std::string answer   = body.value("answer",   "");

        GermanLearner* learner = sessionManager.getSession(username);
        if (!learner) return errResp(404, "Session not found. Please log in.");

        // THE key multi-inheritance call:
        // QuizMixin::evaluate() + User::addXP() in one method on one object
        QuizResult result = learner->submitQuizAnswer(answer);

        json resp;
        resp["correct"]       = result.correct;
        resp["correctAnswer"] = result.correctAnswer;
        resp["xpAwarded"]     = result.xpAwarded;
        resp["totalXP"]       = learner->getXP();
        resp["questionNum"]   = result.questionNum;
        resp["total"]         = result.totalQuestions;

        if (learner->quizDone()) {
            resp["finished"] = true;
            resp["results"]  = learner->buildResultsJson(learner->getXP());
        } else {
            resp["finished"] = false;
            const WordEntry* next = learner->currentQuestion();
            if (next)
                resp["nextQuestion"] = {
                    {"german", next->german},
                    {"roman",  next->roman}
                };
        }

        return okResp(resp);
    });

    // ══════════════════════════════════════════════════════════
    //  QUIZ: FINISH (save progress)
    //  POST /api/quiz/finish
    //  Body: { "username": "rohan", "lessonId": "a1_colors" }
    // ══════════════════════════════════════════════════════════
    CROW_ROUTE(app, "/api/quiz/finish")
    .methods(crow::HTTPMethod::POST)
    ([&](const crow::request& req) {
        auto body = json::parse(req.body, nullptr, false);
        std::string username  = body.value("username",  "");
        std::string lessonId  = body.value("lessonId",  "");
        int         timeSpent = body.value("timeSpent", 0);

        GermanLearner* learner = sessionManager.getSession(username);
        if (!learner) return errResp(404, "Session not found.");

        // ProgressMixin::recordQuizResult() + level unlock check
        learner->finishQuiz(lessonId);
        learner->addTimeSpent(timeSpent);

        // Save to disk
        userStore.saveToDisk();
        progressStore.save(username, *learner);

        return okResp({
            {"saved",            true},
            {"totalXP",          learner->getTotalXP()},
            {"streak",           learner->getStreak()},
            {"unlockedLevel",    learner->getUnlockedLevel()},
            {"passedLesson",     learner->hasPassedLesson(lessonId)},
            {"timeSpentSeconds", learner->getTimeSpentSeconds()}
        });
    });

    // ══════════════════════════════════════════════════════════
    //  QUIZ: RETAKE
    //  POST /api/quiz/retake
    //  Body: { "username": "rohan", "lessonId": "a1_colors" }
    // ══════════════════════════════════════════════════════════
    CROW_ROUTE(app, "/api/quiz/retake")
    .methods(crow::HTTPMethod::POST)
    ([&sessionManager](const crow::request& req) {
        auto body = json::parse(req.body, nullptr, false);
        std::string username = body.value("username", "");
        std::string lessonId = body.value("lessonId", "");

        GermanLearner* learner = sessionManager.getSession(username);
        if (!learner) return errResp(404, "Session not found.");

        if (!learner->startLessonQuiz(lessonId))
            return errResp(404, "Lesson not found.");

        const WordEntry* q = learner->currentQuestion();
        return okResp({
            {"question",    {{"german", q->german}, {"roman", q->roman}}},
            {"questionNum", 1},
            {"total",       learner->getTotalQuestions()},
            {"xp",          learner->getXP()}
        });
    });

    // ══════════════════════════════════════════════════════════
    //  PROGRESS: GET
    //  GET /api/progress/rohan
    // ══════════════════════════════════════════════════════════
    CROW_ROUTE(app, "/api/progress/<string>")
    ([&sessionManager](const crow::request& /*req*/, const std::string& username) {
        GermanLearner* learner = sessionManager.getSession(username);
        if (!learner) return errResp(404, "Session not found.");

        return okResp({
            {"username",      username},
            {"totalXP",       learner->getTotalXP()},
            {"streak",        learner->getStreak()},
            {"unlockedLevel", learner->getUnlockedLevel()},
            {"progress",      learner->toProgressJson()}
        });
    });

    // ══════════════════════════════════════════════════════════
    //  LEADERBOARD
    //  GET /api/leaderboard
    //  Returns top 10 users sorted by XP descending.
    // ══════════════════════════════════════════════════════════
    CROW_ROUTE(app, "/api/leaderboard")
    ([&userStore](const crow::request& /*req*/) {
        // std::sort with custom lambda comparator — O(n log n)
        std::vector<User> all = userStore.getAllUsers();
        std::sort(all.begin(), all.end(), [](const User& a, const User& b) {
            return a.getXP() > b.getXP();
        });

        json board = json::array();
        int limit = std::min((int)all.size(), 10);
        for (int i = 0; i < limit; i++) {
            board.push_back({
                {"rank",     i + 1},
                {"username", all[i].getName()},
                {"xp",       all[i].getXP()},
                {"level",    all[i].getLevelStr()}
            });
        }

        return okResp({{"leaderboard", board}});
    });

    // ── CORS preflight ────────────────────────────────────────
    CROW_ROUTE(app, "/api/<path>")
    .methods(crow::HTTPMethod::OPTIONS)
    ([](const crow::request&, const std::string&) {
        crow::response res(204);
        res.set_header("Access-Control-Allow-Origin",  "*");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        return res;
    });

    std::cout << "\n╔══════════════════════════════════════╗\n";
    std::cout <<   "  Sprachmeister C++ Server\n";
    std::cout <<   "  Open: http://localhost:18080\n";
    std::cout <<   "╚══════════════════════════════════════╝\n\n";

    app.port(18080).multithreaded().run();
    return 0;
}
