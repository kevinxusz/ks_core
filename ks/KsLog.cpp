/*
   Copyright (C) 2015 Preet Desai (preet.desai@gmail.com)

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include <ks/KsLog.hpp>

#ifdef KS_ENV_ANDROID
#include <android/log.h>
#endif

namespace ks
{
    namespace Log
    {
        #ifdef KS_ENV_ANDROID
            void SinkToLogCat::log(std::string const &line)
            {
                m_mutex.lock();
                __android_log_print(ANDROID_LOG_VERBOSE,"ks",line.c_str());
                m_mutex.unlock();
            }
        #endif

        // ============================================================= //

        FBRunTimeMs::FBRunTimeMs() :
            m_time_str("00:00:00.000"),
            m_start(std::chrono::system_clock::now()),
            m_list_num_chars({{'0','1','2','3','4','5','6','7','8','9'}})
        {
            // empty
        }

        FBRunTimeMs::~FBRunTimeMs()
        {
            // empty
        }

        std::string FBRunTimeMs::Get()
        {
            // TODO: should we use steady_clock, not system clock?
            auto now = std::chrono::system_clock::now();
            std::chrono::system_clock::duration elapsed = now-m_start;

            std::chrono::hours hours =
                    std::chrono::duration_cast<std::chrono::hours>(
                        elapsed);
            elapsed -= hours;

            std::chrono::minutes mins =
                    std::chrono::duration_cast<std::chrono::minutes>(
                        elapsed);
            elapsed -= mins;

            std::chrono::seconds secs =
                    std::chrono::duration_cast<std::chrono::seconds>(
                        elapsed);
            elapsed -= secs;

            std::chrono::milliseconds ms =
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        elapsed);

            uint_fast8_t const hours_count = hours.count();
            uint_fast8_t const mins_count = mins.count();
            uint_fast8_t const secs_count = secs.count();
            uint_fast16_t const ms_count = ms.count();

            m_time_str[0] = m_list_num_chars[hours_count/10];
            m_time_str[1] = m_list_num_chars[hours_count%10];

            m_time_str[3] = m_list_num_chars[mins_count/10];
            m_time_str[4] = m_list_num_chars[mins_count%10];

            m_time_str[6] = m_list_num_chars[secs_count/10];
            m_time_str[7] = m_list_num_chars[secs_count%10];

            m_time_str[9] = m_list_num_chars[ms_count/100];
            m_time_str[10] = m_list_num_chars[(ms_count%100)/10];
            m_time_str[11] = m_list_num_chars[(ms_count%100)%10];

            return m_time_str;
        }

        FBCustomStr::FBCustomStr(std::string const &s) : m_s(s)
        {
            // empty
        }

        FBCustomStr::~FBCustomStr()
        {
            // empty
        }

        std::string FBCustomStr::Get()
        {
            return m_s;
        }

        // ============================================================= //

        Logger::Logger()
        {
            m_mutex = make_unique<MutexSTL>();

            // default filter is all on
            m_filter.set(static_cast<size_t>(Level::TRACE));
            m_filter.set(static_cast<size_t>(Level::DEBUG));
            m_filter.set(static_cast<size_t>(Level::INFO));
            m_filter.set(static_cast<size_t>(Level::WARN));
            m_filter.set(static_cast<size_t>(Level::ERROR));
            m_filter.set(static_cast<size_t>(Level::FATAL));
        }

        Logger::Logger(bool thread_safe,
                       shared_ptr<Sink> const &sink,
                       std::array<std::vector<FormatBlock*>,6> && list_fbs)
        {
            if(thread_safe) {
                m_mutex = make_unique<MutexSTL>();
            }
            else {
                m_mutex = make_unique<MutexDummy>();
            }

            // save sink
            m_list_sinks.push_back(sink);

            // save format blocks
            for(uint level=0; level < 6; level++) {
                for(uint fb_idx=0; fb_idx < list_fbs[level].size(); fb_idx++) {
                    m_list_fb[level].push_back(
                                unique_ptr<FormatBlock>(list_fbs[level][fb_idx]));
                }
            }

            // default filter is all on
            m_filter.set(static_cast<size_t>(Level::TRACE));
            m_filter.set(static_cast<size_t>(Level::DEBUG));
            m_filter.set(static_cast<size_t>(Level::INFO));
            m_filter.set(static_cast<size_t>(Level::WARN));
            m_filter.set(static_cast<size_t>(Level::ERROR));
            m_filter.set(static_cast<size_t>(Level::FATAL));
        }

        bool Logger::AddSink(shared_ptr<Sink> const &new_sink)
        {
            m_mutex->lock();

            for(size_t i=0; i < m_list_sinks.size(); i++) {
                shared_ptr<Sink> const &sink =
                        m_list_sinks[i];

                if(sink == new_sink) {
                    m_mutex->unlock();
                    return false;
                }
            }

            m_list_sinks.push_back(new_sink);

            m_mutex->unlock();
            return true;
        }

        bool Logger::RemoveSink(shared_ptr<Sink> const &sink)
        {
            m_mutex->lock();

            for(auto sink_it = m_list_sinks.begin();
                sink_it != m_list_sinks.end();
                ++sink_it)
            {
                if((*sink_it) == sink) {
                    m_list_sinks.erase(sink_it);
                    m_mutex->unlock();
                    return true;
                }
            }
            m_mutex->unlock();
            return false;
        }

        void Logger::SetLevel(Level level)
        {
            m_mutex->lock();
            m_filter.set(static_cast<size_t>(level));
            m_mutex->unlock();
        }

        void Logger::UnsetLevel(Level level)
        {
            m_mutex->lock();
            m_filter.reset(static_cast<size_t>(level));
            m_mutex->unlock();
        }

        void Logger::AddFormatBlock(unique_ptr<FormatBlock> fb,
                                    Level level)
        {
            m_mutex->lock();
            m_list_fb[static_cast<size_t>(level)].push_back(
                        std::move(fb));
            m_mutex->unlock();
        }

        // logging methods
        Logger::Line Logger::Custom(Level level)
        {
            m_mutex->lock();

            size_t const level_int = static_cast<size_t>(level);

            return Line(&m_list_sinks,
                        &(m_list_fb[level_int]),
                        m_mutex.get(),
                        m_filter[level_int]);
        }


        Logger::Line Logger::Trace()
        {
            m_mutex->lock();

            size_t const level = static_cast<size_t>(Level::TRACE);

            return Line(&m_list_sinks,
                        &(m_list_fb[level]),
                        m_mutex.get(),
                        m_filter[level]);
        }

        Logger::Line Logger::Debug()
        {
            m_mutex->lock();

            size_t const level = static_cast<size_t>(Level::DEBUG);

            return Line(&m_list_sinks,
                        &(m_list_fb[level]),
                        m_mutex.get(),
                        m_filter[level]);
        }

        Logger::Line Logger::Info()
        {
            m_mutex->lock();

            size_t const level = static_cast<size_t>(Level::INFO);

            return Line(&m_list_sinks,
                        &(m_list_fb[level]),
                        m_mutex.get(),
                        m_filter[level]);
        }

        Logger::Line Logger::Warn()
        {
            m_mutex->lock();

            size_t const level = static_cast<size_t>(Level::WARN);

            return Line(&m_list_sinks,
                        &(m_list_fb[level]),
                        m_mutex.get(),
                        m_filter[level]);
        }

        Logger::Line Logger::Error()
        {
            m_mutex->lock();

            size_t const level = static_cast<size_t>(Level::ERROR);

            return Line(&m_list_sinks,
                        &(m_list_fb[level]),
                        m_mutex.get(),
                        m_filter[level]);
        }

        Logger::Line Logger::Fatal()
        {
            m_mutex->lock();

            size_t const level = static_cast<size_t>(Level::FATAL);

            return Line(&m_list_sinks,
                        &(m_list_fb[level]),
                        m_mutex.get(),
                        m_filter[level]);
        }

        // ============================================================= //

    } // Log

    Log::Logger LOG(
            true, // thread-safe
#ifdef KS_ENV_ANDROID
            make_shared<Log::SinkToLogCat>(),
#else
            make_shared<Log::SinkToStdOut>(), // log to stdout by default
#endif
            {{ // array init-list
               { new Log::FBRunTimeMs(), new Log::FBCustomStr(": TRACE: KS: ") },
               { new Log::FBRunTimeMs(), new Log::FBCustomStr(": DEBUG: KS: ") },
               { new Log::FBRunTimeMs(), new Log::FBCustomStr(": INFO:  KS: ") },
               { new Log::FBRunTimeMs(), new Log::FBCustomStr(": WARN:  KS: ") },
               { new Log::FBRunTimeMs(), new Log::FBCustomStr(": ERROR: KS: ") },
               { new Log::FBRunTimeMs(), new Log::FBCustomStr(": FATAL: KS: ") }
             }});

//            {{ // array init-list
//               { new Log::FBCustomStr("TRACE: KS: ") },
//               { new Log::FBCustomStr("DEBUG: KS: ") },
//               { new Log::FBCustomStr("INFO:  KS: ") },
//               { new Log::FBCustomStr("WARN:  KS: ") },
//               { new Log::FBCustomStr("ERROR: KS: ") },
//               { new Log::FBCustomStr("FATAL: KS: ") }
//             }});

} // ks
