#include <string>
#include <fstream>
#include <iostream>
#include <stdint.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <stdexcept>
#include <sstream>
#include "LineInfo.h"

using namespace std;

// Constants
const int READ_MAX_LEN = 100;                // Maximum read buffer length
const int NO_OF_LETTER_GUESS_MAX = 12;       // Maximum number of allowed guesses
const int OK = 0;                           // Success return value

int main(int argc, char* argv[])
{
    try {
        // Check if command line arguments are valid
        if (argc != 1) {
            throw domain_error(LineInfo("Usage: ./gclient", __FILE__, __LINE__));
        }
        
        // Define the known named pipe for client to server requests
        string srd_cwr_req_np_str = "./srd_cwr_req_np";
        mkfifo(srd_cwr_req_np_str.c_str(), 0600);

        // Open the known named pipe for writing
        int srd_cwr_req_np_fd = open(srd_cwr_req_np_str.c_str(), O_WRONLY);
        if (srd_cwr_req_np_fd < OK)
            throw domain_error(LineInfo("open FAILURE", __FILE__, __LINE__));

        // Create a unique named pipe name for server writes to client reads
        string swr_crd_np_str = "./swr_crd_np-";
        int i = getpid(); 
        string s; 
        stringstream out; 
        out << i; 
        s = out.str();
        swr_crd_np_str += s;
        
        // Send the unique pipe name to the server
        if (write(srd_cwr_req_np_fd, swr_crd_np_str.c_str(), swr_crd_np_str.size() + 1) < OK)
            throw domain_error(LineInfo("write FAILURE", __FILE__, __LINE__));

        // Create the client's unique named pipe for reading from server
        mkfifo(swr_crd_np_str.c_str(), 0600);

        // Open the client's unique pipe for reading
        int swr_crd_np_fd = open(swr_crd_np_str.c_str(), O_RDONLY);
        if (swr_crd_np_fd < OK)
            throw domain_error(LineInfo("open FAILURE", __FILE__, __LINE__));

        // Read client number from server
        char clientno_ary[READ_MAX_LEN] = { 0 };
        if (read(swr_crd_np_fd, clientno_ary, READ_MAX_LEN) < OK)
            throw domain_error(LineInfo("read FAILURE", __FILE__, __LINE__));
        string clientno_str(clientno_ary);

        // Read random word from server
        char randomWordAry[READ_MAX_LEN] = { 0 };
        if (read(swr_crd_np_fd, randomWordAry, READ_MAX_LEN) < OK)
            throw domain_error(LineInfo("read FAILURE", __FILE__, __LINE__));
        string randomWordAry_str(randomWordAry);

        // Read server's unique pipe name
        char srd_cwr_np_ary[READ_MAX_LEN] = { 0 };
        if (read(swr_crd_np_fd, srd_cwr_np_ary, READ_MAX_LEN) < OK)
            throw domain_error(LineInfo("read FAILURE", __FILE__, __LINE__));
        string srd_cwr_np_str(srd_cwr_np_ary);

        // Create server's unique named pipe
        mkfifo(srd_cwr_np_str.c_str(), 0600);

        // Open server's unique pipe for writing
        int srd_cwr_np_fd = open(srd_cwr_np_str.c_str(), O_WRONLY);
        if (srd_cwr_np_fd < OK)
            throw domain_error(LineInfo("open FAILURE", __FILE__, __LINE__));

        // Initialize game variables
        int noOfTries = 0;
        
        // Game start announcement
        cout << endl << endl << "Game Start" << endl << endl <<
            "You have " << NO_OF_LETTER_GUESS_MAX << " letter guesses to win" << endl;
        
        // Game loop
        do {
            // Read guess word from server
            char guessWordAry[READ_MAX_LEN] = { 0 };
            if (read(swr_crd_np_fd, guessWordAry, READ_MAX_LEN) < OK)
                throw domain_error(LineInfo("read FAILURE", __FILE__, __LINE__));
            string guessword_str(guessWordAry);

            // Check if word is guessed correctly
            if (guessword_str == randomWordAry_str) {
                cout << endl << guessword_str << endl;
                cout << endl << endl << "You Win!" << endl << endl;
                break;
            }

            // Check if maximum tries are exceeded
            if (noOfTries >= NO_OF_LETTER_GUESS_MAX) {
                cout << endl << endl << "Out of tries : " << NO_OF_LETTER_GUESS_MAX << " allowed" << endl <<
                    "The word is: " << randomWordAry_str << endl << endl;
                break;
            }

            // Display game status and prompt for guess
            cout << endl;
            cout << "Current try number : " << noOfTries + 1 << endl <<
                "(Guess) Enter a letter in the word : " << guessword_str << endl;

            // Get guess from user
            char letterGuess_char = ' ';
            cin >> letterGuess_char;
            string letterGuess_str = " ";
            letterGuess_str[0] = letterGuess_char;
            
            // Send guess to server
            if (write(srd_cwr_np_fd, letterGuess_str.c_str(), letterGuess_str.size() + 1) < OK)
                throw domain_error(LineInfo("write FAILURE", __FILE__, __LINE__));

            // Increment try counter
            noOfTries++;

        } while (true);

        // Close all pipes
        close(srd_cwr_req_np_fd);
        close(swr_crd_np_fd);
        close(srd_cwr_np_fd);

        // Unlink (remove) all pipes
        unlink(srd_cwr_req_np_str.c_str());
        unlink(swr_crd_np_str.c_str());
        unlink(srd_cwr_np_str.c_str());
    }
    catch (exception& e) {
        // Handle and display exceptions
        cout << e.what() << endl << endl;
        cout << "Press the enter key once or twice to leave..." << endl << endl;
        cin.ignore(); cin.get();
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}