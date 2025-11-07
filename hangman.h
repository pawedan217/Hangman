#ifndef HANGMAN_H
#define HANGMAN_H

#include <string>
#include <vector>
#include <utility>


struct WordEntry {
    std::string category;
    std::string word;
    std::string difficulty;
};

std::string trim(const std::string& str);
std::string toLower(const std::string& s);

std::vector<WordEntry> loadWords(const std::string& filename);

bool compareScores(const std::pair<std::string, int>& a, const std::pair<std::string, int>& b);

#endif 



