#include "blockchain_utils.h"

#include <iostream>
#include <regex>

constexpr const char *BLOCKCHAIN_BASH_FILE_PATH = "../assignment_1_a.sh";

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <num_blocks_to_fetch>\n";
        return 1;
    }

    int numBlocks;
    try
    {
        numBlocks = std::stoi(argv[1]);
    }
    catch (const std::exception &)
    {
        std::cerr << "Invalid number: " << argv[1] << "\n";
        return 1;
    }

    if (numBlocks < 1 || numBlocks > 50)
    {
        std::cerr << "Error: number of blocks must be between 1 and 50.\n";
        return 1;
    }

    if (!refresh_database(BLOCKCHAIN_BASH_FILE_PATH, numBlocks))
    {
        std::cerr << "Failed to refresh database.\n";
        return 1;
    }

    std::cout << "Database refreshed successfully.\n";
    return 0;
}