/*
 * Java core library component.
 *
 * Copyright (c) 1997, 1998
 *      Transvirtual Technologies, Inc.  All rights reserved.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file.
 */

package java.util;

import java.io.Serializable;

public class Vector
  implements Cloneable, Serializable
{
	protected Object[] elementData;
	protected int elementCount;
	protected int capacityIncrement;

class Enumerator
  implements Enumeration
{
	int index;

public boolean hasMoreElements (){
	return (index < elementCount);
}

public Object nextElement () {
	if ( index >= elementCount )
		throw new NoSuchElementException();
		
	return elementData[index++];
}
}

public Vector() {
	this( 10, 0);
}

public Vector ( int initialCapacity ) {
	this( initialCapacity, 0);
}

/* May be 0 indicating doubling on reallocation */
public Vector(int initialCapacity, int increment)
	{
	elementData = new Object[initialCapacity];
	elementCount = 0;
	capacityIncrement = increment;
}

final public synchronized void addElement(Object obj) {
	if ( elementCount == elementData.length )
		ensureCapacity( elementCount+1);

	elementData[elementCount++] = obj;
}

final public int capacity() {
	return elementData.length;
}

public synchronized Object clone () {
	Vector v = new Vector( elementData.length, capacityIncrement);
	
	System.arraycopy( elementData, 0, v.elementData, 0, elementCount);
	v.elementCount = elementCount;

	return v;
}

final public boolean contains(Object elem)
	{
	return (indexOf(elem) != -1);
}

final public synchronized void copyInto ( Object anArray[] ) {
	System.arraycopy( elementData, 0, anArray, 0, elementCount);
}

final public synchronized Object elementAt ( int index ) {
	if (index >= elementCount)
		throw new ArrayIndexOutOfBoundsException();
	return elementData[index];
}

final public synchronized Enumeration elements () {
	return new Vector.Enumerator();
}

final public synchronized void ensureCapacity ( int minCapacity ) { 
	if ( elementData.length < minCapacity ) {
		int newCapacity = elementData.length + 
						((capacityIncrement > 0) ? capacityIncrement : elementData.length);

		if ( newCapacity < minCapacity ) newCapacity = minCapacity;
		
		Object oldBuffer[] = elementData;
		elementData = new Object[newCapacity];

		System.arraycopy( oldBuffer, 0, elementData, 0, elementCount);
	}
}

final public synchronized Object firstElement () {
	if ( elementCount <= 0 )
		throw new NoSuchElementException();

	return elementData[0];
}

final public int indexOf(Object elem)
	{
	return indexOf(elem, 0);
}

final public synchronized int indexOf(Object elem, int index)
	{
	for (int pos = index; pos < elementCount; pos++) {
		if (elem.equals(elementData[pos])) {
			return (pos);
		}
	}
	return (-1);
}

final public synchronized void insertElementAt ( Object obj, int index ) {

	if ( elementCount == elementData.length )
		ensureCapacity( elementCount+1);

	if ( (index > elementCount) || (index < 0) ) {
		throw new ArrayIndexOutOfBoundsException();
	}
	else if ( index < elementCount ) {
		System.arraycopy( elementData, index, elementData, index+1, elementCount-index);
	}

	elementData[index] = obj;
	elementCount++;
}

final public boolean isEmpty () {
	return (elementCount == 0);
}

final public synchronized Object lastElement () {
	if ( elementCount == 0 )
		throw new NoSuchElementException();

	return elementData[elementCount-1];
}

final public int lastIndexOf(Object elem)
	{
	return (lastIndexOf(elem, size()-1));
}

final public synchronized int lastIndexOf(Object elem, int index)
	{
	for (int pos = index; pos >= 0; pos--) {
		if (elem.equals(elementData[pos])) {
			return (pos);
		}
	}
	return (-1);
}

final public synchronized void removeAllElements () {
	for ( int i=elementCount-1; i>= 0; i-- )
		elementData[i] = null;

	elementCount=0;
}

final public synchronized boolean removeElement(Object obj)
	{
	if (contains(obj)) {
		removeElementAt(indexOf(obj));
		return (true);
	}
	else {
		return (false);
	}
}

final public synchronized void removeElementAt ( int index ) {
	if ( index >= elementCount )
		throw new ArrayIndexOutOfBoundsException();

	System.arraycopy( elementData, index+1, elementData, index, elementCount-index-1);
	elementCount--;

	elementData[elementCount] = null;
}

final public synchronized void setElementAt(Object obj, int index)
	{
	if (index >= elementCount) {
		throw new ArrayIndexOutOfBoundsException();
	}
	elementData[index] = obj;
}

final public synchronized void setSize(int newSize)
	{
	ensureCapacity(newSize);
	elementCount = newSize;
}

final public int size()
	{
	return elementCount;
}

final public synchronized String toString() {
	String result=new String();

	for (int pos=0; pos<size(); pos++) {
		result=result+elementData[pos].toString();
	}

	return result;
}

final public synchronized void trimToSize()
	{
	setSize(size());
}
}
