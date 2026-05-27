#include "player_count.h"

#include <atomic>

namespace ds2sc::player_count {

namespace {
    std::atomic<int> g_n{1};
}

int get() {
    return g_n.load(std::memory_order_relaxed);
}

bool set(int n) {
    if (n < 1) n = 1;
    return g_n.exchange(n, std::memory_order_relaxed) != n;
}

}
