#ifndef BLOCKCHAIN_UTILS_H
#define BLOCKCHAIN_UTILS_H

#include <string>
#include <vector>
#include <optional>
#include <fstream>

struct Block
{
    std::string hash;
    long height;
    long long total;
    std::string time;
    std::string relayed_by;
    std::string prev_block;
};

// Open the CSV database; returns true if file opened
bool openDatabaseCSV(std::ifstream &file, const std::string &filename);

// Parse all blocks from CSV into a vector
std::vector<Block> parseBlocksFromCSV(std::ifstream &file);

// Loads DB from given file
std::vector<Block> load_db(const std::string& filename);

// Prints a single block
void printBlock(const Block &b);

// exe 1
void printChain(const std::vector<Block> &db);

// exe 2i
std::optional<Block> find_block_by_hash(const std::vector<Block>&, const std::string&);

// exe 2ii
std::optional<Block> find_block_by_height(const std::vector<Block>&, long);

// exe 4
bool refresh_database(std::string BashScriptFilePath, int i_NumOfBlocksToGet);

#endif // BLOCKCHAIN_UTILS_H