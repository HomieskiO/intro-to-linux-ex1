#include "blockchain_utils.h"

#include <iostream>
#include <fstream>

constexpr const char *DATABASE_FILE = "../database.csv";

int main()
{
    try
    {
        std::vector<Block> db = load_db(DATABASE_FILE);

        printChain(db);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error loading database: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}