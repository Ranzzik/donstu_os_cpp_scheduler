#include "simulator.h"

SimResult runSimulation(Scheduler& sched, std::uint64_t maxTicks, std::uint64_t switchCost) {
  SimResult res;
  res.algorithm = sched.name();

  std::uint64_t tick = 0;
  int currentPid = -1;
  int prevPid = -1;
  std::uint64_t busyTicks = 0;
  std::uint64_t switchLeft = 0;
  std::uint64_t overheadTicks = 0;

  auto& procs = sched.processes();

  while (tick < maxTicks) {
    // 1. Возвращаем процессы из I/O
    for (auto& p : procs) {
      if (p.state == ProcessState::WAITING && p.ioReturnTick <= tick) {
        p.state = ProcessState::READY;
        sched.onProcessReady(p.pid, tick);
      }
    }
/*
    // 2. Накапливаем waitingTime для процессов в READY
    for (auto& p : procs) {
      if (p.state == ProcessState::READY) {
        p.waitingTime++;
      }
    }
*/
    // 3. Все ли завершены?
    bool allDone = true;
    for (auto& p : procs) {
      if (p.state != ProcessState::TERMINATED) { allDone = false; break; }
    }
    if (allDone) break;

    // 4. Планировщик обновляет очереди
    sched.onTick(tick);

    // 5. Вытеснение
    if (currentPid != -1 && sched.shouldPreempt(currentPid, tick)) {
      Process* p = sched.find(currentPid);
      if (p && !p->isFinished() && p->state == ProcessState::RUNNING) {
        p->state = ProcessState::READY;
        sched.onProcessPreempted(currentPid, tick);
      }
      currentPid = -1;
    }

    // 6. Выбор нового процесса
    if (currentPid == -1) {
      currentPid = sched.pickNext(tick);
      if (currentPid != -1) {
        Process* p = sched.find(currentPid);
        if (p) {
		if (!p->started) {
		  p->started = true;
		  p->startTime = tick;
          }
          p->state = ProcessState::RUNNING;
          p->contextSwitches++;
          if (prevPid != currentPid){ res.contextSwitches++; switchLeft = switchCost;}
        }
      }
    }

    // 7. Выполняем один такт (или тратим его на переключение контекста)
    int ranPid = -1;
    bool overheadTick = (currentPid != -1 && switchLeft > 0);

    if (overheadTick) {
      switchLeft--;
      overheadTicks++;
    } else if (currentPid != -1) {
      Process* p = sched.find(currentPid);
      p->state = ProcessState::RUNNING;
      p->executedTicks++;
      p->remainingTime--;
      busyTicks++;
      ranPid = currentPid;

      // Проверяем ввод-вывод
      if (p->nextIoIndex < p->ioBlocks.size() &&
          p->executedTicks == p->ioBlocks[p->nextIoIndex].atTick) {
        p->ioReturnTick = tick + 1 + p->ioBlocks[p->nextIoIndex].duration;
        sched.onProcessBlocked(currentPid, tick);
        currentPid = -1;
      }
      // Проверяем завершение процесса
      else if (p->remainingTime == 0) {
	p->state = ProcessState::TERMINATED;
	p->finishTime = tick + 1;
        sched.onProcessFinished(currentPid, tick + 1);
        currentPid = -1;
      }
    }

//7a Все, кто остался в READY, ждали этот такт
    for (auto& p : procs) {
      if (p.state == ProcessState::READY) {
        p.waitingTime++;
      }
    }



    // 8. Запись в диаграмму Ганта (по фактически выполнявшемуся процессу)
    if (overheadTick) {
      if (!res.gantt.empty() && res.gantt.back().first == -2) {
        res.gantt.back().second.second = tick + 1;
      } else {
        res.gantt.push_back({-2, {tick, tick + 1}});
      }
    } else if (ranPid != -1) {
      if (!res.gantt.empty() && res.gantt.back().first == ranPid) {
        res.gantt.back().second.second = tick + 1;
      } else {
        res.gantt.push_back({ranPid, {tick, tick + 1}});
      }
    } else {
      if (!res.gantt.empty() && res.gantt.back().first == -1) {
        res.gantt.back().second.second = tick + 1;
      } else {
        res.gantt.push_back({-1, {tick, tick + 1}});
      }
    }
prevPid = overheadTick ? currentPid : ranPid;
tick++;
}



//Метрики
  double sumW = 0, sumT = 0, sumR = 0;
  int finished = 0;
  for (auto& p : procs) {
    if (p.state == ProcessState::TERMINATED) {		
	p.turnaroundTime = p.finishTime - p.arrivalTime;
	p.responseTime = p.startTime - p.arrivalTime;
      sumW += p.waitingTime;
      sumT += p.turnaroundTime;
      sumR += p.responseTime;
      finished++;
    }
  }
  if (finished > 0) {
    res.avgWaiting = sumW / finished;    res.avgTurnaround = sumT / finished;
    res.avgResponse = sumR / finished;
  }
  res.totalTicks = tick;
  res.cpuUtilization = tick > 0 ? 100.0 * busyTicks / tick : 0.0;
  res.throughput = finished;

res.overheadTicks = overheadTicks;
res.overheadPercent = tick > 0 ? 100.0 * overheadTicks / tick : 0.0;


  return res;
}
