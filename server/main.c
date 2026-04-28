#include "server.h"

int main()
{
    srand(time(NULL)); // set seed for random 
    return serve_main(); // start server
}
