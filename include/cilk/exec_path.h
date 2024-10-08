#ifndef EXEC_PATH_H
#define EXEC_PATH_H

#include "cilk/branch.h"

struct exec_path {
    struct branch_vec branches;

    /// Current execution's position in the branches vec.
    ///
    /// When the execution starts, this is zero, but `branches` might not be
    /// empty.
    ///
    /// In order to perform an exhaustive search, the execution is seeded with a
    /// set of branches.
    unsigned int pos;
};

void exec_path_init(struct exec_path *);

#endif
