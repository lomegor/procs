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

#ifndef PROC_SAVER_H
#define PROC_SAVER_H

#include "utils/common.h"
#include "utils/list.h"

struct proc_process 
save_process(pid_t pid,char* name, int nokill);

void 
fetch_chunks_tls(pid_t pid, struct list *l);
void
fetch_chunks_regs(pid_t pid, struct list *l);
void
fetch_chunks_vma(pid_t pid, struct list *l, long *bin_offset);
void 
fetch_chunks_fd(pid_t pid, struct list *l);

#endif
