#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cstring>

using namespace std;

struct WordEntry {
    string category;
    string word;
    string difficulty;
};

string trim(const string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    size_t last = str.find_last_not_of(" \t\r\n");
    if (first == string::npos || last == string::npos)
        return "";
    return str.substr(first, (last - first + 1));
}

string toLower(const string& s) {
    string out = s;
    for (size_t i = 0; i < out.length(); ++i)
        out[i] = tolower(out[i]);
    return out;
}

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

        WordEntry entry;
        entry.category = category;
        entry.word = word;
        entry.difficulty = difficulty;
        words.push_back(entry);
    }

    file.close();
    return words;
}

bool compareScores(const pair<string, int>& a, const pair<string, int>& b) {
    return a.second < b.second;
}

int main() {
    vector<WordEntry> words = loadWords("FinalWordBank.csv");
    if (words.empty()) {
        cout << "No words loaded. Check your file name or path.\n";
        return 1;
    }

    string chosenCategory, chosenDifficulty;
    int mode_select = 0;

    cout << "Please Select a mode:\n1. Regular Hangman\n2. Timed Mode\n3. Leaderboard\n";
    cin >> mode_select;
    cin.ignore();

    if (mode_select == 1 || mode_select == 2) {
        while (true) {
            cout << "Choose a category (Pet, Place, Restaurant): ";
            getline(cin, chosenCategory);
            string catLower = toLower(chosenCategory);
            if (catLower == "pet" || catLower == "place" || catLower == "restaurant") break;
            cout << "Invalid category. Try again.\n";
        }

        while (true) {
            cout << "Choose a difficulty (Easy, Medium, Hard, Expert): ";
            getline(cin, chosenDifficulty);
            string diffLower = toLower(chosenDifficulty);
            if (diffLower == "easy" || diffLower == "medium" || diffLower == "hard" || diffLower == "expert") break;
            cout << "Invalid difficulty. Try again.\n";
        }

        vector<WordEntry> filtered;
        for (vector<WordEntry>::iterator it = words.begin(); it != words.end(); ++it) {
            if (toLower(it->category) == toLower(chosenCategory) &&
                toLower(it->difficulty) == toLower(chosenDifficulty)) {
                filtered.push_back(*it);
            }
        }

        if (filtered.empty()) {
            cout << "No words found for that category or difficulty.\n";
            return 0;
        }

        srand(static_cast<unsigned>(time(NULL)));
        WordEntry chosen = filtered[rand() % filtered.size()];
        string word = chosen.word;
        string hiddenEntry(word.length(), '_');
        char guess;
        int incorrect_inputs = 0, incorrect_max = 6;

        time_t startTime = time(NULL);

        while (hiddenEntry != word && incorrect_inputs != incorrect_max) {
            if (mode_select == 2) {
                time_t currentTime = time(NULL);
                int elapsed = static_cast<int>(difftime(currentTime, startTime));
                if (elapsed > 60) {
                    cout << "\nTime's up! You didn't guess the word in 60 seconds.\n";
                    cout << "The correct word was: " << word << endl;
                    return 3;
                }
                cout << "Time remaining: " << (60 - elapsed) << " seconds\n";
            }

            cout << "Word: " << hiddenEntry << "\nGuess: ";
            cin >> guess;

            bool correct = false;
            for (size_t i = 0; i < word.length(); ++i) {
                if (word[i] == guess) {
                    hiddenEntry[i] = guess;
                    correct = true;
                }
            }

            if (!correct) {
                cout << "The letter " << guess << " is not in the selected word.\n";
                incorrect_inputs++;
            } else {
                cout << "The letter " << guess << " is in the selected word.\n";
            }
        }

        if (hiddenEntry == word) {
            cout << "🎉 The word was correctly guessed!\n";
            if (mode_select == 2) {
                time_t endTime = time(NULL);
                int duration = static_cast<int>(difftime(endTime, startTime));
                string playerName;
                cout << "Enter your name for the leaderboard: ";
                cin.ignore();
                getline(cin, playerName);

                ofstream lb("leaderboard.txt", ios::app);
                if (lb.is_open()) {
                    lb << playerName << "," << duration << "\n";
                    lb.close();
                    cout << "Your time (" << duration << "s) was recorded!\n";
                } else {
                    cerr << "Error: Could not write to leaderboard.\n";
                }
            }
            return 1;
        } 
        else {
            cout << "The word was not correctly guessed.\n";
            return 2;
        }
    } 
    else if (mode_select == 3) {
        ifstream lb("leaderboard.txt");
        if (!lb.is_open()) {
            cerr << "Error: Could not open leaderboard.txt\n";
            return 4;
        }

        vector<pair<string, int> > scores;
        string line;
        while (getline(lb, line)) {
            stringstream ss(line);
            string name;
            int time;
            if (getline(ss, name, ',') && ss >> time) {
                scores.push_back(make_pair(name, time));
            }
        }
        lb.close();

        sort(scores.begin(), scores.end(), compareScores);

        cout << "\n🏆 Leaderboard (Fastest Times):\n";
        for (size_t i = 0; i < scores.size() && i < 10; ++i) {
            cout << i + 1 << ". " << scores[i].first << " - " << scores[i].second << "s\n";
        }
    }

    return 0;
}



