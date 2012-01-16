/*
 *  Copyright 2011 Sebastian Ventura
 *  This file is part of procs.
 *
 *  procs is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
*
 *  procs is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with procs.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _LIST_H
#define _LIST_H

#include <stdlib.h>

struct item {
	void *p;
	struct item *next;
};

struct list {
	struct item *head, *tail; 
	int len;
};

static inline int
list_len (struct list* l) {
	return l->len;
}

static inline void
list_init(struct list *l) {
	l->head = l->tail = NULL;
	l->len=0;
}
static inline void 
list_append(struct list *l, void *p) 
{
	struct item *node = malloc(sizeof(struct item));
	node->p=p;
	node->next=NULL;
	if (l->head==NULL) {
		l->head=l->tail=node;
	} else {
		l->tail->next=node;
		l->tail=node;
	}
	l->len++;
}
static inline void
list_iterate(struct list l, void (f)(void*))
{
	struct item *i;
  for (i = l.head; i; i = i->next) {
  	f(i->p);
	}
}
static inline void
list_iterate2(struct list l, void* a, void (f)(void*,void*))
{
	struct item *i;
  for (i = l.head; i; i = i->next) {
  	f(i->p,a);
	}
}
static inline void
list_iterate3(struct list l, void* a, void* b, void (f)(void*,void*,void*))
{
	struct item *i;
  for (i = l.head; i; i = i->next) {
  	f(i->p,a,b);
	}
}
static inline void
list_free(struct list l) {
	struct item *i,*n;
  for (i = l.head; i; i = n) {
  	n = i->next;
  	free(i);
	}
}
static inline void*
list_delete(struct list* l, void* a, int (f)(void*,void*))
{
	struct item *i;
	struct item *ant = NULL;
  for (i = l->head; i; i = i->next) {
  	if (f(i->p,a)) {
  		if (ant==NULL) {
  			l->head=i->next;
			} else {
				ant->next=i->next;
			}
			if (i->next==NULL) {
				l->tail=ant;
			}
			l->len--;
			return i;
		}
	}
	return NULL;
}
#endif
