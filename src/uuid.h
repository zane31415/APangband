#ifndef _UUID_H
#define _UUID_H


#define UUID_FILE "uuid" // TODO: place in %appdata%
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <string>

// static void init_rng()
// {
//     srand ((unsigned int) time (NULL));
// }

static uint8_t rand_byte()
{
    return rand();
}

static void make_uuid(char* out)
{
    for (uint8_t i=0; i<16; i++) {
        sprintf(out + 2*i, "%02hhx", rand_byte());
    }
}

static std::string get_uuid()
{
    char uuid[33]; uuid[32] = 0;
    std::string uuidFile = UUID_FILE;
    //uuidFile += "." + GAME::Name; // different UUIDs for different games?
    FILE* f = fopen(uuidFile.c_str(), "rb");
    size_t n = 0;
    if (f) {
        n = fread(uuid, 1, 32, f);
        fclose(f);
    }
    if (!f || n < 32) {
        //init_rng();
        make_uuid(uuid);
        f = fopen(UUID_FILE, "wb");
        if (f) {
            n = fwrite(uuid, 1, 32, f);
            fclose(f);
        }
        if (!f || n < 32) {
            printf("Could not write persistant UUID!\n");
        }
    }
    f = nullptr;
    return uuid;
}

#endif // _UUID_H