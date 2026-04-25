#include "server.h"

int main(int argc, char *argv[])
{
    srand(time(NULL)); // set seed for random 

    // start server
    return serve_main(argc, argv);
}