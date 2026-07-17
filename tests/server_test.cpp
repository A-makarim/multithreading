#include "job.hpp"
#include <cassert>
int main(){ auto j=threadforge::parse_job_line("PRIME 7",1,1); auto r=threadforge::execute_job(j); assert(r.ok && r.message=="prime"); }
