// libTorrent - BitTorrent library
// Copyright (C) 2005-2011, Jari Sundell
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// In addition, as a special exception, the copyright holders give
// permission to link the code of portions of this program with the
// OpenSSL library under certain conditions as described in each
// individual source file, and distribute linked combinations
// including the two.
//
// You must obey the GNU General Public License in all respects for
// all of the code used other than OpenSSL.  If you modify file(s)
// with this exception, you may extend this exception to your version
// of the file(s), but you are not obligated to do so.  If you do not
// wish to do so, delete this exception statement from your version.
// If you delete this exception statement from all source files in the
// program, then also delete it here.
//
// Contact:  Jari Sundell <jaris@ifi.uio.no>
//
//           Skomakerveien 33
//           3185 Skoppum, NORWAY

#ifndef LIBTORRENT_UTILS_THREAD_BASE_H
#define LIBTORRENT_UTILS_THREAD_BASE_H

#include <pthread.h>
#include <sys/types.h>
#include <torrent/common.h>

namespace torrent {

class Poll;

class LIBTORRENT_EXPORT lt_cacheline_aligned thread_base {
public:
  typedef void* (*pthread_func)(void*);

  enum state_type {
    STATE_UNKNOWN,
    STATE_INITIALIZED,
    STATE_ACTIVE,
    STATE_INACTIVE
  };

  static const int flag_do_shutdown  = 0x1;
  static const int flag_did_shutdown = 0x2;
  static const int flag_no_timeout   = 0x4;
  static const int flag_polling      = 0x8;

  thread_base();
  virtual ~thread_base() {}

  bool                is_initialized() const { return m_state == STATE_INITIALIZED; }
  bool                is_active()      const { return m_state == STATE_ACTIVE; }
  bool                is_inactive()    const { return m_state == STATE_INACTIVE; }

  bool                is_polling()     const { return (m_flags & flag_polling); }

  bool                has_no_timeout()   const { return (m_flags & flag_no_timeout); }
  bool                has_do_shutdown()  const { return (m_flags & flag_do_shutdown); }
  bool                has_did_shutdown() const { return (m_flags & flag_did_shutdown); }

  state_type          state() const { return m_state; }
  int                 flags() const { return m_flags; }

  Poll*               poll() { return m_poll; }

  virtual void        init_thread() = 0;

  virtual void        start_thread();
  virtual void        stop_thread();

  void                interrupt();

  static inline int   global_queue_size() { return m_global.waiting; }

  static inline void  acquire_global_lock();
  static inline bool  trylock_global_lock();
  static inline void  release_global_lock();
  static inline void  waive_global_lock();

  static inline bool  is_main_polling() { return m_global.main_polling; }
  static inline void  entering_main_polling();
  static inline void  leaving_main_polling();

  static void*        event_loop(thread_base* thread);

protected:
  struct global_lock_type {
    int             waiting;
    int             main_polling;
    pthread_mutex_t lock;
  };

  virtual void        call_events() = 0;
  virtual int64_t     next_timeout_usec() = 0;

  static global_lock_type m_global;

  pthread_t           m_thread;
  state_type          m_state;
  int                 m_flags;

  Poll*               m_poll;
};

inline void
thread_base::acquire_global_lock() {
  __sync_add_and_fetch(&thread_base::m_global.waiting, 1);
  pthread_mutex_lock(&thread_base::m_global.lock);
  __sync_sub_and_fetch(&thread_base::m_global.waiting, 1);
}

inline bool
thread_base::trylock_global_lock() {
  return pthread_mutex_trylock(&thread_base::m_global.lock) == 0;
}

inline void
thread_base::release_global_lock() {
  pthread_mutex_unlock(&thread_base::m_global.lock);
}

inline void
thread_base::waive_global_lock() {
  pthread_mutex_unlock(&thread_base::m_global.lock);

  // Do we need to sleep here? Make a CppUnit test for this.
  acquire_global_lock();
}

// 'entering/leaving_main_polling' is used by the main polling thread
// to indicate to other threads when it is safe to change the main
// thread's event entries.
//
// A thread should first aquire global lock, then if it needs to
// change poll'ed sockets on the main thread it should call
// 'interrupt_main_polling' unless 'is_main_polling() == false'.
inline void
thread_base::entering_main_polling() {
  __sync_lock_test_and_set(&thread_base::m_global.main_polling, 1);
}

inline void
thread_base::leaving_main_polling() {
  __sync_lock_test_and_set(&thread_base::m_global.main_polling, 0);
}

}  

#endif