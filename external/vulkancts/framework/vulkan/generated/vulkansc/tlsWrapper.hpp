#ifndef _TLSWRAPPER_HPP
#define _TLSWRAPPER_HPP

/*
 * Copyright (c) 2021 The Khronos Group Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 *//*!
 * \file
 * \brief Defines TLSContainer for CTS
 */

#include <mutex>
#include <thread>

template <class T>
class TLSContainer
{
    struct item
    {
        std::thread::id owner;
        T *p;
    };
    std::mutex mutex;
    std::vector<item> all;

public:
    T *find(bool remove)
    {
        T *r                = nullptr;
        std::thread::id tid = std::this_thread::get_id();
        mutex.lock();
        for (uint32_t i = 0; i < all.size(); i++)
        {
            if (all[i].owner == tid)
            {
                r = all[i].p;
                if (remove)
                {
                    all.erase(all.begin() + i);
                }
                break;
            }
        }
        mutex.unlock();
        return r;
    }

    void add(T *p)
    {
        item i;
        i.owner = std::this_thread::get_id();
        i.p     = p;
        mutex.lock();
        all.push_back(i);
        mutex.unlock();
    }
    TLSContainer()
    {
    }
    ~TLSContainer()
    {
    }

    static TLSContainer<T> instance;
};

template <class T>
class TLSWrapper
{
public:
    TLSWrapper()
    {
    }
    ~TLSWrapper()
    {
        T *p = TLSContainer<T>::instance.find(true);
        if (p)
        {
            delete p;
        }
    }
    T &attach()
    {
        T *p = TLSContainer<T>::instance.find(false);
        if (!p)
        {
            p = new T();
            TLSContainer<T>::instance.add(p);
        }
        return *p;
    }
    static thread_local TLSWrapper<T> instance;
};

#define TLS_INSTANCE()                         \
    template <class T>                         \
    TLSContainer<T> TLSContainer<T>::instance; \
    template <class T>                         \
    thread_local TLSWrapper<T> TLSWrapper<T>::instance

#endif // _TLSWRAPPER_HPP
