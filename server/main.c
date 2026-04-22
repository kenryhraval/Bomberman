#include "server.h"
#include "map.h"

#define MAP_FILENAME_DEFAULT "map.txt"
#define MAP_ARGUMENT "--map"

int main(int argc, char *argv[]){

    // get --map argument
    const char *map_filename = MAP_FILENAME_DEFAULT;
    for (int i = 1; i < argc - 1; i++)
    {
        if (strcmp(argv[i], MAP_ARGUMENT) == 0)
        {
            map_filename = argv[i + 1];
            break;
        }
    }

    // load map
    map_t map;
    if (load_map(map_filename, &map) < 0)
    {
        fprintf(stderr, "Failed to load map from %s\n", map_filename);
        return 1;
    }

    // start server
    serve_main(&map);

    return 0;
}