#define DEALLOC 0
#define ALLOC 0
#define UNLOCKED 0
#define LOCKED 1

struct bsem
{
    struct spinlock lock;
    int sid;
    int isLocked;
    int state;
};
