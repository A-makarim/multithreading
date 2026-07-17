#include "bounded_queue.hpp"
#include <cassert>
#include <thread>
int main(){ threadforge::BoundedQueue<int> q(2); std::thread p([&]{q.push(1); q.push(2); q.push(3); q.close();}); assert(q.pop()==1); assert(q.pop()==2); assert(q.pop()==3); assert(!q.pop()); p.join(); }
