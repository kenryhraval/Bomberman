#include "server.h"

int main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IONBF, 0);
    srand(time(NULL)); // set seed for random 

    // start server
    return serve_main(argc, argv);
}