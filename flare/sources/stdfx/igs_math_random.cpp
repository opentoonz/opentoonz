#include <limits> /* std::numeric_limits */
#include "igs_math_random.h"

// using igs::math::random;

igs::math::random::random() : seed_(1) {}

/* 乱数種 seed(0〜std::numeric_limits<long>::max()) */
void igs::math::random::seed(unsigned long seed) { this->seed_ = seed; }
unsigned long igs::math::random::seed(void) const { return this->seed_; }

/* 乱数生成 0〜std::numeric_limits<long>::max() */
long igs::math::random::next(void) {
  this->seed_ = this->seed_ * 1103515245UL + 12345UL;
  /* 0x41C64E6D + 0x3039 */
  return static_cast<long>(
      (this->seed_) %
      (static_cast<unsigned long>(std::numeric_limits<long>::max()) + 1UL));
  //	    /* (this->seed_/  65536)%32768 */
  //	return (this->seed_/0x10000)%0x8000;
}
double igs::math::random::next_d(void) { /* 0 ... 1 */
  return static_cast<double>(this->next()) /
         static_cast<double>(std::numeric_limits<long>::max());
}
