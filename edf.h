#pragma once
#include "scheduler.h"
#include <algorithm>
#include <vector>

// РџР»Р°РЅРёСЂРѕРІС‰РёРє СЂРµР°Р»СЊРЅРѕРіРѕ РІСЂРµРјРµРЅРё Earliest Deadline First (EDF)
class EdfScheduler : public Scheduler {
public:
    using Scheduler::Scheduler;

    std::string name() const override { 
        return "EDF"; 
    }

    void onTick(std::uint64_t tick) override {
        for (auto& p : processes_) {
            if (p.arrivalTime == tick && p.state == ProcessState::NEW) {
                p.state = ProcessState::READY;
                ready_.push_back(p.pid);
            }
        }
    }

    void onProcessReady(int pid, std::uint64_t) override {
        Process* p = find(pid);
        if (p) {
            p->state = ProcessState::READY;
            ready_.push_back(pid);
        }
    }

    int pickNext(std::uint64_t) override {
        if (ready_.empty()) return -1;

        // Р’С‹Р±РёСЂР°РµРј РїСЂРѕС†РµСЃСЃ СЃ РќРђР�РњР•РќР¬РЁР�Рњ Р°Р±СЃРѕР»СЋС‚РЅС‹Рј РґРµРґР»Р°Р№РЅРѕРј
        auto it = std::min_element(ready_.begin(), ready_.end(), [&](int a, int b) {
            return find(a)->deadline < find(b)->deadline;
        });

        int pid = *it;
        ready_.erase(it);
        return pid;
    }

    bool shouldPreempt(int currentPid, std::uint64_t) override {
        Process* cur = find(currentPid);
        if (!cur || cur->isFinished()) return false;

        // Р’С‹С‚РµСЃРЅСЏРµРј, РµСЃР»Рё Сѓ РєРѕРіРѕ-С‚Рѕ РІ READY РґРµРґР»Р°Р№РЅ РЎРўР РћР“Рћ РњР•РќР¬РЁР•, С‡РµРј Сѓ С‚РµРєСѓС‰РµРіРѕ
        for (int pid : ready_) {
            if (find(pid)->deadline < cur->deadline) {
                return true; 
            }
        }
        return false;
    }

    void onProcessFinished(int pid, std::uint64_t) override {
        Process* p = find(pid);
        if (p) p->state = ProcessState::TERMINATED;
    }

    void onProcessBlocked(int pid, std::uint64_t) override {
        Process* p = find(pid);
        if (p) p->state = ProcessState::WAITING;
    }

    void onProcessPreempted(int pid, std::uint64_t) override {
        Process* p = find(pid);
        if (p) {
            p->state = ProcessState::READY;
            ready_.push_back(pid);
        }
    }

private:
    std::vector<int> ready_;
};
