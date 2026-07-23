#pragma once

#include "blockos_config.h" // A projekt konfigurációs fájlja [source: 1]

namespace Blockos {

class Spinlock {
private:
    // 0 = szabad, 1 = foglalt. 
    // A volatile kulcsszó kötelező, hogy a fordító ne optimalizálja ki a ciklust.
    volatile int m_lock_state;
    
    // Ide mentjük el az adott CPU mag megszakítási állapotát (RFLAGS regiszter)
    unsigned long m_rflags_state;

public:
    // Konstruktor: a zár kezdetben szabad
    constexpr Spinlock() : m_lock_state(0), m_rflags_state(0) {}

    // Másolás tiltása (egy zárat nem másolhatunk a memóriában)
    Spinlock(const Spinlock&) = delete;
    Spinlock& operator=(const Spinlock&) = delete;

    // Megszerzi a zárat. Ha foglalt, pörög (spin) a CPU, amíg fel nem szabadul.
    void lock();

    // Megpróbálja megszerezni a zárat pörgés nélkül. 
    // Igazzal tér vissza, ha sikerült, hamissal, ha foglalt volt.
    bool try_lock();

    // Elengedi a zárat és visszaállítja a megszakítások állapotát.
    void unlock();
};

// Automatikus, biztonságos hatókör-alapú zárkezelő (RAII minta)
class ScopedLock {
private:
    Spinlock& m_spinlock;

public:
    explicit ScopedLock(Spinlock& lock) : m_spinlock(lock) {
        m_spinlock.lock();
    }
    ~ScopedLock() {
        m_spinlock.unlock();
    }
};

} // namespace Blockos
