// РЎСЂР°РІРЅРµРЅРёРµ СЃС‚СЂСѓРєС‚СѓСЂ РґР°РЅРЅС‹С… РґР»СЏ РѕС‡РµСЂРµРґРё РіРѕС‚РѕРІС‹С… РїСЂРѕС†РµСЃСЃРѕРІ.
// РЎР±РѕСЂРєР°: g++ -std=c++17 -O2 -Wall -Wextra bench.cpp -o bench
#include <algorithm>
#include <chrono>
#include <deque>
#include <iostream>
#include <queue>
#include <random>
#include <vector>

struct Item {
  int pid;
  int key;  // РЅР°РїСЂРёРјРµСЂ, burstTime РёР»Рё РїСЂРёРѕСЂРёС‚РµС‚
};

template <class Fn>
double measureMs(Fn fn, int repeats = 5) {
  std::vector<double> times;
  for (int i = 0; i < repeats; ++i) {
    auto t0 = std::chrono::steady_clock::now();
    fn();
    auto t1 = std::chrono::steady_clock::now();
    times.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
  }
  std::sort(times.begin(), times.end());
  return times[times.size() / 2];  // РјРµРґРёР°РЅР°
}

std::vector<Item> makeItems(int n) {
  std::mt19937 gen(42);  // С„РёРєСЃРёСЂРѕРІР°РЅРЅС‹Р№ seed
  std::vector<Item> v;
  for (int i = 0; i < n; ++i) v.push_back({i, static_cast<int>(gen() % 1000)});
  return v;
}

int main() {
  volatile long long sink = 0;  // С‡С‚РѕР±С‹ РєРѕРјРїРёР»СЏС‚РѕСЂ РЅРµ РІС‹Р±СЂРѕСЃРёР» В«Р»РёС€РЅРёР№В» РєРѕРґ
  std::cout << "N      | FIFO deque | FIFO vector | MIN vector | MIN deque"
            << " | MIN priority_queue\n";
  std::cout << "-----------------------------------------------------------------------\n";
  for (int n : {1000, 5000, 20000}) {
    auto items = makeItems(n);

    // РЎС†РµРЅР°СЂРёР№ 1: FIFO (РєР°Рє РІ FCFS Рё Round-Robin) вЂ” РґРѕР±Р°РІРёС‚СЊ РІСЃРµС…, РїРѕС‚РѕРј Р·Р°Р±РёСЂР°С‚СЊ СЃ РЅР°С‡Р°Р»Р°
    double fifoDeque = measureMs([&] {
      std::deque<Item> q;
      for (auto& it : items) q.push_back(it);
      while (!q.empty()) { sink = sink + q.front().pid; q.pop_front(); }
    });
    double fifoVector = measureMs([&] {
      std::vector<Item> q;
      for (auto& it : items) q.push_back(it);
      while (!q.empty()) { sink = sink + q.front().pid; q.erase(q.begin()); }
    });

    // РЎС†РµРЅР°СЂРёР№ 2: В«РІР·СЏС‚СЊ РјРёРЅРёРјР°Р»СЊРЅС‹Р№ РїРѕ РєР»СЋС‡СѓВ» (РєР°Рє РІ SJF Рё РїСЂРёРѕСЂРёС‚РµС‚РЅРѕРј)
    auto byKey = [](const Item& a, const Item& b) { return a.key < b.key; };
    double minVector = measureMs([&] {
      std::vector<Item> q(items);
      while (!q.empty()) {
        auto it = std::min_element(q.begin(), q.end(), byKey);
        sink = sink + it->pid;
        q.erase(it);
      }
    });
    double minDeque = measureMs([&] {
      std::deque<Item> q(items.begin(), items.end());
      while (!q.empty()) {
        auto it = std::min_element(q.begin(), q.end(), byKey);
        sink = sink + it->pid;
        q.erase(it);
      }
    });
    double minPq = measureMs([&] {
      auto cmp = [](const Item& a, const Item& b) { return a.key > b.key; };
      std::priority_queue<Item, std::vector<Item>, decltype(cmp)> q(cmp);
      for (auto& it : items) q.push(it);
      while (!q.empty()) { sink = sink + q.top().pid; q.pop(); }
    });

    std::cout << n << "\t| " << fifoDeque << "\t   | " << fifoVector << "\t\t | "
              << minVector << "\t      | " << minDeque << "\t  | " << minPq << "   (РјСЃ)\n";
  }
  (void)sink;
}
