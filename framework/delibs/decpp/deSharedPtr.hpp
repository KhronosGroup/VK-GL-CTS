#ifndef _DESHAREDPTR_HPP
#define _DESHAREDPTR_HPP
/*-------------------------------------------------------------------------
 * drawElements C++ Base Library
 * -----------------------------
 *
 * Copyright 2014 The Android Open Source Project
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
 *//*!
 * \file
 * \brief Shared pointer.
 *//*--------------------------------------------------------------------*/

#include "deDefs.hpp"
#include "deAtomic.h"

#include <exception>
#include <algorithm>

namespace de
{

//! Shared pointer self-test.
void SharedPtr_selfTest (void);

class DeadReferenceException : public std::exception
{
public:
	DeadReferenceException (void) throw() : std::exception() {}
	const char* what (void) const throw() { return "DeadReferenceException"; }
};

template<bool threadSafe>
struct ReferenceCount;

template<> struct ReferenceCount<true>	{ typedef volatile int	Type; };
template<> struct ReferenceCount<false>	{ typedef int			Type; };

template<class Deleter, bool threadSafe>
struct SharedPtrState
{
	SharedPtrState (Deleter deleter_)
		: strongRefCount	(0)
		, weakRefCount		(0)
		, deleter			(deleter_)
	{
	}

	typename ReferenceCount<threadSafe>::Type	strongRefCount;
	typename ReferenceCount<threadSafe>::Type	weakRefCount;		//!< WeakPtr references + StrongPtr references.
	Deleter										deleter;
};

template<typename DstDeleterType, typename SrcDeleterType, bool threadSafe>
SharedPtrState<DstDeleterType, threadSafe>* sharedPtrStateCast (SharedPtrState<SrcDeleterType, threadSafe>* state)
{
	return reinterpret_cast<SharedPtrState<DstDeleterType, threadSafe>*>(state);
}

template<typename T, class Deleter, bool threadSafe>
class SharedPtr;

template<typename T, class Deleter, bool threadSafe>
class WeakPtr;

/*--------------------------------------------------------------------*//*!
 * \brief Shared pointer
 *
 * SharedPtr is smart pointer for managing shared ownership to a pointer.
 * Multiple SharedPtr's can maintain ownership to the pointer and it is
 * destructed when last SharedPtr is destroyed.
 *
 * Shared pointers can be assigned (or initialized using copy constructor)
 * and in such case the previous reference is first freed and then a new
 * reference to the new pointer is acquired.
 *
 * SharedPtr can also be empty.
 *
 * If threadSafe template parameter is set to true, it is safe to share
 * data using SharedPtr across threads. SharedPtr object itself is not
 * thread safe and should not be mutated from multiple threads simultaneously.
 *
 * \todo [2012-10-26 pyry] Add custom deleter.
 *//*--------------------------------------------------------------------*/
template<typename T, class Deleter = DefaultDeleter<T>, bool threadSafe = true>
class SharedPtr
{
public:
								SharedPtr			(void);
								SharedPtr			(const SharedPtr<T, Deleter, threadSafe>& other);

	template<typename Y>
	explicit					SharedPtr			(Y* ptr, Deleter deleter = Deleter());

	template<typename Y, class DeleterY>
	explicit					SharedPtr			(const SharedPtr<Y, DeleterY, threadSafe>& other);

	template<typename Y, class DeleterY>
	explicit					SharedPtr			(const WeakPtr<Y, DeleterY, threadSafe>& other);

								~SharedPtr			(void);

	template<typename Y, class DeleterY>
	SharedPtr&					operator=			(const SharedPtr<Y, DeleterY, threadSafe>& other);
	SharedPtr&					operator=			(const SharedPtr<T, Deleter, threadSafe>& other);

	template<typename Y, class DeleterY>
	SharedPtr&					operator=			(const WeakPtr<Y, DeleterY, threadSafe>& other);

	T*							get					(void) const throw() { return m_ptr;	}	//!< Get stored pointer.
	T*							operator->			(void) const throw() { return m_ptr;	}	//!< Get stored pointer.
	T&							operator*			(void) const throw() { return *m_ptr;	}	//!< De-reference pointer.

	operator					bool				(void) const throw() { return !!m_ptr;	}

	void						swap				(SharedPtr<T, Deleter, threadSafe>& other);

	void						clear				(void);

	template<typename Y, class DeleterY>
	operator SharedPtr<Y, DeleterY, threadSafe>	(void) const;

private:
	void						acquire				(void);
	void						acquireFromWeak		(const WeakPtr<T, Deleter, threadSafe>& other);
	void						release				(void);

	T*												m_ptr;
	SharedPtrState<Deleter, threadSafe>*			m_state;

	friend class WeakPtr<T, Deleter, threadSafe>;

	template<typename U, class DeleterU, bool threadSafeU>
	friend class SharedPtr;
};

/*--------------------------------------------------------------------*//*!
 * \brief Weak pointer
 *
 * WeakPtr manages weak references to objects owned by SharedPtr. Shared
 * pointer can be converted to weak pointer and vice versa. Weak pointer
 * differs from SharedPtr by not affecting the lifetime of the managed
 * object.
 *
 * WeakPtr can be converted back to SharedPtr but that operation can fail
 * if the object is no longer live. In such case DeadReferenceException
 * will be thrown.
 *
 * \todo [2012-10-26 pyry] Add custom deleter.
 *//*--------------------------------------------------------------------*/
template<typename T, class Deleter = DefaultDeleter<T>, bool threadSafe = true>
class WeakPtr
{
public:
						WeakPtr				(void);
						WeakPtr				(const WeakPtr<T, Deleter, threadSafe>& other);
	explicit			WeakPtr				(const SharedPtr<T, Deleter, threadSafe>& other);
						~WeakPtr			(void);

	WeakPtr&			operator=			(const WeakPtr<T, Deleter, threadSafe>& other);
	WeakPtr&			operator=			(const SharedPtr<T, Deleter, threadSafe>& other);

	SharedPtr<T, Deleter, threadSafe>	lock	(void);

private:
	void				acquire				(void);
	void				release				(void);

	T*										m_ptr;
	SharedPtrState<Deleter, threadSafe>*	m_state;

	friend class SharedPtr<T, Deleter, threadSafe>;
};

// SharedPtr template implementation.

/*--------------------------------------------------------------------*//*!
 * \brief Construct empty shared pointer.
 *//*--------------------------------------------------------------------*/
template<typename T, class Deleter, bool threadSafe>
inline SharedPtr<T, Deleter, threadSafe>::SharedPtr (void)
	: m_ptr		(DE_NULL)
	, m_state	(DE_NULL)
{
}

/*--------------------------------------------------------------------*//*!
 * \brief Construct shared pointer from pointer.
 * \param ptr Pointer to be managed.
 *
 * Ownership of the pointer will be transferred to SharedPtr and future
 * SharedPtr's initialized or assigned from this SharedPtr.
 *
 * Y* must be convertible to T*.
 *//*--------------------------------------------------------------------*/
template<typename T, class Deleter, bool threadSafe>
template<typename Y>
inline SharedPtr<T, Deleter, threadSafe>::SharedPtr (Y* ptr, Deleter deleter)
	: m_ptr		(DE_NULL)
	, m_state	(DE_NULL)
{
	try
	{
		m_ptr	= ptr;
		m_state	= new SharedPtrState<Deleter, threadSafe>(deleter);
		m_state->strongRefCount	= 1;
		m_state->weakRefCount	= 1;
	}
	catch (...)
	{
		delete m_ptr;
		delete m_state;
		throw;
	}
}

/*--------------------------------------------------------------------*//*!
 * \brief Initialize shared pointer from another SharedPtr.
 * \param other Pointer to be shared.
 *//*--------------------------------------------------------------------*/
template<typename T, class Deleter, bool threadSafe>
inline SharedPtr<T, Deleter, threadSafe>::SharedPtr (const SharedPtr<T, Deleter, threadSafe>& other)
	: m_ptr		(other.m_ptr)
	, m_state	(other.m_state)
{
	acquire();
}

/*--------------------------------------------------------------------*//*!
 * \brief Initialize shared pointer from another SharedPtr.
 * \param other Pointer to be shared.
 *
 * Y* must be convertible to T*.
 *//*--------------------------------------------------------------------*/
template<typename T, class Deleter, bool threadSafe>
template<typename Y, class DeleterY>
inline SharedPtr<T, Deleter, threadSafe>::SharedPtr (const SharedPtr<Y, DeleterY, threadSafe>& other)
	: m_ptr		(other.m_ptr)
	, m_state	(sharedPtrStateCast<Deleter>(other.m_state))
{
	acquire();
}

/*--------------------------------------------------------------------*//*!
 * \brief Initialize shared pointer from weak reference.
 * \param other Pointer to be shared.
 *
 * Y* must be convertible to T*.
 *//*--------------------------------------------------------------------*/
template<typename T, class Deleter, bool threadSafe>
template<typename Y, class DeleterY>
inline SharedPtr<T, Deleter, threadSafe>::SharedPtr (const WeakPtr<Y, DeleterY, threadSafe>& other)
	: m_ptr		(DE_NULL)
	, m_state	(DE_NULL)
{
	acquireFromWeak(other);
}

template<typename T, class Deleter, bool threadSafe>
inline SharedPtr<T, Deleter, threadSafe>::~SharedPtr (void)
{
	release();
}

/*--------------------------------------------------------------------*//*!
 * \brief Assign from other shared pointer.
 * \param other Pointer to be shared.
 * \return Reference to this SharedPtr.
 *
 * Reference to current pointer (if any) will be released first. Then a new
 * reference to the pointer managed by other will be acquired.
 *
 * Y* must be convertible to T*.
 *//*--------------------------------------------------------------------*/
template<typename T, class Deleter, bool threadSafe>
template<typename Y, class DeleterY>
inline SharedPtr<T, Deleter, threadSafe>& SharedPtr<T, Deleter, threadSafe>::operator= (const SharedPtr<Y, DeleterY, threadSafe>& other)
{
	if (*this == other)
		return *this;

	// Release current reference.
	release();

	// Copy from other and acquire reference.
	m_ptr	= other.m_ptr;
	m_state	= sharedPtrStateCast<Deleter>(other.m_state);

	acquire();

	return *this;
}

/*--------------------------------------------------------------------*//*!
 * \brief Assign from other shared pointer.
 * \param other Pointer to be shared.
 * \return Reference to this SharedPtr.
 *
 * Reference to current pointer (if any) will be released first. Then a new
 * reference to the pointer managed by other will be acquired.
 *//*--------------------------------------------------------------------*/
template<typename T, class Deleter, bool threadSafe>
inline SharedPtr<T, Deleter, threadSafe>& SharedPtr<T, Deleter, threadSafe>::operator= (const SharedPtr<T, Deleter, threadSafe>& other)
{
	if (*this == other)
		return *this;

	// Release current reference.
	release();

	// Copy from other and acquire reference.
	m_ptr	= other.m_ptr;
	m_state	= other.m_state;

	acquire();

	return *this;
}

/*--------------------------------------------------------------------*//*!
 * \brief Assign from weak pointer.
 * \param other Weak reference.
 * \return Reference to this SharedPtr.
 *
 * Reference to current pointer (if any) will be released first. Then a
 * reference to pointer managed by WeakPtr is acquired if the pointer
 * is still live (eg. there's at least one strong reference).
 *
 * If pointer is no longer live, DeadReferenceException is thrown.
 *
 * Y* must be convertible to T*.
 *//*--------------------------------------------------------------------*/
template<typename T, class Deleter, bool threadSafe>
template<typename Y, class DeleterY>
inline SharedPtr<T, Deleter, threadSafe>& SharedPtr<T, Deleter, threadSafe>::operator= (const WeakPtr<Y, DeleterY, threadSafe>& other)
{
	// Release current reference.
	release();

	m_ptr	= DE_NULL;
	m_state	= DE_NULL;

	acquireFromWeak(other);

	return *this;
}

/*--------------------------------------------------------------------*//*!
 * \brief Type conversion operator.
 *
 * T* must be convertible to Y*. Since resulting SharedPtr will share the
 * ownership destroying Y* must be equal to destroying T*.
 *//*--------------------------------------------------------------------*/
template<class T, class Deleter, bool threadSafe>
template<typename Y, class DeleterY>
inline SharedPtr<T, Deleter, threadSafe>::operator SharedPtr<Y, DeleterY, threadSafe> (void) const
{
	return SharedPtr<Y, DeleterY, threadSafe>(*this);
}

/*--------------------------------------------------------------------*//*!
 * \brief Compare pointers.
 * \param a A
 * \param b B
 * \return true if A and B point to same object, false otherwise.
 *//*--------------------------------------------------------------------*/
template<class T, class DeleterT, bool threadSafeT, class U, class DeleterU, bool threadSafeU>
inline bool operator== (const SharedPtr<T, DeleterT, threadSafeT>& a, const SharedPtr<U, DeleterU, threadSafeU>& b) throw()
{
	return a.get() == b.get();
}

/*--------------------------------------------------------------------*//*!
 * \brief Compare pointers.
 * \param a A
 * \param b B
 * \return true if A and B point to different objects, false otherwise.
 *//*--------------------------------------------------------------------*/
template<class T, class DeleterT, bool threadSafeT, class U, class DeleterU, bool threadSafeU>
inline bool operator!= (const SharedPtr<T, DeleterT, threadSafeT>& a, const SharedPtr<U, DeleterU, threadSafeU>& b) throw()
{
	return a.get() != b.get();
}

/** Swap pointer contents. */
template<typename T, class Deleter, bool threadSafe>
inline void SharedPtr<T, Deleter, threadSafe>::swap (SharedPtr<T, Deleter, threadSafe>& other)
{
	using std::swap;
	swap(m_ptr,		other.m_ptr);
	swap(m_state,	other.m_state);
}

/** Swap operator for SharedPtr's. */
template<typename T, class Deleter, bool threadSafe>
inline void swap (SharedPtr<T, Deleter, threadSafe>& a, SharedPtr<T, Deleter, threadSafe>& b)
{
	a.swap(b);
}

/*--------------------------------------------------------------------*//*!
 * \brief Set pointer to null.
 *
 * clear() removes current reference and sets pointer to null value.
 *//*--------------------------------------------------------------------*/
template<typename T, class Deleter, bool threadSafe>
inline void SharedPtr<T, Deleter, threadSafe>::clear (void)
{
	release();
	m_ptr	= DE_NULL;
	m_state	= DE_NULL;
}

template<typename T, class Deleter, bool threadSafe>
inline void SharedPtr<T, Deleter, threadSafe>::acquireFromWeak (const WeakPtr<T, Deleter, threadSafe>& weakRef)
{
	DE_ASSERT(!m_ptr && !m_state);

	SharedPtrState<Deleter, threadSafe>* state = weakRef.m_state;

	if (!state)
		return; // Empty reference.

	if (threadSafe)
	{
		int oldCount, newCount;

		// Do atomic compare and increment.
		do
		{
			oldCount = state->strongRefCount;
			if (oldCount == 0)
				throw DeadReferenceException();
			newCount = oldCount+1;
		} while (deAtomicCompareExchange32((deUint32 volatile*)&state->strongRefCount, (deUint32)oldCount, (deUint32)newCount) != (deUint32)oldCount);

		deAtomicIncrement32(&state->weakRefCount);
	}
	else
	{
		if (state->strongRefCount == 0)
			throw DeadReferenceException();

		state->strongRefCount	+= 1;
		state->weakRefCount		+= 1;
	}

	m_ptr	= weakRef.m_ptr;
	m_state	= state;
}

template<typename T, class Deleter, bool threadSafe>
inline void SharedPtr<T, Deleter, threadSafe>::acquire (void)
{
	if (m_state)
	{
		if (threadSafe)
		{
			deAtomicIncrement32((deInt32 volatile*)&m_state->strongRefCount);
			deAtomicIncrement32((deInt32 volatile*)&m_state->weakRefCount);
		}
		else
		{
			m_state->strongRefCount	+= 1;
			m_state->weakRefCount	+= 1;
		}
	}
}

template<typename T, class Deleter, bool threadSafe>
inline void SharedPtr<T, Deleter, threadSafe>::release (void)
{
	if (m_state)
	{
		if (threadSafe)
		{
			if (deAtomicDecrement32(&m_state->strongRefCount) == 0)
			{
				m_state->deleter(m_ptr);
				m_ptr = DE_NULL;
			}

			if (deAtomicDecrement32(&m_state->weakRefCount) == 0)
			{
				delete m_state;
				m_state = DE_NULL;
			}
		}
		else
		{
			m_state->strongRefCount	-= 1;
			m_state->weakRefCount	-= 1;
			DE_ASSERT(m_state->strongRefCount >= 0 && m_state->weakRefCount >= 0);

			if (m_state->strongRefCount == 0)
			{
				m_state->deleter(m_ptr);
				m_ptr = DE_NULL;
			}

			if (m_state->weakRefCount == 0)
			{
				delete m_state;
				m_state = DE_NULL;
			}
		}
	}
}

// WeakPtr template implementation.

/*--------------------------------------------------------------------*//*!
 * \brief Construct empty weak pointer.
 *//*--------------------------------------------------------------------*/
template<typename T, class Deleter, bool threadSafe>
inline WeakPtr<T, Deleter, threadSafe>::WeakPtr (void)
	: m_ptr		(DE_NULL)
	, m_state	(DE_NULL)
{
}

/*--------------------------------------------------------------------*//*!
 * \brief Construct weak pointer from other weak reference.
 * \param other Weak reference.
 *//*--------------------------------------------------------------------*/
template<typename T, class Deleter, bool threadSafe>
inline WeakPtr<T, Deleter, threadSafe>::WeakPtr (const WeakPtr<T, Deleter, threadSafe>& other)
	: m_ptr		(other.m_ptr)
	, m_state	(other.m_state)
{
	acquire();
}

/*--------------------------------------------------------------------*//*!
 * \brief Construct weak pointer from shared pointer.
 * \param other Shared pointer.
 *//*--------------------------------------------------------------------*/
template<typename T, class Deleter, bool threadSafe>
inline WeakPtr<T, Deleter, threadSafe>::WeakPtr (const SharedPtr<T, Deleter, threadSafe>& other)
	: m_ptr		(other.m_ptr)
	, m_state	(other.m_state)
{
	acquire();
}

template<typename T, class Deleter, bool threadSafe>
inline WeakPtr<T, Deleter, threadSafe>::~WeakPtr (void)
{
	release();
}

/*--------------------------------------------------------------------*//*!
 * \brief Assign from another weak pointer.
 * \param other Weak reference.
 * \return Reference to this WeakPtr.
 *
 * The current weak reference is removed first and then a new weak reference
 * to the object pointed by other is taken.
 *//*--------------------------------------------------------------------*/
template<typename T, class Deleter, bool threadSafe>
inline WeakPtr<T, Deleter, threadSafe>& WeakPtr<T, Deleter, threadSafe>::operator= (const WeakPtr<T, Deleter, threadSafe>& other)
{
	if (this == &other)
		return *this;

	release();

	m_ptr	= other.m_ptr;
	m_state	= other.m_state;

	acquire();

	return *this;
}

/*--------------------------------------------------------------------*//*!
 * \brief Assign from shared pointer.
 * \param other Shared pointer.
 * \return Reference to this WeakPtr.
 *
 * The current weak reference is removed first and then a new weak reference
 * to the object pointed by other is taken.
 *//*--------------------------------------------------------------------*/
template<typename T, class Deleter, bool threadSafe>
inline WeakPtr<T, Deleter, threadSafe>& WeakPtr<T, Deleter, threadSafe>::operator= (const SharedPtr<T, Deleter, threadSafe>& other)
{
	release();

	m_ptr	= other.m_ptr;
	m_state	= other.m_state;

	acquire();

	return *this;
}

template<typename T, class Deleter, bool threadSafe>
inline void WeakPtr<T, Deleter, threadSafe>::acquire (void)
{
	if (m_state)
	{
		if (threadSafe)
			deAtomicIncrement32(&m_state->weakRefCount);
		else
			m_state->weakRefCount += 1;
	}
}

template<typename T, class Deleter, bool threadSafe>
inline void WeakPtr<T, Deleter, threadSafe>::release (void)
{
	if (m_state)
	{
		if (threadSafe)
		{
			if (deAtomicDecrement32(&m_state->weakRefCount) == 0)
			{
				delete m_state;
				m_state	= DE_NULL;
				m_ptr	= DE_NULL;
			}
		}
		else
		{
			m_state->weakRefCount -= 1;
			DE_ASSERT(m_state->weakRefCount >= 0);

			if (m_state->weakRefCount == 0)
			{
				delete m_state;
				m_state	= DE_NULL;
				m_ptr	= DE_NULL;
			}
		}
	}
}

} // de

#endif // _DESHAREDPTR_HPP
