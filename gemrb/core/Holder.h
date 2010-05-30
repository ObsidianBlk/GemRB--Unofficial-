/* GemRB - Infinity Engine Emulator
 * Copyright (C) 2003 The GemRB Project
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *
 */

#ifndef HOLDER_H
#define HOLDER_H

#include <cstddef>

template <class T>
class Held {
public:
	Held() : RefCount(1) {}
	void acquire() { ++RefCount; }
	void release() { if (!--RefCount) delete static_cast<T*>(this); }
	size_t GetRefCount() { return RefCount; }
private:
	size_t RefCount;
};

/**
 * @class Holder
 * Intrusive smart pointer.
 *
 * The class T must have member function acquire and release, such that
 * acquire increases the refcount, and release decreses the refcount and
 * frees the object if needed.
 *
 * Derived class of Holder shouldn't add member variables. That way,
 * they can freely converted to Holder without slicing.
 */

template <class T>
class Holder {
public:
	Holder(T* ptr = NULL)
		: ptr(ptr)
	{
	}
	~Holder()
	{
		if (ptr)
			ptr->release();
	}
	Holder(const Holder& rhs)
		: ptr(rhs.ptr)
	{
		if (ptr)
			ptr->acquire();
	}
	Holder& operator=(const Holder& rhs)
	{
		if (rhs.ptr)
			rhs.ptr->acquire();
		if (ptr)
			ptr->release();
		ptr = rhs.ptr;
		return *this;
	}
	T& operator*() const { return *ptr; }
	T* operator->() const { return ptr; }
	bool operator!() const { return !ptr; }
	// Copied from boost/smart_ptr/detail/operator_bool.hpp
#ifndef _MSC_VER
	operator T* Holder<T>::*() const { return ptr == NULL ? NULL : &Holder<T>::ptr; }
#else // MSVC6
	// FIXME: Figure out what version doesn't need this hack.
	operator bool() const { return ptr != NULL; }
#endif
	T* get() const { return ptr; }
	void release() {
		if (ptr)
			ptr->release();
		ptr = NULL;
	}
protected:
	T *ptr;
};

#endif