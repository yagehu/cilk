#ifndef LOCATION_H
#define LOCATION_H

struct location {
    char * file;
    int    line;
};

#define LOC \
    (struct location){ \
        .file = __FILE__, \
        .line = __LINE__, \
    }

#endif
