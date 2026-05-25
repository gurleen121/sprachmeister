# Sprachmeister

A German language learning web app built in **C++17**, demonstrating advanced OOP concepts through a fully functional browser-based quiz and progress system.

![C++17](https://img.shields.io/badge/C%2B%2B-17-blue?logo=cplusplus&logoColor=white)
![Crow](https://img.shields.io/badge/Crow-HTTP%20Framework-darkgreen)
![nlohmann/json](https://img.shields.io/badge/nlohmann%2Fjson-3.x-orange)
![Platform](https://img.shields.io/badge/Platform-Linux-yellow?logo=linux&logoColor=white)
![License](https://img.shields.io/badge/License-MIT-brightgreen)

---

## Features

- **Placement test** — 12-question diagnostic across A1–B2 levels; sets your starting point automatically
- **Lesson quizzes** — vocabulary quizzes per lesson with XP rewards and pass/fail tracking (≥70% to pass)
- **Level progression** — A1 → A2 → B1 → B2 unlocks automatically when all lessons in the current level are passed
- **Streak tracking** — calendar-based consecutive-day streaks using `<chrono>`
- **Leaderboard** — top 10 users ranked by XP
- **Persistent progress** — one JSON file per user saved to `data/progress/`
- **Multi-user** — thread-safe session management supports concurrent users

---

## C++ Concepts Demonstrated

| Concept | Where |
|---|---|
| **Multiple Inheritance** | `GermanLearner` inherits from 5 parent classes simultaneously |
| **Virtual Inheritance** | `public virtual User` solves the diamond problem — one shared `User` copy |
| **Runtime Polymorphism** | `A1Learner`–`B2Learner` override `getWelcomeMessage()`, `getTopics()` |
| **Factory Pattern** | `makeLearner(level)` returns correct subclass as `unique_ptr<GermanLearner>` |
| **Mixin Pattern** | `QuizMixin`, `PlacementMixin`, `ProgressMixin` — trait classes added via inheritance |
| **RAII Locking** | `std::lock_guard<std::mutex>` in `SessionManager` — auto-releases even on exception |
| **`mutable` keyword** | Allows mutex locking inside `const` methods in `SessionManager` |
| **Operator Overloading** | `<`, `>`, `<=`, `>=` on `enum class Level` via `levelToInt()` helper |
| **`enum class`** | Scoped `Level` enum — compiler catches typos, no accidental string bugs |
| **`std::unique_ptr`** | Ownership semantics for learner objects in `SessionManager` |
| **Structured Bindings** | `for (const auto& [name, user] : users)` throughout |
| **`std::sort` + lambda** | Leaderboard sorted by XP descending in `/api/leaderboard` route |
| **`std::chrono`** | Real calendar-date streak calculation in `ProgressMixin` |
| **Fisher-Yates Shuffle** | Randomised quiz and placement question order in `QuizMixin` |
| **Single Responsibility** | Each header owns exactly one domain (users, lessons, quizzes, progress) |

---

## Project Structure

```
sprachmeister/
├── src/
│   └── main.cpp              # HTTP server, all API routes (Crow framework)
├── include/
│   ├── user_module.h         # User, UserStore, Level enum + operators
│   ├── learning_module.h     # WordEntry, Lesson, GermanLanguage, LessonManager
│   ├── assessment_module.h   # QuizMixin, PlacementMixin, QuizResult
│   ├── progress_module.h     # ProgressMixin, ProgressStore, LessonProgress
│   ├── classes.h             # GermanLearner (multi-inheritance), level subclasses, SessionManager
│   ├── crow.h                # Crow HTTP framework (single header)
│   └── nlohmann/
│       └── json.hpp          # nlohmann JSON library (single header)
├── public/
│   └── index.html            # Single-page frontend (vanilla JS, no frameworks)
├── data/
│   └── lessons/
│       ├── a1_lesson.json
│       ├── a2_lesson.json
│       ├── b1_lesson.json
│       └── b2_lesson.json
├── Makefile
└── setup.sh
```

---

## Prerequisites

```bash
sudo apt install g++ libssl-dev
```

The following single-header libraries are included in `include/`:
- **Crow** — C++ HTTP framework (`include/crow.h`)
- **nlohmann/json** — JSON parsing (`include/nlohmann/json.hpp`)

---

## Build & Run

```bash
# Build
make

# Run
./sprachmeister
```

Then open your browser at: **http://localhost:18080**

To rebuild from scratch:
```bash
make clean && make
```

---

## API Routes

| Method | Route | Description |
|---|---|---|
| `POST` | `/api/user/register` | Register new user |
| `POST` | `/api/user/login` | Login + restore progress |
| `POST` | `/api/user/logout` | Save progress + end session |
| `POST` | `/api/placement/start` | Begin placement test |
| `POST` | `/api/placement/answer` | Submit placement answer |
| `POST` | `/api/placement/finish` | Get result + set level |
| `GET` | `/api/lessons` | List lessons for user's level |
| `POST` | `/api/quiz/start` | Start a lesson quiz |
| `POST` | `/api/quiz/answer` | Submit quiz answer |
| `POST` | `/api/quiz/finish` | Save quiz result to progress |
| `GET` | `/api/progress` | Get user's full progress |
| `GET` | `/api/leaderboard` | Top 10 users by XP |

---

## Inheritance Diagram

```
         User  (virtual base — one shared copy)
          |
    ┌─────┴──────────────────────────────────┐
    │         GermanLearner                  │
    │  (also inherits GermanLanguage,        │
    │   QuizMixin, PlacementMixin,           │
    │   ProgressMixin)                       │
    └─────┬───────────────────────────────── ┘
          │
    ┌─────┼─────┬──────┐
  A1Learner  A2Learner  B1Learner  B2Learner
  (override getWelcomeMessage, getTopics, getLevelName)
```

---

## Credits

- **[Crow](https://github.com/CrowCpp/Crow)** — C++ HTTP framework by CrowCpp, used for the REST API server. Licensed under BSD 3-Clause.
- **[nlohmann/json](https://github.com/nlohmann/json)** — Single-header JSON library by Niels Lohmann, used for all JSON serialization and parsing. Licensed under MIT.

---

## License

This project is licensed under the **MIT License** — see the [LICENSE](LICENSE) file for details.

---

## Author

Gurleen Kaur
