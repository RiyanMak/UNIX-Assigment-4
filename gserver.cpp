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
#include <vector> 
#include <iterator>
#include <numeric>
#include <algorithm>
#include "LineInfo.h"

using namespace std;

// Constants
const int READ_MAX_LEN = 100;              // Maximum read buffer length
const int OK = 0;                         // Success return value
const int CHILD_PID = 0;                  // Child process ID after fork
const int NO_OF_LETTER_GUESS_MAX = 12;    // Maximum number of allowed guesses

int main(int argc, char* argv[])
{
    try {
        // Check if command line arguments are valid
        if (argc != 2)
            throw domain_error(LineInfo("Usage: ./gserver words.txt", __FILE__, __LINE__));

        // Vector to store words from file
        vector<string> vectorStrings;

        // Open the words file
        ifstream FileStringStream(argv[1]);
        if (FileStringStream.fail()) {
            stringstream s;
            s << "Error opening file " << argv[1] << endl;
            throw domain_error(LineInfo(s.str(), __FILE__, __LINE__));
        }

        // Read words from file into vector
        istream_iterator<string> inputIt(FileStringStream);
        copy(inputIt, istream_iterator<string>(), back_inserter(vectorStrings));

        // Get number of words in the file
        int NoOfElements = vectorStrings.size();
        static int clientcount = 0;

        // Define the known named pipe for client to server requests
        string srd_cwr_req_np_str = "./srd_cwr_req_np";
        mkfifo(srd_cwr_req_np_str.c_str(), 0600);

        // Open the known named pipe for reading
        int srd_cwr_req_np_fd = open(srd_cwr_req_np_str.c_str(), O_RDONLY);
        if (srd_cwr_req_np_fd < OK)
            throw domain_error(LineInfo("open FAILURE", __FILE__, __LINE__));

        // Read client's unique pipe name
        char swr_crd_np_ary[READ_MAX_LEN] = { 0 };
        if (read(srd_cwr_req_np_fd, swr_crd_np_ary, READ_MAX_LEN) < OK)
            throw domain_error(LineInfo("read FAILURE", __FILE__, __LINE__));
        string swr_crd_np_str(swr_crd_np_ary);

        // Close and unlink the known pipe as it's no longer needed
        close(srd_cwr_req_np_fd);
        unlink(srd_cwr_req_np_str.c_str());

        // Open client's unique pipe for writing
        mkfifo(swr_crd_np_str.c_str(), 0600);
        int swr_crd_np_fd = open(swr_crd_np_str.c_str(), O_WRONLY);
        if (swr_crd_np_fd < OK)
            throw domain_error(LineInfo("open FAILURE", __FILE__, __LINE__));

        // Initialize random number generator
        time_t t;
        srand((unsigned)time(&t));

        // Choose a random word from the vector
        int randomIndexChoice = rand() % NoOfElements;
        string randomword_str = vectorStrings[randomIndexChoice];

        // Create initial guess word with dashes
        string guessword_str(randomword_str.length(), '-');

        // Increment client counter
        clientcount++;

        // Fork to create child process
        pid_t forkpid = fork();
        if (forkpid < 0) {
            stringstream s;
            s << "fork failed" << endl;
            throw domain_error(LineInfo(s.str(), __FILE__, __LINE__));
        }

        // Child process code (game server)
        if (forkpid == CHILD_PID) {
            // Convert client count to string
            stringstream ss;
            ss << clientcount;
            string clientcount_str = ss.str();

            // Send client number to client
            if (write(swr_crd_np_fd, clientcount_str.c_str(), clientcount_str.size() + 1) < OK)
                throw domain_error(LineInfo("write FAILURE", __FILE__, __LINE__));

            // Wait for 3 seconds
            sleep(3);

            // Send random word to client
            if (write(swr_crd_np_fd, randomword_str.c_str(), (randomword_str.size() + 1)) < OK)
                throw domain_error(LineInfo("write FAILURE", __FILE__, __LINE__));

            // Create a unique pipe name for server's read from client
            string srd_cwr_np_str = "./srd_cwr_np-";
            int i = getpid(); 
            string s; 
            stringstream out; 
            out << i; 
            s = out.str();
            srd_cwr_np_str += s;

            // Send server's unique pipe name to client
            if (write(swr_crd_np_fd, srd_cwr_np_str.c_str(), srd_cwr_np_str.size() + 1) < OK)
                throw domain_error(LineInfo("write FAILURE", __FILE__, __LINE__));

            // Create server's unique pipe for reading
            mkfifo(srd_cwr_np_str.c_str(), 0600);
            int srd_cwr_np_fd = open(srd_cwr_np_str.c_str(), O_RDONLY);
            if (srd_cwr_np_fd < OK)
                throw domain_error(LineInfo("open FAILURE", __FILE__, __LINE__));

            // Initialize try counter
            int triesCount = 0;

            // Game loop
            do {
                // Send current guess word to client
                if (write(swr_crd_np_fd, guessword_str.c_str(), guessword_str.size() + 1) < OK)
                    throw domain_error(LineInfo("write FAILURE", __FILE__, __LINE__));

                // Read guessed letter from client
                char guessletter_ary[READ_MAX_LEN] = { 0 };
                if (read(srd_cwr_np_fd, guessletter_ary, READ_MAX_LEN) < OK)
                    throw domain_error(LineInfo("read FAILURE", __FILE__, __LINE__));

                // Update guess word with matches
                for (int i = 0; i < randomword_str.length(); i++)
                    if (randomword_str[i] == guessletter_ary[0])
                        guessword_str[i] = guessletter_ary[0];

                // Increment try counter
                triesCount++;

                // Check if the word is guessed or max tries are reached
                if (guessword_str == randomword_str || triesCount >= NO_OF_LETTER_GUESS_MAX) {
                    // Send the final guess word state
                    if (write(swr_crd_np_fd, guessword_str.c_str(), guessword_str.size() + 1) < OK)
                        throw domain_error(LineInfo("write FAILURE", __FILE__, __LINE__));
                    break;
                }

            } while (true);

            // Clean up pipes in child process
            close(swr_crd_np_fd);
            close(srd_cwr_np_fd);
            unlink(srd_cwr_np_str.c_str());

            // Exit child process
            exit(EXIT_SUCCESS);
        }
        
        // Parent process code
        else {
            // Wait for child to complete
            int status;
            waitpid(forkpid, &status, 0);
            
            // Clean up pipe in parent process
            unlink(swr_crd_np_str.c_str());
        }
    }
    catch (exception& e) {
        // Handle and display exceptions
        cout << e.what() << endl << endl;
        cin.ignore(); cin.get();
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}