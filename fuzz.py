#!python3
from random import *
from typing import List
import os, sys, copy

#========================================

def gen_base(size: int) -> bytearray:
    res = bytearray(size)
    t = random()
    if t < 0.2:
        val = 0 if random() < 0.3 else randint(0, 255)
        for i in range(size):
            res[i] = val
    elif t < 0.4:
        for i in range(size):
            res[i] = i & 255
    elif t < 0.6:
        per = randint(1024, 4096)
        for i in range(size):
            res[i] = (i % per + 13) & 255
    else:
        res = bytearray(os.urandom(size))
    return res

def mix_inputs(arr0: bytearray, arr1: bytearray) -> bytearray:
    t = random()
    size = len(arr0)
    assert(size == len(arr1))
    if size == 0:
        return arr0
    if t < 0.2:
        pos = randint(0, size-1)
        return arr0[0:pos] + arr1[0:size-pos]
    elif t < 0.4:
        [s, e] = sorted([randint(0, size-1), randint(0, size-1)])
        return arr0[0:s] + arr1[0:e-s] + arr0[s:s+size-e]
    elif t < 0.6:
        for i in range(size):
            arr0[i] = arr0[i] ^ arr1[i]
        return arr0
    else:
        res = bytearray()
        i, j = 0, 0
        logmax = randint(6, 12)
        while len(res) < size:
            w = randint(0, 1)
            sz = int(2.0 ** (random() * logmax)) + 1
            if w == 0:
                sz = min(sz, len(arr0) - i)
                res += arr0[i:i+sz]
                i += sz
            else:
                sz = min(sz, len(arr1) - j)
                res += arr1[j:j+sz]
                j += sz
        return res[0:size]

def mutate_input(arr: bytearray) -> bytearray:
    t = random()
    size = len(arr)
    if t < 0.5:
        part = int(2.0 ** uniform(3.0, 15.0))
        places = sample(range(size), max(size // part, 1))
        for k in places:
            arr[k] = randint(0, 255)
        return arr
    else:
        [l,r] = sorted([randint(0, size-1), randint(0, size-1)])
        return arr[0:l] + bytearray(reversed(arr[l:r])) + arr[r:size]

def mutate_local(arr: bytearray) -> bytearray:
    if len(arr) == 0:
        return gen_base(1024)
    t = random()
    size = len(arr)
    if t < 0.2:
        cnt = randint(1, 3) if random() < 0.5 else size // (1 + int(2 ** (uniform(3.0, 16.0))))
        cnt = max(min(cnt, size), 1)
        for k in sample(range(size), cnt):
            arr[k] = randint(0, 255)
    elif t < 0.6:
        insert = random() < 0.5
        for i in range(randint(1, 3)):
            if len(arr) == 0:
                break
            sz = int(2.0 ** uniform(1.0, 15.0))
            if insert:
                pos = randint(0, len(arr))
                sz = min(sz, len(arr) * 2)
                arr = arr[:pos] + gen_base(sz) + arr[pos:]
            else:
                sz = max(min(sz, len(arr) - 1), 1)
                if random() < 0.5:
                    sz = len(arr) - sz
                pos = randint(0, len(arr) - sz)
                arr = arr[:pos] + arr[pos+sz:]
    elif t < 0.8:
        cnt = randint(1, 3) if random() < 0.7 else randint(100, 300)
        for i in range(cnt):
            src = randint(0, len(arr))
            sz = min(int(2.0 ** uniform(10.0, 16.0)), len(arr)-src)
            part = arr[src:src+sz]
            if random() < 0.8:
                arr = arr[:src] + arr[src+sz:]
            dst = randint(0, len(arr))
            arr = arr[:dst] + part + arr[dst+sz:]
    else:
        arr = mix_inputs(arr, gen_base(size))
    return arr

def gen_input() -> bytearray:
    size = int(2.0 ** uniform(8.0, 22.0))
    data = gen_base(size)
    for i in range(randint(0, 3)):
        add = gen_base(size)
        data = mix_inputs(data, add)
    for i in range(randint(0, 3)):
        data = mutate_input(data)
    return data

def gen_local(arr: bytearray) -> bytearray:
    res = copy.copy(arr)
    for i in range(randint(0, 2)):
        res = mutate_local(res)
    return res

#========================================

g_local = False     # if true, then -file local update is tested
g_port = 8001       # default port number in cherryserv.py

def test_single(orig: bytearray, src: str, dst: str) -> bool:
    mod = gen_local(orig)
    with open(src, 'wb') as f:
        f.write(orig)
    with open(dst, 'wb') as f:
        f.write(mod)
    err = os.system('tdmsync prepare %s' % src)
    if err != 0:
        return False
    if g_local:
        cmd = 'tdmsync update -file %s %s 2>nul' % (src, dst)
    else:
        cmd = 'tdmsync update -url http://localhost:%d/%s %s 2>nul' % (g_port, src, dst)
    err = os.system(cmd)
    if err != 0:
        return False
    got = b""
    with open(dst + '.updated', 'rb') as f:
        got = f.read()
    return orig == got

while True:
    orig = gen_input()
    for k in range(10):
        ok = test_single(orig, 'fuzz_src.dat', 'fuzz_dst.dat')
        if not ok:
            sys.exit(0)
            
