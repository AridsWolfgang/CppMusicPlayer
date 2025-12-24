#ifndef TUI_H
#define TUI_H

#include <string>
#include <vector>

class TUI {
public:
    TUI();
    ~TUI();
    
    void showMainMenu();
    void showSearchResults(const std::vector<std::string>& results);
    void showPlayerScreen(const std::string& currentTrack);
    void showMessage(const std::string& msg);
    
    int getMenuChoice();
    std::string getSearchQuery();
    int selectSearchResult(int count);
    
private:
    void clearScreen();
    void drawBox(int x, int y, int width, int height);
    void printCentered(const std::string& text, int line);
};

#endif
