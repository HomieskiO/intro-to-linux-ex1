#include "blockchain_utils.h"

#include <fstream>
#include <iostream>
#include <string>

constexpr const char *DATABASE_FILE = "../database.csv";
constexpr const char *MODE_SEARCH_BY_HASH = "--hash";
constexpr const char *MODE_SEARCH_BY_HEIGHT = "--height";

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0]
                  << " [" << MODE_SEARCH_BY_HASH << " HASH] | ["
                  << MODE_SEARCH_BY_HEIGHT << " HEIGHT]" << std::endl;
        return 1;
    }

    std::string mode = argv[1];
    std::string input = argv[2];

    if (mode != MODE_SEARCH_BY_HASH && mode != MODE_SEARCH_BY_HEIGHT)
    {
        std::cerr << "Invalid mode. Use " << MODE_SEARCH_BY_HASH
                  << " or " << MODE_SEARCH_BY_HEIGHT << "." << std::endl;
        return 1;
    }

    std::vector<Block> db = load_db(DATABASE_FILE);
    bool found = false;

    std::optional<Block> result;
    if (mode == MODE_SEARCH_BY_HASH)
    {
        result = find_block_by_hash(db, input);
    }
    else
    {
        result = find_block_by_height(db, std::stol(input));
    }

    if (result)
    {
        found = true;
        printBlock(*result);
    }
    else if (!result || !found)
    {
        std::cerr << "Block "
                  << (mode == MODE_SEARCH_BY_HASH ? "hash: " : "height: ")
                  << input
                  << " not found. Please run refreshDatabase first."
                  << std::endl;
        return 1;
    }

    return 0;
}