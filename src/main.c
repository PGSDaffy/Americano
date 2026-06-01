#include "cover.h"

int main()
{
    printf("Americano - test start\n");

    // 测试：建一个 2 变量的 cover，往里面加两个 cube
    // 2 个变量 → 高位 1 个 int，低位 1 个 int → wsize=2
    int nwords = 2;
    set_family *F = cover_new(nwords, 8);

    // 造一个 cube: a=1, b=0  (高位 10, 低位 01)
    unsigned int buf[2];
    pset cube = buf;
    set_clear(cube, nwords);
    cube[0] = (ONE << 0) | (ZERO << 2); // 高位: a=1, b=0
    cube[1] = (ONE << 1) | (ONE << 3);  // 低位: a=1(原值), b=0(反值)

    F = cover_add(F, cube);

    printf("cover count: %d\n", F->count);
    printf("test passed\n");

    cover_free(F);
    return 0;
}
