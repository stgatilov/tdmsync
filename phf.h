#ifndef _TDM_PHF_H_816840_
#define _TDM_PHF_H_816840_

#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <vector>
#include <random>

namespace TdmPhf {

typedef std::mt19937 RndGen;

std::string assertFailedMessage(const char *code, const char *file, int line) {
    char buff[256];
    sprintf(buff, "Assertion %s failed in %s on line %d", code, file, line);
    return buff;
}
#define TdmPhfAssert(cond) if (!(cond)) throw std::runtime_error(assertFailedMessage(#cond, __FILE__, __LINE__));

//almost-universal hash function for integers
//https://en.wikipedia.org/wiki/Universal_hashing#Avoiding_modular_arithmetic
struct IntegerUhf {
    static const size_t PRIME = INT_MAX;

    size_t multLo = 0, multHi = 0;
    size_t addLo = 0, addHi = 0;
    size_t mask = 0;

    void create(RndGen &rnd, size_t logSize) {
        multLo = std::uniform_int_distribution<size_t>(0, PRIME - 1)(rnd);
        multHi = std::uniform_int_distribution<size_t>(0, PRIME - 1)(rnd);
        addLo = std::uniform_int_distribution<size_t>(0, PRIME - 1)(rnd);
        addHi = std::uniform_int_distribution<size_t>(0, PRIME - 1)(rnd);
        mask = (size_t(1) << logSize) - 1;
    }
    //note: all keys must be less than 2^16 * PRIME ~= 2^46
    inline size_t evaluate (size_t key) const {
        size_t lo = key & 0xFFFFU;
        size_t hi = key >> 16U;
        size_t hashLo = (multLo * uint64_t(lo) + addLo) % PRIME;
        size_t hashHi = (multHi * uint64_t(hi) + addHi) % PRIME;
        return (hashLo ^ hashHi) & mask;
    }
};


//perfect hash function, graph-based
//http://cmph.sourceforge.net/papers/chm92.pdf
struct PerfectHashFunc {
    typedef uint32_t Key;
    typedef IntegerUhf HashFunc;

    size_t logSize, mask;
    HashFunc funcs[2];
    std::vector<uint32_t> data;

    inline uint32_t evaluate(Key key) const {
        size_t a = funcs[0].evaluate(key);
        size_t b = funcs[1].evaluate(key);
        size_t res = data[a] ^ data[b];
        return res;
    }

    void create(const uint32_t *keys, size_t num) {
        logSize = 5;
        while ((1ULL << logSize) < 3 * num)
            logSize++;
        size_t cells = 1ULL << logSize;
        mask = cells - 1;
        //fprintf(stderr, "%d / %d\n", (int)num, (int)cells);

        RndGen rnd;
        bool ok;
        do {
            funcs[0].create(rnd, logSize);
            funcs[1].create(rnd, logSize);

            struct Edge {
                size_t end;
                size_t value;
            };
            std::vector<std::vector<Edge>> edgeLists(cells);

            for (size_t i = 0; i < num; i++) {
                //detect and avoid duplicates (note: keys must be sorted)
                if (i && keys[i] == keys[i-1])
                    continue;
                uint32_t key = keys[i];
                size_t a = funcs[0].evaluate(key);
                size_t b = funcs[1].evaluate(key);
                edgeLists[a].push_back(Edge{b, i});
                edgeLists[b].push_back(Edge{a, i});
            }
            
            data.assign(cells, 0);
            ok = true;

            std::vector<char> visited(cells, false);
            //series of BFS over graph
            for (size_t s = 0; ok && s < cells; s++) if (!visited[s]) {
                data[s] = 0;
                visited[s] = true;
                std::vector<size_t> qarr;
                qarr.push_back(s);

                for (size_t i = 0; ok && i < qarr.size(); i++) {
                    size_t u = qarr[i];
                    for (const Edge &e : edgeLists[u]) {
                        size_t v = e.end;
                        if (!visited[v]) {
                            data[v] = e.value ^ data[u];
                            visited[v] = true;
                            qarr.push_back(v);
                        }
                        if ((data[u] ^ data[v]) != e.value)
                            ok = false;
                    }
                }
            }

            if (ok) {
                for (size_t i = 0; i < num; i++) {
                    if (i && keys[i] == keys[i-1])
                        continue;
                    TdmPhfAssert(evaluate(keys[i]) == i);
                }
            }

        } while (!ok);
    }
};

}

#endif
