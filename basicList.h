#pragma once

#include <Arduino.h>

// Minimal class to replace std::list

// #define DEBUG

#ifdef DEBUG
#define CONSOLE_LOG(s)  Serial.print(s)
#define CONSOLE_LOG_LN(s)  Serial.println(s)
#else
#define CONSOLE_LOG(s) 
#define CONSOLE_LOG_LN(s)
#endif

template<typename Data>
class List {

public:
	// Class for an iterator to iterate over this list
	class Iterator; 

protected:
	// Class for items in this list
	class ListItem;

	size_t _list_size; // Stores no. of actually stored objects

	// Pointers to first and last objects
	ListItem *_first;
	ListItem *_last;

public:
	// Default constructor
	List() : _list_size(0), _first(NULL), _last(NULL) {};

	void push_front(Data item) {
		// Get pointer to current first item
		ListItem *previousFirst = _first;

		CONSOLE_LOG(F("push_front: Pointer to current first item is: "));
#ifdef DEBUG
		Serial.println((uint32_t)previousFirst, HEX);
#endif

		// Make new ListItem
		ListItem *newFirst = new ListItem (item, NULL, previousFirst);

		// Point old front item at the new one
		if (NULL != previousFirst) {
			previousFirst->setPrevPointer(newFirst);
		} else {
			// If there was no other element, this is the only one so it's also the last
			_last = newFirst;
		}

		// Save pointer
		_first = newFirst;

		// Increment size
		_list_size++;

#ifdef DEBUG
		debug_front_back("push_front");
#endif
   	}

   	void pop_front() {

		// Get pointer to second item
		ListItem *second = _first->next();

		// Free first item
		delete _first;

		// If there's a second element, update its pointers
		if (NULL != second) {
			second->setPrevPointer(NULL);
		} // If there was no other element, this is the only one so it's also the last
		else {
			_last = NULL;
		}
		
		_first = second;

		// Update count
		_list_size--;

#ifdef DEBUG
		debug_front_back("pop_front");
#endif
   	}

   	void push_back(Data item) {
		// Get pointer to current last item
		ListItem *previousLast = _last;

		// Make new ListItem
		ListItem *newLast = new ListItem (item, previousLast, NULL);

		// Point old back item at the new one
		if (NULL != previousLast) {
			previousLast->setNextPointer(newLast);
		} else {
			// If there was no other element, this is the only one so it's also the first
			_first = newLast;
		}

		// Save pointer
		_last = newLast;

		// Increment size
		_list_size++;

#ifdef DEBUG
		debug_front_back("push_back");
#endif
   	}

   	void pop_back() {

		// Get pointer to penultimate item
		ListItem *penult = _last->prev();

		// Free Last item
		delete _last;

		// If there's a penultimate element, update its pointers
		if (NULL != penult) {
			penult->setNextPointer(NULL);
		} // If there was no other element, this is the only one so it's also the first
		else {
			_first = NULL;
		}

		_last = penult;

		// Update count
		_list_size--;

#ifdef DEBUG
		debug_front_back("pop_back");
#endif
   	}

   	Data front() {
   		return _first->getData();
   	}
	Data back() {
   		return _last->getData();
   	}

	Iterator begin() {
		// Return the first iterator
		return Iterator(_first);
	}

	Iterator end() {
		// Return a "past the end" iterator
		return Iterator(true, _last);
	}

   	bool isEmpty() { return _list_size == 0; }

   	size_t size() { return _list_size; }

   	void debug() {
   		ListItem *p = _first;

   		while (NULL != p) {
			Serial.print("'");
			Serial.print(p->getData());
			Serial.print("', ");
   			p = p->next();
   		}
		Serial.println();
   	}

	void debug_front_back(const char * ident) {
		Serial.print(ident); 
		Serial.print(F(": first: 0x"));
		Serial.print((uint32_t)_first, HEX);
		Serial.print(", ");
		Serial.print(F(": last: 0x"));
		Serial.print((uint32_t)_last, HEX);
		Serial.print(", ");
		Serial.print(F(": length: "));
		Serial.println(_list_size);
	}

	

};

template<typename Data>
class List<Data>::ListItem {

	Data _d;
	ListItem *_nextItem;
	ListItem *_prevItem;

	public:

		ListItem(Data d, ListItem *prev, ListItem *next) :
			_d(d), _nextItem(next), _prevItem(prev) {}

		Data getData() { return _d; }

		void setPrevPointer(ListItem * newPrev) { _prevItem = newPrev; }
		void setNextPointer(ListItem * newNext) { _nextItem = newNext; }
		
		ListItem *next() { return _nextItem; }
		ListItem *prev() { return _prevItem; }

};

template<typename Data>
class List<Data>::Iterator {

	ListItem *_prevItem, *_nextItem, *_currentItem;
	bool _isPastTheEnd;

	// Reset the microprocessor in case of emergency segfault
	void(*resetFunc) (void) = 0;

public:

	// Normal constructor
	Iterator(ListItem * startingItem) :
		_currentItem(startingItem),
		_prevItem(startingItem->prev()),
		_nextItem(startingItem->next()), 
		_isPastTheEnd(false) 
	{}

	// Constuctor for "past the end" iterators
	Iterator(bool pastTheEnd, ListItem * lastItem) :
		_currentItem(NULL),
		_prevItem(lastItem),
		_nextItem(NULL),
		_isPastTheEnd(true)
	{}

	// Postfix increment
	Iterator operator++(int) {
		
		// If there's a next element, iterate to it
		if (NULL != _nextItem) {

			_prevItem = _currentItem;
			_currentItem = _nextItem;
			_nextItem = _nextItem->next();

		}
		else {
			// If not, we've reached the end, so become a past the end iterator
			_prevItem = _prevItem->next();
			_currentItem = NULL;
			_nextItem = NULL;

			_isPastTheEnd = true;
		}

		return *this;
	}

	// Postfix decrement
	Iterator operator--(int) {
		
		// If there's a previous element, iterate to it
		if (NULL != _prevItem) {
			
			_nextItem = _currentItem;
			_currentItem = _prevItem;
			_prevItem = _prevItem->prev();

			_isPastTheEnd = false;
		}
		else {
			// If not, we've reached the front so this behaviour is undefined! 
			_prevItem = NULL;
			_currentItem = NULL;
			_nextItem = _currentItem;
		}

		return *this;
	}

	// Overload dereferencing
	Data operator*() {
		if (NULL != _currentItem) {
			return _currentItem->getData();
		}
		else
		{
			// Error! This should never happen but, if it does, reset the microprocessor
			CONSOLE_LOG_LN("Error! Dereferenced a non-existant iterator")
			resetFunc();
		}
	}

	// Check equality
	bool operator==(const Iterator a) const {
		
		if (_isPastTheEnd && a._isPastTheEnd) {
			return _prevItem == a._prevItem;
		}

		return a._currentItem == _currentItem;
	}

	// Check inequality
	bool operator!=(const Iterator a) const { return !(a == *this); }

};