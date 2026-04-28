#include "server.h"

int main()
{
    setvbuf(stdout, NULL, _IONBF, 0);
    srand(time(NULL)); // set seed for random 
    return serve_main(); // start server
}
