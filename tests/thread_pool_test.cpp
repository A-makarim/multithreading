#include "thread_pool.hpp"
#include <cassert>
int main(){ ThreadPool pool(4); auto f=pool.submit([]{return 42;}); assert(f.get()==42); }
