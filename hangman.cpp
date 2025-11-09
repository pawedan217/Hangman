#include <iostream>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <map>
#include <limits>
#include <cctype>
#include <fstream>   // for ifstream/ofstream
#include <sstream>   // for stringstream

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
    for (size_t i = 0; i < out.size(); ++i) {
        out[i] = static_cast<char>(tolower(static_cast<unsigned char>(out[i])));
    }
    return out;
}

void flushLine() {
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

// Load Words
vector<WordEntry> loadWords(const string& filename) {
    vector<WordEntry> words;
    ifstream file(filename.c_str());
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
            WordEntry entry;
            entry.category = category;
            entry.word = word;
            entry.difficulty = toLower(difficulty);
            words.push_back(entry);
        }
    }
    file.close();
    return words;
}

// Hangman art (no raw string literals; use escaped newlines)
static const char* HANGMAN_ARRAY[] = {
    " +---+\n |   |\n     |\n     |\n     |\n     |\n=========",
    " +---+\n |   |\n O   |\n     |\n     |\n     |\n=========",
    " +---+\n |   |\n O   |\n |   |\n     |\n     |\n=========",
    " +---+\n |   |\n O   |\n/|   |\n     |\n     |\n=========",
    " +---+\n |   |\n O   |\n/|\\  |\n     |\n     |\n=========",
    " +---+\n |   |\n O   |\n/|\\  |\n/    |\n     |\n=========",
    " +---+\n |   |\n O   |\n/|\\  |\n/ \\  |\n     |\n========="
};

static const vector<string> HANGMAN_STATES(
    HANGMAN_ARRAY,
    HANGMAN_ARRAY + sizeof(HANGMAN_ARRAY)/sizeof(HANGMAN_ARRAY[0])
);


// Leaderboard System
struct ScoreRow {
    string name;
    int score;
    string mode;
    string dateStr;
    ScoreRow() : score(0) {}
};

string nowString() {
    time_t t = time(NULL);
    tm tmval;
#if defined(_WIN32)
    localtime_s(&tmval, &t);
#else
    tm* ptm = localtime(&t);
    if (ptm) tmval = *ptm;
    else memset(&tmval, 0, sizeof(tmval));
#endif
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tmval);
    return string(buf);
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
        // stoi is C++11; use stringstream
        stringstream sscore(scoreStr);
        sscore >> r.score;
        if (!sscore) r.score = 0;
        rows.push_back(r);
    }
    return rows;
}

bool exportLeaderboard(const vector< pair<string, int> >& sorted, const string& filename) {
    ofstream fout(filename.c_str());
    if (!fout) return false;
    fout << "===== Hangman Leaderboard Export =====\n";
    fout << "Generated: " << nowString() << "\n\n";
    for (size_t i = 0; i < sorted.size() && i < 10; ++i) {
        fout << (i + 1) << ". " << sorted[i].first << " - " << sorted[i].second << " pts\n";
    }
    fout << "======================================\n";
    return true;
}

struct ScoreSort {
    bool operator()(const pair<string,int>& a, const pair<string,int>& b) const {
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
    }
};

void showLeaderboard() {
    vector<ScoreRow> rows = readLeaderboard();
    vector< pair<string, int> > display;
    for (size_t i = 0; i < rows.size(); ++i) {
        const ScoreRow& r = rows[i];
        display.push_back(make_pair(r.name + " (" + r.mode + ", " + r.dateStr + ")", r.score));
    }
    sort(display.begin(), display.end(), ScoreSort());

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
        char c = static_cast<char>(tolower(static_cast<unsigned char>(choice[0])));
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

void drawHangman(int mistakes, int maxMistakes, const string& used, const string& masked, int remaining, int timeLeft) {
    int idx = mistakes;
    if (idx < 0) idx = 0;
    if (idx >= (int)HANGMAN_STATES.size()) idx = (int)HANGMAN_STATES.size() - 1;
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

    // Legacy RNG
    std::srand((unsigned int)std::time(NULL));

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
            string lc = toLower(chosenCategory);
            if (lc == "pet" || lc == "place" || lc == "restaurant") break;
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
        for (size_t i = 0; i < words.size(); ++i) {
            const WordEntry& w = words[i];
            if (toLower(w.category) == toLower(chosenCategory) && toLower(w.difficulty) == toLower(chosenDifficulty)) {
                filtered.push_back(w);
            }
        }
        if (filtered.empty()) {
            cout << "No words found for that category/difficulty.\n";
            continue;
        }

        size_t idx = (size_t)(std::rand() % filtered.size());
        WordEntry chosen = filtered[idx];
        string word = chosen.word;
        string wordLower = toLower(word);

        vector<bool> revealed(word.size(), false);
        for (size_t i = 0; i < word.size(); ++i) {
            if (!isalpha(static_cast<unsigned char>(word[i]))) revealed[i] = true;
        }

        int remainingLetters = 0;
        for (size_t i = 0; i < wordLower.size(); ++i) {
            char c = wordLower[i];
            if (isalpha(static_cast<unsigned char>(c))) ++remainingLetters;
        }

        int maxMistakes = mistakesAllowed(chosen.difficulty);
        int mistakes = 0;
        map<char, bool> used;

        // Timed mode using time()
        time_t startTime = time(NULL);

        while (mistakes < maxMistakes && remainingLetters > 0) {
            string masked = "";
            for (size_t i = 0; i < word.size(); ++i)
                masked += revealed[i] ? word[i] : '_';

            int timeLeft = -1;
            if (mode_select == 2) {
                time_t now = time(NULL);
                int elapsed = (int)difftime(now, startTime);
                timeLeft = (elapsed < 60) ? (60 - elapsed) : 0;
                if (timeLeft == 0) break;
            }

            string usedStr;
            for (map<char,bool>::iterator it = used.begin(); it != used.end(); ++it) {
                if (it->second) {
                    usedStr += it->first;
                    usedStr += ' ';
                }
            }

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
                    for (size_t i = 0; i < revealed.size(); ++i) revealed[i] = true;
                    remainingLetters = 0;
                    break;
                }
                else {
                    cout << "Incorrect!\n";
                    mistakes++;
                    continue;
                }
            }

            char g = tolower(static_cast<unsigned char>(input[0]));
            if (!isalpha(static_cast<unsigned char>(g))) {
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
            if (!revealed[i] && isalpha(static_cast<unsigned char>(word[i]))) finalMasked[i] = '_';

        string usedStr;
        for (map<char,bool>::iterator it = used.begin(); it != used.end(); ++it) {
            if (it->second) {
                usedStr += it->first;
                usedStr += ' ';
            }
        }

        int finalTimeLeft = -1;
        if (mode_select == 2) {
            time_t now = time(NULL);
            int elapsed = (int)difftime(now, startTime);
            finalTimeLeft = (elapsed < 60) ? (60 - elapsed) : 0;
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
            int baseQ = basePoints(chosen.difficulty) / 4;
            int pen = mistakes * 5;
            score = baseQ - pen;
            if (score < 0) score = 0;
        }
        cout << "Score: " << score << " points\n\n";

        // Save score
        cout << "Save score to leaderboard? (y/n): ";
        string yn;
        getline(cin, yn);
        if (!yn.empty() && tolower(static_cast<unsigned char>(yn[0])) == 'y') {
            string name;
            cout << "Enter your name: ";
            getline(cin, name);
            if (name.empty()) name = "Player";
            ScoreRow row;
            row.name = name;
            row.score = score;
            row.mode = (mode_select == 2 ? "Timed" : "Regular");
            if (appendScore(row)) cout << "Score saved!\n\n";
            else cerr << "Failed to save.\n\n";
        }
    }

    return 0;
}
