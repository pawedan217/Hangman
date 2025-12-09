#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <map>
#include <limits>
#include <cctype>
#include <fstream>
#include <sstream>

#include "hangman.h"

#define NOMINMAX
#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

using namespace std;

string trim(const string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    size_t last = str.find_last_not_of(" \t\r\n");
    if (first == string::npos || last == string::npos) 
        return "";
    return str.substr(first, (last - first + 1));
}

string toLower(const string& s) {
    string out = s;
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = (char)tolower((unsigned char)out[i]);
    return out;
}

void flushLine() {
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

void playCorrectSound() {
#ifdef _WIN32
    PlaySoundA("hangman_audio/correct.wav", NULL, SND_FILENAME | SND_ASYNC);
#endif
}

void playIncorrectSound() {
#ifdef _WIN32
    PlaySoundA("hangman_audio/incorrect.wav", NULL, SND_FILENAME | SND_ASYNC);
#endif
}

void playWinSound() {
#ifdef _WIN32
    PlaySoundA("hangman_audio/win.wav", NULL, SND_FILENAME | SND_ASYNC);
#endif
}

void playFailSound() {
#ifdef _WIN32
    PlaySoundA("hangman_audio/fail.wav", NULL, SND_FILENAME | SND_ASYNC);
#endif
}

Category toCategory(const string& s) {
    string lc = toLower(s);
    if (lc == "pet") 
        return Category::Pet;
    if (lc == "place") 
        return Category::Place;
    if (lc == "restaurant") 
        return Category::Restaurant;
    return Category::Invalid;
}

Difficulty toDifficulty(const string& s) {
    string lc = toLower(s);
    if (lc == "easy") 
        return Difficulty::Easy;
    if (lc == "medium") 
        return Difficulty::Medium;
    if (lc == "hard") 
        return Difficulty::Hard;
    if (lc == "expert") 
        return Difficulty::Expert;
    return Difficulty::Invalid;
}

vector<WordEntry> loadWords(const string& filename) {
    vector<WordEntry> words;
    ifstream file(filename.c_str());
    if (!file.is_open()) 
        return words;

    string line;
    getline(file, line);

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
            WordEntry e;
            e.category = category;
            e.word = word;
            e.difficulty = difficulty;
            words.push_back(e);
        }
    }
    return words;
}

bool compareScores(const pair<string, int>& a, const pair<string, int>& b) {
    if (a.second != b.second) 
        return a.second > b.second;
    return a.first < b.first;
}

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
    HANGMAN_ARRAY + sizeof(HANGMAN_ARRAY) / sizeof(HANGMAN_ARRAY[0])
);

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
    tm* pt = localtime(&t);
    if (pt) tmval = *pt;
#endif
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tmval);
    return string(buf);
}

bool appendScore(const ScoreRow& row) {
    ofstream lb("leaderboard.txt", ios::app);
    if (!lb) 
        return false;
    lb << row.name << "|" << row.score << "|" << row.mode << "|" << nowString() << "\n";
    return true;
}

vector<ScoreRow> readLeaderboard() {
    vector<ScoreRow> rows;
    ifstream lb("leaderboard.txt");
    if (!lb) 
        return rows;
    string line;
    while (getline(lb, line)) {
        if (line.empty()) 
            continue;
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
        stringstream s2(scoreStr);
        s2 >> r.score;
        rows.push_back(r);
    }
    return rows;
}

bool exportLeaderboard(const vector<pair<string, int>>& sorted, const string& filename) {
    ofstream f(filename.c_str());
    if (!f) 
        return false;
    f << "===== Hangman Leaderboard Export =====\n";
    f << "Generated: " << nowString() << "\n\n";
    for (size_t i = 0; i < sorted.size() && i < 10; ++i)
        f << (i + 1) << ". " << sorted[i].first << " - " << sorted[i].second << " pts\n";
    f << "======================================\n";
    return true;
}

struct ScoreSort {
    bool operator()(const pair<string, int>& a, const pair<string, int>& b) const {
        if (a.second != b.second) 
            return a.second > b.second;
        return a.first < b.first;
    }
};

void showLeaderboard() {
    vector<ScoreRow> rows = readLeaderboard();
    vector<pair<string, int>> display;
    for (size_t i = 0; i < rows.size(); ++i) {
        const ScoreRow& r = rows[i];
        display.push_back(make_pair(r.name + " (" + r.mode + ", " + r.dateStr + ")", r.score));
    }
    sort(display.begin(), display.end(), ScoreSort());

    cout << "\nLeaderboard (Top 10 Highest Scores)\n";
    cout << "========================================\n";
    if (display.empty()) cout << "No scores recorded yet!\n";
    else {
        for (size_t i = 0; i < display.size() && i < 10; ++i)
            cout << i + 1 << ". " << display[i].first << " - " << display[i].second << " pts\n";
    }
    cout << "========================================\n\n";

    while (true) {
        cout << "[S] Save leaderboard | [B] Back\n";
        string choice;
        getline(cin, choice);
        if (choice.empty()) 
            continue;
        char c = (char)tolower((unsigned char)choice[0]);
        if (c == 'b') 
            return;
        if (c == 's') {
            string filename = "my_leaderboard.txt";
            cout << "Enter filename (blank for default): ";
            string in;
            getline(cin, in);
            if (!trim(in).empty()) filename = trim(in);
            exportLeaderboard(display, filename);
            cout << "Saved.\n\n";
            return;
        }
    }
}

int mistakesAllowed(Difficulty diff) {
    switch (diff) {
    case Difficulty::Easy: 
        return 7;
    case Difficulty::Medium: 
        return 6;
    case Difficulty::Hard:
    case Difficulty::Expert: 
        return 5;
    default: return 6;
    }
}

int basePoints(Difficulty diff) {
    switch (diff) {
    case Difficulty::Easy: 
        return 50;
    case Difficulty::Medium: 
        return 80;
    case Difficulty::Hard:
    case Difficulty::Expert: 
        return 120;
    default: return 0;
    }
}

string buildMaskedWord(const string& word, const vector<bool>& revealed) {
    string masked;
    for (size_t i = 0; i < word.size(); ++i)
        masked += revealed[i] ? word[i] : '_';
    return masked;
}

string buildUsedString(const map<char, bool>& used) {
    string u;
    for (map<char, bool>::const_iterator it = used.begin(); it != used.end(); ++it)
        if (it->second) { u += it->first; u += ' '; }
    return u;
}

void drawHangman(int mistakes, int maxMistakes,
    const string& used,
    const string& masked,
    int remaining,
    int timeLeft) {
    int idx = mistakes;
    if (idx < 0) idx = 0;
    if (idx >= (int)HANGMAN_STATES.size()) idx = (int)HANGMAN_STATES.size() - 1;

    cout << HANGMAN_STATES[idx] << "\n\n";
    cout << "Word: " << masked << " (Letters left: " << remaining << ")\n";
    cout << "Misses: " << mistakes << " / " << maxMistakes << "\n";
    if (timeLeft >= 0) cout << "Time left: " << timeLeft << "s\n";
    cout << "Used: " << (used.empty() ? "(none)" : used) << "\n\n";
}

void playSurvival(const vector<WordEntry>& words, Category cat, Difficulty diff) {
    int maxMistakes = mistakesAllowed(diff);
    int mistakes = 0;
    int totalScore = 0;

    vector<WordEntry> filtered;
    for (size_t i = 0; i < words.size(); ++i)
        if (toCategory(words[i].category) == cat && toDifficulty(words[i].difficulty) == diff)
            filtered.push_back(words[i]);

    if (filtered.empty()) {
        cout << "No words found.\n";
        return;
    }

    bool keepPlaying = true;

    while (keepPlaying && mistakes < maxMistakes) {
        size_t idx = (size_t)(rand() % filtered.size());
        WordEntry c = filtered[idx];
        string word = c.word;
        string lower = toLower(word);

        vector<bool> revealed(word.size(), false);
        for (size_t i = 0; i < word.size(); ++i)
            if (!isalpha((unsigned char)word[i]))
                revealed[i] = true;

        int remaining = 0;
        for (size_t i = 0; i < lower.size(); ++i)
            if (isalpha((unsigned char)lower[i]))
                remaining++;

        map<char, bool> used;

        while (mistakes < maxMistakes && remaining > 0) {
            string masked = buildMaskedWord(word, revealed);
            string usedStr = buildUsedString(used);

            drawHangman(mistakes, maxMistakes, usedStr, masked, remaining, -1);

            cout << "Guess a letter or '!' for full word: ";
            string input;
            getline(cin, input);
            if (input.empty()) 
                continue;

            if (input == "!") {
                cout << "Enter full word: ";
                string guess;
                getline(cin, guess);
                if (toLower(trim(guess)) == lower) {
                    for (size_t i = 0; i < revealed.size(); ++i) revealed[i] = true;
                    remaining = 0;
                    break;
                }
                else {
                    cout << "Incorrect!\n";
                    playIncorrectSound();
                    mistakes++;
                    continue;
                }
            }

            char g = (char)tolower((unsigned char)input[0]);
            if (!isalpha((unsigned char)g)) 
                continue;
            if (used[g]) 
                continue;
            used[g] = true;

            bool hit = false;
            for (size_t i = 0; i < lower.size(); ++i)
                if (lower[i] == g && !revealed[i]) {
                    revealed[i] = true;
                    remaining--;
                    hit = true;
                }

            if (!hit) {
                cout << "Not in word.\n";
                playIncorrectSound();
                mistakes++;
            }
            else {
                cout << "Good guess!\n";
                playCorrectSound();
            }
        }

        string finalMasked = buildMaskedWord(word, revealed);
        string usedStr = buildUsedString(used);

        drawHangman(mistakes, maxMistakes, usedStr, finalMasked, remaining, -1);

        bool won = (remaining == 0);

        if (won) {
            playWinSound();
            cout << "You solved it! The word was: " << word << "\n";
            int roundScore = basePoints(diff) + (maxMistakes - mistakes) * 10;
            totalScore += roundScore;
            cout << "Round score: " << roundScore << " | Total: " << totalScore << "\n";

            if (mistakes >= maxMistakes) 
                break;

            cout << "Next word? (y/n): ";
            string ans;
            getline(cin, ans);
            if (ans.empty() || tolower(ans[0]) != 'y')
                keepPlaying = false;
        }
        else {
            playFailSound();
            cout << "You LOST survival! The word was: " << word << "\n";
            int baseQ = basePoints(diff) / 4;
            int pen = mistakes * 5;
            int fs = baseQ - pen;
            if (fs < 0) fs = 0;
            totalScore += fs;
            cout << "Total: " << totalScore << "\n";
            keepPlaying = false;
        }
    }

    cout << "Survival over. Score: " << totalScore << "\n";

    cout << "Save score? (y/n): ";
    string yn;
    getline(cin, yn);
    if (!yn.empty() && tolower(yn[0]) == 'y') {
        string name;
        cout << "Enter name: ";
        getline(cin, name);
        if (name.empty()) name = "Player";
        ScoreRow r;
        r.name = name;
        r.score = totalScore;
        r.mode = "Survival";
        appendScore(r);
    }
}

int main() {
    vector<WordEntry> words = loadWords("FinalWordBank.csv");
    if (words.empty()) {
        cout << "No words loaded.\n";
        return 1;
    }

    srand((unsigned int)time(NULL));

    while (true) {
        cout << "==============================\n";
        cout << "     HANGMAN GAME\n";
        cout << "==============================\n";
        cout << "1. Regular\n";
        cout << "2. Timed (60s)\n";
        cout << "3. Survival\n";
        cout << "4. Leaderboard\n";
        cout << "5. Quit\n";
        cout << "Choose: ";

        int mode;
        if (!(cin >> mode)) {
            cin.clear();
            flushLine();
            mode = 5;
        }
        flushLine();

        if (mode == 5) {
            cout << endl << "Thank you for playing Hangman!\n";
            break;
        }
            
        if (mode == 4) {
            showLeaderboard();
            continue;
        }
        if (mode < 1 || mode > 3) 
            continue;

        Category cat = Category::Invalid;
        Difficulty diff = Difficulty::Invalid;

        while (cat == Category::Invalid) {
            cout << "Choose category (Pet, Place, Restaurant): ";
            string in;
            getline(cin, in);
            cat = toCategory(in);
        }

        while (diff == Difficulty::Invalid) {
            cout << "Choose difficulty (Easy, Medium, Hard, Expert): ";
            string in;
            getline(cin, in);
            diff = toDifficulty(in);
        }

        if (mode == 3) {
            playSurvival(words, cat, diff);
            continue;
        }

        vector<WordEntry> filtered;
        for (size_t i = 0; i < words.size(); ++i)
            if (toCategory(words[i].category) == cat &&
                toDifficulty(words[i].difficulty) == diff)
                filtered.push_back(words[i]);

        if (filtered.empty()) {
            cout << "No words found.\n";
            continue;
        }

        size_t idx = (size_t)(rand() % filtered.size());
        WordEntry c = filtered[idx];
        string word = c.word;
        string lower = toLower(word);

        vector<bool> revealed(word.size(), false);
        for (size_t i = 0; i < word.size(); ++i)
            if (!isalpha((unsigned char)word[i]))
                revealed[i] = true;

        int remaining = 0;
        for (size_t i = 0; i < lower.size(); ++i)
            if (isalpha((unsigned char)lower[i]))
                remaining++;

        map<char, bool> used;
        int maxMistakes = mistakesAllowed(diff);
        int mistakes = 0;

        time_t startTime = time(NULL);

        while (mistakes < maxMistakes && remaining > 0) {
            string masked = buildMaskedWord(word, revealed);
            string usedStr = buildUsedString(used);

            int timeLeft = -1;
            if (mode == 2) {
                time_t now = time(NULL);
                int elapsed = (int)difftime(now, startTime);
                timeLeft = (elapsed < 60 ? 60 - elapsed : 0);
                if (timeLeft == 0) 
                    break;
            }

            drawHangman(mistakes, maxMistakes, usedStr, masked, remaining, timeLeft);

            cout << "Guess a letter or '!': ";
            string in;
            getline(cin, in);
            if (in.empty()) 
                continue;

            if (in == "!") {
                cout << "Enter full word: ";
                string guess;
                getline(cin, guess);
                if (toLower(trim(guess)) == lower) {
                    for (size_t i = 0; i < revealed.size(); ++i) revealed[i] = true;
                    remaining = 0;
                    break;
                }
                else {
                    cout << "Incorrect!\n";
                    playIncorrectSound();
                    mistakes++;
                    continue;
                }
            }

            char g = (char)tolower((unsigned char)in[0]);
            if (!isalpha((unsigned char)g)) 
                continue;
            if (used[g]) 
                continue;
            used[g] = true;

            bool hit = false;
            for (size_t i = 0; i < lower.size(); ++i)
                if (lower[i] == g && !revealed[i]) {
                    revealed[i] = true;
                    remaining--;
                    hit = true;
                }

            if (!hit) {
                cout << "Not in word.\n";
                playIncorrectSound();
                mistakes++;
            }
            else {
                cout << "Good guess!\n";
                playCorrectSound();
            }
        }

        string maskedFinal = buildMaskedWord(word, revealed);
        string usedStr = buildUsedString(used);

        int finalTimeLeft = -1;
        if (mode == 2) {
            time_t now = time(NULL);
            int el = (int)difftime(now, startTime);
            finalTimeLeft = (el < 60 ? 60 - el : 0);
        }

        drawHangman(mistakes, maxMistakes, usedStr, maskedFinal, remaining, finalTimeLeft);

        int score = 0;
        bool won = (remaining == 0);

        if (won) {
            playWinSound();
            cout << "You WIN! The word was: " << word << "\n";
            score = basePoints(diff) + (maxMistakes - mistakes) * 10;
            if (mode == 2) score += finalTimeLeft * 2;
        }
        else {
            playFailSound();
            cout << "Game Over! The word was: " << word << "\n";
            int baseQ = basePoints(diff) / 4;
            int pen = mistakes * 5;
            score = baseQ - pen;
            if (score < 0) score = 0;
        }

        cout << "Score: " << score << "\n\n";

        cout << "Save score? (y/n): ";
        string yn;
        getline(cin, yn);
        if (!yn.empty() && tolower(yn[0]) == 'y') {
            string name;
            cout << "Enter name: ";
            getline(cin, name);
            if (name.empty()) name = "Player";
            ScoreRow r;
            r.name = name;
            r.score = score;
            r.mode = (mode == 2 ? "Timed" : "Regular");
            appendScore(r);
        }
    }

    return 0;
}
