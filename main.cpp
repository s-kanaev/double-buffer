#include "double_buffer.h"

#include <iostream>
#include <thread>
#include <atomic>
#include <stdio.h>

struct Buffer {
    static constexpr size_t SIZE = 1000;
    unsigned long long v[SIZE];
};

using DB = DoubleBuffer<Buffer>;
using DBR = DoubleBufferReader<Buffer>;
using DBW = DoubleBufferWriter<Buffer>;

void producer(std::atomic<bool> &cont, DBW &double_buf) {
  unsigned long long idx = 1;
  Buffer b;

  while (cont.load()) {
    printf("[G] Idx = %llu\n", idx);
    for (size_t i = 0; i < b.SIZE; ++i) {
      b.v[i] = idx;
    }

    printf("[G] W\n");
    double_buf.write(b);

    ++idx;
    //std::this_thread::yield();
  }
}

void consumer(std::atomic<bool> &cont, DBR &double_buf) {
  unsigned long long idx = 0;
  unsigned long long idx2 = std::numeric_limits<unsigned long long>::max();
  int pause_count = 0;
  const int pause_count_limit = 100;
  while (true) {
    printf("[C] R\n");
    const auto b = double_buf.read();

    idx = b.v[0];

    printf("[C] Idx = %llu\n", idx);

    if (std::numeric_limits<unsigned long long>::max() == idx2) {
      idx2 = idx;
    } else if (idx2 == idx) {
      if (++pause_count > pause_count_limit) {
        printf("Paused for > limit times @ %llu\n", idx);
        cont.store(false);
        break;
      }
    }

    for (size_t i = 0; i < b.SIZE; ++i) {
      if (b.v[i] != idx) {
        printf("[C] Fail @ Idx = %llu\n", idx);
        cont.store(false);
        break;
      }
    }

    //std::this_thread::yield();
  }
}

int main() {
  std::thread thr1, thr2;

  std::atomic<bool> cont{true};
  DB db{&std::this_thread::yield};

  thr1 = std::thread([&db, &cont] () { producer(cont, db); });
  thr2 = std::thread([&db, &cont] () { consumer(cont, db); });

  thr1.join();
  thr2.join();

  std::cout << "Hello, World!" << std::endl;
  return 0;
}
