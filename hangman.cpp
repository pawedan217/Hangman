#include "hangman.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <map>
#include <limits>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <thread>

#define NOMINMAX
#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

// AI includes
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using namespace std;

// ===== Helpers =====
string trim(const string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    size_t last = str.find_last_not_of(" \t\r\n");
    if (first == string::npos || last == string::npos) return "";
    return str.substr(first, (last - first + 1));
}

string toLower(const string& s) {
    string out = s;
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = static_cast<char>(tolower(static_cast<unsigned char>(out[i])));
    return out;
}

void flushLine() {
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}


Category toCategory(const string& s) {
    string lc = toLower(s);
    if (lc == "pet") return Category::Pet;
    if (lc == "place") return Category::Place;
    if (lc == "restaurant") return Category::Restaurant;
    return Category::Invalid;
}

Difficulty toDifficulty(const string& s) {
    string lc = toLower(s);
    if (lc == "easy") return Difficulty::Easy;
    if (lc == "medium") return Difficulty::Medium;
    if (lc == "hard") return Difficulty::Hard;
    if (lc == "expert") return Difficulty::Expert;
    return Difficulty::Invalid;
}

vector<WordEntry> loadWords(const string& filename) {
    vector<WordEntry> words;
    ifstream file(filename.c_str());
    if (!file.is_open()) return words;
    string line;
    getline(file, line); // skip header
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
            WordEntry e{category, word, difficulty};
            words.push_back(e);
        }
    }
    return words;
}

bool compareScores(const pair<string, int>& a, const pair<string, int>& b) {
    if (a.second != b.second) return a.second > b.second;
    return a.first < b.first;
}

// ===== Conversion Helpers for AI =====
string categoryToString(Category c) {
    switch(c) {
        case Category::Pet: return "Pet";
        case Category::Place: return "Place";
        case Category::Restaurant: return "Restaurant";
        default: return "Invalid";
    }
}
string difficultyToString(Difficulty d) {
    switch(d) {
        case Difficulty::Easy: return "Easy";
        case Difficulty::Medium: return "Medium";
        case Difficulty::Hard: return "Hard";
        case Difficulty::Expert: return "Expert";
        default: return "Invalid";
    }
}

// ===== Sound Effects =====
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

// ===== Hangman Art =====
static const char* HANGMAN_ARRAY[] = {
    " +---+\n |   |\n     |\n     |\n     |\n     |\n=========",
    " +---+\n |   |\n O   |\n     |\n     |\n     |\n=========",
    " +---+\n |   |\n O   |\n |   |\n     |\n     |\n=========",
    " +---+\n |   |\n O   |\n/|   |\n     |\n     |\n=========",
    " +---+\n |   |\n O   |\n/|\\  |\n     |\n     |\n=========",
    " +---+\n |   |\n O   |\n/|\\  |\n/    |\n     |\n=========",
    " +---+\n |   |\n O   |\n/|\\  |\n/ \\  |\n     |\n========="
};
static const vector<string> HANGMAN_STATES(HANGMAN_ARRAY, HANGMAN_ARRAY + sizeof(HANGMAN_ARRAY)/sizeof(HANGMAN_ARRAY[0]));

// ===== Leaderboard =====
struct ScoreRow {
    string name;
    int score;
    string mode;
    string dateStr;
    ScoreRow() : score(0) {}
    ScoreRow(const string& n, int s, const string& m, const string& d)
        : name(n), score(s), mode(m), dateStr(d) {}
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
        stringstream s2(scoreStr);
        s2 >> r.score;
        rows.push_back(r);
    }
    return rows;
}

void showLeaderboard() {
    vector<ScoreRow> rows = readLeaderboard();
    vector<pair<string, int>> display;
    for (size_t i = 0; i < rows.size(); ++i) {
        const ScoreRow& r = rows[i];
        display.push_back(make_pair(r.name + " (" + r.mode + ", " + r.dateStr + ")", r.score));
    }
    sort(display.begin(), display.end(), compareScores);

    cout << "\nLeaderboard (Top 10 Highest Scores)\n";
    cout << "========================================\n";
    if (display.empty()) cout << "No scores recorded yet!\n";
    else {
        for (size_t i = 0; i < display.size() && i < 10; ++i)
            cout << i + 1 << ". " << display[i].first << " - " << display[i].second << " pts\n";
    }
    cout << "========================================\n\n";
}

// ===== Difficulty & Scoring =====
int mistakesAllowed(Difficulty diff) {
    switch(diff) {
        case Difficulty::Easy: return 7;
        case Difficulty::Medium: return 6;
        case Difficulty::Hard:
        case Difficulty::Expert: return 5;
        default: return 6;
    }
}
int basePoints(Difficulty diff) {
    switch(diff) {
        case Difficulty::Easy: return 50;
        case Difficulty::Medium: return 80;
        case Difficulty::Hard:
        case Difficulty::Expert: return 120;
        default: return 0;
    }
}

// ===== Curl Callback =====
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((string*)userp)->append((char*)contents, size*nmemb);
    return size*nmemb;
}

// ===== AI Word Selection =====
string getAIWord(Category cat, Difficulty diff) {
    string categoryStr = categoryToString(cat);
    string diffStr = difficultyToString(diff);

    const char* apiKey = "AIzaSyA7ahbn_LksRIMrVMHzz8p97bl8GC8RKqo"; 
    CURL* curl = curl_easy_init();
    string readBuffer;
    if (!curl) return "error";

    string prompt = "Pick one Hangman word from the category '" + categoryStr +
                    "' at '" + diffStr + "' difficulty. Return only the word.";
    string jsonData = R"({"contents":[{"parts":[{"text":")" + prompt + R"("}]}]})";

    string url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key=" +
                 string(apiKey);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonData.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        cerr << "Curl error: " << curl_easy_strerror(res) << "\n";
        return "error";
    }

    try {
        auto j = nlohmann::json::parse(readBuffer);
        if (j.contains("error")) {
            cerr << "API error (" << httpCode << "): "
                 << j["error"]["message"].get<string>() << "\n";
            return "error";
        }
        if (!j.contains("candidates") || j["candidates"].empty())
            throw runtime_error("No candidates");

        const auto& parts = j["candidates"][0]["content"]["parts"];
        if (!parts.is_array() || parts.empty())
            throw runtime_error("Missing parts");

        string aiText = parts[0]["text"].get<string>();
        aiText.erase(remove_if(aiText.begin(), aiText.end(), ::isspace), aiText.end());
        return aiText;
    } catch (const exception& e) {
        cerr << "AI parse error: " << e.what() << "\n";
        cerr << "HTTP " << httpCode << " body: " << readBuffer << "\n";
        return "error";
    }
}

// ===== AI Guess =====
char getAIGuess(const string& maskedWord, const string& usedLetters) {
    const char* apiKey = "AIzaSyA7ahbn_LksRIMrVMHzz8p97bl8GC8RKqo";
    CURL* curl = curl_easy_init();
    string readBuffer;
    if (!curl) return 'e';

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    string prompt = "You are playing Hangman. Word: " + maskedWord +
                    ". Already used letters: " + usedLetters +
                    ". Suggest one next letter guess.";
    string jsonData = R"({"contents":[{"parts":[{"text":")" + prompt + R"("}]}]})";

    string url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key=" +
                 string(apiKey);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonData.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        cerr << "Curl error: " << curl_easy_strerror(res) << "\n";
        return 'e';
    }

    try {
        auto j = nlohmann::json::parse(readBuffer);
        if (!j.contains("candidates") || j["candidates"].empty())
            throw runtime_error("No candidates");

        const auto& parts = j["candidates"][0]["content"]["parts"];
        if (!parts.is_array() || parts.empty())
            throw runtime_error("Missing parts");

        string aiText = parts[0]["text"].get<string>();
        for (char ch : aiText) {
            unsigned char uch = static_cast<unsigned char>(ch);
            if (isalpha(uch)) return static_cast<char>(tolower(uch));
        }
        return 'e';
    } catch (const exception& e) {
        cerr << "AI parse error: " << e.what() << "\n";
        cerr << "HTTP " << httpCode << " body: " << readBuffer << "\n";
        return 'e';
    }
}

char throttledAIGuess(const string& maskedWord, const string& usedLetters) {
    static auto lastCall = chrono::steady_clock::now();
    auto now = chrono::steady_clock::now();
    auto elapsed = chrono::duration_cast<chrono::seconds>(now - lastCall).count();
    if (elapsed < 12) this_thread::sleep_for(chrono::seconds(12 - elapsed));
    lastCall = chrono::steady_clock::now();
    return getAIGuess(maskedWord, usedLetters);
}

// ===== Gameplay Helpers =====
string buildMaskedWord(const string& word, const vector<bool>& revealed) {
    string masked;
    for (size_t i = 0; i < word.size(); ++i)
        masked += revealed[i] ? word[i] : '_';
    return masked;
}
string buildUsedString(const map<char,bool>& used) {
    string u;
    for (auto& kv : used) if (kv.second) { u += kv.first; u += ' '; }
    return u;
}
void drawHangman(int mistakes,int maxMistakes,const string& used,const string& masked,int remaining,int timeLeft){
    int idx=mistakes;
    if(idx<0) idx=0;
    if(idx>=(int)HANGMAN_STATES.size()) idx=(int)HANGMAN_STATES.size()-1;
    cout<<HANGMAN_STATES[idx]<<"\n\n";
    cout<<"Word: "<<masked<<" (Letters left: "<<remaining<<")\n";
    cout<<"Misses: "<<mistakes<<" / "<<maxMistakes<<"\n";
    if(timeLeft>=0) cout<<"Time left: "<<timeLeft<<"s\n";
    cout<<"Used: "<<(used.empty()?"(none)":used)<<"\n\n";
}

// ===== Survival Mode =====
void playSurvival(const vector<WordEntry>& words, Category cat, Difficulty diff) {
    int maxMistakes = mistakesAllowed(diff);
    int mistakes = 0;
    int totalScore = 0;

    vector<WordEntry> filtered;
    for (auto& w : words)
        if (toCategory(w.category) == cat && toDifficulty(w.difficulty) == diff)
            filtered.push_back(w);

    if (filtered.empty()) {
        cout << "No words found.\n";
        return;
    }

    bool keepPlaying = true;
    while (keepPlaying && mistakes < maxMistakes) {
        WordEntry c = filtered[rand() % filtered.size()];
        string word = c.word;
        string lower = toLower(word);

        vector<bool> revealed(word.size(), false);
        for (size_t i = 0; i < word.size(); ++i)
            if (!isalpha((unsigned char)word[i])) revealed[i] = true;

        int remaining = 0;
        for (char ch : lower) if (isalpha((unsigned char)ch)) remaining++;

        map<char,bool> used;
        while (mistakes < maxMistakes && remaining > 0) {
            string masked = buildMaskedWord(word, revealed);
            string usedStr = buildUsedString(used);
            drawHangman(mistakes, maxMistakes, usedStr, masked, remaining, -1);

            cout << "Guess a letter or '!' for full word: ";
            string input; getline(cin, input);
            if (input.empty()) continue;

            if (input == "!") {
                cout << "Enter full word: ";
                string guess; getline(cin, guess);
                if (toLower(trim(guess)) == lower) {
                    for (size_t i = 0; i < revealed.size(); ++i) revealed[i] = true;
                    remaining = 0; break;
                } else {
                    cout << "Incorrect!\n"; playIncorrectSound(); mistakes++; continue;
                }
            }

            char g = (char)tolower((unsigned char)input[0]);
            if (!isalpha((unsigned char)g)) continue;
            if (used[g]) continue;
            used[g] = true;

            bool hit = false;
            for (size_t i = 0; i < lower.size(); ++i)
                if (lower[i] == g && !revealed[i]) { revealed[i] = true; remaining--; hit = true; }

            if (!hit) { cout << "Not in word.\n"; playIncorrectSound(); mistakes++; }
            else { cout << "Good guess!\n"; playCorrectSound(); }
        }

        bool won = (remaining == 0);
        if (won) {
            playWinSound();
            cout << "You solved it! The word was: " << word << "\n";
            int roundScore = basePoints(diff) + (maxMistakes - mistakes) * 10;
            totalScore += roundScore;
            cout << "Round score: " << roundScore << " | Total: " << totalScore << "\n";
            cout << "Next word? (y/n): ";
            string ans; getline(cin, ans);
            if (ans.empty() || tolower(ans[0]) != 'y') keepPlaying = false;
        } else {
            playFailSound();
            cout << "You LOST survival! The word was: " << word << "\n";
            int baseQ = basePoints(diff) / 4;
            int pen = mistakes * 5;
            int fs = max(0, baseQ - pen);
            totalScore += fs;
            cout << "Total: " << totalScore << "\n";
            keepPlaying = false;
        }
    }

    cout << "Survival over. Score: " << totalScore << "\n";
    cout << "Save score? (y/n): ";
    string yn; getline(cin, yn);
    if (!yn.empty() && tolower(yn[0]) == 'y') {
        string name; cout << "Enter name: "; getline(cin, name);
        if (name.empty()) name = "Player";
        ScoreRow r{ name, totalScore, "Survival", nowString() };
        appendScore(r);
    }
}

// ===== VS Mode =====
void playVsMode(const string& word, Difficulty diff) {
    string wordLower = toLower(word);
    vector<bool> revealed(word.size(), false);
    for (size_t i = 0; i < word.size(); ++i)
        if (!isalpha((unsigned char)word[i])) revealed[i] = true;

    int remaining = 0;
    for (char c : wordLower) if (isalpha((unsigned char)c)) remaining++;

    int maxMistakes = mistakesAllowed(diff);
    int mistakesHuman = 0, mistakesAI = 0;
    map<char,bool> used;
    bool humanTurn = true;

    while ((mistakesHuman < maxMistakes && mistakesAI < maxMistakes) && remaining > 0) {
        string masked = buildMaskedWord(word, revealed);
        string usedStr = buildUsedString(used);

        char g;
        if (humanTurn) {
            drawHangman(mistakesHuman, maxMistakes, usedStr, masked, remaining, -1);
            cout << "Player's turn. Guess a letter or '!' for full word: ";
            string input; getline(cin, input);
            if (input.empty()) continue;
            if (input == "!") {
                cout << "Enter full word: "; string guess; getline(cin, guess);
                if (toLower(trim(guess)) == wordLower) {
                    for (size_t i = 0; i < revealed.size(); ++i) revealed[i] = true;
                    remaining = 0; break;
                } else { cout << "Incorrect full word!\n"; mistakesHuman++; humanTurn=false; continue; }
            }
            g = tolower((unsigned char)input[0]);
        } else {
            drawHangman(mistakesAI, maxMistakes, usedStr, masked, remaining, -1);
            g = throttledAIGuess(masked, usedStr);
            cout << "AI guesses: " << g << "\n";
        }

        if (!isalpha((unsigned char)g)) continue;
        if (used[g]) continue;
        used[g] = true;

        bool hit = false;
        for (size_t i = 0; i < wordLower.size(); ++i)
            if (wordLower[i] == g && !revealed[i]) { revealed[i] = true; remaining--; hit = true; }

        if (!hit) { if (humanTurn) mistakesHuman++; else mistakesAI++; }
        humanTurn = !humanTurn;
    }

    cout << "\n=== VS Mode Results ===\n";
    cout << "Human mistakes: " << mistakesHuman << " / " << maxMistakes << "\n";
    cout << "AI mistakes: " << mistakesAI << " / " << maxMistakes << "\n";
    cout << "The word was: " << word << "\n";
}

// ===== Main =====
int main() {
    vector<WordEntry> words = loadWords("FinalWordBank.csv");
    if (words.empty()) { cout << "No words loaded.\n"; return 1; }
    srand((unsigned int)time(NULL));

    while (true) {
        cout << "==============================\n";
        cout << "     HANGMAN GAME\n";
        cout << "==============================\n";
        cout << "1. Regular\n";
        cout << "2. Timed (60s)\n";
        cout << "3. Survival\n";
        cout << "4. VS Mode (Player vs AI)\n";
        cout << "5. Leaderboard\n";
        cout << "6. Quit\n";
        cout << "Choose: ";

        int mode;
        if (!(cin >> mode)) { cin.clear(); flushLine(); mode = 6; }
        flushLine();

        if (mode == 6) { cout << "Goodbye!\n"; break; }
        if (mode == 5) { showLeaderboard(); continue; }
        if (mode == 3) {
            Category cat = Category::Invalid;
            Difficulty diff = Difficulty::Invalid;
            while (cat == Category::Invalid) {
                cout << "Choose category (Pet, Place, Restaurant): ";
                string in; getline(cin, in); cat = toCategory(in);
            }
            while (diff == Difficulty::Invalid) {
                cout << "Choose difficulty (Easy, Medium, Hard, Expert): ";
                string in; getline(cin, in); diff = toDifficulty(in);
            }
            playSurvival(words, cat, diff);
            continue;
        }
        if (mode == 4) {
            Category cat = Category::Invalid;
            Difficulty diff = Difficulty::Invalid;
            while (cat == Category::Invalid) {
                cout << "Choose category (Pet, Place, Restaurant): ";
                string in; getline(cin, in); cat = toCategory(in);
            }
                        while (diff == Difficulty::Invalid) {
                cout << "Choose difficulty (Easy, Medium, Hard, Expert): ";
                string in; getline(cin, in);
                diff = toDifficulty(in);
            }
            // pick a random word from local bank for VS mode
            vector<WordEntry> filtered;
            for (auto& w : words)
                if (toCategory(w.category) == cat && toDifficulty(w.difficulty) == diff)
                    filtered.push_back(w);
            if (filtered.empty()) { cout << "No words found.\n"; continue; }
            string word = filtered[rand() % filtered.size()].word;
            playVsMode(word, diff);
            continue;
        }

        // ===== Regular / Timed =====
        Category cat = Category::Invalid;
        Difficulty diff = Difficulty::Invalid;
        while (cat == Category::Invalid) {
            cout << "Choose category (Pet, Place, Restaurant): ";
            string in; getline(cin, in);
            cat = toCategory(in);
        }
        while (diff == Difficulty::Invalid) {
            cout << "Choose difficulty (Easy, Medium, Hard, Expert): ";
            string in; getline(cin, in);
            diff = toDifficulty(in);
        }

        string word = getAIWord(cat, diff);
        if (word == "error" || word.empty()) {
            cout << "AI failed. Falling back to local bank.\n";
            vector<WordEntry> filtered;
            for (auto& w : words)
                if (toCategory(w.category) == cat && toDifficulty(w.difficulty) == diff)
                    filtered.push_back(w);
            if (filtered.empty()) { cout << "No words found.\n"; continue; }
            word = filtered[rand() % filtered.size()].word;
        }
        string lower = toLower(word);

        vector<bool> revealed(word.size(), false);
        for (size_t i = 0; i < word.size(); ++i)
            if (!isalpha((unsigned char)word[i])) revealed[i] = true;

        int remaining = 0;
        for (char c : lower) if (isalpha((unsigned char)c)) remaining++;

        map<char,bool> used;
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
                if (timeLeft == 0) break;
            }
            drawHangman(mistakes, maxMistakes, usedStr, masked, remaining, timeLeft);

            cout << "Guess a letter or '!': ";
            string in; getline(cin, in);
            if (in.empty()) continue;
            if (in == "!") {
                cout << "Enter full word: ";
                string guess; getline(cin, guess);
                if (toLower(trim(guess)) == lower) {
                    for (size_t i = 0; i < revealed.size(); ++i) revealed[i] = true;
                    remaining = 0; break;
                } else { cout << "Incorrect!\n"; playIncorrectSound(); mistakes++; continue; }
            }
            char g = (char)tolower((unsigned char)in[0]);
            if (!isalpha((unsigned char)g)) continue;
            if (used[g]) continue;
            used[g] = true;
            bool hit = false;
            for (size_t i = 0; i < lower.size(); ++i)
                if (lower[i] == g && !revealed[i]) { revealed[i] = true; remaining--; hit = true; }
            if (!hit) { cout << "Not in word.\n"; playIncorrectSound(); mistakes++; }
            else { cout << "Good guess!\n"; playCorrectSound(); }
        }

        string maskedFinal = buildMaskedWord(word, revealed);
        string usedStr = buildUsedString(used);
        int finalTimeLeft = -1;
        if (mode == 2) {
            time_t now = time(NULL);
            int elapsed = (int)difftime(now, startTime);
            finalTimeLeft = (elapsed < 60 ? 60 - elapsed : 0);
        }
        drawHangman(mistakes, maxMistakes, usedStr, maskedFinal, remaining, finalTimeLeft);

        int score = 0;
        bool won = (remaining == 0);
        if (won) {
            playWinSound();
            cout << "You WIN! The word was: " << word << "\n";
            score = basePoints(diff) + (maxMistakes - mistakes) * 10;
            if (mode == 2) score += finalTimeLeft * 2;
        } else {
            playFailSound();
            cout << "Game Over! The word was: " << word << "\n";
            int baseQ = basePoints(diff) / 4;
            int pen = mistakes * 5;
            score = max(0, baseQ - pen);
        }
        cout << "Score: " << score << "\n\n";

        cout << "Save score? (y/n): ";
        string yn; getline(cin, yn);
        if (!yn.empty() && tolower((unsigned char)yn[0]) == 'y') {
            string name; cout << "Enter name: "; getline(cin, name);
            if (name.empty()) name = "Player";
            ScoreRow r{ name, score, (mode == 2 ? "Timed" : "Regular"), nowString() };
            appendScore(r);
        }
    }
    return 0;
}
