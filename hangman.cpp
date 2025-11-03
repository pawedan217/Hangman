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
    std::string category;
    std::string word;
    std::string difficulty;
};


std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    size_t last = str.find_last_not_of(" \t\r\n");
    if (first == std::string::npos || last == std::string::npos)
        return "";
    return str.substr(first, (last - first + 1));
}


std::vector<WordEntry> loadWords(const std::string& filename) {
    std::vector<WordEntry> words;
    std::ifstream file(filename);

    if (!file.is_open()) {
        std::cerr << " Error: Could not open " << filename << std::endl;
        return words;
    }

    std::string line;
    std::getline(file, line); // skip header

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string category, word, difficulty;

        std::getline(ss, category, ',');
        std::getline(ss, word, ',');
        std::getline(ss, difficulty, ',');

        // Trim any extra spaces or line breaks
        category = trim(category);
        word = trim(word);
        difficulty = trim(difficulty);

        std::replace(word.begin(), word.end(), '_', ' '); // underscores → spaces

        WordEntry entry;
        entry.category = category;
        entry.word = word;
        entry.difficulty = difficulty;
        words.push_back(entry);
    }

    file.close();
    return words;
}


std::string toLower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), ::tolower);
    return out;
}


int main() {
    std::vector<WordEntry> words = loadWords("FinalWordBank.csv");

    if (words.empty()) {
        std::cout << " No words loaded. Check your file name or path.\n";
        return 1;
    }

    std::string chosenCategory;
    std::string chosenDifficulty;


    while (true) {
        std::cout << "Choose a category (Pet, Place, Restaurant): ";
        std::getline(std::cin, chosenCategory);
        std::string catLower = toLower(chosenCategory);
        if (catLower == "pet" || catLower == "place" || catLower == "restaurant") break;
        std::cout << " Invalid category. Try again.\n";
    }


    while (true) {
        std::cout << "Choose a difficulty (Easy, Medium, Hard, Expert): ";
        std::getline(std::cin, chosenDifficulty);
        std::string diffLower = toLower(chosenDifficulty);
        if (diffLower == "easy" || diffLower == "medium" || diffLower == "hard" || diffLower == "expert") break;
        std::cout << "Invalid difficulty. Try again.\n";
    }

    std::vector<WordEntry> filtered;
    for (size_t i = 0; i < words.size(); ++i) {
        if (toLower(words[i].category) == toLower(chosenCategory) &&
            toLower(words[i].difficulty) == toLower(chosenDifficulty)) {
            filtered.push_back(words[i]);
        }
    }

    if (filtered.empty()) {
        std::cout << "No words found for that category/difficulty.\n";
        return 0;
    }

    
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    WordEntry chosen = filtered[std::rand() % filtered.size()];
	std::string word= chosen.word;
	string hiddenEntry(word.length(), '_');
	char charArr[word.length() + 1];
	strcpy(charArr, word.c_str());
	
	for(size_t i=0; i<word.length(); i++){
		if(word[i]==guess){
			hiddenEntry[i]=guess;
		}
	}
	
	cout<<"Word: " << hiddenEntry<<endl;
	
	
	
    return 0;
}




