#include "simulator.h"
#include <algorithm>
#include <vector>

SimResult runSimulationMulti(Scheduler& sched, int cores, std::uint64_t maxTicks) {
    SimResult res;
    res.algorithm = sched.name() + " x" + std::to_string(cores);
    res.coreGantt.resize(cores);

    std::vector<int> current(cores, -1);  // РєС‚Рѕ СЃРµР№С‡Р°СЃ РЅР° РєР°Р¶РґРѕРј СЏРґСЂРµ
    std::vector<int> prev(cores, -1);     // РєС‚Рѕ РІС‹РїРѕР»РЅСЏР»СЃСЏ РЅР° СЏРґСЂРµ РІ РїСЂРѕС€Р»РѕРј С‚Р°РєС‚Рµ
    std::uint64_t busyTicks = 0;          // СЃСѓРјРјР°СЂРЅРѕ РїРѕ РІСЃРµРј СЏРґСЂР°Рј
    std::uint64_t tick = 0;
    auto& procs = sched.processes();

    while (tick < maxTicks) {
        // 1. Р’РѕР·РІСЂР°С‚ РёР· I/O
        for (auto& p : procs) {
            if (p.state == ProcessState::WAITING && tick >= p.ioReturnTick) {
                sched.onProcessReady(p.pid, tick);
            }
        }

        // 2. РџСЂРѕРІРµСЂРєР° Р·Р°РІРµСЂС€РµРЅРёСЏ РІСЃРµС… РїСЂРѕС†РµСЃСЃРѕРІ
        bool allFinished = true;
        for (const auto& p : procs) {
            if (p.state != ProcessState::TERMINATED) {
                allFinished = false;
                break;
            }
        }
        if (allFinished) break;

        // 3. РўР°РєС‚ РїР»Р°РЅРёСЂРѕРІС‰РёРєР° (РІС‹Р·С‹РІР°РµС‚СЃСЏ СЃС‚СЂРѕРіРѕ РћР”Р�Рќ СЂР°Р· Р·Р° РѕР±С‰РёР№ СЃРёСЃС‚РµРјРЅС‹Р№ С‚Р°РєС‚)
        sched.onTick(tick);

        // 4. Р’С‹С‚РµСЃРЅРµРЅРёРµ: РѕРїСЂР°С€РёРІР°РµРј shouldPreempt РґР»СЏ РєР°Р¶РґРѕРіРѕ Р°РєС‚РёРІРЅРѕРіРѕ СЏРґСЂР°
        for (int c = 0; c < cores; ++c) {
            if (current[c] != -1) {
                if (sched.shouldPreempt(current[c], tick)) {
                    sched.onProcessPreempted(current[c], tick);
                    current[c] = -1;
                }
            }
        }

        // 5. Р’С‹Р±РѕСЂ РЅРѕРІС‹С… РїСЂРѕС†РµСЃСЃРѕРІ РґР»СЏ РєР°Р¶РґРѕРіРѕ РѕСЃРІРѕР±РѕРґРёРІС€РµРіРѕСЃСЏ СЏРґСЂР° РїРѕ РѕС‡РµСЂРµРґРё
        for (int c = 0; c < cores; ++c) {
            if (current[c] == -1) {
                current[c] = sched.pickNext(tick);
                if (current[c] != -1) {
                    Process* p = sched.find(current[c]);
                    if (p && !p->started) {
                        p->started = true;
                        p->startTime = tick;
                    }
                }
            }
        }

        // 6. Р’С‹РїРѕР»РЅРµРЅРёРµ РѕРґРЅРѕРіРѕ С‚Р°РєС‚Р° РЅР° РєР°Р¶РґРѕРј СЏРґСЂРµ
        std::vector<int> ranPid(cores, -1);
        for (int c = 0; c < cores; ++c) {
            if (current[c] != -1) {
                Process* p = sched.find(current[c]);
                if (p && p->remainingTime > 0) {
                    ranPid[c] = current[c];
                    p->remainingTime--;
                    p->executedTicks++;
                    busyTicks++;

                    // РџСЂРѕРІРµСЂСЏРµРј С‚РѕС‡РєСѓ I/O Р±Р»РѕРєРёСЂРѕРІРєРё
                    if (p->nextIoIndex < p->ioBlocks.size() &&
                        p->executedTicks == p->ioBlocks[p->nextIoIndex].atTick) {
                        
                        if (p->remainingTime == 0) {
                            p->state = ProcessState::TERMINATED;
                            p->finishTime = tick + 1;
                            sched.onProcessFinished(current[c], tick + 1);
                            current[c] = -1;
                        } else {
                            p->state = ProcessState::WAITING;
                            p->ioReturnTick = tick + 1 + p->ioBlocks[p->nextIoIndex].duration;
                            sched.onProcessBlocked(current[c], tick);
                            current[c] = -1;
                        }
                    }
                    // РџСЂРѕРІРµСЂСЏРµРј РѕР±С‹С‡РЅРѕРµ Р·Р°РІРµСЂС€РµРЅРёРµ
                    else if (p->remainingTime == 0) {
                        p->state = ProcessState::TERMINATED;
                        p->finishTime = tick + 1;
                        sched.onProcessFinished(current[c], tick + 1);
                        current[c] = -1;
                    }
                }
            }
        }

        // 7. РќР°РєР°РїР»РёРІР°РµРј РІСЂРµРјСЏ РѕР¶РёРґР°РЅРёСЏ РґР»СЏ С‚РµС…, РєС‚Рѕ РѕСЃС‚Р°Р»СЃСЏ РІ READY
        for (auto& p : procs) {
            if (p.state == ProcessState::READY) {
                p.waitingTime++;
            }
        }

        // 8. Р—Р°РїРёСЃСЊ РґРёР°РіСЂР°РјРјС‹ Р“Р°РЅС‚Р° РґР»СЏ РєР°Р¶РґРѕРіРѕ СЏРґСЂР° РЅРµР·Р°РІРёСЃРёРјРѕ
        for (int c = 0; c < cores; ++c) {
            int pid = ranPid[c];
            if (pid != -1) {
                if (!res.coreGantt[c].empty() && res.coreGantt[c].back().first == pid) {
                    res.coreGantt[c].back().second.second = tick + 1;
                } else {
                    res.coreGantt[c].push_back({pid, {tick, tick + 1}});
                }
            } else {
                if (!res.coreGantt[c].empty() && res.coreGantt[c].back().first == -1) {
                    res.coreGantt[c].back().second.second = tick + 1;
                } else {
                    res.coreGantt[c].push_back({-1, {tick, tick + 1}});
                }
            }
            prev[c] = ranPid[c];
        }

        tick++;
    }

    // Р Р°СЃС‡С‘С‚ РёС‚РѕРіРѕРІС‹С… СЃСЂРµРґРЅРёС… РјРµС‚СЂРёРє
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
        res.avgWaiting = sumW / finished;
        res.avgTurnaround = sumT / finished;
        res.avgResponse = sumR / finished;
    }
    res.totalTicks = tick;
    res.cpuUtilization = tick > 0 ? (100.0 * busyTicks) / (tick * cores) : 0.0;
    res.throughput = finished;

    return res;
}
