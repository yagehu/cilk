#include "cilk/exec_path.h"

void exec_path_init(struct exec_path * path) {
    path->pos = 0;
    branch_vec_init(&path->branches);
}