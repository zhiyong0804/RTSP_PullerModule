/**
 * @file MutexLock.h
 * @brief 线程锁实现
 * @author kofera.deng <dengyi@comtom.cn>
 * @version 1.0.0
 * @date 2013-12-30
 */

#ifndef MUTEX_LOCK_H
#define MUTEX_LOCK_H

#include <pthread.h>

/**
 * @brief 线程锁类
 */
class CMutexLock
{
	public:
		CMutexLock (pthread_mutex_t* mutex) : m_mutex(mutex), m_locked(true)
		{
			pthread_mutex_lock(m_mutex);
		}

		~CMutexLock()
		{
			if (m_locked)
				pthread_mutex_unlock(m_mutex);
		}

		inline void enter() 
		{
			if (!m_locked)
			{
				m_locked = true;
				pthread_mutex_lock(m_mutex);
			}
		}

		inline void leave()
		{
			if (m_locked)
			{
				pthread_mutex_unlock(m_mutex);
				m_locked = false;
			}
		}

	private:
		CMutexLock(const CMutexLock&);
		CMutexLock& operator=(const CMutexLock&);

		bool m_locked;
		pthread_mutex_t *m_mutex;
};

#endif 
