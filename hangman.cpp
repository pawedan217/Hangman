#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <random>
#include <chrono>
#include <map>
#include <limits>
#include <cctype>

using namespace std;

struct WordEntry {
    string category;
    string word;
    string difficulty;
};

// Helpers
string trim(const string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    size_t last = str.find_last_not_of(" \t\r\n");
    if (first == string::npos || last == string::npos) return "";
    return str.substr(first, (last - first + 1));
}

string toLower(const string& s) {
    string out = s;
    for (char& c : out) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    return out;
}

void flushLine() {
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

// Load Words
vector<WordEntry> loadWords(const string& filename) {
    vector<WordEntry> words;
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error: Could not open " << filename << endl;
        return words;
    }

    string line;
    getline(file, line); // Skip header

    while (getline(file, line)) {
        stringstream ss(line);
        string category, word, difficulty;
        getline(ss, category, ',');
        getline(ss, word, ',');
        getline(ss, difficulty, ',');

        category = trim(category);
        word = trim(word);
        difficulty = trim(difficulty);
        replace(word.begin(), word.end(), '_', ' ');

        if (!word.empty()) {
            WordEntry entry{ category, word, toLower(difficulty) };
            words.push_back(entry);
        }
    }
    file.close();
    return words;
}

// Hangman art
static const vector<string> HANGMAN_STATES = {
    R"( +---+
 |   |
     |
     |
     |
     |
=========)",
    R"( +---+
 |   |
 O   |
     |
     |
     |
=========)",
    R"( +---+
 |   |
 O   |
 |   |
     |
     |
=========)",
    R"( +---+
 |   |
 O   |
/|   |
     |
     |
=========)",
    R"( +---+
 |   |
 O   |
/|\  |
     |
     |
=========)",
    R"( +---+
 |   |
 O   |
/|\  |
/    |
     |
=========)",
    R"( +---+
 |   |
 O   |
/|\  |
/ \  |
     |
=========)"
};

// Leaderboard System
struct ScoreRow {
    string name;
    int score = 0;
    string mode;
    string dateStr;
};

string nowString() {
    time_t t = time(nullptr);
    tm tmval{};
#if defined(_WIN32)
    localtime_s(&tmval, &t);
#else
    tmval = *localtime(&t);
#endif
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tmval);
    return buf;
}

bool appendScore(const ScoreRow& row) {
    ofstream lb("leaderboard.txt", ios::app);
    if (!lb) return false;
    lb << row.name << "|" << row.score << "|" << row.mode << "|" << nowString() << "\n";
    return true;
}

vector<ScoreRow> readLeaderboard() {
    vector<ScoreRow> rows;
    ifstream lb("leaderboard.txt");
    if (!lb) return rows;
    string line;
    while (getline(lb, line)) {
        if (line.empty()) continue;
        stringstream ss(line);
        ScoreRow r;
        string scoreStr;
        getline(ss, r.name, '|');
        getline(ss, scoreStr, '|');
        getline(ss, r.mode, '|');
        getline(ss, r.dateStr);
        r.name = trim(r.name);
        r.mode = trim(r.mode);
        r.dateStr = trim(r.dateStr);
        try { r.score = stoi(scoreStr); }
        catch (...) { r.score = 0; }
        rows.push_back(r);
    }
    return rows;
}

bool exportLeaderboard(const vector<pair<string, int>>& sorted, const string& filename) {
    ofstream fout(filename);
    if (!fout) return false;
    fout << "===== Hangman Leaderboard Export =====\n";
    fout << "Generated: " << nowString() << "\n\n";
    for (size_t i = 0; i < sorted.size() && i < 10; ++i) {
        fout << (i + 1) << ". " << sorted[i].first << " - " << sorted[i].second << " pts\n";
    }
    fout << "======================================\n";
    return true;
}

void showLeaderboard() {
    auto rows = readLeaderboard();
    vector<pair<string, int>> display;
    for (auto& r : rows) {
        display.emplace_back(r.name + " (" + r.mode + ", " + r.dateStr + ")", r.score);
    }
    sort(display.begin(), display.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
        });

    cout << "\nLeaderboard (Top 10 Highest Scores)\n";
    cout << "========================================\n";
    if (display.empty()) {
        cout << "No scores recorded yet!\n";
    }
    else {
        for (size_t i = 0; i < display.size() && i < 10; ++i) {
            cout << i + 1 << ". " << display[i].first << " - " << display[i].second << " pts\n";
        }
    }
    cout << "========================================\n\n";

    // Interactive Save / Back
    while (true) {
        cout << "[S] Save this leaderboard to file | [B] Back to menu\n";
        cout << "Choose (S/B): ";
        string choice;
        getline(cin, choice);
        if (choice.empty()) continue;
        char c = tolower(choice[0]);
        if (c == 'b') {
            cout << "\n";
            return;
        }
        if (c == 's') {
            string filename = "my_leaderboard.txt";
            cout << "Enter filename (blank = '" << filename << "'): ";
            string input;
            getline(cin, input);
            if (!trim(input).empty()) filename = trim(input);
            if (exportLeaderboard(display, filename)) {
                cout << "Leaderboard saved to '" << filename << "'!\n\n";
            }
            else {
                cerr << "Error: Could not save file.\n\n";
            }
            break;
        }
        cout << "Please type S or B.\n";
    }
}

// Game Logic
int mistakesAllowed(const string& diff) {
    if (diff == "easy") return 7;
    if (diff == "hard" || diff == "expert") return 5;
    return 6;
}

int basePoints(const string& diff) {
    if (diff == "easy") return 50;
    if (diff == "hard" || diff == "expert") return 120;
    return 80;
}

void drawHangman(int mistakes, int maxMistakes, const string& used, const string& masked, int remaining, int timeLeft = -1) {
    int idx = min(mistakes, (int)HANGMAN_STATES.size() - 1);
    cout << HANGMAN_STATES[idx] << "\n\n";
    cout << "Word: " << masked << "  (Letters left: " << remaining << ")\n";
    cout << "Misses: " << mistakes << " / " << maxMistakes << "\n";
    if (timeLeft >= 0) cout << "Time left: " << timeLeft << "s\n";
    cout << "Used: " << (used.empty() ? "(none)" : used) << "\n\n";
}

// Main Game Loop
int main() {
    vector<WordEntry> words = loadWords("FinalWordBank.csv");
    if (words.empty()) {
        cout << "No words loaded. Check 'FinalWordBank.csv'.\n";
        return 1;
    }

    random_device rd;
    mt19937 gen(rd());

    while (true) {
        cout << "==============================\n";
        cout << "     HANGMAN GAME\n";
        cout << "==============================\n";
        cout << "1. Regular Hangman\n";
        cout << "2. Timed Mode (60s)\n";
        cout << "3. Leaderboard\n";
        cout << "4. Quit\n";
        cout << "Choose: ";
        int mode_select;
        if (!(cin >> mode_select)) {
            cin.clear();
            flushLine();
            mode_select = 4;
        }
        flushLine();

        if (mode_select == 4) {
			cout << "\n";
            cout << "Goodbye!\n";
            break;
        }
        if (mode_select == 3) {
            showLeaderboard();
            continue;
        }
        if (mode_select != 1 && mode_select != 2) {
            cout << "Invalid choice.\n";
            continue;
        }

        // Category & Difficulty
        string chosenCategory, chosenDifficulty;
        while (true) {
            cout << "Choose category (Pet, Place, Restaurant): ";
            getline(cin, chosenCategory);
            if (toLower(chosenCategory) == "pet" || toLower(chosenCategory) == "place" || toLower(chosenCategory) == "restaurant") break;
            cout << "Invalid. Try again.\n";
        }
        while (true) {
            cout << "Choose difficulty (Easy, Medium, Hard, Expert): ";
            getline(cin, chosenDifficulty);
            string d = toLower(chosenDifficulty);
            if (d == "easy" || d == "medium" || d == "hard" || d == "expert") break;
            cout << "Invalid. Try again.\n";
        }

        // Filter & pick word
        vector<WordEntry> filtered;
        for (const auto& w : words) {
            if (toLower(w.category) == toLower(chosenCategory) && toLower(w.difficulty) == toLower(chosenDifficulty)) {
                filtered.push_back(w);
            }
        }
        if (filtered.empty()) {
            cout << "No words found for that category/difficulty.\n";
            continue;
        }

        uniform_int_distribution<size_t> dist(0, filtered.size() - 1);
        WordEntry chosen = filtered[dist(gen)];
        string word = chosen.word;
        string wordLower = toLower(word);

        vector<bool> revealed(word.size(), false);
        for (size_t i = 0; i < word.size(); ++i)
            if (!isalpha(static_cast<unsigned char>(word[i]))) revealed[i] = true;

        int remainingLetters = 0;
        for (char c : wordLower) if (isalpha(static_cast<unsigned char>(c))) ++remainingLetters;

        int maxMistakes = mistakesAllowed(chosen.difficulty);
        int mistakes = 0;
        map<char, bool> used;

        auto startTime = chrono::steady_clock::now();

        while (mistakes < maxMistakes && remainingLetters > 0) {
            string masked = "";
            for (size_t i = 0; i < word.size(); ++i)
                masked += revealed[i] ? word[i] : '_';

            int timeLeft = -1;
            if (mode_select == 2) {
                auto elapsed = chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - startTime).count();
                timeLeft = max(0, 60 - (int)elapsed);
                if (timeLeft == 0) break;
            }

            string usedStr;
            for (auto& p : used) if (p.second) { usedStr += p.first; usedStr += ' '; }

            drawHangman(mistakes, maxMistakes, usedStr, masked, remainingLetters, timeLeft);

            cout << "Guess a letter or '!' for full word: ";
            string input;
            getline(cin, input);
            if (input.empty()) continue;

            if (input == "!") {
                cout << "Enter full word: ";
                string guess;
                getline(cin, guess);
                if (toLower(trim(guess)) == wordLower) {
                    fill(revealed.begin(), revealed.end(), true);
                    remainingLetters = 0;
                    break;
                }
                else {
                    cout << "Incorrect!\n";
                    mistakes++;
                    continue;
                }
            }

            char g = tolower(input[0]);
            if (!isalpha(g)) {
                cout << "Please guess a letter.\n";
                continue;
            }
            if (used[g]) {
                cout << "Already guessed '" << g << "'.\n";
                continue;
            }
            used[g] = true;

            bool hit = false;
            for (size_t i = 0; i < wordLower.size(); ++i) {
                if (wordLower[i] == g && !revealed[i]) {
                    revealed[i] = true;
                    --remainingLetters;
                    hit = true;
                }
            }
            if (!hit) {
                cout << "Not in word.\n";
                mistakes++;
            }
            else {
                cout << "Good guess!\n";
            }
        }

        // Final screen
        string finalMasked = word;
        for (size_t i = 0; i < word.size(); ++i)
            if (!revealed[i] && isalpha(word[i])) finalMasked[i] = '_';

        string usedStr;
        for (auto& p : used) if (p.second) { usedStr += p.first; usedStr += ' '; }
        int finalTimeLeft = -1;
        if (mode_select == 2) {
            auto elapsed = chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - startTime).count();
            finalTimeLeft = max(0, 60 - (int)elapsed);
        }
        drawHangman(mistakes, maxMistakes, usedStr, finalMasked, remainingLetters, finalTimeLeft);

        int score = 0;
        bool won = (remainingLetters == 0);

        if (won) {
            cout << "You WIN! The word was: " << word << "\n";
            score = basePoints(chosen.difficulty) + (maxMistakes - mistakes) * 10;
            if (mode_select == 2) score += finalTimeLeft * 2;
        }
        else {
            cout << "Game Over! The word was: " << word << "\n";
            score = max(0, basePoints(chosen.difficulty) / 4 - mistakes * 5);
        }
        cout << "Score: " << score << " points\n\n";

        // Save score
        cout << "Save score to leaderboard? (y/n): ";
        string yn;
        getline(cin, yn);
        if (!yn.empty() && tolower(yn[0]) == 'y') {
            string name;
            cout << "Enter your name: ";
            getline(cin, name);
            if (name.empty()) name = "Player";
            ScoreRow row{ name, score, (mode_select == 2 ? "Timed" : "Regular") };
            if (appendScore(row)) cout << "Score saved!\n\n";
            else cerr << "Failed to save.\n\n";
        }
    }

    return 0;
}
