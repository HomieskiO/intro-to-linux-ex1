#include "blockchain_utils.h"
#include <iostream>
#include <string>
#include <limits>
#include <algorithm> // for std::all_of
#include <cctype>    // for std::isdigit

constexpr const char *BLOCKCHAIN_BASH_FILE_PATH = "../assignment_1_a.sh";
constexpr const char *DATABASE_FILE = "../database.csv";

// --- Helpers -----------------------------------------------------------------

static void printMenu()
{
    std::cout << "    --MENU--     " << std::endl;
    std::cout << "1) Print Database" << std::endl;
    std::cout << "2) Print Block By Hash" << std::endl;
    std::cout << "3) Print Block By Height" << std::endl;
    std::cout << "4) Export Data To .csv" << std::endl;
    std::cout << "5) Refresh Database (last N blocks)" << std::endl;
    std::cout << "6) Exit" << std::endl;
    std::cout << "Enter Your Choice (1 <--> 6): ";
}

/// Prompts the user, reads a line, returns true + parsed integer if all digits.
static bool promptNumber(const std::string &prompt, int &outNum)
{
    std::cout << prompt;
    std::string s;
    std::getline(std::cin, s);

    if (s.empty() || !std::all_of(s.begin(), s.end(),
                                  [](char ch)
                                  { return std::isdigit(ch); }))
    {
        return false;
    }

    outNum = std::stoi(s);
    return true;
}

// --- Methods------------------------------------------------------------------

static void handlePrintDatabase()
{
    std::vector<Block> db = load_db(DATABASE_FILE);

    std::cout << "Printing Database:" << std::endl;
    printChain(db);
}

static void handlePrintByHash()
{
    std::vector<Block> db = load_db(DATABASE_FILE);
    std::string hash;

    std::cout << "Enter block hash: ";
    std::getline(std::cin, hash);

    auto opt = find_block_by_hash(db, hash);

    if (opt)
    {
        std::cout << "Printing block with hash: " << hash << std::endl;
        printBlock(*opt);
    }
    else
    {
        std::cout << "Block not found. Try refreshing." << std::endl;
    }
}

static void handlePrintByHeight()
{
    int height;

    if (!promptNumber("Enter block height: ", height))
    {
        std::cout << "Invalid height." << std::endl;
        return;
    }

    auto db = load_db(DATABASE_FILE);
    auto opt = find_block_by_height(db, height);

    if (opt)
    {
        std::cout << "Printing block with height: " << height << std::endl;
        printBlock(*opt);
    }
    else
    {
        std::cout << "Block not found. Try refreshing." << std::endl;
    }
}

static void handleExportData()
{
    // this does nothing because data is exported to .CSV file when bash script is executed
    // leaving blanc for now, Gabi approved
}

static void handleRefreshDatabase()
{
    int n;
    if (!promptNumber("Enter number of blocks to refresh: ", n) ||
        !refresh_database(BLOCKCHAIN_BASH_FILE_PATH, n))
    {
        std::cout << "Refresh failed or invalid number." << std::endl;
    }
}

int main()
{
    int choice;

    while (true)
    {
        printMenu();
        std::cin >> choice;
        std::cin.ignore();      // Clear newline from buffer
        try
        {
            switch (choice)
            {
            case 1:
                handlePrintDatabase();
                break;
            case 2:
                handlePrintByHash();
                break;
            case 3:
                handlePrintByHeight();
                break;
            case 4:
                handleExportData();
                break;
            case 5:
                handleRefreshDatabase();
                break;
            case 6:
                std::cout << "Goodbye!" << std::endl << std::endl;
                return 0;
            default:
                std::cout << "Please choose a number between 1 to 6." << std::endl;
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error: " << e.what() << std::endl;
        }

        std::cout << std::endl;
    }
}