# Bitcoin Mainchain Block Explorer

## Authors

- Roy Davidovich
- Omer Chomsky
- Noam Fishbain

## Course Mentor

- Gavriel Karasin

---

## Overview

This project is a Bitcoin blockchain block explorer designed for a university systems programming assignment. It includes:

- A Bash script to fetch the last **N** blocks from the Bitcoin mainchain using the [BlockCypher API](https://www.blockcypher.com/dev/bitcoin/#blockchain-api).
- Multiple C++ programs for viewing and managing blockchain data:
  - `printDatabase`: Displays all blocks in the local database.
  - `blockFinder`: Locates a block by hash or height.
  - `refreshDatabase`: The Bash script reloads the latest **N** blocks.
  - `bitcoinShell`: An interactive CLI shell that wraps all the above tools.

---

## Setup Instructions

1. **Clone the Repository**  
   ```bash
   git clone https://github.com/royDavidovich/uni-intro-to-linux-dev-ex1.git
   cd uni-intro-to-linux-dev-ex1
   ```

2. **Build the Project**  
   Use the provided `Makefile`:
   ```bash
   make
   ```

3. **Run the Interactive Shell**  
   You **must** run the shell from within the `./bin` directory:
   ```bash
   cd ./bin
   ./bitcoinShell
   ```

> 🔥 **Important:**  
> The `Export to .CSV` functionality is handled entirely by the Bash script and **not implemented** in a separate C++ script. Gabi approved this approach.<br />

>Since there’s no database.csv in a fresh clone, make **‘Refresh Database’** your first choice in the interactive menu script

---

## Program Features

- **Fetch Blockchain Data:**  
  Use `refreshDatabase` or the shell menu to fetch the latest N blocks.

- **View Database:**  
  Display the stored blockchain data with `printDatabase`.

- **Search by Hash or Height:**  
  Use `blockFinder` for quick block lookups.

- **Interactive Menu:**  
  `bitcoinShell` provides an easy-to-use interface for all operations.

---

## Notes

- The database is stored as a .CSV file: `../database.csv`.
- The Bash script (`assignment_1_a.sh`) handles API communication.
- No external JSON parsers (`jq`, etc.) are used—pure Bash/grep/curl/regex parsing.

---

## License

This repository is intended for educational purposes only.
