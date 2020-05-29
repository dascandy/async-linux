#include "async_linux.h"
#include <future>

future<void> co_main() {
  co_return;
}

int main() {
  auto f = co_main();
  async_run();
  f.get();
  return 0;
}

