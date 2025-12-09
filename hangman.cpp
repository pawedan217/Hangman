#include <iostream>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <map>
#include <limits>
#include <cctype>
#include <fstream>
#include <sstream>
#include <vector>
#include <thread>

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
void flushLine() { cin.ignore(numeric_limits<streamsize>::max(), '\n'); }

// ===== Word Bank =====
struct WordEntry { string category; string word; string difficulty; };
vector<WordEntry> loadWords(const string& filename) {
    vector<WordEntry> words;
    ifstream file(filename.c_str());
    if (!file.is_open()) { cerr << "Error: Could not open " << filename << endl; return words; }
    string line; getline(file, line); // skip header
    while (getline(file, line)) {
        stringstream ss(line);
        string category, word, difficulty;
        getline(ss, category, ','); getline(ss, word, ','); getline(ss, difficulty, ',');
        category = trim(category); word = trim(word); difficulty = trim(difficulty);
        replace(word.begin(), word.end(), '_', ' ');
        if (!word.empty()) words.push_back({category, word, toLower(difficulty)});
    }
    return words;
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
// ===== Leaderboard System =====
struct ScoreRow {
    string name;
    int score;
    string mode;
    string dateStr;
    ScoreRow() : score(0) {}   // default constructor only
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
    ofstream lb("leaderboard.txt", ios::app); if (!lb) return false;
    lb << row.name << "|" << row.score << "|" << row.mode << "|" << nowString() << "\n"; return true;
}
vector<ScoreRow> readLeaderboard() {
    vector<ScoreRow> rows; ifstream lb("leaderboard.txt"); if (!lb) return rows;
    string line; while (getline(lb,line)) {
        if (line.empty()) continue; stringstream ss(line); ScoreRow r; string scoreStr;
        getline(ss,r.name,'|'); getline(ss,scoreStr,'|'); getline(ss,r.mode,'|'); getline(ss,r.dateStr);
        r.name=trim(r.name); r.mode=trim(r.mode); r.dateStr=trim(r.dateStr);
        stringstream sscore(scoreStr); sscore>>r.score; if(!sscore) r.score=0; rows.push_back(r);
    } return rows;
}
struct ScoreSort { bool operator()(const pair<string,int>& a,const pair<string,int>& b) const {
    if(a.second!=b.second) return a.second>b.second; return a.first<b.first; } };
void showLeaderboard() {
    vector<ScoreRow> rows=readLeaderboard(); vector<pair<string,int>> display;
    for(auto&r:rows) display.push_back({r.name+" ("+r.mode+", "+r.dateStr+")",r.score});
    sort(display.begin(),display.end(),ScoreSort());
    cout<<"\nLeaderboard (Top 10 Highest Scores)\n========================================\n";
    if(display.empty()) cout<<"No scores recorded yet!\n";
    else for(size_t i=0;i<display.size()&&i<10;++i) cout<<i+1<<". "<<display[i].first<<" - "<<display[i].second<<" pts\n";
    cout<<"========================================\n\n";
}

// ===== Game Logic =====
int mistakesAllowed(const string& diff){ if(diff=="easy")return 7; if(diff=="hard"||diff=="expert")return 5; return 6; }
int basePoints(const string& diff){ if(diff=="easy")return 50; if(diff=="hard"||diff=="expert")return 120; return 80; }
void drawHangman(int mistakes,int maxMistakes,const string& used,const string& masked,int remaining,int timeLeft){
    int idx=mistakes; if(idx<0)idx=0; if(idx>=(int)HANGMAN_STATES.size())idx=(int)HANGMAN_STATES.size()-1;
    cout<<HANGMAN_STATES[idx]<<"\n\n"; cout<<"Word: "<<masked<<"  (Letters left: "<<remaining<<")\n";
    cout<<"Misses: "<<mistakes<<" / "<<maxMistakes<<"\n"; if(timeLeft>=0)cout<<"Time left: "<<timeLeft<<"s\n";
    cout<<"Used: "<<(used.empty()?"(none)":used)<<"\n\n";
}

// ===== Curl Callback =====
static size_t WriteCallback(void* contents,size_t size,size_t nmemb,void* userp){
    ((string*)userp)->append((char*)contents,size*nmemb); return size*nmemb;
}
//Get AI word for Single Player mode
std::string getAIWord(const std::string& category, const std::string& difficulty) {
    const char* apiKey = "AIzaSyA7ahbn_LksRIMrVMHzz8p97bl8GC8RKqo";
    CURL* curl = curl_easy_init();
    std::string readBuffer;
    if (!curl) return "error";

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    std::string prompt = "Pick one Hangman word from the category '" + category +
                         "' at '" + difficulty +
                         "' difficulty. Return only the word, no explanation.";
    std::string jsonData = R"({"contents":[{"parts":[{"text":")" + prompt + R"("}]}]})";

    std::string url =
        "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key=" +
        std::string(apiKey);

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
        std::cerr << "Curl error: " << curl_easy_strerror(res) << "\n";
        return "error";
    }

    try {
        auto j = nlohmann::json::parse(readBuffer);

        if (j.contains("error")) {
            std::cerr << "API error (" << httpCode << "): "
                      << j["error"]["message"].get<std::string>() << "\n";
            return "error";
        }

        if (!j.contains("candidates") || j["candidates"].empty())
            throw std::runtime_error("No candidates");

        const auto& parts = j["candidates"][0]["content"]["parts"];
        if (!parts.is_array() || parts.empty())
            throw std::runtime_error("Missing parts");

        std::string aiText = parts[0]["text"].get<std::string>();

        // Normalize: strip whitespace/newlines
        aiText.erase(remove_if(aiText.begin(), aiText.end(), ::isspace), aiText.end());

        return aiText;
    } catch (const std::exception& e) {
        std::cerr << "AI parse error: " << e.what() << "\n";
        std::cerr << "HTTP " << httpCode << " body: " << readBuffer << "\n";
        return "error";
    }
}


// ===== AI Guess (2.5 Flash) =====
char getAIGuess(const std::string& maskedWord, const std::string& usedLetters) {
    const char* apiKey = "AIzaSyA7ahbn_LksRIMrVMHzz8p97bl8GC8RKqo";

    CURL* curl = curl_easy_init();
    std::string readBuffer;
    if (!curl) return 'e';

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    std::string prompt = "You are playing Hangman. Word: " + maskedWord +
                         ". Already used letters: " + usedLetters +
                         ". Suggest one next letter guess.";
    std::string jsonData = R"({"contents":[{"parts":[{"text":")" + prompt + R"("}]}]})";

    std::string url =
        "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key=" +
        std::string(apiKey);

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
        std::cerr << "Curl error: " << curl_easy_strerror(res) << "\n";
        return 'e';
    }

    try {
        auto j = nlohmann::json::parse(readBuffer);

        if (j.contains("error")) {
            const auto& err = j["error"];
            std::string msg = err.contains("message") && err["message"].is_string()
                                ? err["message"].get<std::string>()
                                : "(no error message)";
            std::cerr << "API error (" << httpCode << "): " << msg << "\n";
            return 'e';
        }

        if (!j.contains("candidates") || !j["candidates"].is_array() || j["candidates"].empty())
            throw std::runtime_error("No candidates");

        const auto& c0 = j["candidates"][0];
        if (!c0.contains("content") || !c0["content"].is_object())
            throw std::runtime_error("Missing content");

        const auto& parts = c0["content"]["parts"];
        if (!parts.is_array() || parts.empty())
            throw std::runtime_error("Missing parts");

        const auto& p0 = parts[0];
        if (!p0.contains("text") || !p0["text"].is_string())
            throw std::runtime_error("Missing text");

        std::string aiText = p0["text"].get<std::string>();
        for (char ch : aiText) {
            unsigned char uch = static_cast<unsigned char>(ch);
            if (std::isalpha(uch)) {
                char g = static_cast<char>(std::tolower(uch));
                return g;
            }
        }

        return 'e';
    } catch (const std::exception& e) {
        std::cerr << "AI parse error: " << e.what() << "\n";
        std::cerr << "HTTP " << httpCode << " body: " << readBuffer << "\n";
        return 'e';
    }
}

// Enforces ~12s delay between calls (≈5 RPM)
char throttledAIGuess(const std::string& maskedWord, const std::string& usedLetters) {
    static auto lastCall = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastCall).count();
    if (elapsed < 12) {
        std::this_thread::sleep_for(std::chrono::seconds(12 - elapsed));
    }

    lastCall = std::chrono::steady_clock::now();
    return getAIGuess(maskedWord, usedLetters);
}



// ===== VS Mode =====
void playVsMode(const string& word,const string& difficulty){
    string wordLower=toLower(word); vector<bool> revealed(word.size(),false);
    for(size_t i=0;i<word.size();++i) if(!isalpha((unsigned char)word[i])) revealed[i]=true;
    int remainingLetters=0; for(char c:wordLower) if(isalpha((unsigned char)c)) remainingLetters++;
    int maxMistakes=mistakesAllowed(difficulty); 
    int mistakesHuman=0, mistakesAI=0;
    map<char,bool> used;
    bool humanTurn=true;

    while((mistakesHuman<maxMistakes && mistakesAI<maxMistakes) && remainingLetters>0){
        string masked;
        for(size_t i=0;i<word.size();++i) masked += revealed[i]?word[i]:'_';
        string usedStr; for(auto&kv:used) if(kv.second){ usedStr+=kv.first; usedStr+=' '; }

        char g;
        if(humanTurn){
            drawHangman(mistakesHuman,maxMistakes,usedStr,masked,remainingLetters,-1);
            cout<<"Player's turn. Guess a letter or '!' for full word: ";
            string input; getline(cin,input);
            if(input.empty()) continue;
            if(input=="!"){
                cout<<"Enter full word: "; string guess; getline(cin,guess);
                if(toLower(trim(guess))==wordLower){
                    for(size_t i=0;i<revealed.size();++i) revealed[i]=true;
                    remainingLetters=0; break;
                } else {
                    cout<<"Incorrect full word guess!\n"; mistakesHuman++; humanTurn=false; continue;
                }
            }
            g=tolower((unsigned char)input[0]);
        } else {
            drawHangman(mistakesAI,maxMistakes,usedStr,masked,remainingLetters,-1);
            // AI occasionally tries full word
            if(rand()%20==0){
                cout<<"AI attempts full word!\n";
                string aiGuess=wordLower; // placeholder (could call Gemini for full word)
                if(aiGuess==wordLower){
                    for(size_t i=0;i<revealed.size();++i) revealed[i]=true;
                    remainingLetters=0; break;
                } else {
                    cout<<"AI full word guess was wrong!\n"; mistakesAI++; humanTurn=true; continue;
                }
            }
            g=throttledAIGuess(masked,usedStr);
            cout<<"AI guesses: "<<g<<"\n";
        }

        if(!isalpha((unsigned char)g)){ cout<<"Invalid guess.\n"; continue; }
        if(used[g]){ cout<<"Already guessed.\n"; continue; }
        used[g]=true;

        bool hit=false;
        for(size_t i=0;i<wordLower.size();++i){
            if(wordLower[i]==g && !revealed[i]){
                revealed[i]=true; --remainingLetters; hit=true;
            }
        }
        if(!hit){ if(humanTurn) mistakesHuman++; else mistakesAI++; }
        humanTurn=!humanTurn;
    }

    // Final results
    cout<<"\n=== VS Mode Results ===\n";
    cout<<"Human mistakes: "<<mistakesHuman<<" / "<<maxMistakes<<"\n";
    cout<<"AI mistakes: "<<mistakesAI<<" / "<<maxMistakes<<"\n";
    bool solved=(remainingLetters==0);
    int scoreHuman=0, scoreAI=0;
    if(solved){
        int base=basePoints(difficulty);
        scoreHuman=base+(maxMistakes-mistakesHuman)*10;
        scoreAI=base+(maxMistakes-mistakesAI)*10;
    } else {
        int baseQ=basePoints(difficulty)/4;
        scoreHuman=max(0,baseQ-mistakesHuman*5);
        scoreAI=max(0,baseQ-mistakesAI*5);
    }
    cout << "The word was: " << word << "\n";
    cout<<"Player Score: "<<scoreHuman<<"\n";
    cout<<"AI Score: "<<scoreAI<<"\n";
}

// ===== Main Game Loop =====
int main(){
    vector<WordEntry> words=loadWords("FinalWordBank.csv");
    if(words.empty()){ cout<<"No words loaded. Check 'FinalWordBank.csv'.\n"; return 1; }
    srand((unsigned int)time(NULL));

    while(true){
        cout<<"==============================\n";
        cout<<"     HANGMAN GAME\n";
        cout<<"==============================\n";
        cout<<"1. Regular Hangman\n";
        cout<<"2. Timed Mode (60s)\n";
        cout<<"3. VS Mode (Player vs AI)\n";
        cout<<"4. Leaderboard\n";
        cout<<"5. Quit\n";
        cout<<"Choose: ";
        int mode_select; if(!(cin>>mode_select)){ cin.clear(); flushLine(); mode_select=5; }
        flushLine();
        if(mode_select==5){ cout<<"Goodbye!\n"; break; }
        if(mode_select==4){ showLeaderboard(); continue; }
        if(mode_select!=1 && mode_select!=2 && mode_select!=3){ cout<<"Invalid choice.\n"; continue; }

        // Category & Difficulty
        string chosenCategory,chosenDifficulty;
        while(true){
            cout<<"Choose category (Pet, Place, Restaurant): ";
            getline(cin,chosenCategory);
            string lc=toLower(chosenCategory);
            if(lc=="pet"||lc=="place"||lc=="restaurant") break;
            cout<<"Invalid. Try again.\n";
        }
        while(true){
            cout<<"Choose difficulty (Easy, Medium, Hard, Expert): ";
            getline(cin,chosenDifficulty);
            string d=toLower(chosenDifficulty);
            if(d=="easy"||d=="medium"||d=="hard"||d=="expert") break;
            cout<<"Invalid. Try again.\n";
        }

        // ===== Pick word =====
        string word;
        string wordLower;

        if(mode_select == 3) {
            // VS Mode: use local word bank
            vector<WordEntry> filtered;
            for(auto&w:words)
                if(toLower(w.category)==toLower(chosenCategory) && toLower(w.difficulty)==toLower(chosenDifficulty))
                    filtered.push_back(w);
            if(filtered.empty()){ cout<<"No words found for that category/difficulty.\n"; continue; }
            WordEntry chosen=filtered[rand()%filtered.size()];
            word = chosen.word;
            wordLower = toLower(word);
            playVsMode(word, chosen.difficulty);
            continue; // skip leaderboard for VS mode
        } else {
            // Regular / Timed Mode: ask AI to generate word
            word = getAIWord(chosenCategory, chosenDifficulty);

            // Fallback if API fails
            if(word=="error" || word.empty()) {
                cout<<"AI failed to generate a word. Falling back to local bank.\n";
                vector<WordEntry> filtered;
                for(auto&w:words)
                    if(toLower(w.category)==toLower(chosenCategory) && toLower(w.difficulty)==toLower(chosenDifficulty))
                        filtered.push_back(w);
                if(filtered.empty()){ cout<<"No words found for that category/difficulty.\n"; continue; }
                WordEntry chosen=filtered[rand()%filtered.size()];
                word = chosen.word;
            }

            // Normalize
            wordLower = toLower(trim(word));
        }

        // ===== Regular/Timed Mode Gameplay =====
        vector<bool> revealed(word.size(),false);
        for(size_t i=0;i<word.size();++i) if(!isalpha((unsigned char)word[i])) revealed[i]=true;
        int remainingLetters=0; for(char c:wordLower) if(isalpha((unsigned char)c)) remainingLetters++;
        int maxMistakes=mistakesAllowed(chosenDifficulty); int mistakes=0; map<char,bool> used;
        time_t startTime=time(NULL);

        while(mistakes<maxMistakes && remainingLetters>0){
            string masked; for(size_t i=0;i<word.size();++i) masked+=revealed[i]?word[i]:'_';
            int timeLeft=-1;
            if(mode_select==2){ time_t now=time(NULL); int elapsed=(int)difftime(now,startTime); timeLeft=(elapsed<60)?(60-elapsed):0; if(timeLeft==0) break; }
            string usedStr; for(auto&kv:used) if(kv.second){ usedStr+=kv.first; usedStr+=' '; }
            drawHangman(mistakes,maxMistakes,usedStr,masked,remainingLetters,timeLeft);

            cout<<"Guess a letter or '!' for full word: ";
            string input; getline(cin,input); if(input.empty()) continue;
            if(input=="!"){ cout<<"Enter full word: "; string guess; getline(cin,guess);
                if(toLower(trim(guess))==wordLower){ for(size_t i=0;i<revealed.size();++i) revealed[i]=true; remainingLetters=0; break; }
                else{ cout<<"Incorrect!\n"; mistakes++; continue; } }
            char g=tolower((unsigned char)input[0]);
            if(!isalpha((unsigned char)g)){ cout<<"Please guess a letter.\n"; continue; }
            if(used[g]){ cout<<"Already guessed '"<<g<<"'.\n"; continue; }
            used[g]=true;
            bool hit=false; for(size_t i=0;i<wordLower.size();++i) if(wordLower[i]==g && !revealed[i]){ revealed[i]=true; --remainingLetters; hit=true; }
            if(!hit){ cout<<"Not in word.\n"; mistakes++; } 
                        else cout<<"Good guess!\n";
        }

        // ===== Final screen =====
        string finalMasked=word;
        for(size_t i=0;i<word.size();++i)
            if(!revealed[i] && isalpha((unsigned char)word[i])) finalMasked[i]='_';

        string usedStr;
        for(auto&kv:used) if(kv.second){ usedStr+=kv.first; usedStr+=' '; }

        int finalTimeLeft=-1;
        if(mode_select==2){
            time_t now=time(NULL);
            int elapsed=(int)difftime(now,startTime);
            finalTimeLeft=(elapsed<60)?(60-elapsed):0;
        }
        drawHangman(mistakes,maxMistakes,usedStr,finalMasked,remainingLetters,finalTimeLeft);

        int score=0;
        bool won=(remainingLetters==0);
        if(won){
            cout<<"You WIN! The word was: "<<word<<"\n";
            score=basePoints(chosenDifficulty)+(maxMistakes-mistakes)*10;
            if(mode_select==2) score+=finalTimeLeft*2;
        } else {
            cout<<"Game Over! The word was: "<<word<<"\n";
            int baseQ=basePoints(chosenDifficulty)/4;
            int pen=mistakes*5;
            score=baseQ-pen; if(score<0) score=0;
        }
        cout<<"Score: "<<score<<" points\n\n";

        // ===== Save score to leaderboard (modes 1 & 2 only) =====
        cout<<"Save score to leaderboard? (y/n): ";
        string yn; getline(cin,yn);
        if(!yn.empty() && tolower((unsigned char)yn[0])=='y'){
            string name; cout<<"Enter your name: "; getline(cin,name);
            if(name.empty()) name="Player";
            ScoreRow row; row.name=name; row.score=score;
            row.mode=(mode_select==2?"Timed":"Regular"); row.dateStr=nowString();
            if(appendScore(row)) cout<<"Score saved!\n\n";
            else cerr<<"Failed to save.\n\n";
        }
    }
    return 0;
}



